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
    GuaranteeObserver(std::shared_ptr<Observer<T,E>> downstream, const Task<None,E>& task, std::shared_ptr<Scheduler> sched);
    DeferredRef<Ack,E> onNext(T value);
    void onError(E error);
    void onComplete();

    void cleanup();

private:
    std::shared_ptr<Observer<T,E>> downstream;
    Task<None,E> task;
    std::shared_ptr<Scheduler> sched;
    std::atomic_flag completed;
};

template <class T, class E>
GuaranteeObserver<T,E>::GuaranteeObserver(std::shared_ptr<Observer<T,E>> downstream, const Task<None,E>& task, std::shared_ptr<Scheduler> sched)
    : downstream(downstream)
    , task(task)
    , sched(sched)
    , completed(false)
{}

template <class T, class E>
DeferredRef<Ack,E> GuaranteeObserver<T,E>::onNext(T value) {
    return downstream->onNext(value);
}

template <class T, class E>
void GuaranteeObserver<T,E>::onError(E error) {
    downstream->onError(error);
    cleanup();
}

template <class T, class E>
void GuaranteeObserver<T,E>::onComplete() {
    downstream->onComplete();
    cleanup();
}

template <class T, class E>
void GuaranteeObserver<T,E>::cleanup() {
    if(!completed.test_and_set()) {
        task.run(sched)->await();
    }
}

}

#endif