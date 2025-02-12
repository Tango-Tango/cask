//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_QUEUE_H_
#define _CASK_QUEUE_H_

#include "Task.hpp"
#include "Ref.hpp"
#include "queue/QueueState.hpp"

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
        return Task<None,E>::defer([self_weak = this->weak_from_this(), value = std::forward<Arg>(value)] {
            if(auto self = self_weak.lock()) {
                std::lock_guard guard(self->mutex);
                auto &&[nextState, task] = self->state.put(value);
                self->state = nextState;
                return task;
            } else {
                throw std::runtime_error("Queue has been destroyed");
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
        std::function<void()> thunk;
        bool completed = false;

        {
            std::lock_guard guard(mutex);
            auto&& [nextState, given_completed, given_thunk] = state.tryPut(std::forward<Arg>(value));
            state = nextState;
            completed = given_completed;
            thunk = std::move(given_thunk);
        }

        thunk();
        return completed;
    }

    /**
     * Attempt to take a value from the Queue. If the queue is
     * currently empty then the caller be queued and forced
     * to asynchronously await for a value to become available.
     *
     * @return A task that completes when a value has been taken.
     */
    Task<T,E> take()  {
        return Task<T,E>::defer([self_weak = this->weak_from_this()] {
            if(auto self = self_weak.lock()) {
                std::lock_guard guard(self->mutex);
                auto&& [nextState, task] = self->state.take();
                self->state = nextState;

                return task;
            } else {
                throw std::runtime_error("Queue has been destroyed");
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
        std::function<void()> thunk;
        std::optional<T> valueOpt;

        {
            std::lock_guard guard(mutex);

            auto&& [nextState, given_value, given_thunk] = state.tryTake();
            state = nextState;
            valueOpt = given_value;
            thunk = std::move(given_thunk);
        }

        thunk();
        return valueOpt;
    }

    /**
     * Reset this queue to its initial, empty, state. If any values
     * are in the queue they are dropped. If there are any pending
     * puts or takes then they are canceled.
     */
    void reset() {
        std::function<void()> thunk;

        {
            std::lock_guard guard(mutex);
            auto&& [nextState, given_thunk] = state.reset();
            thunk = std::move(given_thunk);
            state = std::move(nextState);
        }

        thunk();
    }

    Queue(const std::shared_ptr<Scheduler>& sched, uint32_t max_size)
        : state(sched, max_size)
    {}

private:
    std::mutex mutex;
    queue::QueueState<T,E> state;
};

} // namespace cask

#endif