//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_BOTH_TASK_OBSERVABLE_H_
#define _CASK_MAP_BOTH_TASK_OBSERVABLE_H_

#include "../Observable.hpp"
#include "MapBothTaskObserver.hpp"

namespace cask::observable {

/**
 * Represents an observable that transforms each element from an upstream observable
 * using the given predicate function. Normally obtained by calling `Observable<T>::map`.
 */
template <class TI, class TO, class EI, class EO>
class MapBothTaskObservable final : public Observable<TO,EO> {
public:
    MapBothTaskObservable(
        const ObservableConstRef<TI,EI>& upstream,
        std::function<Task<TO,EO>(TI&&)>&& successPredicate,
        std::function<Task<TO,EO>(EI&&)>&& errorPredicate
    );
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,EO>>& observer) const override;
private:
    ObservableConstRef<TI,EI> upstream;
    std::function<Task<TO,EO>(TI&&)> successPredicate;
    std::function<Task<TO,EO>(EI&&)> errorPredicate;
};

template <class TI, class TO, class EI, class EO>
MapBothTaskObservable<TI,TO,EI,EO>::MapBothTaskObservable(
    const ObservableConstRef<TI,EI>& upstream,
    std::function<Task<TO,EO>(TI&&)>&& successPredicate,
    std::function<Task<TO,EO>(EI&&)>&& errorPredicate
)
    : upstream(upstream)
    , successPredicate(std::move(successPredicate))
    , errorPredicate(std::move(errorPredicate))
{}

template <class TI, class TO, class EI, class EO>
FiberRef<None,None> MapBothTaskObservable<TI,TO,EI,EO>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,EO>>& observer) const {
    auto mapObserver = std::make_shared<MapBothTaskObserver<TI,TO,EI,EO>>(successPredicate, errorPredicate, observer);
    return upstream->subscribe(sched, mapObserver);
}

} // namespace cask::observable

#endif