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
    TakeWhileObservable(const ObservableConstRef<T,E>& upstream, std::function<bool(const T&)>&& predicate, bool inclusive);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const;
private:
    ObservableConstRef<T,E> upstream;
    std::function<bool(const T&)> predicate;
    bool inclusive;
};

template <class T, class E>
TakeWhileObservable<T,E>::TakeWhileObservable(const ObservableConstRef<T,E>& upstream, std::function<bool(const T&)>&& predicate, bool inclusive)
    : upstream(upstream)
    , predicate(std::move(predicate))
    , inclusive(inclusive)
{}

template <class T, class E>
FiberRef<None,None> TakeWhileObservable<T,E>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const {
    auto takeWhileObserver = std::make_shared<TakeWhileObserver<T,E>>(observer, predicate, inclusive);
    return upstream->subscribe(sched, takeWhileObserver);
}

} // namespace cask::observable

#endif