//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_QUEUE_STATE_H_
#define _CASK_QUEUE_STATE_H_

#include <optional>
#include "../List.hpp"
#include "../Promise.hpp"
#include "../Task.hpp"

namespace cask::queue {

/**
 * Represents the internal state of an MVar. It should not be used directly by consumers. It does
 * not provide any concurrency protection on its own and is expected to be protected by a `Ref`.
 */
template <class T, class E>
class QueueState {
public:
    using PendingPut = std::tuple<PromiseRef<None,E>,T>;

    std::shared_ptr<Scheduler> sched;
    ListRef<T> values;
    ListRef<PendingPut> pendingPuts;
    ListRef<PromiseRef<T,E>> pendingTakes;
    uint32_t max_size;

    template <class Arg>
    std::tuple<QueueState<T,E>,bool,std::function<void()>> tryPut(Arg&& value) const  {
        auto filteredTakes = pendingTakes->filter([](auto promise) {
            return !promise->isCancelled();
        });

        auto filteredPuts = pendingPuts->filter([](auto pending) {
            auto promise = std::get<0>(pending);
            return !promise->isCancelled();
        });

        if(auto head = filteredTakes->head()) {
            auto takePromise = *head;
            return std::make_tuple(
                QueueState(sched, max_size, values, filteredPuts, filteredTakes->tail()),
                true,
                [takePromise = std::move(takePromise), value = std::forward<Arg>(value)] {
                    takePromise->success(std::forward<Arg>(value));
                }
            );
        } else if(values->size() < max_size) {
            return std::make_tuple(
                QueueState(sched, max_size, values->append(std::forward<Arg>(value)), filteredPuts, filteredTakes),
                true,
                []{}
            );
        } else {
            return std::make_tuple(
                QueueState(sched, max_size, values, filteredPuts, filteredTakes),
                false,
                []{}
            );
        }
    }

    template <class Arg>
    std::tuple<QueueState<T,E>,Task<None,E>> put(Arg&& value) const  {
        auto&& [nextState, completed, thunk] = tryPut(std::forward<Arg>(value));

        if(!completed) {
            auto promise = Promise<None,E>::create(nextState.sched);
            auto pending = std::make_tuple(promise, value);

            return std::make_tuple(
                QueueState(nextState.sched, nextState.max_size, nextState.values, nextState.pendingPuts->append(std::move(pending)), nextState.pendingTakes),
                Task<None,E>::forPromise(std::move(promise))
            );
        } else {
            return std::make_tuple(std::move(nextState), Task<None,E>::eval([thunk = std::move(thunk)] {
                thunk();
                return None();
            }));
        }
    }

    std::tuple<QueueState<T,E>,std::optional<T>,std::function<void()>> tryTake() const  {
        auto filteredPuts = pendingPuts->filter([](auto pending) {
            auto promise = std::get<0>(pending);
            return !promise->isCancelled();
        });

        auto filteredTakes = pendingTakes->filter([](auto promise) {
            return !promise->isCancelled();
        });

        if(!values->is_empty()) {
            return std::make_tuple(
                QueueState(sched, max_size, values->tail(), filteredPuts, filteredTakes),
                values->head(),
                []{}
            );
        } else if(auto head = filteredPuts->head()) {
            auto pending = *head;
            auto putPromise = std::get<0>(pending);
            auto value = std::get<1>(pending);
            return std::make_tuple(
                QueueState(sched, max_size, values, filteredPuts->tail(), filteredTakes),
                std::optional<T>(value),
                [putPromise] {
                    putPromise->success(None());
                }
            );
        } else {
            return std::make_tuple(
                QueueState(sched, max_size, values, filteredPuts, filteredTakes),
                std::optional<T>(),
                []{}
            );
        }
    }

    std::tuple<QueueState<T,E>,Task<T,E>> take() const {
        auto result = tryTake();
        auto nextState = std::get<0>(result);
        auto valueOpt = std::get<1>(result);
        auto thunk = std::get<2>(result);

        if(valueOpt.has_value()) {
            return std::make_tuple(nextState, Task<T,E>::eval([thunk, value = *valueOpt] {
                thunk();
                return value;
            }));
        } else {
            auto promise = Promise<T,E>::create(sched);
            return std::make_tuple(
                QueueState(nextState.sched, nextState.max_size, nextState.values, nextState.pendingPuts, nextState.pendingTakes->append(promise)),
                Task<T,E>::forPromise(promise)
            );
        }
    }

    std::tuple<QueueState<T,E>,std::function<void()>> reset() const  {
        return std::make_tuple(
            QueueState(sched, max_size, List<T>::empty(), List<PendingPut>::empty(), List<PromiseRef<T,E>>::empty()),
            [pendingPuts = pendingPuts, pendingTakes = pendingTakes] {
                pendingPuts->foreach([](auto put) {
                    auto promise = std::get<0>(put);
                    promise->cancel();
                });

                pendingTakes->foreach([](auto take) {
                    take->cancel();
                });
            }
        );
    }
    
    QueueState(const std::shared_ptr<Scheduler>& sched, uint32_t max_size)
        : sched(sched)
        , values(List<T>::empty())
        , pendingPuts(List<PendingPut>::empty())
        , pendingTakes(List<PromiseRef<T,E>>::empty())
        , max_size(max_size)
    {}

    QueueState(const std::shared_ptr<Scheduler>& sched, uint32_t max_size, const ListRef<T>& values, const ListRef<PendingPut>& pendingPuts, const ListRef<PromiseRef<T,E>>& pendingTakes)
        : sched(sched)
        , values(values)
        , pendingPuts(pendingPuts)
        , pendingTakes(pendingTakes)
        , max_size(max_size)
    {}
};

} // namespace cask::queue

#endif