//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_APPEND_ALL_OBSERVER_H_
#define _CASK_APPEND_ALL_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {


template <class T, class E>
class AppendAllObserver final : public Observer<T,E> {
public:
    AppendAllObserver(
        const std::shared_ptr<Scheduler>& sched,
        const std::shared_ptr<Observer<T,E>>& downstream,
        const ObservableConstRef<T,E>& next
    );

    Task<Ack,None> onNext(T&& value) override;
    Task<None,None> onError(E&& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::shared_ptr<Scheduler> sched;
    std::shared_ptr<Observer<T,E>> downstream;
    ObservableConstRef<T,E> next;
};


template <class T, class E>
AppendAllObserver<T,E>::AppendAllObserver(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& downstream,
    const ObservableConstRef<T,E>& next
)
    : sched(sched)
    , downstream(downstream)
    , next(next)
{}

template <class T, class E>
Task<Ack,None> AppendAllObserver<T,E>::onNext(T&& value) {
    return downstream->onNext(std::forward<T>(value));
}

template <class T, class E>
Task<None,None> AppendAllObserver<T,E>::onError(E&& error) {
    return downstream->onError(std::forward<E>(error));
}

template <class T, class E>
Task<None,None> AppendAllObserver<T,E>::onComplete() {
    return next
        ->template mapBothTask<Ack,None>(
            [downstream = downstream](T&& value) {
                return downstream->onNext(std::forward<T>(value));
            },
            [downstream = downstream](E&& error) {
                return downstream
                    ->onError(std::forward<E>(error))
                    .template flatMap<Ack>([](auto&&) {
                        return Task<Ack,None>::raiseError(None());
                    });
            }
        )
        ->takeWhileInclusive([](auto&& ack){
            return ack == Continue;
        })
        ->completed()
        .materialize()
        .template flatMap<None>([downstream = downstream](auto&& result) {
            if(result.is_left()) {
                return downstream->onComplete();
            } else {
                return Task<None,None>::none();
            }
        });
}

template <class T, class E>
Task<None,None> AppendAllObserver<T,E>::onCancel() {
    return downstream->onCancel();
}

} // namespace cask::observable

#endif