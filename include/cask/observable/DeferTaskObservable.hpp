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
    DeferTaskObservable(std::function<Task<T,E>()> predicate);
    CancelableRef<E> subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const;
private:
    std::function<Task<T,E>()> predicate;
};

template <class T, class E>
DeferTaskObservable<T,E>::DeferTaskObservable(std::function<Task<T,E>()> predicate)
    : predicate(predicate)
{}

template <class T, class E>
CancelableRef<E> DeferTaskObservable<T,E>::subscribe(
    std::shared_ptr<Scheduler> sched,
    std::shared_ptr<Observer<T,E>> observer) const
{
    try {
        return predicate()
        .template flatMap<Ack>([observer](auto result) {
            return Task<Ack,E>::deferAction([observer,result](auto) {
                return observer->onNext(result);
            });
        })
        .onError([observer](auto error) {
            observer->onError(error);
        })
        .template map<None>([observer](auto) {
            observer->onComplete();
            return None();
        })
        .run(sched);
    } catch(E& error) {
        observer->onError(error);
        return std::make_shared<IgnoreCancelation<E>>();
    }
}

}

#endif