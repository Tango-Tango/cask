//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_ERROR_OBSERVER_H_
#define _CASK_MAP_ERROR_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each error received on a stream to a new value and emits the
 * transformed error to a downstream observer. Normally obtained by calling `Observable<T>::mapError` and
 * then subscribing to the resulting observable.
 */
template <class T, class EI, class EO>
class MapErrorObserver final : public Observer<T,EI> {
public:
    MapErrorObserver(std::function<EO(EI)> predicate, std::shared_ptr<Observer<T,EO>> downstream, std::shared_ptr<Scheduler> sched);
    DeferredRef<Ack,EI> onNext(T value);
    void onError(EI error);
    void onComplete();
private:
    std::function<EO(EI)> predicate;
    std::shared_ptr<Observer<T,EO>> downstream;
    std::shared_ptr<Scheduler> sched;
};


template <class T, class EI, class EO>
MapErrorObserver<T,EI,EO>::MapErrorObserver(std::function<EO(EI)> predicate, std::shared_ptr<Observer<T,EO>> downstream, std::shared_ptr<Scheduler> sched)
    : predicate(predicate)
    , downstream(downstream)
    , sched(sched)
{}

template <class T, class EI, class EO>
DeferredRef<Ack,EI> MapErrorObserver<T,EI,EO>::onNext(T value) {
    auto promise = Promise<Ack, EI>::create(sched);
    auto deferred = downstream->onNext(value);
    deferred->template chainDownstream<Ack, EI>(
        promise,
        [predicate=predicate](auto result) {
            if(result.is_right()) { return Either<Ack, EI>::right(predicate(result.get_right())); }
            else { return Either<Ack,EI>::left(result.get_left()); }
        }
    );
    return Deferred<Ack,EI>::forPromise(promise);
}

template <class T, class EI, class EO>
void MapErrorObserver<T,EI,EO>::onError(EI error) {
    downstream->onError(predicate(error));
}

template <class T, class EI, class EO>
void MapErrorObserver<T,EI,EO>::onComplete() {
    downstream->onComplete();
}

}

#endif
