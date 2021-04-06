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
    MapErrorObservable(std::shared_ptr<const Observable<T,EI>> upstream, std::function<EO(EI)> predicate);
    CancelableRef subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,EO>>& observer) const;
private:
    std::shared_ptr<const Observable<T,EI>> upstream;
    std::function<EO(EI)> predicate;
};

template <class T, class EI, class EO>
MapErrorObservable<T,EI,EO>::MapErrorObservable(std::shared_ptr<const Observable<T,EI>> upstream, std::function<EO(EI)> predicate)
    : upstream(upstream)
    , predicate(predicate)
{}

template <class T, class EI, class EO>
CancelableRef MapErrorObservable<T,EI,EO>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,EO>>& observer) const {
    auto mapObserver = std::make_shared<MapErrorObserver<T,EI,EO>>(predicate, observer);
    return upstream->subscribe(sched, mapObserver);
}

}

#endif
