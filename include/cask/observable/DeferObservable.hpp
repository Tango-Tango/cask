//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_DEFER_OBSERVABLE_
#define _CASK_DEFER_OBSERVABLE_

#include "../Observable.hpp"
#include "../Observer.hpp"

namespace cask::observable {

template <class T, class E>
class DeferObservable final : public Observable<T,E> {
public:
    DeferObservable(std::function<ObservableRef<T,E>()> predicate);
    CancelableRef<E> subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const;
private:
    std::function<ObservableRef<T,E>()> predicate;
};

template <class T, class E>
DeferObservable<T,E>::DeferObservable(std::function<ObservableRef<T,E>()> predicate)
    : predicate(predicate)
{}

template <class T, class E>
CancelableRef<E> DeferObservable<T,E>::subscribe(
    std::shared_ptr<Scheduler> sched,
    std::shared_ptr<Observer<T,E>> observer) const
{
    try {
        return predicate()->subscribe(sched, observer);
    } catch (E& error) {
        observer->onError(error);
        return std::make_unique<IgnoreCancelation<E>>();
    }
}

}

#endif
