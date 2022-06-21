//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FLAT_SCAN_OBSERVER_H_
#define _CASK_FLAT_SCAN_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class TI, class TO, class E>
class FlatScanObserver final : public Observer<TI,E>, public std::enable_shared_from_this<FlatScanObserver<TI,TO,E>> {
public:
    FlatScanObserver(const TO& seed,
                     const std::function<ObservableRef<TO,E>(const TO&, const TI&)>& predicate,
                     const std::shared_ptr<Observer<TO,E>>& downstream);
    

    Task<Ack,None> onNext(const TI& value) override;
    Task<None,None> onError(const E& value) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    TO state;
    std::function<ObservableRef<TO,E>(const TO&, const TI&)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    std::atomic_flag completed = ATOMIC_FLAG_INIT;
};


template <class TI, class TO, class E>
FlatScanObserver<TI,TO,E>::FlatScanObserver(
    const TO& seed,
    const std::function<ObservableRef<TO,E>(const TO&, const TI&)>& predicate,
    const std::shared_ptr<Observer<TO,E>>& downstream)
    : state(seed)
    , predicate(predicate)
    , downstream(downstream)
{}

template <class TI, class TO, class E>
Task<Ack,None> FlatScanObserver<TI,TO,E>::onNext(const TI& value) {
    auto self_weak = this->weak_from_this();

    return predicate(state, value)
        ->template mapBothTask<Ack,None>(
            [self_weak](auto updated_state) {
                if (auto self = self_weak.lock()) {
                    self->state = updated_state;
                    return self->downstream->onNext(self->state);
                } else {
                    return Task<Ack,None>::pure(Stop);
                }
            },
            [self_weak](auto error) {
                if(auto self = self_weak.lock()) {
                    return self->onError(error).template map<Ack>([](auto) {
                        return Stop;
                    });
                } else {
                    return Task<Ack,None>::pure(Stop);
                }
            }
        )
        ->takeWhileInclusive([](auto ack){
            return ack == Continue;
        })
        ->last()
        .template map<Ack>([self_weak](auto lastAckOpt) {
            if(lastAckOpt.has_value()) {
                return *lastAckOpt;
            } else {
                return Continue;
            }
        });
}

template <class TI, class TO, class E>
Task<None,None> FlatScanObserver<TI,TO,E>::onError(const E& error) {
    if(!completed.test_and_set()) {
        return downstream->onError(error);
    } else {
        return Task<None,None>::none();
    }
}

template <class TI, class TO, class E>
Task<None,None> FlatScanObserver<TI,TO,E>::onComplete() {
    if(!completed.test_and_set()) {
        return downstream->onComplete();
    } else {
        return Task<None,None>::none();
    }
}

template <class TI, class TO, class E>
Task<None,None> FlatScanObserver<TI,TO,E>::onCancel() {
    if(!completed.test_and_set()) {
        return downstream->onCancel();
    } else {
        return Task<None,None>::none();
    }
}

} // namespace cask::observable

#endif