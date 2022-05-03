//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_DISTINCT_UNTIL_CHANGED_OBSERVABLE_H_
#define _CASK_DISTINCT_UNTIL_CHANGED_OBSERVABLE_H_

#include "../Observable.hpp"
#include "DistinctUntilChangedObserver.hpp"

namespace cask::observable {

/**
 * Represents an observable that suppresses consecutive events that are the same - emitting
 * only the first.
 */
template <class T, class E>
class DistinctUntilChangedObservable final : public Observable<T,E> {
public:
    explicit DistinctUntilChangedObservable(const std::shared_ptr<const Observable<T,E>>& upstream);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    std::shared_ptr<const Observable<T,E>> upstream;
};

template <class T, class E>
DistinctUntilChangedObservable<T,E>::DistinctUntilChangedObservable(const std::shared_ptr<const Observable<T,E>>& upstream)
    : upstream(upstream)
{}

template <class T, class E>
FiberRef<None,None> DistinctUntilChangedObservable<T,E>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const {
    auto distinctObserver = std::make_shared<DistinctUntilChangedObserver<T,E>>(observer);
    return upstream->subscribe(sched, distinctObserver);
}

} // namespace cask::observable

#endif