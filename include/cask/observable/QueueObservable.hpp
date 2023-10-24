//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_QUEUE_OBSERVABLE_
#define _CASK_QUEUE_OBSERVABLE_

#include "../Observable.hpp"
#include "../Observer.hpp"
#include "QueueObserver.hpp"
#include "QueueOverflowStrategy.hpp"

namespace cask::observable {

template <class T, class E>
class QueueObservable final : public Observable<T,E> {
public:
    explicit QueueObservable(const ObservableConstRef<T,E>& upstream, uint32_t queue_size, QueueOverflowStrategy overflow_strategy = Backpressure);
    FiberRef<None,None> subscribe(const std::shared_ptr<Scheduler>& sched, const std::shared_ptr<Observer<T,E>>& observer) const override;
private:
    ObservableConstRef<T,E> upstream;
    uint32_t queue_size;
    QueueOverflowStrategy overflow_strategy;
};

template <class T, class E>
QueueObservable<T,E>::QueueObservable(const ObservableConstRef<T,E>& upstream, uint32_t queue_size, QueueOverflowStrategy overflow_strategy)
    : upstream(upstream)
    , queue_size(queue_size)
    , overflow_strategy(overflow_strategy)
{}

template <class T, class E>
FiberRef<None,None> QueueObservable<T,E>::subscribe(
    const std::shared_ptr<Scheduler>& sched,
    const std::shared_ptr<Observer<T,E>>& observer) const
{
    auto queueObserver = std::make_shared<QueueObserver<T,E>>(observer, queue_size, overflow_strategy, sched);
    return upstream->subscribe(sched, queueObserver);
}

} // namespace cask::observable

#endif
