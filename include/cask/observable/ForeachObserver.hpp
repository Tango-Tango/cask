//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#ifndef _CASK_FOREACH_OBSERVER_H_
#define _CASK_FOREACH_OBSERVER_H_

#include "../Observer.hpp"
#include "../Deferred.hpp"

namespace cask::observable {

/**
 * Implements an observer that memoizes the latest event in the stream and, upon stream
 * completion, completes a promise with the last event seen (or nothing, if the stream
 * was empty). Normally obtained by using `Observer<T>::last()`.
 */
template <class T, class E>
class ForeachObserver final : public Observer<T,E> {
public:
    explicit ForeachObserver(
        const std::weak_ptr<Promise<None,E>>& promise,
        const std::function<void(const T& value)>& predicate);

    Task<Ack,None> onNext(const T& value) override;
    Task<None,None> onError(const E& error) override;
    Task<None,None> onComplete() override;
private:
    std::weak_ptr<Promise<None,E>> promise;
    std::function<void(const T& value)> predicate;
};

template <class T, class E>
ForeachObserver<T,E>::ForeachObserver(
    const std::weak_ptr<Promise<None,E>>& promise,
    const std::function<void(const T& value)>& predicate
)
    : promise(promise)
    , predicate(predicate)
{}

template <class T, class E>
Task<Ack, None> ForeachObserver<T,E>::onNext(const T& value) {
    predicate(value);
    return Task<Ack,None>::pure(cask::Continue);
}

template <class T, class E>
Task<None,None> ForeachObserver<T,E>::onError(const E& error) {
    if(auto promiseLock = promise.lock()) {
        promiseLock->error(error);
    }

    return Task<None,None>::none();
}

template <class T, class E>
Task<None,None>  ForeachObserver<T,E>::onComplete() {
    if(auto promiseLock = promise.lock()) {
        promiseLock->success(None());
    }

    return Task<None,None>::none();
}

}

#endif