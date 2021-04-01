//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_OBSERVER_H_
#define _CASK_MAP_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class TI, class TO, class E>
class MapObserver final : public Observer<TI,E> {
public:
    MapObserver(std::function<TO(TI)> predicate, std::shared_ptr<Observer<TO,E>> downstream);
    Task<Ack,None> onNext(TI value);
    Task<None,None> onError(E error);
    Task<None,None> onComplete();
private:
    std::function<TO(TI)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
};


template <class TI, class TO, class E>
MapObserver<TI,TO,E>::MapObserver(std::function<TO(TI)> predicate, std::shared_ptr<Observer<TO,E>> downstream)
    : predicate(predicate)
    , downstream(downstream)
{}

template <class TI, class TO, class E>
Task<Ack,None> MapObserver<TI,TO,E>::onNext(TI value) {
    return downstream->onNext(predicate(value));
}

template <class TI, class TO, class E>
Task<None,None> MapObserver<TI,TO,E>::onError(E error) {
    return downstream->onError(error);
}

template <class TI, class TO, class E>
Task<None,None> MapObserver<TI,TO,E>::onComplete() {
    return downstream->onComplete();
}

}

#endif