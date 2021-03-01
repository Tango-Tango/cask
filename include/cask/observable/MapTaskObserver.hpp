//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_TASK_OBSERVER_H_
#define _CASK_MAP_TASK_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class TI, class TO, class E>
class MapTaskObserver final : public Observer<TI,E> {
public:
    MapTaskObserver(std::function<Task<TO,E>(TI)> predicate, std::shared_ptr<Observer<TO,E>> downstream, std::shared_ptr<Scheduler> sched);

    DeferredRef<Ack,E> onNext(TI value);
    void onError(E error);
    void onComplete();
private:
    std::function<Task<TO,E>(TI)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    std::shared_ptr<Scheduler> sched;
};


template <class TI, class TO, class E>
MapTaskObserver<TI,TO,E>::MapTaskObserver(std::function<Task<TO,E>(TI)> predicate, ObserverRef<TO,E> downstream, std::shared_ptr<Scheduler> sched)
    : predicate(predicate)
    , downstream(downstream)
    , sched(sched)
{}

template <class TI, class TO, class E>
DeferredRef<Ack,E> MapTaskObserver<TI,TO,E>::onNext(TI value) {
    return predicate(value)
    .template flatMap<Ack>([downstream = downstream](auto result) {
        return Task<Ack,E>::deferAction([downstream, result](auto) {
            return downstream->onNext(result);
        });
    })
    .run(sched);
}

template <class TI, class TO, class E>
void MapTaskObserver<TI,TO,E>::onError(E error) {
    downstream->onError(error);
}

template <class TI, class TO, class E>
void MapTaskObserver<TI,TO,E>::onComplete() {
    downstream->onComplete();
}

}

#endif