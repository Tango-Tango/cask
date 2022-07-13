//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_GUARANTEE_OBSERVER_H_
#define _CASK_GUARANTEE_OBSERVER_H_

#include <atomic>
#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class T, class E>
class GuaranteeObserver final : public Observer<T,E> {
public:
    GuaranteeObserver(const std::shared_ptr<Observer<T,E>>& downstream, const Task<None,None>& task);
    Task<Ack,None> onNext(T&& value) override;
    Task<None,None> onError(const E& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;

private:
    std::shared_ptr<Observer<T,E>> downstream;
    Task<None,None> task;
    std::shared_ptr<Scheduler> sched;
    std::shared_ptr<std::atomic_flag> completed;
};

template <class T, class E>
GuaranteeObserver<T,E>::GuaranteeObserver(const std::shared_ptr<Observer<T,E>>& downstream, const Task<None,None>& task)
    : downstream(downstream)
    , task(task)
    , completed(std::make_shared<std::atomic_flag>())
{
    completed->clear();
}

template <class T, class E>
Task<Ack,None> GuaranteeObserver<T,E>::onNext(T&& value) {
    return downstream->onNext(std::forward<T>(value)).template flatMap<Ack>([task = task, completed = completed](auto ack) {
        if(ack == cask::Stop && !completed->test_and_set()) {
            return task.template map<Ack>([](auto) {
                return cask::Stop;
            });
        } else {
            return Task<Ack,None>::pure(ack);
        }
    });
}

template <class T, class E>
Task<None,None> GuaranteeObserver<T,E>::onError(const E& error) {
    if(!completed->test_and_set()) {
        return downstream->onError(error)
            .template flatMap<None>([task = task](auto) {
                return task;
            });
    } else {
        return Task<None,None>::none();
    }
}

template <class T, class E>
Task<None,None> GuaranteeObserver<T,E>::onComplete() {
    if(!completed->test_and_set()) {
        return downstream->onComplete()
            .template flatMap<None>([task = task](auto) {
                return task;
            });
    } else {
        return Task<None,None>::none();
    }
}

template <class T, class E>
Task<None,None> GuaranteeObserver<T,E>::onCancel() {
    if(!completed->test_and_set()) {
        return downstream->onCancel().template guarantee<None>(task);
    } else {
        return Task<None,None>::none();
    }
}

} // namespace cask::observable

#endif