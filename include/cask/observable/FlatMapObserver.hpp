//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FLAT_MAP_OBSERVER_H_
#define _CASK_FLAT_MAP_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

/**
 * Represents an observer that transforms each event received on a stream to a new value and emits the
 * transformed event to a downstream observer. Normally obtained by calling `Observable<T>::map` and
 * then subscribring to the resulting observable.
 */
template <class TI, class TO, class E>
class FlatMapObserver final : public Observer<TI,E>, public std::enable_shared_from_this<FlatMapObserver<TI,TO,E>> {
public:
    FlatMapObserver(const std::function<ObservableRef<TO,E>(TI&&)>& predicate,
                    const std::shared_ptr<Observer<TO,E>>& downstream);
    

    Task<Ack,None> onNext(TI&& value) override;
    Task<None,None> onError(const E& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;

    Task<Ack,None> onNextInternal(TO&& value);
    Task<None,None> onErrorInternal(const E& error);
    Task<None,None> onCompleteInternal();
    Task<None,None> onCancelInternal();
private:
    std::function<ObservableRef<TO,E>(TI)> predicate;
    std::shared_ptr<Observer<TO,E>> downstream;
    bool stopped;
};


template <class TI, class TO, class E>
FlatMapObserver<TI,TO,E>::FlatMapObserver(
    const std::function<ObservableRef<TO,E>(TI&&)>& predicate,
    const std::shared_ptr<Observer<TO,E>>& downstream)
    : predicate(predicate)
    , downstream(downstream)
    , stopped(false)
{}

template <class TI, class TO, class E>
Task<Ack,None> FlatMapObserver<TI,TO,E>::onNext(TI&& value) {
    auto self = this->shared_from_this();
    auto next_observable = self->predicate(value);

    return Task<None,None>::deferFiber([self, next_observable](auto sched) {
            return next_observable->subscribeHandlers(
                sched,
                [self] (auto value) { return self->onNextInternal(std::move(value)); },
                [self] (auto error) { return self->onErrorInternal(error); },
                [self] { return self->onCompleteInternal(); },
                [self] { return self->onCancelInternal(); }
            );
        })
        .template map<Ack>([self](auto) {
            if (self->stopped) {
                return Stop;
            } else {
                return Continue;
            }
        });
}

template <class TI, class TO, class E>
Task<None,None> FlatMapObserver<TI,TO,E>::onError(const E& error) {
    return downstream->onError(error);
}

template <class TI, class TO, class E>
Task<None,None> FlatMapObserver<TI,TO,E>::onComplete() {
    return downstream->onComplete();
}

template <class TI, class TO, class E>
Task<None,None> FlatMapObserver<TI,TO,E>::onCancel() {
    return downstream->onCancel();
}

template <class TI, class TO, class E>
Task<Ack,None> FlatMapObserver<TI,TO,E>::onNextInternal(TO&& value) {
    auto self = this->shared_from_this();
    return downstream->onNext(std::move(value))
        .template map<Ack>([self](auto ack) {
            if (ack == Stop) {
                self->stopped = true;
            }

            return ack;
        });
}

template <class TI, class TO, class E>
Task<None,None> FlatMapObserver<TI,TO,E>::onErrorInternal(const E& error) {
    stopped = true;
    return downstream->onError(error);
}

template <class TI, class TO, class E>
Task<None,None> FlatMapObserver<TI,TO,E>::onCompleteInternal() {
    return Task<None,None>::none();
}

template <class TI, class TO, class E>
Task<None,None> FlatMapObserver<TI,TO,E>::onCancelInternal() {
    stopped = true;
    return downstream->onCancel();
}

} // namespace cask::observable

#endif