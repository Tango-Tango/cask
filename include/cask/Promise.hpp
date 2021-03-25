//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_PROMISE_H_
#define _CASK_PROMISE_H_

#include <any>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
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

    void onCancel(std::function<void()> callback) override;
    void cancel() override;
private:
    friend deferred::PromiseDeferred<T,E>;

    std::optional<Either<T,E>> resultOpt;
    bool canceled;
    mutable std::mutex mutex;
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
    , mutex()
    , completeCallbacks()
    , cancelCallbacks()
    , sched(sched)
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
    bool runCallbacks = false;

    {
        std::lock_guard guard(mutex);
        if(!resultOpt.has_value() && !canceled) {
            resultOpt = value;
            runCallbacks = true;
        } else {
            if(resultOpt.has_value()) {
                if(resultOpt->is_left()) {
                    throw std::runtime_error("Promise already successfully completed.");
                } else {
                    throw std::runtime_error("Promise already completed with an error.");
                }
            }
        }
    }

    if(runCallbacks) {
        for(auto& callback : completeCallbacks) {
            sched->submit(std::bind(callback, *resultOpt));
        }

        for(auto& callback : eitherCallbacks) {
            sched->submit(std::bind(callback, *resultOpt, false));
        }
    }
}

template <class T, class E>
void Promise<T,E>::cancel() {
    bool runCallbacks = false;

    {
        std::lock_guard guard(mutex);
        if(!resultOpt.has_value() && !canceled) {
            canceled = true;
            runCallbacks = true;
        }
    }

    if(runCallbacks) {
        // Cancel callbacks are run schronously to try
        // and expedite the process of cancelling.
        for(auto& callback : cancelCallbacks) {
            callback();
        }
    }
}

template <class T, class E>
bool Promise<T,E>::isCancelled() const {
    return canceled;
}

template <class T, class E>
std::optional<Either<T,E>> Promise<T,E>::get() const {
    std::lock_guard guard(mutex);
    if(canceled) {
        return {};
    } else {
        return resultOpt;
    }
}

template <class T, class E>
void Promise<T,E>::onCancel(std::function<void()> callback) {
    bool runNow = false;

    {
        std::lock_guard guard(mutex);
        if(canceled) {
            runNow = true;
        } else {
            cancelCallbacks.push_back(callback);
        }
    }

    if(runNow) {
        callback();
    }
}

template <class T, class E>
void Promise<T,E>::onComplete(std::function<void(Either<T,E>)> callback) {
    bool immediateSubmit = false;

    {
        std::lock_guard guard(mutex);
        if(resultOpt.has_value()) {
            immediateSubmit = true;
        } else {
            completeCallbacks.push_back(callback);
        }
    }
    
    if(immediateSubmit) {
        sched->submit(std::bind(callback, *resultOpt));
    }
}

}

#endif