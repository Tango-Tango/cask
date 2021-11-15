//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SWITCH_MAP_OBSERVER_H_
#define _CASK_SWITCH_MAP_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. When upstream provides a new value the downstream observable
 * is cancelled and a new subscription is started.  Normally obtained by calling `Observable<T>::switchMap`
 * and then subscribring to the resulting observable.
 */
template <class TI, class TO, class E>
class SwitchMapObserver final : public Observer<TI,E> {
public:
    SwitchMapObserver(const std::function<ObservableRef<TO,E>(const TI&)>& predicate,
                      const std::shared_ptr<Observer<TO,E>>& downstream,
                      const std::shared_ptr<Scheduler>& sched);

    Task<Ack,None> onNext(const TI& value) override;
    Task<None,None> onError(const E& value) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;

    int cancelRunningSubscription();
private:
    std::function<ObservableRef<TO,E>(TI)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    std::shared_ptr<Scheduler> sched;

    std::shared_ptr<std::atomic_bool> upstream_complete;
    std::shared_ptr<std::atomic_bool> downstream_complete;
    std::shared_ptr<std::atomic_bool> error_encountered;
    std::shared_ptr<std::atomic_bool> stopped;
    std::shared_ptr<std::atomic_int> running_id;
    CancelableRef running_subscription;
};

template <class TI, class TO, class E>
SwitchMapObserver<TI,TO,E>::SwitchMapObserver(
    const std::function<ObservableRef<TO,E>(const TI&)>& predicate,
    const std::shared_ptr<Observer<TO,E>>& downstream,
    const std::shared_ptr<Scheduler>& sched)
    : predicate(predicate)
    , downstream(downstream)
    , sched(sched)
    , upstream_complete(std::make_shared<std::atomic_bool>(false))
    , downstream_complete(std::make_shared<std::atomic_bool>(true))
    , error_encountered(std::make_shared<std::atomic_bool>(false))
    , stopped(std::make_shared<std::atomic_bool>(false))
    , running_id(std::make_shared<std::atomic_int>(0))
    , running_subscription(nullptr)
{}

template <class TI, class TO, class E>
Task<Ack,None> SwitchMapObserver<TI,TO,E>::onNext(const TI& value) {
    if(upstream_complete->load() || error_encountered->load() || stopped->load()) {
        return Task<Ack,None>::pure(Stop);
    } else {
        int my_id = cancelRunningSubscription();
        downstream_complete->exchange(false);

        CancelableRef next_subscription = predicate(value)->subscribeHandlers(
            sched,
            [downstream = downstream, stopped = stopped, running_id = running_id, my_id](const TO& value) {
                // Provide the value to downstream. If downstream provides a stop, we need to provide that
                // stop upstream at the next opportunity. If this subscription is no longer valid (because
                // the ID doesn't match the running ID) then go ahead and provide back a Stop.

                if(running_id->load() == my_id) {
                    return downstream->onNext(value)
                        .template map<Ack>([stopped](auto ack) {
                            if(ack == Stop) {
                                stopped->exchange(true);
                            }
                            return ack;
                        });
                } else {
                    return Task<Ack,None>::pure(Stop);
                }
            },
            [downstream = downstream, error_encountered = error_encountered, running_id = running_id, my_id](const E& error) {
                // Provide the error to downstream if an error has not already been provided and this
                // subscription is still valid (because the ID matches the running ID).

                if(running_id->load() == my_id && !error_encountered->exchange(true)) {
                    return downstream->onError(error);
                } else {
                    return Task<None,None>::none();
                }
            },
            [downstream = downstream, upstream_complete = upstream_complete, downstream_complete = downstream_complete, running_id = running_id, my_id] {
                // If both upstream is already complete and this subscription is still valid
                // (because the ID matches the running ID) then push the complete to downstream.

                if(running_id->load() == my_id && !downstream_complete->exchange(true) && upstream_complete->load()) {
                    return downstream->onComplete();
                } else {
                    return Task<None,None>::none();
                }
            },
            [downstream = downstream, running_id = running_id, my_id] {
                // If upstream provides a cancel then we simply provide that cancel downstream
                // so long as the downstream subscription is still valid

                if(running_id->load() == my_id) {
                    return downstream->onCancel();
                } else {
                    return Task<None,None>::none();
                }
            }
        );

        std::atomic_exchange(&running_subscription, next_subscription);
        return Task<Ack,None>::pure(Continue);
    }
}

template <class TI, class TO, class E>
Task<None,None> SwitchMapObserver<TI,TO,E>::onError(const E& error) {
    // When upstream provides an error we push that error downstream if
    // and error has not already been provided. We also cancel the ongoing
    // background computation.

    if(!error_encountered->exchange(true)) {
        cancelRunningSubscription();
        return downstream->onError(error);
    } else {
        return Task<None,None>::none();
    }
}

template <class TI, class TO, class E>
Task<None,None> SwitchMapObserver<TI,TO,E>::onComplete() {
    // When upstream completes we must _also_ wait for the inner
    // subscription to complete before calling onComplete downstream.

    if(!upstream_complete->exchange(true) && downstream_complete->load()) {
        return downstream->onComplete();
    } else {
        return Task<None,None>::none();
    }
}

template <class TI, class TO, class E>
Task<None,None> SwitchMapObserver<TI,TO,E>::onCancel() {
    auto cleanup_task = Task<None,None>::eval([self_weak = this->weak_from_this()] {
        if(auto self = self_weak.lock()) {
            auto casted_self = std::static_pointer_cast<SwitchMapObserver<TI,TO,E>>(self);
            casted_self->cancelRunningSubscription();
        }
        return None();
    });
    
    if(!upstream_complete->exchange(true) && !downstream_complete->load()) {
        return downstream->onCancel().template flatMap<None>([cleanup_task](auto) {
            return cleanup_task;
        });
    } else {
        return Task<None,None>::none();
    }
}

template <class TI, class TO, class E>
int SwitchMapObserver<TI,TO,E>::cancelRunningSubscription() {
    // Cancel the running observer by atomically replacing its
    // ref with nullptr - and then calling cancel on it. We'll
    // also increment the running_id and return the new ID to the
    // caller. This will invalidate any processing which may still
    // be running but is uncancelable for whatever reason.

    CancelableRef nullref;
    auto observer = std::atomic_exchange(&running_subscription, nullref);
    if(observer != nullptr) {
        observer->cancel();
    }
    return ++(*running_id);
}

}

#endif