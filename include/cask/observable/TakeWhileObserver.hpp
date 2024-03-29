//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_TAKE_WHILE_OBSERVER_H_
#define _CASK_TAKE_WHILE_OBSERVER_H_

#include "../Observer.hpp"
#include "../Deferred.hpp"

namespace cask::observable {

/**
 * Implements an observer that accumulates a given number of events from the stream and
 * once set number of events is accumulated completes a promise and stops the stream. This
 * is normally used via the `Observable<T>::take` method.
 */
template <class T, class E>
class TakeWhileObserver final : public Observer<T,E>, public std::enable_shared_from_this<TakeWhileObserver<T,E>> {
public:
    TakeWhileObserver(
        const ObserverRef<T,E>& downstream,
        const std::function<bool(const T&)>& predicate,
        bool inclusive
    );
    Task<Ack,None> onNext(T&& value) override;
    Task<None,None> onError(E&& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    ObserverRef<T,E> downstream;
    std::function<bool(const T&)> predicate;
    bool inclusive;
    std::atomic_flag completed = ATOMIC_FLAG_INIT;
};

template <class T, class E>
TakeWhileObserver<T,E>::TakeWhileObserver(
    const ObserverRef<T,E>& downstream,
    const std::function<bool(const T&)>& predicate,
    bool inclusive
)
    : downstream(downstream)
    , predicate(predicate)
    , inclusive(inclusive)
{}

template <class T, class E>
Task<Ack,None> TakeWhileObserver<T,E>::onNext(T&& value) {
    if(predicate(value)) {
        return downstream->onNext(std::forward<T>(value));
    } else {
        if(inclusive) {
            return downstream->onNext(std::forward<T>(value))
                .template flatMap<None>([self_weak = this->weak_from_this()](auto) {
                    if(auto self = self_weak.lock()) {
                        return self->onComplete();
                    } else {
                        return Task<None,None>::none();
                    }
                })
                .template map<Ack>([](auto) {
                    return Stop;
                });
        } else {
            return onComplete().template map<Ack>([](auto) {
                return Stop;
            });
        }
    }
}

template <class T, class E>
Task<None,None> TakeWhileObserver<T,E>::onError(E&& error) {
    if(!completed.test_and_set()) {
        return downstream->onError(std::forward<E>(error));
    } else {
        return Task<None,None>::none();
    }
}

template <class T, class E>
Task<None,None> TakeWhileObserver<T,E>::onComplete() {
    if(!completed.test_and_set()) {
        return downstream->onComplete();
    } else {
        return Task<None,None>::none();
    }
}

template <class T, class E>
Task<None,None> TakeWhileObserver<T,E>::onCancel() {
    if(!completed.test_and_set()) {
        return downstream->onCancel();
    } else {
        return Task<None,None>::none();
    }
}

} // namespace cask::observable

#endif