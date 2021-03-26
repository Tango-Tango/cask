//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_TAKE_WHILE_OBSERVER_H_
#define _CASK_TAKE_WHILE_OBSERVER_H_

#include "../Observer.hpp"
#include "../Deferred.hpp"

namespace cask::observable {

/**
 * Implements an observer that accumulates a given number of events from the stream and
 * once set number of events is accumulated completes a promise and stops the stream. This
 * is normally used via the `Observable<T>::take` method.
 */
template <class T, class E>
class TakeWhileObserver final : public Observer<T,E> {
public:
    TakeWhileObserver(
        std::shared_ptr<Scheduler> sched,
        ObserverRef<T,E> downstream,
        std::function<bool(T)> predicate,
        bool inclusive
    );
    DeferredRef<Ack,E> onNext(T value);
    void onError(E error);
    void onComplete();
private:
    std::shared_ptr<Scheduler> sched;
    ObserverRef<T,E> downstream;
    std::function<bool(T)> predicate;
    bool inclusive;
    std::atomic_flag completed;
};

template <class T, class E>
TakeWhileObserver<T,E>::TakeWhileObserver(
    std::shared_ptr<Scheduler> sched,
    ObserverRef<T,E> downstream,
    std::function<bool(T)> predicate,
    bool inclusive
)
    : sched(sched)
    , downstream(downstream)
    , predicate(predicate)
    , inclusive(inclusive)
    , completed(false)
{}

template <class T, class E>
DeferredRef<Ack,E> TakeWhileObserver<T,E>::onNext(T value) {
    if(predicate(value)) {
        return downstream->onNext(value);
    } else {
        if(inclusive) {
            return Task<Ack,E>::deferAction([downstream = this->downstream, value](auto) {
                    return downstream->onNext(value);
                })
                .template map<Ack>([this](auto) {
                    onComplete();
                    return Stop;
                })
                .run(sched);
        } else {
            onComplete();
            return Deferred<Ack,E>::pure(Stop);
        }
    }
}

template <class T, class E>
void TakeWhileObserver<T,E>::onError(E error) {
    if(!completed.test_and_set()) {
        downstream->onError(error);
    }
}

template <class T, class E>
void TakeWhileObserver<T,E>::onComplete() {
    if(!completed.test_and_set()) {
        downstream->onComplete();
    }
}

}

#endif