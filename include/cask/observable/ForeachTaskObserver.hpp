//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#ifndef _CASK_FOREACH_TASK_OBSERVER_H_
#define _CASK_FOREACH_TASK_OBSERVER_H_

#include "../Observer.hpp"
#include "../Deferred.hpp"

namespace cask::observable {

/**
 * Implements an observer that memoizes the latest event in the stream and, upon stream
 * completion, completes a promise with the last event seen (or nothing, if the stream
 * was empty). Normally obtained by using `Observer<T>::last()`.
 */
template <class T, class E>
class ForeachTaskObserver final : public Observer<T,E>, public std::enable_shared_from_this<ForeachTaskObserver<T,E>> {
public:
    explicit ForeachTaskObserver(
        const std::weak_ptr<Promise<None,E>>& promise,
        const std::function<Task<None,E>(T&& value)>& predicate);

    Task<Ack,None> onNext(T&& value) override;
    Task<None,None> onError(E&& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::weak_ptr<Promise<None,E>> promise;
    std::function<Task<None,E>(T&& value)> predicate;
};

template <class T, class E>
ForeachTaskObserver<T,E>::ForeachTaskObserver(
    const std::weak_ptr<Promise<None,E>>& promise,
    const std::function<Task<None,E>(T&& value)>& predicate
)
    : promise(promise)
    , predicate(predicate)
{}

template <class T, class E>
Task<Ack, None> ForeachTaskObserver<T,E>::onNext(T&& value) {
    return predicate(std::forward<T>(value))
        .template flatMapBoth<Ack, None>(
            [](auto&&) {
                return Task<Ack,None>::pure(cask::Continue);
            },
            [self_weak = this->weak_from_this()](auto&& error) {
                if(auto self = self_weak.lock()) {
                    return self->onError(std::forward<E>(error)).template flatMap<Ack>([](auto&&) {
                        return Task<Ack,None>::raiseError(None());
                    });
                } else {
                    return Task<Ack,None>::raiseError(None());
                }
            }
        );
}

template <class T, class E>
Task<None,None> ForeachTaskObserver<T,E>::onError(E&& error) {
    if(auto promiseLock = promise.lock()) {
        promiseLock->error(std::forward<E>(error));
    }

    return Task<None,None>::none();
}

template <class T, class E>
Task<None,None> ForeachTaskObserver<T,E>::onComplete() {
    if(auto promiseLock = promise.lock()) {
        promiseLock->success(None());
    }

    return Task<None,None>::none();
}

template <class T, class E>
Task<None,None> ForeachTaskObserver<T,E>::onCancel() {
    if(auto promiseLock = promise.lock()) {
        promiseLock->cancel();
    }

    return Task<None,None>::none();
}

} // namespace cask::observable

#endif