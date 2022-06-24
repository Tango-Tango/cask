//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SWITCH_MAP_OBSERVABLE_H_
#define _CASK_SWITCH_MAP_OBSERVABLE_H_

#include "../Observable.hpp"
#include "SwitchMapObserver.hpp"

namespace cask::observable {

/**
 * Represents an observable that transforms each element from an upstream observable
 * using the given predicate function. When upstream provides a new value the downstream
 * observable is cancelled and a new subscription is started. Normally obtained by calling
 * `Observable<T>::switchMap`.
 */
template <class TI, class TO, class E>
class SwitchMapObservable final : public Observable<TO,E> {
public:
    SwitchMapObservable(const ObservableConstRef<TI,E>& upstream, const std::function<ObservableRef<TO,E>(const TI&)>& predicate);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,E>>& observer) const override;
private:
    ObservableConstRef<TI,E> upstream;
    std::function<ObservableRef<TO,E>(const TI&)> predicate;
};


template <class TI, class TO, class E>
SwitchMapObservable<TI,TO,E>::SwitchMapObservable(
    const ObservableConstRef<TI,E>& upstream,
    const std::function<ObservableRef<TO,E>(const TI&)>& predicate
)
    : upstream(upstream)
    , predicate(predicate)
{}

template <class TI, class TO, class E>
FiberRef<None,None> SwitchMapObservable<TI,TO,E>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,E>>& observer) const {
    auto switchMapObserver = std::make_shared<SwitchMapObserver<TI,TO,E>>(predicate, observer, sched);
    return upstream->subscribe(sched, switchMapObserver);
}

} // namespace cask::observable

#endif