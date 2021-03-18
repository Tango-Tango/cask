//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_TAKE_OBSERVER_H_
#define _CASK_TAKE_OBSERVER_H_

#include "../Observer.hpp"
#include "../Deferred.hpp"

namespace cask::observable {

/**
 * Implements an observer that accumulates a given number of events from the stream and
 * once set number of events is accumulated completes a promise and stops the stream. This
 * is normally used via the `Observable<T>::take` method.
 */
template <class T, class E>
class TakeObserver final : public Observer<T,E> {
public:
    TakeObserver(unsigned int amount, std::weak_ptr<Promise<std::vector<T>,E>> promise);
    DeferredRef<Ack,E> onNext(T value);
    void onError(E error);
    void onComplete();
private:
    int remaining;
    std::vector<T> entries;
    std::weak_ptr<Promise<std::vector<T>,E>> promise;
};

template <class T, class E>
TakeObserver<T,E>::TakeObserver(unsigned int amount, std::weak_ptr<Promise<std::vector<T>,E>> promise)
    : remaining(amount)
    , entries()
    , promise(promise)
{}

template <class T, class E>
DeferredRef<Ack,E> TakeObserver<T,E>::onNext(T value) {
    entries.push_back(value);
    remaining -= 1;

    if(remaining <= 0) {
        if(auto promiseLock = promise.lock()) {
            promiseLock->success(entries);
        }
        return Deferred<Ack,E>::pure(Stop);
    } else {
        return Deferred<Ack,E>::pure(Continue);
    }
}

template <class T, class E>
void TakeObserver<T,E>::onError(E error) {
    if(auto promiseLock = promise.lock()) {
        promiseLock->error(error);
    }
    
}

template <class T, class E>
void TakeObserver<T,E>::onComplete() {
    if(auto promiseLock = promise.lock()) {
        if(!promiseLock->get().has_value()) {
            promiseLock->success(entries);
        }
    }
}

}

#endif