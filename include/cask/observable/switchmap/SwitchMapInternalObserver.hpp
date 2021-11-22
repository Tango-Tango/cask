//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SWITCH_INTERNAL_MAP_OBSERVER_H_
#define _CASK_SWITCH_INTERNAL_MAP_OBSERVER_H_

#include "../../Observer.hpp"

namespace cask::observable::switchmap {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. When upstream provides a new value the downstream observable
 * is cancelled and a new subscription is started.  Normally obtained by calling `Observable<T>::switchMap`
 * and then subscribring to the resulting observable.
 */
template <class T, class E>
class SwitchMapInternalObserver final : public Observer<T,E> {
public:
    SwitchMapInternalObserver(const std::shared_ptr<Observer<T,E>>& downstream,
                              const std::shared_ptr<MVar<switchmap::SwitchMapState,None>>& stateVar);

    Task<Ack,None> onNext(const T& value) override;
    Task<None,None> onError(const E& value) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::shared_ptr<Observer<T,E>> downstream;
    std::shared_ptr<MVar<switchmap::SwitchMapState,None>> stateVar;
};

template <class T, class E>
SwitchMapInternalObserver<T,E>::SwitchMapInternalObserver(
    const std::shared_ptr<Observer<T,E>>& downstream,
    const std::shared_ptr<MVar<switchmap::SwitchMapState,None>>& stateVar)
    : downstream(downstream)
    , stateVar(stateVar)
{}

template <class T, class E>
Task<Ack,None> SwitchMapInternalObserver<T,E>::onNext(const T& value) {
    return stateVar->template modify<Ack>([downstream = downstream, value](auto state) {
        return downstream->onNext(value).template map<StatefulResult<Ack>>([state](auto ack) {
            switchmap::SwitchMapState updated_state = state;
            updated_state.downstream_ack = ack;
            return StatefulResult<Ack>({updated_state, ack});
        });
    });
}

template <class T, class E>
Task<None,None> SwitchMapInternalObserver<T,E>::onError(const E& error) {
    return stateVar->template modify<None>([downstream = downstream, error](auto state) {
        switchmap::SwitchMapState updated_state = state;
        updated_state.downstream_ack = Stop;
        return downstream->onError(error).template map<StatefulResult<None>>([updated_state](auto) {
            return StatefulResult<None>({updated_state, None()});
        });
    });
}

template <class T, class E>
Task<None,None> SwitchMapInternalObserver<T,E>::onComplete() {
    return stateVar->template modify<None>([downstream = downstream](auto state) {
        switchmap::SwitchMapState updated_state = state;
        updated_state.downstream_completed = true;

        if(state.upstream_completed) {
            return downstream->onComplete().template map<StatefulResult<None>>([updated_state](auto) {
                return StatefulResult<None>({updated_state, None()});
            });
        } else {
            return Task<StatefulResult<None>,None>::pure({updated_state, None()});
        }
    });
}

template <class T, class E>
Task<None,None> SwitchMapInternalObserver<T,E>::onCancel() {
    return Task<None,None>::none();
}

}

#endif