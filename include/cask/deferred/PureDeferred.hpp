//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_PURE_DEFERRED_H_
#define _CASK_PURE_DEFERRED_H_

#include "../Deferred.hpp"

namespace cask::deferred {

template <class T, class E>
class PureDeferred final : public Deferred<T,E> {
public:
    constexpr PureDeferred(const T& value);
    constexpr PureDeferred(T&& value);

    const T value;

    void onComplete(std::function<void(Either<T,E>)> callback) override;
    void onSuccess(std::function<void(T)> callback) override;
    void onError(std::function<void(E)> callback) override;
    void cancel(const E& override) override;
    T await();
private:
};

template <class T, class E>
constexpr PureDeferred<T,E>::PureDeferred(const T& value)
    : value(value)
{}

template <class T, class E>
constexpr PureDeferred<T,E>::PureDeferred(T&& value)
    : value(std::move(value))
{}

template <class T, class E>
void PureDeferred<T,E>::onComplete(std::function<void(Either<T,E>)> callback) {
    return callback(Either<T,E>::left(value));
}

template <class T, class E>
void PureDeferred<T,E>::onSuccess(std::function<void(T)> callback) {
    return callback(value);
}

template <class T, class E>
void PureDeferred<T,E>::onError(std::function<void(E)> callback) {
    return;
}

template <class T, class E>
void PureDeferred<T,E>::cancel(const E& error) {
    return;
}

template <class T, class E>
T PureDeferred<T,E>::await() {
    return value;
}

}

#endif