//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_QUEUE_STATE_H_
#define _CASK_QUEUE_STATE_H_

#include "../List.hpp"
#include "../Promise.hpp"
#include "../Task.hpp"
#include <optional>

namespace cask::queue {

/**
 * Represents the internal state of an MVar. It should not be used directly by consumers. It does
 * not provide any concurrency protection on its own and is expected to be protected by a `Ref`.
 */
template <class T, class E> class QueueState {
public:
    using PendingPut = std::tuple<PromiseRef<None, E>, T>;

    std::shared_ptr<Scheduler> sched;
    ListRef<T> values;
    ListRef<PendingPut> pendingPuts;
    ListRef<PromiseRef<T, E>> pendingTakes;
    uint32_t max_size;

    std::tuple<QueueState<T, E>, bool, std::function<void()>> tryPut(const T& value) const;
    std::tuple<QueueState<T, E>, Task<None, E>> put(const T& value) const;
    std::tuple<QueueState<T, E>, std::optional<T>, std::function<void()>> tryTake() const;
    std::tuple<QueueState<T, E>, Task<T, E>> take() const;

    QueueState(const std::shared_ptr<Scheduler>& sched, uint32_t max_size);
    QueueState(const std::shared_ptr<Scheduler>& sched,
               uint32_t max_size,
               const ListRef<T>& values,
               const ListRef<PendingPut>& pendingPuts,
               const ListRef<PromiseRef<T, E>>& pendingTakes);
    QueueState(std::shared_ptr<Scheduler>&& sched,
               uint32_t max_size,
               ListRef<T>&& values,
               ListRef<PendingPut>&& pendingPuts,
               ListRef<PromiseRef<T, E>>&& pendingTakes);
};

template <class T, class E>
QueueState<T, E>::QueueState(const std::shared_ptr<Scheduler>& sched, uint32_t max_size)
    : sched(sched)
    , values(List<T>::empty())
    , pendingPuts(List<PendingPut>::empty())
    , pendingTakes(List<PromiseRef<T, E>>::empty())
    , max_size(max_size) {}

template <class T, class E>
QueueState<T, E>::QueueState(const std::shared_ptr<Scheduler>& sched,
                             uint32_t max_size,
                             const ListRef<T>& values,
                             const ListRef<PendingPut>& pendingPuts,
                             const ListRef<PromiseRef<T, E>>& pendingTakes)
    : sched(sched)
    , values(values)
    , pendingPuts(pendingPuts)
    , pendingTakes(pendingTakes)
    , max_size(max_size) {}

template <class T, class E>
QueueState<T, E>::QueueState(std::shared_ptr<Scheduler>&& sched,
                             uint32_t max_size,
                             ListRef<T>&& values,
                             ListRef<PendingPut>&& pendingPuts,
                             ListRef<PromiseRef<T, E>>&& pendingTakes)
    : sched(sched)
    , values(values)
    , pendingPuts(pendingPuts)
    , pendingTakes(pendingTakes)
    , max_size(max_size) {}

template <class T, class E>
std::tuple<QueueState<T, E>, bool, std::function<void()>> QueueState<T, E>::tryPut(const T& value) const {
    auto filteredTakes = pendingTakes->dropWhile([](auto promise) {
        return promise->isCancelled();
    });

    if (!filteredTakes->is_empty()) {
        auto takePromise = *(filteredTakes->head());
        return std::make_tuple(
            QueueState(sched, max_size, values, pendingPuts, filteredTakes->tail()), true, [takePromise, value] {
                takePromise->success(value);
            });
    } else if (values->size() < max_size) {
        return std::make_tuple(
            QueueState(sched, max_size, values->append(value), pendingPuts, filteredTakes), true, [] {});
    } else {
        return std::make_tuple(QueueState(sched, max_size, values, pendingPuts, filteredTakes), false, [] {});
    }
}

template <class T, class E> std::tuple<QueueState<T, E>, Task<None, E>> QueueState<T, E>::put(const T& value) const {
    auto result = tryPut(value);
    auto nextState = std::get<0>(result);
    auto completed = std::get<1>(result);
    auto thunk = std::get<2>(result);

    if (!completed) {
        auto promise = Promise<None, E>::create(nextState.sched);
        auto pending = std::make_tuple(promise, value);

        return std::make_tuple(QueueState(nextState.sched,
                                          nextState.max_size,
                                          nextState.values,
                                          nextState.pendingPuts->append(pending),
                                          nextState.pendingTakes),
                               Task<None, E>::forPromise(promise));
    } else {
        return std::make_tuple(nextState, Task<None, E>::eval([thunk] {
                                   thunk();
                                   return None();
                               }));
    }
}

template <class T, class E>
std::tuple<QueueState<T, E>, std::optional<T>, std::function<void()>> QueueState<T, E>::tryTake() const {
    auto filteredPuts = pendingPuts->dropWhile([](auto pending) {
        auto promise = std::get<0>(pending);
        return promise->isCancelled();
    });

    if (!values->is_empty()) {
        return std::make_tuple(
            QueueState(sched, max_size, values->tail(), filteredPuts, pendingTakes), values->head(), [] {});
    } else if (!filteredPuts->is_empty()) {
        auto pending = *(filteredPuts->head());
        auto putPromise = std::get<0>(pending);
        auto value = std::get<1>(pending);
        return std::make_tuple(QueueState(sched, max_size, values, filteredPuts->tail(), pendingTakes),
                               std::optional<T>(value),
                               [putPromise] {
                                   putPromise->success(None());
                               });
    } else {
        return std::make_tuple(
            QueueState(sched, max_size, values, filteredPuts, pendingTakes), std::optional<T>(), [] {});
    }
}

template <class T, class E> std::tuple<QueueState<T, E>, Task<T, E>> QueueState<T, E>::take() const {
    auto result = tryTake();
    auto nextState = std::get<0>(result);
    auto valueOpt = std::get<1>(result);
    auto thunk = std::get<2>(result);

    if (valueOpt.has_value()) {
        return std::make_tuple(nextState, Task<T, E>::eval([thunk, value = *valueOpt] {
                                   thunk();
                                   return value;
                               }));
    } else {
        auto promise = Promise<T, E>::create(sched);
        return std::make_tuple(QueueState(nextState.sched,
                                          nextState.max_size,
                                          nextState.values,
                                          nextState.pendingPuts,
                                          pendingTakes->append(promise)),
                               Task<T, E>::forPromise(promise));
    }
}

} // namespace cask::queue

#endif