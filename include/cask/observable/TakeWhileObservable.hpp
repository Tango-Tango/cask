//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_TAKE_WHILE_OBSERVABLE_H_
#define _CASK_TAKE_WHILE_OBSERVABLE_H_

#include "../Observable.hpp"
#include "TakeWhileObserver.hpp"

namespace cask::observable {

/**
 * Represents an observable that transforms each element from an upstream observable
 * using the given predicate function. Normally obtained by calling `Observable<T>::map`.
 */
template <class T, class E>
class TakeWhileObservable final : public Observable<T,E> {
public:
    TakeWhileObservable(std::shared_ptr<const Observable<T,E>> upstream, std::function<bool(T)> predicate, bool inclusive);
    CancelableRef subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const;
private:
    std::shared_ptr<const Observable<T,E>> upstream;
    std::function<bool(T)> predicate;
    bool inclusive;
};

template <class T, class E>
TakeWhileObservable<T,E>::TakeWhileObservable(std::shared_ptr<const Observable<T,E>> upstream, std::function<bool(T)> predicate, bool inclusive)
    : upstream(upstream)
    , predicate(predicate)
    , inclusive(inclusive)
{}

template <class T, class E>
CancelableRef TakeWhileObservable<T,E>::subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const {
    auto takeWhileObserver = std::make_shared<TakeWhileObserver<T,E>>(observer, predicate, inclusive);
    return upstream->subscribe(sched, takeWhileObserver);
}

}

#endif