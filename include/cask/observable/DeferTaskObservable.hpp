//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_DEFER_TASK_OBSERVABLE_
#define _CASK_DEFER_TASK_OBSERVABLE_

#include "../Observable.hpp"
#include "../Observer.hpp"

namespace cask::observable {

template <class T, class E>
class DeferTaskObservable final : public Observable<T,E> {
public:
    explicit DeferTaskObservable(std::function<Task<T,E>()> predicate);
    CancelableRef subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const;
private:
    std::function<Task<T,E>()> predicate;
};

template <class T, class E>
DeferTaskObservable<T,E>::DeferTaskObservable(std::function<Task<T,E>()> predicate)
    : predicate(predicate)
{}

template <class T, class E>
CancelableRef DeferTaskObservable<T,E>::subscribe(
    std::shared_ptr<Scheduler> sched,
    std::shared_ptr<Observer<T,E>> observer) const
{
    try {
        return predicate()
        .template flatMapBoth<None,None>(
            [observer](auto result) {
                return observer->onNext(result)
                .template flatMap<None>([observer](auto) {
                    return observer->onComplete();
                });
            },
            [observer](auto error) {
                return observer->onError(error);
            }
        )
        .run(sched);
    } catch(E& error) {
        return observer->onError(error).run(sched);
    }
}

}

#endif