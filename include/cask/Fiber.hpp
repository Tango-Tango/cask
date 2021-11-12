//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_H_
#define _CASK_FIBER_H_

#include <atomic>
#include "cask/FiberOp.hpp"
#include "cask/Scheduler.hpp"

namespace cask {

enum FiberState { READY, RUNNING, WAITING, DELAYED, COMPLETED, CANCELED };

template <class T, class E>
class Fiber final : public std::enable_shared_from_this<Fiber<T,E>> {
public:
    using ShutdownCallback = std::function<void(Fiber<T,E>*)>;

    static std::shared_ptr<Fiber<T,E>> create(const std::shared_ptr<const FiberOp>& op);

    Fiber(const std::shared_ptr<const FiberOp>& op);
    ~Fiber();

    bool resumeSync();
    bool resume(const std::shared_ptr<Scheduler>& sched);

    FiberState getState();
    std::optional<T> getValue();
    std::optional<E> getError();

    void cancel();
    void onShutdown(const ShutdownCallback& callback);

private:
    void asyncError(const Erased& error);
    void asyncSuccess(const Erased& value);
    void delayFinished();

    template <bool Async>
    bool resumeUnsafe(const std::shared_ptr<Scheduler>& sched, unsigned int batch_size);

    std::shared_ptr<const FiberOp> op;
    std::shared_ptr<Scheduler> sched;
    FiberValue value;
    FiberOp::FlatMapPredicate nextOp;
    std::atomic<FiberState> state;
    DeferredRef<Erased,Erased> waitingOn;
    CancelableRef delayedBy;
    std::mutex callback_mutex;
    std::vector<ShutdownCallback> callbacks;
};

template <class T, class E>
std::shared_ptr<Fiber<T,E>> Fiber<T,E>::create(const std::shared_ptr<const FiberOp>& op) {
    return std::make_shared<Fiber<T,E>>(op);
}

template <class T, class E>
Fiber<T,E>::Fiber(const std::shared_ptr<const FiberOp>& op)
    : op(op)
    , value()
    , nextOp()
    , state(READY)
    , waitingOn()
    , delayedBy()
    , callback_mutex()
    , callbacks()
{}

template <class T, class E>
Fiber<T,E>::~Fiber()
{
    while(state.load() == RUNNING);

    if(state.load() != COMPLETED) {
        cancel();
    }
}

template <class T, class E>
bool Fiber<T,E>::resumeSync() {
    return resumeUnsafe<false>(nullptr, 1024);
}

template <class T, class E>
bool Fiber<T,E>::resume(const std::shared_ptr<Scheduler>& sched) {
    return resumeUnsafe<true>(sched, 1024);
}

template <class T, class E>
template <bool Async>
bool Fiber<T,E>::resumeUnsafe(const std::shared_ptr<Scheduler>& sched, unsigned int batch_size) {
    FiberState expected = READY;

    if(!state.compare_exchange_strong(expected, RUNNING)) {
        return false;
    }

    while(batch_size-- > 0) {
        switch(op->opType) {
            case VALUE:
            {
                const FiberOp::ConstantData* data = op->data.constantData;
                value.setValue(data->get_left());
                op = nullptr;
            }
            break;
            case ERROR:
            {
                const FiberOp::ConstantData* data = op->data.constantData;
                value.setError(data->get_right());
                op = nullptr;
            }
            break;
            case THUNK:
            {
                const FiberOp::ThunkData* thunk = op->data.thunkData;
                value.setValue((*thunk)());
                op = nullptr;
            }
            break;
            case ASYNC:
            {
                if constexpr(Async) {
                    state.store(WAITING);
                    const FiberOp::AsyncData* data = op->data.asyncData;
                    auto deferred = (*data)(sched);
                    auto self_weak = std::weak_ptr<Fiber<T,E>>(this->shared_from_this());
                    waitingOn = deferred;
                    
                    deferred->onSuccess([self_weak, sched](auto value) {
                        sched->submit([self_weak, value, sched] {
                            if(auto self = self_weak.lock()) {
                                self->asyncSuccess(value);
                                self->resume(sched);
                            }
                        });
                    });

                    deferred->onError([self_weak, sched](auto error) {
                        sched->submit([self_weak, error, sched] {
                            if(auto self = self_weak.lock()) {
                                self->asyncError(error);
                                self->resume(sched);
                            }
                        });
                    });

                    return true;
                } else {
                    state.store(READY);
                    return true;
                }
            }
            break;
            case FLATMAP:
            {
                const FiberOp::FlatMapData* data = op->data.flatMapData;
                nextOp = data->second;
                op = data->first;
            }
            break;
            case DELAY:
            {
                if constexpr(Async) {
                    state.store(DELAYED);
                    const FiberOp::DelayData* data = op->data.delayData;
                    auto self_weak = std::weak_ptr<Fiber<T,E>>(this->shared_from_this());
                    delayedBy = sched->submitAfter(*data, [self_weak, sched] {
                        if(auto self = self_weak.lock()) {
                            self->delayFinished();
                            self->resume(sched);
                        }
                    });
                    return true;
                } else {
                    state.store(READY);
                    return true;
                }
            }
            break;
        }

        if(!nextOp) {
            state.store(COMPLETED);
            std::vector<ShutdownCallback> local_callbacks;

            {
                std::lock_guard<std::mutex> guard(callback_mutex);
                for(auto& callback: callbacks) {
                    local_callbacks.emplace_back(callback);
                }
            }

            for(auto& callback: local_callbacks) {
                callback(this);
            }

            return true;
        } else if(op == nullptr) {
            op = nextOp(value);
            nextOp = nullptr;
        }
    }

    return true;
}

template <class T, class E>
FiberState Fiber<T,E>::getState() {
    return state;
}

template <class T, class E>
std::optional<T> Fiber<T,E>::getValue() {
    if(state.load() == COMPLETED && value.isValue()) {
        auto erased_value_opt = value.getValue();
        return erased_value_opt->template get<T>();
    } else {
        return {};
    }
}

template <class T, class E>
std::optional<E> Fiber<T,E>::getError() {
    if(state.load() == COMPLETED && value.isError()) {
        auto erased_error_opt = value.getError();
        return erased_error_opt->template get<E>();
    } else {
        return {};
    }
}

template <class T, class E>
void Fiber<T,E>::asyncError(const Erased& error) {
    if(state.load() == WAITING) {
        value.setError(error);
        waitingOn = nullptr;

        if(nextOp) {
            op = nextOp(value);
            nextOp = nullptr;
            state.store(READY);
        } else {
            state.store(COMPLETED);
        }
    }
}

template <class T, class E>
void Fiber<T,E>::asyncSuccess(const Erased& new_value) {
    if(state.load() == WAITING) {
        value.setValue(new_value);
        this->waitingOn = nullptr;

        if(nextOp) {
            op = nextOp(value);
            nextOp = nullptr;
            state.store(READY);
        } else {
            state.store(COMPLETED);
        }
    }
}

template <class T, class E>
void Fiber<T,E>::delayFinished() {
    if(state.load() == DELAYED) {
        if(nextOp) {
            delayedBy = nullptr;
            op = nextOp(value);
            nextOp = nullptr;
            state.store(READY);
        } else {
            state.store(COMPLETED);
        }
    }
}

template <class T, class E>
void Fiber<T,E>::cancel() {
    auto previous_state = state.exchange(CANCELED);

    if(previous_state == WAITING) {
        auto localWaiting = waitingOn;
        if(localWaiting) {
            localWaiting->cancel();
        }
        waitingOn = nullptr;
    } else if(previous_state == DELAYED) {
        auto localDelayedBy = delayedBy;

        if(localDelayedBy) {
            delayedBy->cancel();
        }

        delayedBy = nullptr;
    }

    if(previous_state != CANCELED) {
        std::vector<ShutdownCallback> local_callbacks;

        {
            std::lock_guard<std::mutex> guard(callback_mutex);
            for(auto& callback: callbacks) {
                local_callbacks.emplace_back(callback);
            }
        }

        for(auto& callback: local_callbacks) {
            callback(this);
        }
    }
}

template <class T, class E>
void Fiber<T,E>::onShutdown(const ShutdownCallback& callback) {
    bool run_callback_now = false;

    {
        std::lock_guard<std::mutex> guard(callback_mutex);
        if(state.load() != COMPLETED) {
            callbacks.emplace_back(callback);
        } else {
            run_callback_now = true;
        }
    }

    if(run_callback_now) {
        callback(this);
    }
}

}

#endif