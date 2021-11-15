//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_DEFER_TASK_OBSERVABLE_
#define _CASK_DEFER_TASK_OBSERVABLE_

#include "../Observable.hpp"
#include "../Observer.hpp"
#include <iostream>

namespace cask::observable {

template <class T, class E>
class DeferTaskObservable final : public Observable<T,E> {
public:
    explicit DeferTaskObservable(const std::function<Task<T,E>()>& predicate);
    CancelableRef subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    std::function<Task<T,E>()> predicate;
};

template <class T, class E>
DeferTaskObservable<T,E>::DeferTaskObservable(const std::function<Task<T,E>()>& predicate)
    : predicate(predicate)
{}

template <class T, class E>
CancelableRef DeferTaskObservable<T,E>::subscribe(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& observer) const
{
    auto downstreamTask = Task<None,None>::defer([predicate = predicate, observer] {
        try {
            return predicate().template flatMapBoth<None,None>(
                [observer](auto result) {
                    return observer->onNext(result)
                    .template flatMap<None>([observer](auto) {
                        return observer->onComplete();
                    });
                },
                [observer](auto error) {
                    return observer->onError(error);
                }
            );
        } catch(E& error) {
            return observer->onError(error);
        }
    });

    return downstreamTask
        .doOnCancel(Task<None,None>::defer([observer] {
            std::cout << "CANCEL FIRED" << std::endl;
            return observer->onCancel();
        }))
        .run(sched);
}

}

#endif