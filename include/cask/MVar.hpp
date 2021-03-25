//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MVAR_H_
#define _CASK_MVAR_H_

#include <any>
#include <atomic>
#include <memory>
#include <mutex>
#include <deque>
#include "Task.hpp"

namespace cask {

template <class T, class E>
class MVar;

template <class T, class E = std::any>
using MVarRef = std::shared_ptr<MVar<T,E>>;

/**
 * An MVar is a simple mailbox that can be used to:
 *   1. Hold some state by using the put and take methods to coordinate access
 *      in the same manner that locking and unlocking a mutex would
 *   2. Coordinate between two proceses, with backpressure, where one producer
 *      process inserts items with `put` and a consume process takes items with `take`.
 *
 * MVar can be used to manage both mutable and immutable structures - though when using
 * mutable structures (e.g. from the STL) take care not to allow a reference to the
 * structure to be used outside of a take / modify / put cycle.
 */
template <class T, class E = std::any>
class MVar : public std::enable_shared_from_this<MVar<T,E>> {
public:
    /**
     * Create an MVar that currently holds no data.
     *
     * @return An empty MVar reference.
     */
    static MVarRef<T,E> empty();

    /**
     * Create an MVar that initially holds a value.
     *
     * @param initialValue The initial value to store in the MVar.
     * @return A non-empty MVar reference.
     */
    static MVarRef<T,E> create(const T& initialValue);

    /**
     * Attempt to store the given value in the MVar. If the MVar
     * is already holding a value the put will be queued and
     * the caller forced to asynchonously await.
     *
     * @param value The value to put into the MVar.
     * @return A task that completes when the value has been stored.
     */
    Task<None,E> put(const T& value);

    /**
     * Attempt to take a value from the MVar. If the MVar is
     * currently empty then the caller be queued and forced
     * to asynchronously await for a value to become available.
     *
     * @return A task that completes when a value has been taken.
     */
    Task<T,E> take();

    /**
     * Read a value without permanently taking it. This is equivalent
     * to performing a take operation and then immediately putting the
     * value back. If the MVar contains no value then the caller will
     * be queued and forced to asynchronously wait for a value to
     * become available.
     *
     * @return A task that completes when a value has been read.
     */
    Task<T,E> read();
private:
    MVar();
    explicit MVar(const T& initialValue);

    std::recursive_mutex mutex;
    std::optional<T> valueOpt;
    std::deque<std::pair<PromiseRef<None,E>,T>> pendingPuts;
    std::deque<PromiseRef<T,E>> pendingTakes;

    static DeferredRef<None,E> pushPutOntoQueue(const MVarRef<T,E>& self, const std::shared_ptr<Scheduler>& sched, const T& newValue);
    static DeferredRef<None,E> resolveTakeOrStore(const MVarRef<T,E>& self, const T& newValue);

    static DeferredRef<T,E> pushTakeOntoQueue(const MVarRef<T,E>& self, const std::shared_ptr<Scheduler>& sched);
    static DeferredRef<T,E> resolvePutOrClear(const MVarRef<T,E>& self);

    void cleanupTake(PromiseRef<T,E>);
    void cleanupPut(PromiseRef<None,E>);
};

template <class T, class E>
MVarRef<T,E> MVar<T,E>::empty() {
    return std::shared_ptr<MVar<T,E>>(new MVar<T,E>());
}

template <class T, class E>
MVarRef<T,E> MVar<T,E>::create(const T& initialValue) {
    return std::shared_ptr<MVar<T,E>>(new MVar<T,E>(initialValue));
}

template <class T, class E>
MVar<T,E>::MVar()
    : mutex()
    , valueOpt()
    , pendingPuts()
    , pendingTakes()
{}

template <class T, class E>
MVar<T,E>::MVar(const T& value)
    : mutex()
    , valueOpt(value)
    , pendingPuts()
    , pendingTakes()
{}

template <class T, class E>
Task<None,E> MVar<T,E>::put(const T& newValue) {
    auto self = this->shared_from_this();

    return Task<None,E>::deferAction([self, newValue](auto sched) {
        std::lock_guard(self->mutex);
        if(self->valueOpt.has_value()) {
            return pushPutOntoQueue(self, sched, newValue);
        } else {
            return resolveTakeOrStore(self, newValue);
        }
    });
}

template <class T, class E>
Task<T,E> MVar<T,E>::take() {
    auto self = this->shared_from_this();

    return Task<T,E>::deferAction([self](auto sched) {
        std::lock_guard(self->mutex);
        if(self->valueOpt.has_value()) {
            return resolvePutOrClear(self);
        } else {
            return pushTakeOntoQueue(self, sched);
        }
    });
}

template <class T, class E>
Task<T,E> MVar<T,E>::read() {
    auto self = this->shared_from_this();
    return take().template flatMap<T>([self](auto value) {
        return self->put(value).template map<T>([value](auto) {
            return value;
        });
    });
}

template <class T, class E>
DeferredRef<None,E> MVar<T,E>::pushPutOntoQueue(const MVarRef<T,E>& self, const std::shared_ptr<Scheduler>& sched, const T& newValue) {
    auto promise = Promise<None,E>::create(sched);
    self->pendingPuts.emplace_back(promise, newValue);
    promise->onCancel([promiseWeak = std::weak_ptr<Promise<None,E>>(promise), selfWeak = std::weak_ptr<MVar<T,E>>(self)]() {
        if(auto self = selfWeak.lock()) {
            if(auto promise = promiseWeak.lock()) {
                self->cleanupPut(promise);
            }
        }
    });
    return Deferred<None,E>::forPromise(promise);
}

template <class T, class E>
DeferredRef<None,E> MVar<T,E>::resolveTakeOrStore(const MVarRef<T,E>& self, const T& newValue) {
    if(!self->pendingTakes.empty()) {
        auto nextTake = self->pendingTakes.front();
        nextTake->success(newValue);
        self->pendingTakes.pop_front();
    } else {
        self->valueOpt = newValue;
    }

    return Deferred<None,E>::pure(None());
}

template <class T, class E>
DeferredRef<T,E> MVar<T,E>::pushTakeOntoQueue(const MVarRef<T,E>& self, const std::shared_ptr<Scheduler>& sched) {
    auto promise = Promise<T,E>::create(sched);
    self->pendingTakes.emplace_back(promise);
    promise->onCancel([promiseWeak = std::weak_ptr<Promise<T,E>>(promise), selfWeak = std::weak_ptr<MVar<T,E>>(self)]() {
        if(auto self = selfWeak.lock()) {
            if(auto promise = promiseWeak.lock()) {
                self->cleanupTake(promise);
            }
        }
    });
    return Deferred<T,E>::forPromise(promise);
}

template <class T, class E>
DeferredRef<T,E> MVar<T,E>::resolvePutOrClear(const MVarRef<T,E>& self) {
    auto value = *(self->valueOpt);

    if(!self->pendingPuts.empty()) {
        auto nextPut = self->pendingPuts.front();
        self->valueOpt = nextPut.second;
        nextPut.first->success(None());
        self->pendingPuts.pop_front();
    } else {
        self->valueOpt.reset();
    }

    return Deferred<T,E>::pure(value);
}

template <class T, class E>
void MVar<T,E>::cleanupTake(PromiseRef<T,E> promise) {
    std::lock_guard guard(mutex);

    auto result = std::find_if(
        std::begin(pendingTakes),
        std::end(pendingTakes),
        [promise] (auto other) { return promise.get() == other.get(); }
    );

    if(result != std::end(pendingTakes)) {
        pendingTakes.erase(result);
    }
}

template <class T, class E>
void MVar<T,E>::cleanupPut(PromiseRef<None,E> promise) {
    std::lock_guard guard(mutex);

    auto result = std::find_if(
        std::begin(pendingPuts),
        std::end(pendingPuts),
        [promise] (auto other) { return promise.get() == other.first.get(); }
    );

    if(result != std::end(pendingPuts)) {
        pendingPuts.erase(result);
    }
}

}

#endif