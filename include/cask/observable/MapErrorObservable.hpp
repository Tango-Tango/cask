//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_ERROR_OBSERVABLE_H_
#define _CASK_MAP_ERROR_OBSERVABLE_H_

#include "../Observable.hpp"
#include "MapErrorObserver.hpp"

namespace cask::observable {

/**
 * Represents an observable that transforms each error from an upstream observable
 * using the given predicate function. Normally obtained by calling `Observable<T>::mapError`.
 */
template <class T, class EI, class EO>
class MapErrorObservable final : public Observable<T,EO> {
public:
    MapErrorObservable(const ObservableConstRef<T,EI>& upstream, const std::function<EO(const EI&)>& predicate);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,EO>>& observer) const override;
private:
    ObservableConstRef<T,EI> upstream;
    std::function<EO(const EI&)> predicate;
};

template <class T, class EI, class EO>
MapErrorObservable<T,EI,EO>::MapErrorObservable(const ObservableConstRef<T,EI>& upstream, const std::function<EO(const EI&)>& predicate)
    : upstream(upstream)
    , predicate(predicate)
{}

template <class T, class EI, class EO>
FiberRef<None,None> MapErrorObservable<T,EI,EO>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,EO>>& observer) const {
    auto mapObserver = std::make_shared<MapErrorObserver<T,EI,EO>>(predicate, observer);
    return upstream->subscribe(sched, mapObserver);
}

} // namespace cask::observable

#endif
