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
    RepeatTaskObservable(Task<T,E> task);
    CancelableRef<E> subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const;
private:
    Task<T,E> task;
};

template <class T, class E>
RepeatTaskObservable<T,E>::RepeatTaskObservable(Task<T,E> task)
    : task(task)
{}

template <class T, class E>
CancelableRef<E> RepeatTaskObservable<T,E>::subscribe(std::shared_ptr<Scheduler> sched, std::shared_ptr<Observer<T,E>> observer) const {
    std::function<Task<Ack,E>(T)> pushToObserver =
        [observer = observer](T value) -> Task<Ack,E> {
            return Task<Ack,E>::deferAction([observer,value](auto) {
                return observer->onNext(value);
            });
        };

    return task.flatMap(pushToObserver)
        .restartUntil([](auto ack) { return ack == Stop; })
        .run(sched);
}

}

#endif