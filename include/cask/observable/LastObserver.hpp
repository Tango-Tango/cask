//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#ifndef _CASK_LAST_OBSERVER_H_
#define _CASK_LAST_OBSERVER_H_

#include "../Observer.hpp"
#include "../Deferred.hpp"

namespace cask::observable {

/**
 * Implements an observer that memoizes the latest event in the stream and, upon stream
 * completion, completes a promise with the last event seen (or nothing, if the stream
 * was empty). Normally obtained by using `Observer<T>::last()`.
 */
template <class T, class E>
class LastObserver final : public Observer<T,E> {
public:
    explicit LastObserver(std::weak_ptr<Promise<std::optional<T>,E>> promise);

    DeferredRef<Ack,E> onNext(T value);
    void onError(E error);
    void onComplete();
private:
    std::optional<T> lastValue;
    std::weak_ptr<Promise<std::optional<T>,E>> promise;
};

template <class T, class E>
LastObserver<T,E>::LastObserver(std::weak_ptr<Promise<std::optional<T>,E>> promise)
    : lastValue()
    , promise(promise)
{}

template <class T, class E>
DeferredRef<Ack, E> LastObserver<T,E>::onNext(T value) {
    lastValue = value;
    return Deferred<Ack,E>::pure(Continue);
}

template <class T, class E>
void LastObserver<T,E>::onError(E error) {
    if(auto promiseLock = promise.lock()) {
        promiseLock->error(error);
    }
}

template <class T, class E>
void LastObserver<T,E>::onComplete() {
    if(auto promiseLock = promise.lock()) {
        promiseLock->success(lastValue);
    }   
}

}

#endif