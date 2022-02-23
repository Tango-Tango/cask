//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_DEFER_OBSERVABLE_
#define _CASK_DEFER_OBSERVABLE_

#include "../Observable.hpp"
#include "../Observer.hpp"

namespace cask::observable {

template <class T, class E> class DeferObservable final : public Observable<T, E> {
public:
    explicit DeferObservable(const std::function<ObservableRef<T, E>()>& predicate);
    FiberRef<None, None> subscribe(const std::shared_ptr<Scheduler>& sched,
                                   const std::shared_ptr<Observer<T, E>>& observer) const override;

private:
    std::function<ObservableRef<T, E>()> predicate;
};

template <class T, class E>
DeferObservable<T, E>::DeferObservable(const std::function<ObservableRef<T, E>()>& predicate)
    : predicate(predicate) {}

template <class T, class E>
FiberRef<None, None> DeferObservable<T, E>::subscribe(const std::shared_ptr<Scheduler>& sched,
                                                      const std::shared_ptr<Observer<T, E>>& observer) const {
    try {
        return predicate()->subscribe(sched, observer);
    } catch (E& error) {
        return observer->onError(error).run(sched);
    }
}

} // namespace cask::observable

#endif
