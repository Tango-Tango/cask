//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_TASK_OBSERVER_H_
#define _CASK_MAP_TASK_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class TI, class TO, class E>
class MapTaskObserver final : public Observer<TI,E>, public std::enable_shared_from_this<MapTaskObserver<TI,TO,E>> {
public:
    MapTaskObserver(const std::function<Task<TO,E>(TI&&)>& predicate, const std::shared_ptr<Observer<TO,E>>& downstream);

    Task<Ack,None> onNext(TI&& value) override;
    Task<None,None> onError(E&& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::function<Task<TO,E>(TI&&)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    std::atomic_flag completed = ATOMIC_FLAG_INIT;
};


template <class TI, class TO, class E>
MapTaskObserver<TI,TO,E>::MapTaskObserver(const std::function<Task<TO,E>(TI&&)>& predicate, const ObserverRef<TO,E>& downstream)
    : predicate(predicate)
    , downstream(downstream)
{}

template <class TI, class TO, class E>
Task<Ack,None> MapTaskObserver<TI,TO,E>::onNext(TI&& value) {
    return predicate(std::forward<TI>(value)).template flatMapBoth<Ack,None>(
        [downstream = downstream](auto&& downstreamValue) { return downstream->onNext(std::forward<TO>(downstreamValue)); },
        [self_weak = this->weak_from_this()](auto&& error) {
            if(auto self = self_weak.lock()) {
                return self->onError(std::forward<E>(error)).template flatMap<Ack>([](auto&&) {
                    return Task<Ack,None>::raiseError(None());
                });
            } else {
                return Task<Ack,None>::pure(Stop);
            }
        }
    );
}

template <class TI, class TO, class E>
Task<None,None> MapTaskObserver<TI,TO,E>::onError(E&& error) {
    if(!completed.test_and_set()) {
        return downstream->onError(std::forward<E>(error));
    } else {
        return Task<None,None>::none();
    }
}

template <class TI, class TO, class E>
Task<None,None> MapTaskObserver<TI,TO,E>::onComplete() {
    if(!completed.test_and_set()) {
        return downstream->onComplete();
    } else {
        return Task<None,None>::none();
    }
}

template <class TI, class TO, class E>
Task<None,None> MapTaskObserver<TI,TO,E>::onCancel() {
    if(!completed.test_and_set()) {
        return downstream->onCancel();
    } else {
        return Task<None,None>::none();
    }
}

} // namespace cask::observable

#endif