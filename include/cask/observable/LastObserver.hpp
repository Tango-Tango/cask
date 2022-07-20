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
    explicit LastObserver(const std::weak_ptr<Promise<std::optional<T>,E>>& promise);

    Task<Ack,None> onNext(T&& value) override;
    Task<None,None> onError(E&& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::optional<T> lastValue;
    std::weak_ptr<Promise<std::optional<T>,E>> promise;
    std::atomic_flag completed = ATOMIC_FLAG_INIT;
};

template <class T, class E>
LastObserver<T,E>::LastObserver(const std::weak_ptr<Promise<std::optional<T>,E>>& promise)
    : lastValue()
    , promise(promise)
{}

template <class T, class E>
Task<Ack, None> LastObserver<T,E>::onNext(T&& value) {
    lastValue = value;
    return Task<Ack,None>::pure(Continue);
}

template <class T, class E>
Task<None,None> LastObserver<T,E>::onError(E&& error) {
    if(!completed.test_and_set()) {
        if(auto promiseLock = promise.lock()) {
            promiseLock->error(std::forward<E>(error));
        }
    }

    return Task<None,None>::none();
}

template <class T, class E>
Task<None,None>  LastObserver<T,E>::onComplete() {
    if(!completed.test_and_set()) {
        if(auto promiseLock = promise.lock()) {
            promiseLock->success(lastValue);
        }
    }

    return Task<None,None>::none();
}

template <class T, class E>
Task<None,None>  LastObserver<T,E>::onCancel() {
    if(!completed.test_and_set()) {
        if(auto promiseLock = promise.lock()) {
            promiseLock->cancel();
        }
    }

    return Task<None,None>::none();
}

} // namespace cask::observable

#endif