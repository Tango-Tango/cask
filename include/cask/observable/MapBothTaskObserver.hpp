//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_BOTH_TASK_OBSERVER_H_
#define _CASK_MAP_BOTH_TASK_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class TI, class TO, class EI, class EO>
class MapBothTaskObserver final : public Observer<TI, EI> {
public:
    MapBothTaskObserver(const std::function<Task<TO, EO>(const TI&)>& successPredicate,
                        const std::function<Task<TO, EO>(const EI&)>& errorPredicate,
                        const std::shared_ptr<Observer<TO, EO>>& downstream);

    Task<Ack, None> onNext(const TI& value) override;
    Task<None, None> onError(const EI& error) override;
    Task<None, None> onComplete() override;
    Task<None, None> onCancel() override;

private:
    std::function<Task<TO, EO>(const TI&)> successPredicate;
    std::function<Task<TO, EO>(const EI&)> errorPredicate;
    std::shared_ptr<Observer<TO, EO>> downstream;
    std::shared_ptr<std::atomic_flag> completed;
};

template <class TI, class TO, class EI, class EO>
MapBothTaskObserver<TI, TO, EI, EO>::MapBothTaskObserver(const std::function<Task<TO, EO>(const TI&)>& successPredicate,
                                                         const std::function<Task<TO, EO>(const EI&)>& errorPredicate,
                                                         const ObserverRef<TO, EO>& downstream)
    : successPredicate(successPredicate)
    , errorPredicate(errorPredicate)
    , downstream(downstream)
    , completed(std::make_shared<std::atomic_flag>(false)) {}

template <class TI, class TO, class EI, class EO>
Task<Ack, None> MapBothTaskObserver<TI, TO, EI, EO>::onNext(const TI& value) {
    return successPredicate(value).template flatMapBoth<Ack, None>(
        [downstream = downstream](TO downstreamValue) -> Task<Ack, None> {
            return downstream->onNext(downstreamValue);
        },
        [downstream = downstream, completed = completed](EO downstreamError) -> Task<Ack, None> {
            if (!completed->test_and_set()) {
                return downstream->onError(downstreamError).template map<Ack>([](auto) {
                    return Stop;
                });
            } else {
                return Task<Ack, None>::pure(Stop);
            }
        });
}

template <class TI, class TO, class EI, class EO>
Task<None, None> MapBothTaskObserver<TI, TO, EI, EO>::onError(const EI& error) {
    if (!completed->test_and_set()) {
        return errorPredicate(error).template flatMapBoth<None, None>(
            [downstream = downstream](TO downstreamValue) -> Task<None, None> {
                return downstream->onNext(downstreamValue).template flatMap<None>([downstream](auto) {
                    return downstream->onComplete();
                });
            },
            [downstream = downstream](EO downstreamError) -> Task<None, None> {
                return downstream->onError(downstreamError);
            });
    } else {
        return Task<None, None>::none();
    }
}

template <class TI, class TO, class EI, class EO>
Task<None, None> MapBothTaskObserver<TI, TO, EI, EO>::onComplete() {
    if (!completed->test_and_set()) {
        return downstream->onComplete();
    } else {
        return Task<None, None>::none();
    }
}

template <class TI, class TO, class EI, class EO>
Task<None, None> MapBothTaskObserver<TI, TO, EI, EO>::onCancel() {
    if (!completed->test_and_set()) {
        return downstream->onCancel();
    } else {
        return Task<None, None>::none();
    }
}

} // namespace cask::observable

#endif