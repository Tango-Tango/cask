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
class Queue {
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
        return stateRef->template modify<Task<None,E>>([value = std::forward<Arg>(value)](const auto& state) {
                return state.put(value);
            })
            .template flatMap<None>([](auto&& task) {
                return task;
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
        using IntermediateResult = std::tuple<bool,std::function<void()>>;

        auto result_opt = stateRef->template modify<IntermediateResult>([value = std::forward<Arg>(value)](const auto& state) {
            auto&& [nextState, completed, thunk] = state.tryPut(value);
            return std::make_tuple(std::move(nextState), std::make_tuple(std::move(completed), std::move(thunk)));
        })
        .template map<bool>([](IntermediateResult&& result) {
            auto&& [completed, thunk] = result;
            thunk();
            return completed;
        })
        .runSync();

        if(result_opt && result_opt->is_left()) {
            return result_opt->get_left();
        } else {
            return false;
        }
    }

    /**
     * Attempt to take a value from the Queue. If the queue is
     * currently empty then the caller be queued and forced
     * to asynchronously await for a value to become available.
     *
     * @return A task that completes when a value has been taken.
     */
    Task<T,E> take()  {
        return stateRef->template modify<Task<T,E>>([](const auto& state) {
                return state.take();
            })
            .template flatMap<T>([](auto&& task) {
                return task;
            });
    }

    /**
     * Attempt to take a value from the Queue. If the queue is
     * currently empty then no value will be provided.
     *
     * @return A value or nothing.
     */
    std::optional<T> tryTake()  {
        using IntermediateResult = std::tuple<std::optional<T>,std::function<void()>>;

        auto result_opt = stateRef->template modify<IntermediateResult>([](const auto& state) {
            auto&& [nextState, valueOpt, thunk] = state.tryTake();
            return std::make_tuple(std::move(nextState), std::make_tuple(std::move(valueOpt), std::move(thunk)));
        })
        .template map<std::optional<T>>([](IntermediateResult&& result) {
            auto&& [valueOpt, thunk] = result;
            thunk();
            return valueOpt;
        })
        .runSync();

        if(result_opt && result_opt->is_left()) {
            return result_opt->get_left();
        } else {
            return std::optional<T>();
        }
    }

    /**
     * Reset this queue to its initial, empty, state. If any values
     * are in the queue they are dropped. If there are any pending
     * puts or takes then they are canceled.
     */
    void reset()  {
        using IntermediateResult = std::function<void()>;

        stateRef->template modify<IntermediateResult>([](const auto& state) {
            return state.reset();
        })
        .template map<None>([](auto&& thunk) {
            thunk();
            return None();
        })
        .runSync();
    }

    Queue(const std::shared_ptr<Scheduler>& sched, uint32_t max_size)
        : stateRef(Ref<queue::QueueState<T,E>,E>::create(sched, max_size))
    {}

private:
    std::shared_ptr<Ref<queue::QueueState<T,E>,E>> stateRef;
};

} // namespace cask

#endif