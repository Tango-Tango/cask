//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FLAT_SCAN_OBSERVABLE_H_
#define _CASK_FLAT_SCAN_OBSERVABLE_H_

#include "../Observable.hpp"
#include "FlatScanObserver.hpp"

namespace cask::observable {

template <class TI, class TO, class E>
class FlatScanObservable final : public Observable<TO,E> {
public:
    FlatScanObservable(
        const ObservableConstRef<TI,E>& upstream,
        const TO& seed,
        const std::function<ObservableRef<TO,E>(const TO&, const TI&)>& predicate);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,E>>& observer) const override;
private:
    ObservableConstRef<TI,E> upstream;
    TO seed;
    std::function<ObservableRef<TO,E>(const TO&, const TI&)> predicate;
};


template <class TI, class TO, class E>
FlatScanObservable<TI,TO,E>::FlatScanObservable(
    const ObservableConstRef<TI,E>& upstream,
    const TO& seed,
    const std::function<ObservableRef<TO,E>(const TO&, const TI&)>& predicate
)
    : upstream(upstream)
    , seed(seed)
    , predicate(predicate)
{}

template <class TI, class TO, class E>
FiberRef<None,None> FlatScanObservable<TI,TO,E>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<TO,E>>& observer) const {
    auto flatScanObserver = std::make_shared<FlatScanObserver<TI,TO,E>>(seed, predicate, observer);
    return upstream->subscribe(sched, flatScanObserver);
}

} // namespace cask::observable

#endif