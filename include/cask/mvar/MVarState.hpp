//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MVAR_STATE_H_
#define _CASK_MVAR_STATE_H_

#include <optional>
#include "../List.hpp"
#include "../Promise.hpp"
#include "../Task.hpp"

namespace cask::mvar {

/**
 * Represents the internal state of an MVar. It should not be used directly by consumers. It does
 * not provide any concurrency protection on its own and is expected to be protected by a `Ref`.
 */
template <class T, class E>
class MVarState {
public:
    using PendingPut = std::tuple<PromiseRef<None,E>,T>;

    std::shared_ptr<Scheduler> sched;
    std::optional<T> valueOpt;
    ListRef<PendingPut> pendingPuts;
    ListRef<PromiseRef<T,E>> pendingTakes;

    std::tuple<MVarState<T,E>,bool,std::function<void()>> tryPut(const T& value) const;

    std::tuple<MVarState<T,E>,Task<None,E>> put(const T& value) const;

    std::tuple<MVarState<T,E>,Task<T,E>> take() const;

    explicit MVarState(const std::shared_ptr<Scheduler>& sched);
    MVarState(const std::shared_ptr<Scheduler>& sched, const T& initialValue);
    MVarState(const std::shared_ptr<Scheduler>& sched, const std::optional<T>& valueOpt, const ListRef<PendingPut>& pendingPuts, const ListRef<PromiseRef<T,E>>& pendingTakes);
    MVarState(std::shared_ptr<Scheduler>&& sched, std::optional<T>&& valueOpt, ListRef<PendingPut>&& pendingPuts, ListRef<PromiseRef<T,E>>&& pendingTakes);
};

template <class T, class E>
MVarState<T,E>::MVarState(const std::shared_ptr<Scheduler>& sched)
    : sched(sched)
    , valueOpt()
    , pendingPuts(List<PendingPut>::empty())
    , pendingTakes(List<PromiseRef<T,E>>::empty())
{}

template <class T, class E>
MVarState<T,E>::MVarState(const std::shared_ptr<Scheduler>& sched, const T& value)
    : sched(sched)
    , valueOpt(value)
    , pendingPuts(List<PendingPut>::empty())
    , pendingTakes(List<PromiseRef<T,E>>::empty())
{}

template <class T, class E>
MVarState<T,E>::MVarState(const std::shared_ptr<Scheduler>& sched, const std::optional<T>& valueOpt, const ListRef<PendingPut>& pendingPuts, const ListRef<PromiseRef<T,E>>& pendingTakes)
    : sched(sched)
    , valueOpt(valueOpt)
    , pendingPuts(pendingPuts)
    , pendingTakes(pendingTakes)
{}

template <class T, class E>
MVarState<T,E>::MVarState(std::shared_ptr<Scheduler>&& sched, std::optional<T>&& valueOpt, ListRef<PendingPut>&& pendingPuts, ListRef<PromiseRef<T,E>>&& pendingTakes)
    : sched(sched)
    , valueOpt(valueOpt)
    , pendingPuts(pendingPuts)
    , pendingTakes(pendingTakes)
{}

template <class T, class E>
std::tuple<MVarState<T,E>,bool,std::function<void()>> MVarState<T,E>::tryPut(const T& value) const {
    auto filteredTakes = pendingTakes->dropWhile([](auto promise) {
        return promise->isCancelled();
    });

    if(!filteredTakes->is_empty()) {
        auto takePromise = *(filteredTakes->head());
        return std::make_tuple(
            MVarState(sched, valueOpt, pendingPuts, filteredTakes->tail()),
            true,
            [takePromise, value]{takePromise->success(value);}
        );
    } else if(!valueOpt.has_value()) {
        return std::make_tuple(
            MVarState(sched, value, pendingPuts, filteredTakes),
            true,
            []{}
        );
    } else {
        return std::make_tuple(
            MVarState(sched, valueOpt, pendingPuts, filteredTakes),
            false,
            []{}
        );
    }
}

template <class T, class E>
std::tuple<MVarState<T,E>,Task<None,E>> MVarState<T,E>::put(const T& value) const {
    auto result = tryPut(value);
    auto nextState = std::get<0>(result);
    auto completed = std::get<1>(result);
    auto thunk = std::get<2>(result);

    if(!completed) {
        auto promise = Promise<None,E>::create(nextState.sched);
        auto pending = std::make_tuple(promise, value);

        return std::make_tuple(
            MVarState(nextState.sched, nextState.valueOpt, nextState.pendingPuts->append(pending), nextState.pendingTakes),
            Task<None,E>::forPromise(promise)
        );
    } else {
        return std::make_tuple(nextState, Task<None,E>::eval([thunk] {
            thunk();
            return None();
        }));
    }
}

template <class T, class E>
std::tuple<MVarState<T,E>,Task<T,E>> MVarState<T,E>::take() const {
    auto filteredPuts = pendingPuts->dropWhile([](auto pending) {
        auto promise = std::get<0>(pending);
        return promise->isCancelled();
    });

    if(valueOpt.has_value()) {
        return std::make_tuple(
            MVarState(sched, {}, filteredPuts, pendingTakes),
            Task<T,E>::pure(*valueOpt)
        );
    } else if(!filteredPuts->is_empty()) {
        auto pending = *(filteredPuts->head());
        auto putPromise = std::get<0>(pending);
        auto value = std::get<1>(pending);
        return std::make_tuple(
            MVarState(sched, valueOpt, filteredPuts->tail(), pendingTakes),
            Task<T,E>::eval([putPromise, value] {
                putPromise->success(None());
                return value;
            })
        );
    } else {
        auto promise = Promise<T,E>::create(sched);
        return std::make_tuple(
            MVarState(sched, valueOpt, filteredPuts, pendingTakes->append(promise)),
            Task<T,E>::forPromise(promise)
        );
    }
}

} // namespace cask::mvar

#endif