//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SWITCH_MAP_OBSERVER_H_
#define _CASK_SWITCH_MAP_OBSERVER_H_

#include "../Observer.hpp"
#include "../MVar.hpp"
#include "switchmap/SwitchMapState.hpp"

namespace cask::observable {

template <class U>
using StatefulResult = std::tuple<switchmap::SwitchMapState,U>;

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. When upstream provides a new value the downstream observable
 * is cancelled and a new subscription is started.  Normally obtained by calling `Observable<T>::switchMap`
 * and then subscribring to the resulting observable.
 */
template <class TI, class TO, class E>
class SwitchMapObserver final : public Observer<TI,E>, public std::enable_shared_from_this<SwitchMapObserver<TI,TO,E>> {
public:
    SwitchMapObserver(const std::function<ObservableRef<TO,E>(TI&&)>& predicate,
                      const std::shared_ptr<Observer<TO,E>>& downstream,
                      const std::shared_ptr<Scheduler>& sched);

    Task<Ack,None> onNext(TI&& value) override;
    Task<None,None> onError(E&& value) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::function<ObservableRef<TO,E>(TI&&)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    std::shared_ptr<Scheduler> sched;
    std::shared_ptr<MVar<switchmap::SwitchMapState,None>> stateVar;

    Task<StatefulResult<Ack>,None> onNextUnsafe(TI value, const switchmap::SwitchMapState& state);
    Task<StatefulResult<None>,None> onErrorUnsafe(E value, const switchmap::SwitchMapState& state);
    Task<StatefulResult<FiberRef<None,None>>,None> onCompleteUnsafe(const switchmap::SwitchMapState& state);
    Task<StatefulResult<None>,None> onCancelUnsafe(const switchmap::SwitchMapState& state);
};

} // namespace cask::observable

#include "switchmap/SwitchMapInternalObserver.hpp"

namespace cask::observable {

template <class TI, class TO, class E>
SwitchMapObserver<TI,TO,E>::SwitchMapObserver(
    const std::function<ObservableRef<TO,E>(TI&&)>& predicate,
    const std::shared_ptr<Observer<TO,E>>& downstream,
    const std::shared_ptr<Scheduler>& sched)
    : predicate(predicate)
    , downstream(downstream)
    , sched(sched)
    , stateVar(MVar<switchmap::SwitchMapState,None>::create(sched, {}))
{}

template <class TI, class TO, class E>
Task<Ack,None> SwitchMapObserver<TI,TO,E>::onNext(TI&& value) {
    return stateVar->template modify<Ack>([self_weak = this->weak_from_this(), value = std::forward<TI>(value)](const auto& state) {
        if(auto self = self_weak.lock()) {
            return self->onNextUnsafe(value, state);
        } else {
            return Task<StatefulResult<Ack>,None>::pure(state, Stop);
        }
    });
}

template <class TI, class TO, class E>
Task<None,None> SwitchMapObserver<TI,TO,E>::onError(E&& error) {
    return stateVar->template modify<None>([self_weak = this->weak_from_this(), error = std::forward<E>(error)](const auto& state) {
        if(auto self = self_weak.lock()) {
            return self->onErrorUnsafe(error, state);
        } else {
            return Task<StatefulResult<None>,None>::pure(state, None());
        }
    });
}

template <class TI, class TO, class E>
Task<None,None> SwitchMapObserver<TI,TO,E>::onComplete() {
    return stateVar->template modify<FiberRef<None,None>>([self_weak = this->weak_from_this()](const auto& state) {
        if(auto self = self_weak.lock()) {
            return self->onCompleteUnsafe(state);
        } else {
            return Task<StatefulResult<FiberRef<None,None>>,None>::pure(state, nullptr);
        }
    }).template flatMap<None>([sched = sched](auto subscription) {
        if(subscription) {
            auto promise = Promise<None,None>::create(sched);
            subscription->onFiberShutdown([promise](auto) {
                promise->success(None());
            });
            return Task<None,None>::forPromise(promise);
        } else {
            return Task<None,None>::none();
        }
    });
}

template <class TI, class TO, class E>
Task<None,None> SwitchMapObserver<TI,TO,E>::onCancel() {
    return stateVar->template modify<None>([self_weak = this->weak_from_this()](const auto& state) {
        if(auto self = self_weak.lock()) {
            return self->onCancelUnsafe(state);
        } else {
            return Task<StatefulResult<None>,None>::pure(state, None());
        }
    });
}

template <class TI, class TO, class E>
Task<StatefulResult<Ack>,None> SwitchMapObserver<TI,TO,E>::onNextUnsafe(TI value, const switchmap::SwitchMapState& state) {
    if(state.subscription && state.downstream_ack == Continue) {
        auto promise = Promise<None,None>::create(sched);

        state.subscription->cancel();
        state.subscription->onFiberShutdown([promise](auto) {
            promise->success(None());
        });

        return Task<None,None>::forPromise(promise)
            .template flatMap<StatefulResult<Ack>>([value = std::forward<TI>(value), state, self_weak = this->weak_from_this()](None) {
                switchmap::SwitchMapState updated_state = state;
                updated_state.subscription = nullptr;

                if (auto self = self_weak.lock()) {
                    return self->onNextUnsafe(value, updated_state);
                } else {
                    return Task<StatefulResult<Ack>,None>::pure(updated_state, Stop);
                }
            });
    } else if(state.downstream_ack == Continue) {
        auto observer = std::make_shared<switchmap::SwitchMapInternalObserver<TO,E>>(downstream, stateVar);

        switchmap::SwitchMapState updated_state = state;
        updated_state.subscription = predicate(std::forward<TI>(value))->subscribe(sched, observer);
        updated_state.subscription_completed = false;

        return Task<StatefulResult<Ack>,None>::pure(updated_state, Continue);
    } else {
        return Task<StatefulResult<Ack>,None>::pure(state, Stop);
    }
}

template <class TI, class TO, class E>
Task<StatefulResult<None>,None> SwitchMapObserver<TI,TO,E>::onErrorUnsafe(E error, const switchmap::SwitchMapState& state) {
    if(state.subscription) {
        auto promise = Promise<None,None>::create(sched);

        state.subscription->cancel();
        state.subscription->onFiberShutdown([promise](auto) {
            promise->success(None());
        });

        return Task<None,None>::forPromise(promise)
            .template flatMap<StatefulResult<None>>([error, state, self_weak = this->weak_from_this()](auto&&) {
                switchmap::SwitchMapState updated_state = state;
                updated_state.subscription = nullptr;

                if (auto self = self_weak.lock()) {
                    return self->onErrorUnsafe(error, updated_state);
                } else {
                    return Task<StatefulResult<None>,None>::pure(updated_state, None());
                }
            });
    } else {
        return downstream->onError(std::forward<E>(error)).template map<StatefulResult<None>>([state](auto&&) {
            return StatefulResult<None>({state, None()});
        });
    }
}

template <class TI, class TO, class E>
Task<StatefulResult<FiberRef<None,None>>,None> SwitchMapObserver<TI,TO,E>::onCompleteUnsafe(const switchmap::SwitchMapState& state) {
    switchmap::SwitchMapState updated_state = state;
    updated_state.upstream_completed = true;
    updated_state.subscription = nullptr;

    StatefulResult<FiberRef<None,None>> next_state(updated_state, state.subscription);

    if(state.subscription_completed) {
        // The internal subscriber has already completed and is waiting for upstream
        // to complete before sending the signal to downstream

        return downstream->onComplete().template map<StatefulResult<FiberRef<None,None>>>([next_state](auto&&) {
            return next_state;
        });

    } else {
        // Need to wait for downstream to complete. Update state and then wait before
        // providing a result

        return Task<StatefulResult<FiberRef<None,None>>, None>::pure(next_state);
    }
}

template <class TI, class TO, class E>
Task<StatefulResult<None>,None> SwitchMapObserver<TI,TO,E>::onCancelUnsafe(const switchmap::SwitchMapState& state) {
    if(state.subscription) {
        auto promise = Promise<None,None>::create(sched);

        state.subscription->cancel();
        state.subscription->onFiberShutdown([promise](auto) {
            promise->success(None());
        });

        return Task<None,None>::forPromise(promise)
            .template flatMap<StatefulResult<None>>([state, self_weak = this->weak_from_this()](auto&&) {
                switchmap::SwitchMapState updated_state = state;
                updated_state.subscription = nullptr;

                if (auto self = self_weak.lock()) {
                    auto casted_self = std::static_pointer_cast<SwitchMapObserver<TI,TO,E>>(self);
                    return casted_self->onCancelUnsafe(updated_state);
                } else {
                    return Task<StatefulResult<None>,None>::pure(updated_state, None());
                }
            });
    } else {
        return downstream->onCancel().template map<StatefulResult<None>>([state](auto&&) {
            return StatefulResult<None>({state, None()});
        });
    }
}

} // namespace cask::observable

#endif