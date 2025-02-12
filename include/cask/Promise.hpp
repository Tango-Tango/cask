//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_PROMISE_H_
#define _CASK_PROMISE_H_

#include <atomic>
#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include "Cancelable.hpp"
#include "Either.hpp"
#include "Scheduler.hpp"
#include "None.hpp"

namespace cask {

template <class T, class E>
class Promise;

template <class T = None, class E = std::any>
using PromiseRef = std::shared_ptr<Promise<T,E>>;

namespace deferred {
template <class T, class E>
class PromiseDeferred;
}

/**
 * A `Promise` represents the "producer" side of a running asynchronous operation. A producer
 * completes an asynchronous operation by calling the `complete` method at which point all
 * consumers will be notified of the available result via an attached `Deferred` instance.
 */
template <class T = None, class E = std::any>
class Promise final : public Cancelable {
public:
    /**
     * Create a promise which executes any deffered callbacks or transformations
     * on the given scheduler.
     * 
     * @param sched The scheduler to perform any computation on.
     * @return A reference to the created promise.
     */
    static PromiseRef<T,E> create(std::shared_ptr<Scheduler> sched);

    /**
     * Construct a promise. Provided purely for compatibility with `std::make_shared`
     * and can cause issues if used directly. Please use `Promise<T,E>::create` instead.
     */
    explicit Promise(std::shared_ptr<Scheduler> sched);

    /**
     * Complete this promise with the given value.
     * 
     * @param value The value to use when completing the promise.
     */
    void success(const T& value);
    
    /**
     * Complete this promise with the given error.
     * 
     * @param value The error to use when completing the promise.
     */
    void error(const E& error);

    /**
     * Complete this promise with the given value OR error.
     * 
     * @param value The value OR error to use when completing the promise.
     */
    void complete(const Either<T,E>& result);

    /**
     * Attempt to retrieve the value of this promise. Will return nothing
     * if the promise has not yet completed.
     *
     * @return The result value of this promise or nothing.
     */
    std::optional<Either<T,E>> get() const;

    /**
     * Check if this promise is already cancelled.
     *
     * @return True iff the promise is cancelled.
     */
    bool isCancelled() const;

    void onCancel(const std::function<void()>& callback) override;
    void onShutdown(const std::function<void()>& callback) override;
    void cancel() override;
private:
    friend deferred::PromiseDeferred<T,E>;

    std::optional<Either<T,E>> resultOpt;
    bool canceled;
    mutable std::atomic_flag lock = ATOMIC_FLAG_INIT;
    std::vector<std::function<void(Either<T,E>)>> completeCallbacks;
    std::vector<std::function<void()>> cancelCallbacks;
    std::vector<std::function<void(Either<T,E>, bool)>> eitherCallbacks;
    std::shared_ptr<Scheduler> sched;

    void onComplete(std::function<void(Either<T,E>)> callback);
};


template <class T, class E>
std::shared_ptr<Promise<T,E>> Promise<T,E>::create(std::shared_ptr<Scheduler> sched) {
    return std::make_shared<Promise<T,E>>(sched);
}

template <class T, class E>
Promise<T,E>::Promise(std::shared_ptr<Scheduler> sched)
    : resultOpt(std::nullopt)
    , canceled(false)
    , completeCallbacks()
    , cancelCallbacks()
    , sched(std::move(sched))
{}

template <class T, class E>
void Promise<T,E>::success(const T& value) {
    complete(Either<T,E>::left(value));
}

template <class T, class E>
void Promise<T,E>::error(const E& error) {
    complete(Either<T,E>::right(error));
}

template <class T, class E>
void Promise<T,E>::complete(const Either<T,E>& value) {
    std::vector<std::function<void(Either<T,E>)>> complete_callbacks_to_run;
    std::vector<std::function<void(Either<T,E>, bool)>> either_callbacks_to_run;

    {
        while(lock.test_and_set(std::memory_order_acquire));

        if(!resultOpt.has_value() && !canceled) {
            resultOpt = value;
            std::swap(completeCallbacks, complete_callbacks_to_run);
            std::swap(eitherCallbacks, either_callbacks_to_run);
            cancelCallbacks.clear();
        } else {
            if(resultOpt.has_value()) {
                if(resultOpt->is_left()) {
                    lock.clear(std::memory_order_release);
                    throw std::runtime_error("Promise already successfully completed.");
                } else {
                    lock.clear(std::memory_order_release);
                    throw std::runtime_error("Promise already completed with an error.");
                }
            }
        }

        lock.clear(std::memory_order_release);
    }

    for(auto& callback : complete_callbacks_to_run) {
        sched->submit(std::bind(callback, *resultOpt));
    }

    for(auto& callback : either_callbacks_to_run) {
        sched->submit(std::bind(callback, *resultOpt, false));
    }
}

template <class T, class E>
void Promise<T,E>::cancel() {
    std::vector<std::function<void()>> cancel_callbacks_to_run;

    {
        while(lock.test_and_set(std::memory_order_acquire));
        if(!resultOpt.has_value() && !canceled) {
            canceled = true;
            std::swap(cancelCallbacks, cancel_callbacks_to_run);
            completeCallbacks.clear();
            eitherCallbacks.clear();
        }
        lock.clear(std::memory_order_release);
    }

    for(auto& callback : cancel_callbacks_to_run) {
        callback();
    }
}

template <class T, class E>
bool Promise<T,E>::isCancelled() const {
    return canceled;
}

template <class T, class E>
std::optional<Either<T,E>> Promise<T,E>::get() const {
    while(lock.test_and_set(std::memory_order_acquire));
    if(canceled) {
        lock.clear(std::memory_order_release);
        return {};
    } else {
        lock.clear(std::memory_order_release);
        return resultOpt;
    }
}

template <class T, class E>
void Promise<T,E>::onCancel(const std::function<void()>& callback) {
    bool runNow = false;

    {
        while(lock.test_and_set(std::memory_order_acquire));
        if(canceled) {
            runNow = true;
        } else {
            cancelCallbacks.push_back(callback);
        }
        lock.clear(std::memory_order_release);
    }

    if(runNow) {
        callback();
    }
}

template <class T, class E>
void Promise<T,E>::onShutdown(const std::function<void()>& callback) {
    onComplete([callback](auto) {
        return callback();
    });
}

template <class T, class E>
void Promise<T,E>::onComplete(std::function<void(Either<T,E>)> callback) {
    bool immediateSubmit = false;

    {
        while(lock.test_and_set(std::memory_order_acquire));
        if(resultOpt.has_value()) {
            immediateSubmit = true;
        } else {
            completeCallbacks.push_back(callback);
        }
        lock.clear(std::memory_order_release);
    }
    
    if(immediateSubmit) {
        sched->submit(std::bind(callback, *resultOpt));
    }
}

} // namespace cask

#endif