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
    TakeObserver(uint32_t amount, const std::weak_ptr<Promise<std::vector<T>,E>>& promise);
    Task<Ack,None> onNext(const T& value) override;
    Task<None,None> onError(const E& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    uint32_t remaining;
    std::vector<T> entries;
    std::weak_ptr<Promise<std::vector<T>,E>> promise;
};

template <class T, class E>
TakeObserver<T,E>::TakeObserver(uint32_t amount, const std::weak_ptr<Promise<std::vector<T>,E>>& promise)
    : remaining(amount)
    , entries()
    , promise(promise)
{}

template <class T, class E>
Task<Ack,None> TakeObserver<T,E>::onNext(const T& value) {
    entries.push_back(value);
    remaining -= 1;

    if(remaining <= 0) {
        if(auto promiseLock = promise.lock()) {
            promiseLock->success(entries);
        }
        return Task<Ack,None>::pure(Stop);
    } else {
        return Task<Ack,None>::pure(Continue);
    }
}

template <class T, class E>
Task<None,None> TakeObserver<T,E>::onError(const E& error) {
    if(auto promiseLock = promise.lock()) {
        promiseLock->error(error);
    }

    return Task<None,None>::none();
}

template <class T, class E>
Task<None,None> TakeObserver<T,E>::onComplete() {
    if(auto promiseLock = promise.lock()) {
        if(!promiseLock->get().has_value()) {
            promiseLock->success(entries);
        }
    }

    return Task<None,None>::none();
}

template <class T, class E>
Task<None,None> TakeObserver<T,E>::onCancel() {
    if(auto promiseLock = promise.lock()) {
        promiseLock->cancel();
    }

    return Task<None,None>::none();
}

} // namespace cask::observable

#endif