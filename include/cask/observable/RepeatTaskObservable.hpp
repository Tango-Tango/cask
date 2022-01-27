//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_REPEAT_TASK_OBSERVABLE_H_
#define _CASK_REPEAT_TASK_OBSERVABLE_H_

#include "../Observable.hpp"
#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents and observable that repeatedly evaluates the given task and supplies the result
 * to the downstream observer. Usually obtained via `Observable<T>::repeatTask`.
 */
template <class T, class E>
class RepeatTaskObservable final : public Observable<T,E> {
public: 
    explicit RepeatTaskObservable(const Task<T,E>& task);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    Task<T,E> task;
};

template <class T, class E>
RepeatTaskObservable<T,E>::RepeatTaskObservable(const Task<T,E>& task)
    : task(task)
{}

template <class T, class E>
FiberRef<None,None> RepeatTaskObservable<T,E>::subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const {
    std::function<Task<Ack,None>(T)> pushToObserver =
        [observer = observer](T value) -> Task<Ack,None> {
            return observer->onNext(value);
        };

    std::function<Task<Ack,None>(E)> pushError =
        [observer = observer](E error) -> Task<Ack,None> {
            return observer->onError(error)
                .template map<Ack>([](auto) {
                    return Stop;
                });
        };


    return task.template flatMapBoth<Ack,None>(pushToObserver,pushError)
        .restartUntil([](auto ack) { return ack == Stop; })
        .template map<None>([](auto) { return None(); })
        .doOnCancel(Task<None,None>::defer([observer] { return observer->onCancel(); }))
        .run(sched);
}

} // namespace cask::observable

#endif