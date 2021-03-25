//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_PURE_ERROR_DEFERRED_H_
#define _CASK_PURE_ERROR_DEFERRED_H_

#include "../Deferred.hpp"

namespace cask::deferred {

template <class T, class E>
class PureErrorDeferred final : public Deferred<T,E> {
public:
    constexpr explicit PureErrorDeferred(const E& error);
    const E error;

    void onComplete(std::function<void(Either<T,E>)> callback) override;
    void onSuccess(std::function<void(T)> callback) override;
    void onError(std::function<void(E)> callback) override;
    void cancel() override;
    T await() override;
};

template <class T, class E>
constexpr PureErrorDeferred<T,E>::PureErrorDeferred(const E& error)
    : error(error)
{}

template <class T, class E>
void PureErrorDeferred<T,E>::onComplete(std::function<void(Either<T,E>)> callback) {
    return callback(Either<T,E>::right(error));
}

template <class T, class E>
void PureErrorDeferred<T,E>::onSuccess(std::function<void(T)>) {
    return;
}

template <class T, class E>
void PureErrorDeferred<T,E>::onError(std::function<void(E)> callback) {
    return callback(error);
}

template <class T, class E>
void PureErrorDeferred<T,E>::cancel() {
    return;
}

template <class T, class E>
T PureErrorDeferred<T,E>::await() {
    throw error;
}

}

#endif