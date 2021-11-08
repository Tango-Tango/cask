//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_DEFERRED_H_
#define _CASK_DEFERRED_H_

#include <memory>
#include <functional>
#include "Promise.hpp"
#include <exception>

namespace cask {

template <class T, class E>
class Deferred;

template <class T = None, class E = std::any>
using DeferredRef = std::shared_ptr<Deferred<T,E>>;

/**
 * A `Deferred` represents the "consumer" side of a running asynchronous operation. Consumers
 * are notified of results from a producer either synchronously (via blocking with `await`) or
 * asynchronously (via `onComplete`). `Deferred` instances can also be chained together with
 * other synchronous (via `map`) or asynchronouse (via `flatMap`) operations.
 */
template <class T = None, class E = std::any>
class Deferred : public Cancelable, public std::enable_shared_from_this<Deferred<T,E>> {
public:
    /**
     * Create a deferred instance wrapping the already computed pure vale.
     *
     * @param value The value of this completed deferred.
     * @return A deferred wrapping the given value.
     */
    constexpr static DeferredRef<T,E> pure(const T& value) noexcept;
    constexpr static DeferredRef<T,E> pure(T&& value) noexcept;

    /**
     * Create a defered instance wrapping the given error.
     *
     * @param value The value of this completed deferred.
     * @return A deferred wrapping the given value.
     */
    constexpr static DeferredRef<T,E> raiseError(const E& error) noexcept;

    /**
     * Create a deferred which whose completion is govered by
     * the supplied promise.
     *
     * @param promise The promise which, when complete, should
     *                also complete this deferred.
     * @return A deferred instance for the given promise.
     */
    static DeferredRef<T,E> forPromise(PromiseRef<T,E> promise);

    /**
     * Create a value-less deferred which completes when the given cancelable
     * shuts down.
     * 
     * @param cancelable The cancelable to watch for shutdown.
     * @param sched The scheduler use for the result deferred.
     * @return A deferred which completes with the given cancelable shuts down.
     */
    static DeferredRef<None,None> forCancelable(CancelableRef cancelable, SchedulerRef sched);

    /**
     * Properly chain this deferred to another promise which is
     * relying on its results. This not only chains normal values
     * an errors from this source to its downstream promise - but
     * also properly communicates cancellations from the downstream
     * promise back to this deferred.
     *
     * Don't try and do this on your own. You'll create memory leaks.
     * Use this method whenever chaining together promises rather
     * that using the `onComplete` and `onCancel` methods to do it
     * yourself.
     *
     * @param downstream The downstream promise that should be completed
     *                   based on the results of this deferred and
     *                   the provided transform methods.
     * @param transformDownstream Transform the result of this deferred (which
     *                            may be either a value or error) into its
     *                            downstream representation.
     */
    template <class T2, class E2>
    void chainDownstream(
        PromiseRef<T2,E2> downstream,
        std::function<Either<T2,E2>(Either<T,E>)> transformDownstream
    );

    /**
     * Properly chain this deferred to another promise which is
     * relying on its results. This not only chains normal values
     * an errors from this source to its downstream promise - but
     * also properly communicates cancellations from the downstream
     * promise back to this deferred.
     *
     * Don't try and do this on your own. You'll create memory leaks.
     * Use this method whenever chaining together promises rather
     * that using the `onComplete` and `onCancel` methods to do it
     * yourself.
     *
     * This implementation differs from `chainDownstream` in that it
     * allows a downstream transform to also provide an asynchronous
     * result.
     *
     * @param downstream The downstream promise that should be completed
     *                   based on the results of this deferred and
     *                   the provided transform methods.
     * @param transformDownstream Transform the result of this deferred (which
     *                            may be either a value or error) into its
     *                            downstream representation. The results of this
     *                            transform will be provided asynchronously.
     */
    template <class T2, class E2>
    void chainDownstreamAsync(
        PromiseRef<T2,E2> downstream,
        std::function<DeferredRef<T2,E2>(Either<T,E>)> transformDownstream
    );

    /**
     * Register a callback to be evaluated with the asynchronous
     * operation completes on success OR error.
     * 
     * @param callback The callback to execute.
     */
    virtual void onComplete(std::function<void(Either<T,E>)> callback) = 0;

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

    virtual ~Deferred() {};
};

}

#include "deferred/PromiseDeferred.hpp"
#include "deferred/PureDeferred.hpp"
#include "deferred/PureErrorDeferred.hpp"

namespace cask {

template<class T, class E>
constexpr DeferredRef<T,E> Deferred<T,E>::pure(const T& value) noexcept {
    return std::make_shared<deferred::PureDeferred<T,E>>(value);
}

template<class T, class E>
constexpr DeferredRef<T,E> Deferred<T,E>::pure(T&& value) noexcept {
    return std::make_shared<deferred::PureDeferred<T,E>>(std::move(value));
}

template<class T, class E>
constexpr DeferredRef<T,E> Deferred<T,E>::raiseError(const E& error) noexcept {
    return std::make_shared<deferred::PureErrorDeferred<T,E>>(error);
}

template<class T, class E>
DeferredRef<T,E> Deferred<T,E>::forPromise(PromiseRef<T,E> promise) {
    return std::make_shared<deferred::PromiseDeferred<T,E>>(promise);
}

template<class T, class E>
DeferredRef<None,None> Deferred<T,E>::forCancelable(CancelableRef cancelable, SchedulerRef sched) {
    auto promise = Promise<cask::None,cask::None>::create(sched);

    cancelable->onCancel([promise]() {
        promise->cancel();
    });

    cancelable->onShutdown([promise]() {
        promise->success(cask::None());
    });

    return std::make_shared<deferred::PromiseDeferred<cask::None,cask::None>>(promise);
}

template<class T, class E>
template<class T2, class E2>
void Deferred<T,E>::chainDownstream(
    PromiseRef<T2,E2> downstream,
    std::function<Either<T2,E2>(Either<T,E>)> transform
) {
    auto upstream = this->shared_from_this();

    int handle = downstream->onCancel([upstream]() {
        upstream->cancel();
    });

    upstream->onComplete([downstreamWeak = std::weak_ptr<Promise<T2,E2>>(downstream), transform, handle](Either<T,E> result) {
        auto transformResult = transform(result);

        if(auto downstream = downstreamWeak.lock()) {
            downstream->complete(transformResult);
            downstream->unregisterCancelCallback(handle);
        }
    });
}

template<class T, class E>
template <class T2, class E2>
void Deferred<T,E>::chainDownstreamAsync(
    PromiseRef<T2,E2> downstream,
    std::function<DeferredRef<T2,E2>(Either<T,E>)> transform
) {
    auto upstream = this->shared_from_this();

    int handle = downstream->onCancel([upstream]() {
        upstream->cancel();
    });

    upstream->onComplete([downstreamWeak = std::weak_ptr<Promise<T2,E2>>(downstream), transform, handle](Either<T,E> result) {
        DeferredRef<T2,E2> asyncTransformResult = transform(result);

        if(auto downstream = downstreamWeak.lock()) {
            asyncTransformResult->template chainDownstream<T2,E2>(
                downstream,
                [](auto result){ return result; }
            );

            downstream->unregisterCancelCallback(handle);
        }
    });
}

}

#endif