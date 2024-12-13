//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_QUEUE_OBSERVER_H_
#define _CASK_QUEUE_OBSERVER_H_

#include "../Observer.hpp"
#include "../Queue.hpp"
#include "QueueOverflowStrategy.hpp"

namespace cask::observable {

template <class T, class E>
class QueueObserver final : public Observer<T,E>, public std::enable_shared_from_this<QueueObserver<T,E>> {
public:
    QueueObserver(
        const std::shared_ptr<Observer<T,E>>& downstream,
        uint32_t queue_size,
        QueueOverflowStrategy overflow_strategy,
        const std::shared_ptr<Scheduler>& sched
    );
    Task<Ack,None> onNext(T&& value) override;
    Task<None,None> onError(E&& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    class QueueEvent {
    public:
        virtual ~QueueEvent() = default;
    };

    class NextEvent : public QueueEvent {
    public:
        T value;
        explicit NextEvent(T&& value) : value(std::forward<T>(value)) {}
    };

    class ErrorEvent : public QueueEvent {
    public:
        E error;
        explicit ErrorEvent(E&& error) : error(std::forward<E>(error)) {}
    };

    class CompleteEvent : public QueueEvent {};
    class CancelEvent : public QueueEvent {};

    Task<None,None> onEvent(const std::shared_ptr<QueueEvent>& event);
    void initDownstreamFiber();
    Task<Ack,None> upstreamResult();

    std::shared_ptr<Observer<T,E>> downstream;
    std::shared_ptr<Scheduler> sched;
    QueueRef<std::shared_ptr<QueueEvent>,None> queue;
    QueueOverflowStrategy overflow_strategy;
    std::atomic_bool stopped;
    PromiseRef<None,None> downstream_shutdown_complete;
    FiberRef<None,None> downstream_fiber;
};

template <class T, class E>
QueueObserver<T,E>::QueueObserver(
        const std::shared_ptr<Observer<T,E>>& downstream,
        uint32_t queue_size,
        QueueOverflowStrategy overflow_strategy,
        const std::shared_ptr<Scheduler>& sched)
    : downstream(downstream)
    , sched(sched)
    , queue(Queue<std::shared_ptr<QueueEvent>,None>::empty(sched, queue_size))
    , overflow_strategy(overflow_strategy)
    , stopped(false)
    , downstream_shutdown_complete(Promise<None,None>::create(sched))
    , downstream_fiber()
{}

template <class T, class E>
Task<Ack,None> QueueObserver<T,E>::onNext(T&& value) {
    auto self_weak = this->weak_from_this();
    auto event = std::make_shared<NextEvent>(std::forward<T>(value));
    auto syncPutResult = queue->tryPut(event);

    initDownstreamFiber();

    if (syncPutResult || overflow_strategy == QueueOverflowStrategy::TailDrop) {
        return upstreamResult();
    } else {
        return queue->put(event)
            .onCancelRaiseError(None())
            .template flatMapBoth<Ack,None>(
                [self_weak](auto) {
                    if (auto self = self_weak.lock()) {
                        return self->upstreamResult();
                    } else {
                        return Task<Ack,None>::pure(Stop);
                    }
                },
                [](auto) {
                    return Task<Ack,None>::pure(Stop);
                }
            );
    }
}

template <class T, class E>
Task<None,None> QueueObserver<T,E>::onError(E&& error) {
    if (downstream_fiber == nullptr) {
        return downstream->onError(std::forward<E>(error));
    } else {
        auto self_weak = this->weak_from_this();
        auto event = std::make_shared<ErrorEvent>(std::forward<E>(error));

        return queue->put(event)
            .onCancelRaiseError(None())
            .template flatMapBoth<None,None>(
                [self_weak](auto) {
                    if (auto self = self_weak.lock()) {
                        return Task<None,None>::forPromise(self->downstream_shutdown_complete);
                    } else {
                        return Task<None,None>::none();
                    }
                },
                [](auto) {
                    return Task<None,None>::none();
                }
            );
    }
}

template <class T, class E>
Task<None,None> QueueObserver<T,E>::onComplete() {
    if (downstream_fiber == nullptr) {
        return downstream->onComplete();
    } else {
        auto self_weak = this->weak_from_this();
        auto event = std::make_shared<CompleteEvent>();

        return queue->put(event)
            .onCancelRaiseError(None())
            .template flatMapBoth<None,None>(
                [self_weak](auto) {
                    if (auto self = self_weak.lock()) {
                        return Task<None,None>::forPromise(self->downstream_shutdown_complete);
                    } else {
                        return Task<None,None>::none();
                    }
                },
                [](auto) {
                    return Task<None,None>::none();
                }
            );
    }
}

template <class T, class E>
Task<None,None> QueueObserver<T,E>::onCancel() {

    if (downstream_fiber == nullptr) {
        return downstream->onCancel();
    } else {
        auto self_weak = this->weak_from_this();
        auto event = std::make_shared<CancelEvent>();

        return queue->put(event)
            .onCancelRaiseError(None())
            .template flatMapBoth<None,None>(
                [self_weak](auto) {
                    if (auto self = self_weak.lock()) {
                        return Task<None,None>::forPromise(self->downstream_shutdown_complete);
                    } else {
                        return Task<None,None>::none();
                    }
                },
                [](auto) {
                    return Task<None,None>::none();
                }
            );
    }
}

template <class T, class E>
Task<None,None> QueueObserver<T,E>::onEvent(const std::shared_ptr<QueueEvent>& event) {
    auto self_weak = this->weak_from_this();

    if (auto next = std::dynamic_pointer_cast<NextEvent>(event)) {
        return downstream->onNext(std::forward<T>(next->value))
            .template map<None>([self_weak](auto ack) {
                if (ack == Stop) {
                    if (auto self = self_weak.lock()) {
                        self->stopped = true;
                        self->downstream_fiber->cancel();
                    }
                }

                return None();
            });
    } else if (auto error = std::dynamic_pointer_cast<ErrorEvent>(event)) {
        return downstream->onError(std::forward<E>(error->error))
            .template map<None>([self_weak](auto) {
                if (auto self = self_weak.lock()) {
                    self->downstream_fiber->cancel();
                }
                return None();
            });
    } else if (auto completed = std::dynamic_pointer_cast<CompleteEvent>(event)) {
        return downstream->onComplete()
            .template map<None>([self_weak](auto) {
                if (auto self = self_weak.lock()) {
                    self->downstream_fiber->cancel();
                }
                return None();
            });
    } else {
        return downstream->onCancel()
            .template map<None>([self_weak](auto) {
                if (auto self = self_weak.lock()) {
                    self->downstream_fiber->cancel();
                }
                return None();
            });
    }
}

template <class T, class E>
void QueueObserver<T,E>::initDownstreamFiber() {
    if (downstream_fiber == nullptr) {
        auto self_weak = this->weak_from_this();

        downstream_fiber = Observable<std::shared_ptr<QueueEvent>,None>::repeatTask(queue->take())
            ->template mapTask<None>([self_weak](auto event){
                if (auto self = self_weak.lock()) {
                    return self->onEvent(event);
                } else {
                    return Task<None,None>::none();
                }
            })
            ->completed()
            .run(sched);

        downstream_fiber->onFiberShutdown([self_weak](auto) {
            if (auto self = self_weak.lock()) {
                self->downstream_shutdown_complete->success(None());
                self->downstream_fiber = nullptr;
                self->queue->reset();
            }
        });
    }
}

template <class T, class E>
Task<Ack,None> QueueObserver<T,E>::upstreamResult() {
    if (stopped.load()) {
        return Task<Ack,None>::pure(Stop);
    } else {
        return Task<Ack,None>::pure(Continue);
    }
}

} // namespace cask::observable

#endif