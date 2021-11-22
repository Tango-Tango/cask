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
    explicit AppendAllObservable(const std::shared_ptr<const Observable<T,E>>& first, const std::shared_ptr<const Observable<T,E>>& second);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    std::shared_ptr<const Observable<T,E>> first;
    std::shared_ptr<const Observable<T,E>> second;
};

template <class T, class E>
AppendAllObservable<T,E>::AppendAllObservable(const std::shared_ptr<const Observable<T,E>>& first, const std::shared_ptr<const Observable<T,E>>& second)
    : first(first)
    , second(second)
{}

template <class T, class E>
FiberRef<None,None> AppendAllObservable<T,E>::subscribe(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& observer) const
{
    auto appendObserver = std::make_shared<AppendAllObserver<T,E>>(sched, observer, second);
    return first->subscribe(sched, appendObserver);
}

}

#endif
