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
    

    Task<Ack,None> onNext(TI value);
    Task<None,None> onError(E value);
    Task<None,None> onComplete();
private:
    std::function<ObservableRef<TO,E>(TI)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    std::shared_ptr<Scheduler> sched;
    std::atomic_flag completed;
};


template <class TI, class TO, class E>
FlatMapObserver<TI,TO,E>::FlatMapObserver(
    std::function<ObservableRef<TO,E>(TI)> predicate,
    std::shared_ptr<Observer<TO,E>> downstream,
    std::shared_ptr<Scheduler> sched)
    : predicate(predicate)
    , downstream(downstream)
    , sched(sched)
    , completed(false)
{}

template <class TI, class TO, class E>
Task<Ack,None> FlatMapObserver<TI,TO,E>::onNext(TI value) {
    return predicate(value)
        ->template mapBothTask<Ack,None>(
            [downstream = downstream](auto result) {
                return downstream->onNext(result);
            },
            [this](auto error) {
                return onError(error).template map<Ack>([](auto) {
                    return Stop;
                });
            }
        )
        ->takeWhileInclusive([](auto ack){
            return ack == Continue;
        })
        ->last()
        .template map<Ack>([downstream = downstream](auto lastAckOpt) {
            if(lastAckOpt.has_value()) {
                return *lastAckOpt;
            } else {
                return Continue;
            }
        });
}

template <class TI, class TO, class E>
Task<None,None> FlatMapObserver<TI,TO,E>::onError(E error) {
    if(!completed.test_and_set()) {
        return downstream->onError(error);
    } else {
        return Task<None,None>::none();
    }
}

template <class TI, class TO, class E>
Task<None,None> FlatMapObserver<TI,TO,E>::onComplete() {
    if(!completed.test_and_set()) {
        return downstream->onComplete();
    } else {
        return Task<None,None>::none();
    }
}

}

#endif