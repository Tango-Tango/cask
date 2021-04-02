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
    MapTaskObserver(std::function<Task<TO,E>(TI)> predicate, std::shared_ptr<Observer<TO,E>> downstream);

    Task<Ack,None> onNext(TI value);
    Task<None,None> onError(E error);
    Task<None,None> onComplete();
private:
    std::function<Task<TO,E>(TI)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    std::atomic_flag completed;
};


template <class TI, class TO, class E>
MapTaskObserver<TI,TO,E>::MapTaskObserver(std::function<Task<TO,E>(TI)> predicate, ObserverRef<TO,E> downstream)
    : predicate(predicate)
    , downstream(downstream)
    , completed(false)
{}

template <class TI, class TO, class E>
Task<Ack,None> MapTaskObserver<TI,TO,E>::onNext(TI value) {
    return predicate(value).template flatMapBoth<Ack,None>(
        [this](auto downstreamValue) { return downstream->onNext(downstreamValue); },
        [this](auto error) {
            return onError(error).template map<Ack>([](auto) {
                return Stop;
            });
        }
    );
}

template <class TI, class TO, class E>
Task<None,None> MapTaskObserver<TI,TO,E>::onError(E error) {
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