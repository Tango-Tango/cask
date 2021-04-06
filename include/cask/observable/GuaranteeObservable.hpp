//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_GUARANTEE_OBSERVABLE_H_
#define _CASK_GUARANTEE_OBSERVABLE_H_

#include "../Observable.hpp"
#include "GuaranteeObserver.hpp"

namespace cask::observable {

/**
 * Represents an observable that transforms each element from an upstream observable
 * using the given predicate function. Normally obtained by calling `Observable<T>::map`.
 */
template <class T, class E>
class GuaranteeObservable final : public Observable<T,E> {
public:
    GuaranteeObservable(std::shared_ptr<const Observable<T,E>> upstream, const Task<None,None>& task);
    CancelableRef subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const;
private:
    std::shared_ptr<const Observable<T,E>> upstream;
    Task<None,None> task;
};

template <class T, class E>
GuaranteeObservable<T,E>::GuaranteeObservable(std::shared_ptr<const Observable<T,E>> upstream, const Task<None,None>& task)
    : upstream(upstream)
    , task(task)
{}

template <class T, class E>
CancelableRef GuaranteeObservable<T,E>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const {
    auto guaranteeObserver = std::make_shared<GuaranteeObserver<T,E>>(observer, task);
    auto subscription = upstream->subscribe(sched, guaranteeObserver);
    subscription->onCancel([guaranteeObserver, sched]() {
        guaranteeObserver->onCancel().run(sched)->await();
    });
    return subscription;
}

}

#endif