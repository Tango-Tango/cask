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
 * An Queue is a simple mailbox that can be used to:
 *   1. Hold some state by using the put and take methods to coordinate access
 *      in the same manner that locking and unlocking a mutex would
 *   2. Coordinate between two proceses, with backpressure, where one producer
 *      process inserts items with `put` and a consume process takes items with `take`.
 *
 * Queue can be used to manage both mutable and immutable structures - though when using
 * mutable structures (e.g. from the STL) take care not to allow a reference to the
 * structure to be used outside of a take / modify / put cycle.
 */
template <class T, class E = std::any>
class Queue {
public:
    /**
     * Create an Queue that currently holds no data.
     *
     * @param sched The scheduler on which Queue will schedule
     *              any asynchronous puts or takes.
     * @return An empty Queue reference.
     */
    static QueueRef<T,E> empty(const std::shared_ptr<Scheduler>& sched, uint32_t max_size);

    /**
     * Attempt to store the given value in the Queue. If the Queue
     * is already holding a value the put will be queued and
     * the caller forced to asynchonously await.
     *
     * @param value The value to put into the Queue.
     * @return A task that completes when the value has been stored.
     */
    Task<None,E> put(const T& value);

     /**
     * Attempt to store the given value in the Queue. If the Queue
     * is already holding a value the put will fail.
     *
     * @param value The value to put into the Queue.
     * @return True iff the value was stored in the Queue or
     *         pushed to an observer via a queued take.
     */
    bool tryPut(const T& value);

    /**
     * Attempt to take a value from the Queue. If the Queue is
     * currently empty then the caller be queued and forced
     * to asynchronously await for a value to become available.
     *
     * @return A task that completes when a value has been taken.
     */
    Task<T,E> take();
private:
    Queue(const std::shared_ptr<Scheduler>& sched, uint32_t max_size);
    explicit Queue(const std::shared_ptr<Scheduler>& sched, const T& initialValue);

    std::shared_ptr<Ref<queue::QueueState<T,E>,E>> stateRef;
};

template <class T, class E>
QueueRef<T,E> Queue<T,E>::empty(const std::shared_ptr<Scheduler>& sched, uint32_t max_size) {
    return std::shared_ptr<Queue<T,E>>(new Queue<T,E>(sched, max_size));
}

template <class T, class E>
Queue<T,E>::Queue(const std::shared_ptr<Scheduler>& sched, uint32_t max_size)
    : stateRef(Ref<queue::QueueState<T,E>,E>::create(queue::QueueState<T,E>(sched, max_size)))
{}

template <class T, class E>
Queue<T,E>::Queue(const std::shared_ptr<Scheduler>& sched, const T& value)
    : stateRef(Ref<queue::QueueState<T,E>,E>::create(queue::QueueState<T,E>(sched, value)))
{}

template <class T, class E>
Task<None,E> Queue<T,E>::put(const T& value) {
    return stateRef->template modify<Task<None,E>>([value](auto state) {
            return state.put(value);
        })
        .template flatMap<None>([](auto task) {
            return task;
        });
}

template <class T, class E>
bool Queue<T,E>::tryPut(const T& value) {
    using IntermediateResult = std::tuple<bool,std::function<void()>>;

    auto result = stateRef->template modify<IntermediateResult>([value](auto state) {
        auto result = state.tryPut(value);
        auto nextState = std::get<0>(result);
        auto completed = std::get<1>(result);
        auto thunk = std::get<2>(result);
        return std::make_tuple(nextState, std::make_tuple(completed, thunk));
    })
    .template map<bool>([](IntermediateResult result) {
        auto completed = std::get<0>(result);
        auto thunk = std::get<1>(result);
        thunk();
        return completed;
    })
    .runSync();

    // The operation above is guaranteed to run synchronously and without error
    // so  we just need to unwrap the result here.
    return result.get_left().get_left();
}

template <class T, class E>
Task<T,E> Queue<T,E>::take() {
    return stateRef->template modify<Task<T,E>>([](auto state) {
            return state.take();
        })
        .template flatMap<T>([](auto task) {
            return task;
        });
}

}

#endif