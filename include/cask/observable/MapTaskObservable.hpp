//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_TASK_OBSERVABLE_H_
#define _CASK_MAP_TASK_OBSERVABLE_H_

#include "../Observable.hpp"
#include "MapTaskObserver.hpp"

namespace cask::observable {

/**
 * Represents an observable that transforms each element from an upstream observable
 * using the given predicate function. Normally obtained by calling `Observable<T>::map`.
 */
template <class TI, class TO, class E>
class MapTaskObservable final : public Observable<TO,E> {
public:
    MapTaskObservable(std::shared_ptr<const Observable<TI,E>> upstream, std::function<Task<TO,E>(TI)> predicate);
    CancelableRef subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,E>>& observer) const;
private:
    std::shared_ptr<const Observable<TI,E>> upstream;
    std::function<Task<TO,E>(TI)> predicate;
};

template <class TI, class TO, class E>
MapTaskObservable<TI,TO,E>::MapTaskObservable(std::shared_ptr<const Observable<TI,E>> upstream, std::function<Task<TO,E>(TI)> predicate)
    : upstream(upstream)
    , predicate(predicate)
{}

template <class TI, class TO, class E>
CancelableRef MapTaskObservable<TI,TO,E>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,E>>& observer) const {
    auto mapObserver = std::make_shared<MapTaskObserver<TI,TO,E>>(predicate, observer);
    return upstream->subscribe(sched, mapObserver);
}

}

#endif