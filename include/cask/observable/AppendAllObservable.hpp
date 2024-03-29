//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_APPEND_ALL_OBSERVABLE_
#define _CASK_APPEND_ALL_OBSERVABLE_

#include "../Observable.hpp"
#include "../Observer.hpp"
#include "AppendAllObserver.hpp"

namespace cask::observable {

template <class T, class E>
class AppendAllObservable final : public Observable<T,E> {
public:
    explicit AppendAllObservable(ObservableConstRef<T,E>&& first, ObservableConstRef<T,E>&& second);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    ObservableConstRef<T,E> first;
    ObservableConstRef<T,E> second;
};

template <class T, class E>
AppendAllObservable<T,E>::AppendAllObservable(ObservableConstRef<T,E>&& first, ObservableConstRef<T,E>&& second)
    : first(std::move(first))
    , second(std::move(second))
{}

template <class T, class E>
FiberRef<None,None> AppendAllObservable<T,E>::subscribe(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& observer) const
{
    auto appendObserver = std::make_shared<AppendAllObserver<T,E>>(sched, observer, second);
    return first->subscribe(sched, appendObserver);
}

} // namespace cask::observable

#endif
