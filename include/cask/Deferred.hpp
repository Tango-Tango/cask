//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_DEFERRED_H_
#define _CASK_DEFERRED_H_

#include <exception>
#include <functional>
#include <memory>

#include "Promise.hpp"

namespace cask {

template <class T, class E> class Fiber;

template <class T, class E> using FiberRef = std::shared_ptr<Fiber<T, E>>;

template <class T, class E> class Deferred;

template <class T = None, class E = std::any> using DeferredRef = std::shared_ptr<Deferred<T, E>>;

/**
 * A `Deferred` represents the "consumer" side of a running asynchronous operation. Consumers
 * are notified of results from a producer either synchronously (via blocking with `await`) or
 * asynchronously (via `onComplete`). `Deferred` instances can also be chained together with
 * other synchronous (via `map`) or asynchronouse (via `flatMap`) operations.
 */
template <class T = None, class E = std::any>
class Deferred
    : public Cancelable
    , public std::enable_shared_from_this<Deferred<T, E>> {
public:
    /**
     * Create a deferred instance wrapping the already computed pure vale.
     *
     * @param value The value of this completed deferred.
     * @return A deferred wrapping the given value.
     */
    constexpr static DeferredRef<T, E> pure(const T& value) noexcept;
    constexpr static DeferredRef<T, E> pure(T&& value) noexcept;

    /**
     * Create a defered instance wrapping the given error.
     *
     * @param value The value of this completed deferred.
     * @return A deferred wrapping the given value.
     */
    constexpr static DeferredRef<T, E> raiseError(const E& error) noexcept;

    /**
     * Create a deferred which whose completion is govered by
     * the supplied promise.
     *
     * @param promise The promise which, when complete, should
     *                also complete this deferred.
     * @return A deferred instance for the given promise.
     */
    static DeferredRef<T, E> forPromise(PromiseRef<T, E> promise);

    /**
     * Create a deferred which whose completion is govered by
     * the supplied fiber.
     *
     * @param promise The fiber which, when complete, should
     *                also complete this deferred.
     * @return A deferred instance for the given fiber.
     */
    static DeferredRef<T, E> forFiber(FiberRef<T, E> promise);

    /**
     * Create a value-less deferred which completes when the given cancelable
     * shuts down.
     *
     * @param cancelable The cancelable to watch for shutdown.
     * @param sched The scheduler use for the result deferred.
     * @return A deferred which completes with the given cancelable shuts down.
     */
    static DeferredRef<None, None> forCancelable(const CancelableRef& cancelable, const SchedulerRef& sched);

    /**
     * Create a new deferred which maps both the value and error results of
     * this deferred to new values or errors using a pair of provided transformers.
     *
     * @param value_transform The transformer to run on success value results.
     * @param error_transform The transfomer to run on error results.
     * @return A deferred which presents the transformed values and errors.
     */
    template <class T2, class E2>
    DeferredRef<T2, E2> mapBoth(std::function<T2(const T&)> value_transform,
                                std::function<E2(const E&)> error_transform);

    /**
     * Register a callback to be evaluated with the asynchronous
     * operation completes on success OR error.
     *
     * @param callback The callback to execute.
     */
    virtual void onComplete(std::function<void(Either<T, E>)> callback) = 0;

    /**
     * Register a callback to be evaluated with the asynchronous
     * operation completes on success.
     *
     * @param callback The callback to execute.
     */
    virtual void onSuccess(std::function<void(T)> callback) = 0;

    /**
     * Register a callback to be evaluated with the asynchronous
     * operation completes on error.
     *
     * @param callback The callback to execute.
     */
    virtual void onError(std::function<void(E)> callback) = 0;

    /**
     * Await the result of this operation by blocking the current
     * thread until the result is available. If the result is
     * an error this method with throw said error.
     *
     * @return The result value of the asynchronous computation.
     */
    virtual T await() = 0;

    virtual ~Deferred(){};
};

} // namespace cask

#include "deferred/FiberDeferred.hpp"
#include "deferred/MapDeferred.hpp"
#include "deferred/PromiseDeferred.hpp"
#include "deferred/PureDeferred.hpp"
#include "deferred/PureErrorDeferred.hpp"

namespace cask {

template <class T, class E> constexpr DeferredRef<T, E> Deferred<T, E>::pure(const T& value) noexcept {
    return std::make_shared<deferred::PureDeferred<T, E>>(value);
}

template <class T, class E> constexpr DeferredRef<T, E> Deferred<T, E>::pure(T&& value) noexcept {
    return std::make_shared<deferred::PureDeferred<T, E>>(std::move(value));
}

template <class T, class E> constexpr DeferredRef<T, E> Deferred<T, E>::raiseError(const E& error) noexcept {
    return std::make_shared<deferred::PureErrorDeferred<T, E>>(error);
}

template <class T, class E> DeferredRef<T, E> Deferred<T, E>::forPromise(PromiseRef<T, E> promise) {
    return std::make_shared<deferred::PromiseDeferred<T, E>>(promise);
}

template <class T, class E> DeferredRef<T, E> Deferred<T, E>::forFiber(FiberRef<T, E> fiber) {
    return std::make_shared<deferred::FiberDeferred<T, E>>(fiber);
}

template <class T, class E>
DeferredRef<None, None> Deferred<T, E>::forCancelable(const CancelableRef& cancelable, const SchedulerRef& sched) {
    auto promise = Promise<cask::None, cask::None>::create(sched);

    cancelable->onCancel([promise]() {
        promise->cancel();
    });

    cancelable->onShutdown([promise]() {
        promise->success(cask::None());
    });

    return std::make_shared<deferred::PromiseDeferred<cask::None, cask::None>>(promise);
}

template <class T, class E>
template <class T2, class E2>
DeferredRef<T2, E2> Deferred<T, E>::mapBoth(std::function<T2(const T&)> value_transform,
                                            std::function<E2(const E&)> error_transform) {
    return std::make_shared<deferred::MapDeferred<T, T2, E, E2>>(
        this->shared_from_this(), value_transform, error_transform);
}

} // namespace cask

#endif