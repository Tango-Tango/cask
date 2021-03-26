//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FILTER_OBSERVER_H_
#define _CASK_FILTER_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class T, class E>
class FilterObserver final : public Observer<T,E> {
public:
    FilterObserver(std::function<bool(T)> predicate, std::shared_ptr<Observer<T,E>> downstream);
    DeferredRef<Ack,E> onNext(T value) override;
    void onError(E error) override;
    void onComplete() override;
private:
    std::function<bool(T)> predicate;
    std::shared_ptr<Observer<T,E>> downstream;
};


template <class T, class E>
FilterObserver<T,E>::FilterObserver(std::function<bool(T)> predicate, std::shared_ptr<Observer<T,E>> downstream)
    : predicate(predicate)
    , downstream(downstream)
{}

template <class T, class E>
DeferredRef<Ack,E> FilterObserver<T,E>::onNext(T value) {
    if(predicate(value)) {
        return downstream->onNext(value);
    } else {
        return Deferred<Ack,E>::pure(Continue);
    }
}

template <class T, class E>
void FilterObserver<T,E>::onError(E error) {
    downstream->onError(error);
}

template <class T, class E>
void FilterObserver<T,E>::onComplete() {
    downstream->onComplete();
}

}

#endif