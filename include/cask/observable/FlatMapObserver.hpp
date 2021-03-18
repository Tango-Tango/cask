//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FLAT_MAP_OBSERVER_H_
#define _CASK_FLAT_MAP_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class TI, class TO, class E>
class FlatMapObserver final : public Observer<TI,E> {
public:
    FlatMapObserver(std::function<ObservableRef<TO,E>(TI)> predicate,
                    std::shared_ptr<Observer<TO,E>> downstream,
                    std::shared_ptr<Scheduler> sched);
    

    DeferredRef<Ack,E> onNext(TI value);
    void onError(E value);
    void onComplete();
private:

    std::function<ObservableRef<TO,E>(TI)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    std::shared_ptr<Scheduler> sched;
};


template <class TI, class TO, class E>
FlatMapObserver<TI,TO,E>::FlatMapObserver(
    std::function<ObservableRef<TO,E>(TI)> predicate,
    std::shared_ptr<Observer<TO,E>> downstream,
    std::shared_ptr<Scheduler> sched)
    : predicate(predicate)
    , downstream(downstream)
    , sched(sched)
{}

template <class TI, class TO, class E>
DeferredRef<Ack,E> FlatMapObserver<TI,TO,E>::onNext(TI value) {
    return predicate(value)
    ->template mapTask<Ack>([downstream = downstream](auto resultValue) {
        return Task<Ack,E>::deferAction([downstream, resultValue](auto) {
            return downstream->onNext(resultValue);
        });
    })
    ->takeWhileInclusive([](auto ack){
        return ack == Continue;
    })
    ->last()
    .onError([downstream = downstream](auto error) {
        downstream->onError(error);
    })
    .template map<Ack>([downstream = downstream](auto lastAckOpt) {
        if(lastAckOpt.has_value()) {
            return *lastAckOpt;
        } else {
            return Continue;
        }
    })
    .run(sched);
}

template <class TI, class TO, class E>
void FlatMapObserver<TI,TO,E>::onError(E error) {
    return downstream->onError(error);
}

template <class TI, class TO, class E>
void FlatMapObserver<TI,TO,E>::onComplete() {
    return downstream->onComplete();
}

}

#endif