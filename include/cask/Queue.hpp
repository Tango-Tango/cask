//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_QUEUE_H_
#define _CASK_QUEUE_H_

#include "Task.hpp"
#include <mutex>
#include <optional>
#include <vector>

namespace cask {

template <class T, class E>
class Queue;

template <class T, class E = std::any>
using QueueRef = std::shared_ptr<Queue<T,E>>;

/**
 * A Queue is a concurrent queue which implements asynchronous semantic blocking for puts and takes. It
 * can be used to coordinate between two proceses, with backpressure, where one producer process inserts
 * items with `put` and a consume process takes items with `take`.
 */
template <class T, class E = std::any>
class Queue : public std::enable_shared_from_this<Queue<T,E>> {
public:
    /**
     * Create an empty queue.
     *
     * @param sched The scheduler on which Queue will schedule
     *              any asynchronous puts or takes.
     * @param max_size The maximum size to bound the queue to.
     * @return An empty Queue reference.
     */
    static QueueRef<T,E> empty(const std::shared_ptr<Scheduler>& sched, uint32_t max_size) {
        return std::make_shared<Queue<T,E>>(sched, max_size);
    }

    /**
     * Enqueue the given value. If the queue is full then the put
     * will be queued and the caller forced to asynchonously await.
     *
     * @param value The value to put into the Queue.
     * @return A task that completes when the value has been stored.
     */
    template <class Arg>
    Task<None,E> put(Arg&& value)  {
        return Task<None,E>::defer([self_weak = this->weak_from_this(), value = std::forward<Arg>(value)]() {
            if (auto self = self_weak.lock()) {
                std::lock_guard lock(self->mutex);
                self->unsafe_cleanup_queues();
                auto result = self->unsafe_try_put(value);
                auto completed = std::get<0>(result);
                auto thunk = std::get<1>(result);

                if (completed) {
                    return Task<None,E>::eval([thunk] {
                        thunk();
                        return None();
                    });
                } else {
                    auto promise = Promise<None,E>::create(self->sched);
                    self->pending_puts.emplace_back(promise, value);
                    return Task<None,E>::defer([promise, thunk] {
                        thunk();
                        return Task<None,E>::forPromise(promise);
                    });
                }
            } else {
                return Task<None,E>::cancel();
            }
        });
    }

     /**
     * Attempt to enqueue the given value. If the queue is
     * currently full then the put will fail.
     *
     * @param value The value to put into the Queue.
     * @return True iff the value was stored in the queue or
     *         pushed to an observer via a queued take.
     */
    template <class Arg>
    bool tryPut(Arg&& value) {
        bool success;
        std::function<void()> thunk;

        {
            std::lock_guard lock(mutex);
            unsafe_cleanup_queues();
            auto result = unsafe_try_put(std::forward<Arg>(value));
            success = std::get<0>(result);
            thunk = std::get<1>(result);
        }

        thunk();
        return success;
    }

    /**
     * Attempt to take a value from the Queue. If the queue is
     * currently empty then the caller be queued and forced
     * to asynchronously await for a value to become available.
     *
     * @return A task that completes when a value has been taken.
     */
    Task<T,E> take()  {
        return Task<T,E>::defer([self_weak = this->weak_from_this()]() {
            if (auto self = self_weak.lock()) {
                std::lock_guard lock(self->mutex);
                self->unsafe_cleanup_queues();
                auto result = self->unsafe_try_take();
                auto value = std::get<0>(result);
                auto thunk = std::get<1>(result);

                if (value.has_value()) {
                    return Task<T,E>::eval([thunk, value = *value] {
                        thunk();
                        return value;
                    });
                } else {
                    auto promise = Promise<T,E>::create(self->sched);
                    self->pending_takes.emplace_back(promise);
                    return Task<T,E>::defer([promise, thunk] {
                        thunk();
                        return Task<T,E>::forPromise(promise);
                    });
                }
            } else {
                return Task<T,E>::cancel();
            }
        });
    }

    /**
     * Attempt to take a value from the Queue. If the queue is
     * currently empty then no value will be provided.
     *
     * @return A value or nothing.
     */
    std::optional<T> tryTake()  {
        std::optional<T> value;
        std::function<void()> thunk;

        {
            std::lock_guard lock(mutex);
            unsafe_cleanup_queues();
            auto result = unsafe_try_take();
            value = std::get<0>(result);
            thunk = std::get<1>(result);
        }

        thunk();
        return value;
    }

    /**
     * Reset this queue to its initial, empty, state. If any values
     * are in the queue they are dropped. If there are any pending
     * puts or takes then they are canceled.
     */
    void reset()  {
        std::vector<PendingPut> cancel_puts;
        std::vector<PendingTake> cancel_takes;

        {
            std::lock_guard lock(mutex);
            std::swap(pending_puts, cancel_puts);
            std::swap(pending_takes, cancel_takes);
            values.clear();
        }

        for(auto&& put : cancel_puts) {
            auto promise = std::get<0>(put);
            promise->cancel();
        }

        for(auto&& take : cancel_takes) {
            take->cancel();
        }
    }

    Queue(const std::shared_ptr<Scheduler>& sched, uint32_t max_size)
        : max_size(max_size)
        , sched(sched)
        , mutex()
        , values()
        , pending_puts()
        , pending_takes()
    {
        values.reserve(max_size);
    }

private:
    using PendingPut = std::tuple<PromiseRef<None,E>,T>;
    using PendingTake = PromiseRef<T,E>;
    using Thunk = std::function<void()>;

    const std::size_t max_size;
    std::shared_ptr<Scheduler> sched;

    std::mutex mutex;
    std::vector<T> values;
    std::vector<PendingPut> pending_puts;
    std::vector<PendingTake> pending_takes;

    void unsafe_cleanup_queues() {
        std::vector<PendingPut> new_puts;
        std::vector<PendingTake> new_takes;

        for(auto&& put : pending_puts) {
            auto promise = std::get<0>(put);

            if(promise->isCancelled()) {
                continue;
            }

            new_puts.emplace_back(std::move(put));
        }

        for(auto&& take : pending_takes) {
            if(take->isCancelled()) {
                continue;
            }

            new_takes.emplace_back(std::move(take));
        }

        std::swap(pending_puts, new_puts);
        std::swap(pending_takes, new_takes);
    }

    std::tuple<std::optional<T>,Thunk> unsafe_try_take() {
        if (!values.empty()) {
            auto value = values.front();
            values.erase(values.begin());
            return std::make_tuple(value, []{});
        } else if (!pending_puts.empty()) {
            auto put = pending_puts.front();
            auto promise = std::get<0>(put);
            auto value = std::get<1>(put);
            pending_puts.erase(pending_puts.begin());
            return std::make_tuple(value, [promise]{
                promise->success(None());
            });
        } else {
            return std::make_tuple(std::optional<T>(), []{});
        }
    }

    template <class Arg>
    std::tuple<bool,Thunk> unsafe_try_put(Arg&& value) {
        if (!pending_takes.empty()) {
            auto promise = pending_takes.front();
            pending_takes.erase(pending_takes.begin());
            return std::make_tuple(true, [promise, value = std::forward<Arg>(value)]{
                promise->success(value);
            });
        } else if (values.size() < max_size) {
            values.emplace_back(value);
            return std::make_tuple(true, []{});
        } else {
            return std::make_tuple(false, []{});
        }
    }
};

} // namespace cask

#endif