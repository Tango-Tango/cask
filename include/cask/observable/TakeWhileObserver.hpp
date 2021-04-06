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
class TakeWhileObserver final : public Observer<T,E> {
public:
    TakeWhileObserver(
        ObserverRef<T,E> downstream,
        std::function<bool(T)> predicate,
        bool inclusive
    );
    Task<Ack,None> onNext(const T& value);
    Task<None,None> onError(const E& error);
    Task<None,None> onComplete();
private:
    ObserverRef<T,E> downstream;
    std::function<bool(T)> predicate;
    bool inclusive;
    std::atomic_flag completed;
};

template <class T, class E>
TakeWhileObserver<T,E>::TakeWhileObserver(
    ObserverRef<T,E> downstream,
    std::function<bool(T)> predicate,
    bool inclusive
)
    : downstream(downstream)
    , predicate(predicate)
    , inclusive(inclusive)
    , completed(false)
{}

template <class T, class E>
Task<Ack,None> TakeWhileObserver<T,E>::onNext(const T& value) {
    if(predicate(value)) {
        return downstream->onNext(value);
    } else {
        if(inclusive) {
            return downstream->onNext(value)
                .template flatMap<None>([this](auto) {
                    return onComplete();
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
Task<None,None> TakeWhileObserver<T,E>::onError(const E& error) {
    if(!completed.test_and_set()) {
        downstream->onError(error);
    }

    return Task<None,None>::none();
}

template <class T, class E>
Task<None,None> TakeWhileObserver<T,E>::onComplete() {
    if(!completed.test_and_set()) {
        downstream->onComplete();
    }

    return Task<None,None>::none();
}

}

#endif