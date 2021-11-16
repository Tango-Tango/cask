//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FLAT_MAP_OBSERVABLE_H_
#define _CASK_FLAT_MAP_OBSERVABLE_H_

#include "../Observable.hpp"
#include "FlatMapObserver.hpp"

namespace cask::observable {

/**
 * Represents an observable that transforms each element from an upstream observable
 * using the given predicate function. Normally obtained by calling `Observable<T>::map`.
 */
template <class TI, class TO, class E>
class FlatMapObservable final : public Observable<TO,E> {
public:
    FlatMapObservable(const std::shared_ptr<const Observable<TI,E>>& upstream, const std::function<ObservableRef<TO,E>(const TI&)>& predicate);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,E>>& observer) const override;
private:
    std::shared_ptr<const Observable<TI,E>> upstream;
    std::function<ObservableRef<TO,E>(const TI&)> predicate;
};


template <class TI, class TO, class E>
FlatMapObservable<TI,TO,E>::FlatMapObservable(
    const std::shared_ptr<const Observable<TI,E>>& upstream,
    const std::function<ObservableRef<TO,E>(const TI&)>& predicate
)
    : upstream(upstream)
    , predicate(predicate)
{}

template <class TI, class TO, class E>
FiberRef<None,None> FlatMapObservable<TI,TO,E>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,E>>& observer) const {
    auto flatMapObserver = std::make_shared<FlatMapObserver<TI,TO,E>>(predicate, observer);
    return upstream->subscribe(sched, flatMapObserver);
}

}

#endif