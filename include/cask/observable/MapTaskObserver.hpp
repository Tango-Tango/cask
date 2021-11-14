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
class MapTaskObserver final : public Observer<TI,E> {
public:
    MapTaskObserver(const std::function<Task<TO,E>(const TI&)>& predicate, const std::shared_ptr<Observer<TO,E>>& downstream);

    Task<Ack,None> onNext(const TI& value) override;
    Task<None,None> onError(const E& error) override;
    Task<None,None> onComplete() override;
private:
    std::function<Task<TO,E>(const TI&)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    std::atomic_flag completed;
};


template <class TI, class TO, class E>
MapTaskObserver<TI,TO,E>::MapTaskObserver(const std::function<Task<TO,E>(const TI&)>& predicate, const ObserverRef<TO,E>& downstream)
    : predicate(predicate)
    , downstream(downstream)
    , completed(false)
{}

template <class TI, class TO, class E>
Task<Ack,None> MapTaskObserver<TI,TO,E>::onNext(const TI& value) {
    return predicate(value).template flatMapBoth<Ack,None>(
        [downstream = downstream](auto downstreamValue) { return downstream->onNext(downstreamValue); },
        [self_weak = this->weak_from_this()](auto error) {
            if(auto self = self_weak.lock()) {
                return self->onError(error).template map<Ack>([](auto) {
                    return Stop;
                });
            } else {
                return Task<Ack,None>::pure(Stop);
            }
        }
    );
}

template <class TI, class TO, class E>
Task<None,None> MapTaskObserver<TI,TO,E>::onError(const E& error) {
    if(!completed.test_and_set()) {
        return downstream->onError(error);
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

}

#endif