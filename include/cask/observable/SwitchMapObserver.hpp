//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SWITCH_MAP_OBSERVER_H_
#define _CASK_SWITCH_MAP_OBSERVER_H_

#include "../MVar.hpp"
#include "../Observer.hpp"
#include "switchmap/SwitchMapState.hpp"

namespace cask::observable {

template <class U>
using StatefulResult = std::tuple<switchmap::SwitchMapState, U>;

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. When upstream provides a new value the downstream observable
 * is cancelled and a new subscription is started.  Normally obtained by calling `Observable<T>::switchMap`
 * and then subscribring to the resulting observable.
 */
template <class TI, class TO, class E>
class SwitchMapObserver final : public Observer<TI, E> {
public:
    SwitchMapObserver(const std::function<ObservableRef<TO, E>(const TI&)>& predicate,
                      const std::shared_ptr<Observer<TO, E>>& downstream,
                      const std::shared_ptr<Scheduler>& sched);

    Task<Ack, None> onNext(const TI& value) override;
    Task<None, None> onError(const E& value) override;
    Task<None, None> onComplete() override;
    Task<None, None> onCancel() override;

private:
    std::function<ObservableRef<TO, E>(TI)> predicate;
    std::shared_ptr<Observer<TO, E>> downstream;
    std::shared_ptr<Scheduler> sched;
    std::shared_ptr<MVar<switchmap::SwitchMapState, None>> stateVar;

    Task<StatefulResult<Ack>, None> onNextUnsafe(const TI& value, const switchmap::SwitchMapState& state);
    Task<StatefulResult<None>, None> onErrorUnsafe(const E& value, const switchmap::SwitchMapState& state);
    Task<StatefulResult<FiberRef<None, None>>, None> onCompleteUnsafe(const switchmap::SwitchMapState& state);
    Task<StatefulResult<None>, None> onCancelUnsafe(const switchmap::SwitchMapState& state);
};

} // namespace cask::observable

#include "switchmap/SwitchMapInternalObserver.hpp"

namespace cask::observable {

template <class TI, class TO, class E>
SwitchMapObserver<TI, TO, E>::SwitchMapObserver(const std::function<ObservableRef<TO, E>(const TI&)>& predicate,
                                                const std::shared_ptr<Observer<TO, E>>& downstream,
                                                const std::shared_ptr<Scheduler>& sched)
    : predicate(predicate)
    , downstream(downstream)
    , sched(sched)
    , stateVar(MVar<switchmap::SwitchMapState, None>::create(sched, {})) {}

template <class TI, class TO, class E>
Task<Ack, None> SwitchMapObserver<TI, TO, E>::onNext(const TI& value) {
    return stateVar->template modify<Ack>([self = this->shared_from_this(), value](auto state) {
        auto casted_self = std::static_pointer_cast<SwitchMapObserver<TI, TO, E>>(self);
        return casted_self->onNextUnsafe(value, state);
    });
}

template <class TI, class TO, class E>
Task<None, None> SwitchMapObserver<TI, TO, E>::onError(const E& error) {
    return stateVar->template modify<None>([self = this->shared_from_this(), error](auto state) {
        auto casted_self = std::static_pointer_cast<SwitchMapObserver<TI, TO, E>>(self);
        return casted_self->onErrorUnsafe(error, state);
    });
}

template <class TI, class TO, class E>
Task<None, None> SwitchMapObserver<TI, TO, E>::onComplete() {
    return stateVar
        ->template modify<FiberRef<None, None>>([self = this->shared_from_this()](auto state) {
            auto casted_self = std::static_pointer_cast<SwitchMapObserver<TI, TO, E>>(self);
            return casted_self->onCompleteUnsafe(state);
        })
        .template flatMap<None>([sched = sched](auto subscription) {
            if (subscription) {
                auto promise = Promise<None, None>::create(sched);
                subscription->onFiberShutdown([promise](auto) {
                    promise->success(None());
                });
                return Task<None, None>::forPromise(promise);
            } else {
                return Task<None, None>::none();
            }
        });
}

template <class TI, class TO, class E>
Task<None, None> SwitchMapObserver<TI, TO, E>::onCancel() {
    return stateVar->template modify<None>([self = this->shared_from_this()](auto state) {
        auto casted_self = std::static_pointer_cast<SwitchMapObserver<TI, TO, E>>(self);
        return casted_self->onCancelUnsafe(state);
    });
}

template <class TI, class TO, class E>
Task<StatefulResult<Ack>, None> SwitchMapObserver<TI, TO, E>::onNextUnsafe(const TI& value,
                                                                           const switchmap::SwitchMapState& state) {
    if (state.subscription && state.downstream_ack == Continue) {
        auto promise = Promise<None, None>::create(sched);

        state.subscription->cancel();
        state.subscription->onFiberShutdown([promise](auto) {
            promise->success(None());
        });

        return Task<None, None>::forPromise(promise).template flatMap<StatefulResult<Ack>>(
            [value, state, self = this->shared_from_this()](auto) {
                switchmap::SwitchMapState updated_state = state;
                updated_state.subscription = nullptr;

                auto casted_self = std::static_pointer_cast<SwitchMapObserver<TI, TO, E>>(self);
                return casted_self->onNextUnsafe(value, updated_state);
            });
    } else if (state.downstream_ack == Continue) {
        auto observer = std::make_shared<switchmap::SwitchMapInternalObserver<TO, E>>(downstream, stateVar);

        switchmap::SwitchMapState updated_state = state;
        updated_state.subscription = predicate(value)->subscribe(sched, observer);
        updated_state.subscription_completed = false;

        return Task<StatefulResult<Ack>, None>::pure({updated_state, Continue});
    } else {
        return Task<StatefulResult<Ack>, None>::pure({state, Stop});
    }
}

template <class TI, class TO, class E>
Task<StatefulResult<None>, None> SwitchMapObserver<TI, TO, E>::onErrorUnsafe(const E& error,
                                                                             const switchmap::SwitchMapState& state) {
    if (state.subscription) {
        auto promise = Promise<None, None>::create(sched);

        state.subscription->cancel();
        state.subscription->onFiberShutdown([promise](auto) {
            promise->success(None());
        });

        return Task<None, None>::forPromise(promise).template flatMap<StatefulResult<None>>(
            [error, state, self = this->shared_from_this()](auto) {
                switchmap::SwitchMapState updated_state = state;
                updated_state.subscription = nullptr;

                auto casted_self = std::static_pointer_cast<SwitchMapObserver<TI, TO, E>>(self);
                return casted_self->onErrorUnsafe(error, updated_state);
            });
    } else {
        return downstream->onError(error).template map<StatefulResult<None>>([state](auto) {
            return StatefulResult<None>({state, None()});
        });
    }
}

template <class TI, class TO, class E>
Task<StatefulResult<FiberRef<None, None>>, None>
SwitchMapObserver<TI, TO, E>::onCompleteUnsafe(const switchmap::SwitchMapState& state) {
    switchmap::SwitchMapState updated_state = state;
    updated_state.upstream_completed = true;
    updated_state.subscription = nullptr;

    StatefulResult<FiberRef<None, None>> next_state(updated_state, state.subscription);

    if (state.subscription_completed) {
        // The internal subscriber has already completed and is waiting for upstream
        // to complete before sending the signal to downstream

        return downstream->onComplete().template map<StatefulResult<FiberRef<None, None>>>([next_state](auto) {
            return next_state;
        });

    } else {
        // Need to wait for downstream to complete. Update state and then wait before
        // providing a result

        return Task<StatefulResult<FiberRef<None, None>>, None>::pure(next_state);
    }
}

template <class TI, class TO, class E>
Task<StatefulResult<None>, None> SwitchMapObserver<TI, TO, E>::onCancelUnsafe(const switchmap::SwitchMapState& state) {
    if (state.subscription) {
        auto promise = Promise<None, None>::create(sched);

        state.subscription->cancel();
        state.subscription->onFiberShutdown([promise](auto) {
            promise->success(None());
        });

        return Task<None, None>::forPromise(promise).template flatMap<StatefulResult<None>>(
            [state, self = this->shared_from_this()](auto) {
                switchmap::SwitchMapState updated_state = state;
                updated_state.subscription = nullptr;

                auto casted_self = std::static_pointer_cast<SwitchMapObserver<TI, TO, E>>(self);
                return casted_self->onCancelUnsafe(updated_state);
            });
    } else {
        return downstream->onCancel().template map<StatefulResult<None>>([state](auto) {
            return StatefulResult<None>({state, None()});
        });
    }
}

} // namespace cask::observable

#endif