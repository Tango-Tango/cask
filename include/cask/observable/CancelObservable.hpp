//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_CANCEL_OBSERVABLE_H_
#define _CASK_CANCEL_OBSERVABLE_H_

#include "../Observable.hpp"
#include "../Observer.hpp"

namespace cask::observable {

template <class T, class E>
class CancelObservable final : public Observable<T,E> {
public:
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    friend class Observable<T,E>;
};

template <class T, class E>
FiberRef<None,None> CancelObservable<T,E>::subscribe(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& observer) const
{
    return Task<None,None>::defer([observer] {
            return observer->onCancel();
        })
        .doOnCancel(Task<None,None>::defer([observer] {
            return observer->onCancel();
        }))
        .run(sched);

}

} // namespace cask::observable

#endif