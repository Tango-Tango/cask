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

template <class T, class E>
class MVarState {
public:
    using PendingPut = std::tuple<PromiseRef<None,E>,T>;

    std::shared_ptr<Scheduler> sched;
    std::optional<T> valueOpt;
    ListRef<PendingPut> pendingPuts;
    ListRef<PromiseRef<T,E>> pendingTakes;

    std::tuple<MVarState<T,E>,Task<None,E>> put(const T& value) const;
    std::tuple<MVarState<T,E>,Task<T,E>> take() const;
    
    MVarState(const std::shared_ptr<Scheduler>& sched);
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
std::tuple<MVarState<T,E>,Task<None,E>> MVarState<T,E>::put(const T& value) const {
    if(!pendingTakes->is_empty()) {
        auto takePromise = *(pendingTakes->head());
        return std::make_tuple(
            MVarState(sched, valueOpt, pendingPuts, pendingTakes->tail()),
            Task<None,E>::eval([takePromise, value] {
                takePromise->success(value);
                return None();
            })
        );
    } else if(!valueOpt.has_value()) {
        return std::make_tuple(
            MVarState(sched, value, pendingPuts, pendingTakes),
            Task<None,E>::none()
        );
    } else {
        auto promise = Promise<None,E>::create(sched);
        auto pending = std::make_tuple(promise, value);

        return std::make_tuple(
            MVarState(sched, valueOpt, pendingPuts->append(pending), pendingTakes),
            Task<None,E>::forPromise(promise)
        );
    }
}

template <class T, class E>
std::tuple<MVarState<T,E>,Task<T,E>> MVarState<T,E>::take() const {
    if(valueOpt.has_value()) {
        return std::make_tuple(
            MVarState(sched, {}, pendingPuts, pendingTakes),
            Task<T,E>::pure(*valueOpt)
        );
    } else if(!pendingPuts->is_empty()) {
        auto pending = *(pendingPuts->head());
        auto putPromise = std::get<0>(pending);
        auto value = std::get<1>(pending);
        return std::make_tuple(
            MVarState(sched, valueOpt, pendingPuts->tail(), pendingTakes),
            Task<T,E>::eval([putPromise, value] {
                putPromise->success(None());
                return value;
            })
        );
    } else {
        auto promise = Promise<T,E>::create(sched);
        return std::make_tuple(
            MVarState(sched, valueOpt, pendingPuts, pendingTakes->append(promise)),
            Task<T,E>::forPromise(promise)
        );
    }
}

}

#endif