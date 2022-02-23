//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MAP_DEFERRED_H_
#define _CASK_MAP_DEFERRED_H_

#include "../Deferred.hpp"

namespace cask::deferred {

template <class T1, class T2, class E1, class E2>
class MapDeferred final : public Deferred<T2, E2> {
public:
    MapDeferred(DeferredRef<T1, E1> deferred,
                std::function<T2(T1)> value_transform,
                std::function<E2(E1)> error_transform);

    void onComplete(std::function<void(Either<T2, E2>)> callback) override;
    void onSuccess(std::function<void(T2)> callback) override;
    void onError(std::function<void(E2)> callback) override;
    void onCancel(const std::function<void()>& callback) override;
    void onShutdown(const std::function<void()>& callback) override;
    void cancel() override;
    T2 await() override;

private:
    DeferredRef<T1, E1> deferred;
    std::function<T2(T1)> value_transform;
    std::function<E2(E1)> error_transform;
};

template <class T1, class T2, class E1, class E2>
MapDeferred<T1, T2, E1, E2>::MapDeferred(DeferredRef<T1, E1> deferred,
                                         std::function<T2(T1)> value_transform,
                                         std::function<E2(E1)> error_transform)
    : deferred(deferred)
    , value_transform(value_transform)
    , error_transform(error_transform) {}

template <class T1, class T2, class E1, class E2>
void MapDeferred<T1, T2, E1, E2>::onComplete(std::function<void(Either<T2, E2>)> callback) {
    deferred->onComplete([callback, value_transform = value_transform, error_transform = error_transform](auto result) {
        if (result.is_left()) {
            callback(Either<T2, E2>::left(value_transform(result.get_left())));
        } else {
            callback(Either<T2, E2>::right(error_transform(result.get_right())));
        }
    });
}

template <class T1, class T2, class E1, class E2>
void MapDeferred<T1, T2, E1, E2>::onSuccess(std::function<void(T2)> callback) {
    deferred->onSuccess([callback, value_transform = value_transform](auto result) {
        callback(value_transform(result));
    });
}

template <class T1, class T2, class E1, class E2>
void MapDeferred<T1, T2, E1, E2>::onError(std::function<void(E2)> callback) {
    deferred->onError([callback, error_transform = error_transform](auto result) {
        callback(error_transform(result));
    });
}

template <class T1, class T2, class E1, class E2>
void MapDeferred<T1, T2, E1, E2>::onCancel(const std::function<void()>& callback) {
    deferred->onCancel(callback);
}

template <class T1, class T2, class E1, class E2>
void MapDeferred<T1, T2, E1, E2>::onShutdown(const std::function<void()>& callback) {
    deferred->onShutdown(callback);
}

template <class T1, class T2, class E1, class E2>
void MapDeferred<T1, T2, E1, E2>::cancel() {
    deferred->cancel();
}

template <class T1, class T2, class E1, class E2>
T2 MapDeferred<T1, T2, E1, E2>::await() {
    try {
        return value_transform(deferred->await());
    } catch (const E1& error) {
        throw error_transform(error);
    }
}

} // namespace cask::deferred

#endif