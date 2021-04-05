//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MVAR_H_
#define _CASK_MVAR_H_

#include "Task.hpp"
#include "Ref.hpp"
#include "mvar/MVarState.hpp"

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
     * @param sched The scheduler on which MVar will schedule
     *              any asynchronous puts or takes.
     * @return An empty MVar reference.
     */
    static MVarRef<T,E> empty(const std::shared_ptr<Scheduler>& sched);

    /**
     * Create an MVar that initially holds a value.
     *
     * @param sched The scheduler on which MVar will schedule
     *              any asynchronous puts or takes.
     * @param initialValue The initial value to store in the MVar.
     * @return A non-empty MVar reference.
     */
    static MVarRef<T,E> create(const std::shared_ptr<Scheduler>& sched, const T& initialValue);

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
    MVar(const std::shared_ptr<Scheduler>& sched);
    explicit MVar(const std::shared_ptr<Scheduler>& sched, const T& initialValue);

    std::shared_ptr<Ref<mvar::MVarState<T,E>,E>> stateRef;
};

template <class T, class E>
MVarRef<T,E> MVar<T,E>::empty(const std::shared_ptr<Scheduler>& sched) {
    return std::shared_ptr<MVar<T,E>>(new MVar<T,E>(sched));
}

template <class T, class E>
MVarRef<T,E> MVar<T,E>::create(const std::shared_ptr<Scheduler>& sched, const T& initialValue) {
    return std::shared_ptr<MVar<T,E>>(new MVar<T,E>(sched, initialValue));
}

template <class T, class E>
MVar<T,E>::MVar(const std::shared_ptr<Scheduler>& sched)
    : stateRef(Ref<mvar::MVarState<T,E>,E>::create(mvar::MVarState<T,E>(sched)))
{}

template <class T, class E>
MVar<T,E>::MVar(const std::shared_ptr<Scheduler>& sched, const T& value)
    : stateRef(Ref<mvar::MVarState<T,E>,E>::create(mvar::MVarState<T,E>(sched, value)))
{}

template <class T, class E>
Task<None,E> MVar<T,E>::put(const T& value) {
    return stateRef->template modify<Task<None,E>>([value](auto state) {
            return state.put(value);
        })
        .template flatMap<None>([](auto task) {
            return task;
        });
}

template <class T, class E>
Task<T,E> MVar<T,E>::take() {
    return stateRef->template modify<Task<T,E>>([](auto state) {
            return state.take();
        })
        .template flatMap<T>([](auto task) {
            return task;
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

}

#endif