//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_H_
#define _CASK_FIBER_H_

#include <atomic>
#include <iostream>
#include "cask/FiberOp.hpp"
#include "cask/Scheduler.hpp"

namespace cask {

enum FiberState { READY, RUNNING, WAITING, DELAYED, RACING, COMPLETED, CANCELED };

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
    const FiberValue& getRawValue();
    std::optional<T> getValue();
    std::optional<E> getError();

    void cancel();
    void onShutdown(const ShutdownCallback& callback);

private:
    void asyncError(const Erased& error);
    void asyncSuccess(const Erased& value);
    void asyncCancel();
    void delayFinished();
    bool racerFinished(const std::shared_ptr<Fiber<Erased,Erased>> racer);

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
    std::atomic_bool attempting_cancel;
    std::atomic_bool awaiting_first_racer;
    std::vector<std::shared_ptr<Fiber<Erased,Erased>>> racing_fibers;
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
    , attempting_cancel(false)
    , awaiting_first_racer(true)
    , racing_fibers()
{}

template <class T, class E>
Fiber<T,E>::~Fiber()
{
    if(state.load() != COMPLETED) {
        cancel();
        resumeSync();
    }

    while(state.load() == RUNNING);
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
    FiberState current_state = state.load();

    if(attempting_cancel.load()) {
        while(true) {
            current_state = state.load();
            if(state == RUNNING || state == COMPLETED || state == CANCELED) {
                return false;
            } else {
                if(state.compare_exchange_strong(current_state, RUNNING)) {
                    break;
                }
            }
        }

        value.setCanceled();

        if(current_state == WAITING) {
            waitingOn->cancel();
            waitingOn = nullptr;
        } else if(current_state == DELAYED) {
            delayedBy->cancel();
            delayedBy = nullptr;
        } else if(current_state == RACING) {
            racing_fibers.clear();
        }

    } else if(state != READY || (state == READY && !state.compare_exchange_strong(current_state, RUNNING))) {
        return false;
    }

    while(batch_size-- > 0) {
        switch(op->opType) {
            case VALUE:
            if(!value.isCanceled()) {
                const FiberOp::ConstantData* data = op->data.constantData;
                value.setValue(data->get_left());
                op = nullptr;
            }
            break;
            case ERROR:
            if(!value.isCanceled()) {
                const FiberOp::ConstantData* data = op->data.constantData;
                value.setError(data->get_right());
                op = nullptr;
            }
            break;
            case THUNK:
            if(!value.isCanceled()) {
                const FiberOp::ThunkData* thunk = op->data.thunkData;
                value.setValue((*thunk)());
                op = nullptr;
            }
            break;
            case ASYNC:
            if(!value.isCanceled()) {
                if constexpr(Async) {
                    state.store(WAITING);
                    const FiberOp::AsyncData* data = op->data.asyncData;
                    auto deferred = (*data)(sched);
                    waitingOn = deferred;
                    
                    deferred->onSuccess([self = this->shared_from_this(), sched](auto value) {
                        self->asyncSuccess(value);
                        sched->submit([self, sched] {
                            self->resume(sched);
                        });
                    });

                    deferred->onError([self = this->shared_from_this(), sched](auto error) {
                        self->asyncError(error);
                        sched->submit([self, sched] {
                            self->resume(sched);
                        });
                    });

                    deferred->onCancel([self = this->shared_from_this(), sched]() {
                        self->asyncCancel();
                        sched->submit([self, sched] {
                            self->resume(sched);
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
            if(!value.isCanceled()) {
                if constexpr(Async) {
                    state.store(DELAYED);
                    const FiberOp::DelayData* data = op->data.delayData;
                    delayedBy = sched->submitAfter(*data, [self = this->shared_from_this(), sched] {
                        self->delayFinished();
                        self->resume(sched);
                    });
                    return true;
                } else {
                    state.store(READY);
                    return true;
                }
            }
            break;
            case RACE:
            if(!value.isCanceled()) {
                if constexpr(Async) {
                    state.store(RACING);
                    awaiting_first_racer.store(true);

                    const FiberOp::RaceData* data = op->data.raceData;

                    for(auto& racer: *data) {
                        std::cout << "SETTING UP RACER" << std::endl;
                        auto fiber = Fiber<Erased,Erased>::create(racer);
                        fiber->onShutdown([self = this->shared_from_this(), fiber, sched](auto){
                            if(self->racerFinished(fiber)) {
                                sched->submit([self, sched] {
                                    self->resume(sched);
                                });
                            }
                        });
                        racing_fibers.emplace_back(fiber);
                    }

                    for(auto& fiber: racing_fibers) {
                        std::cout << "LAUNCHING RACER" << std::endl;
                        sched->submit([fiber, sched] {
                            fiber->resume(sched);
                        });
                    }

                    return true;
                } else {
                    state.store(READY);
                    return true;
                }
            }
            break;
        }

        if(!nextOp) {
            if(value.isCanceled()) {
                state.store(CANCELED);
            } else {
                state.store(COMPLETED);
            }
            
            std::vector<ShutdownCallback> local_callbacks;

            {
                std::lock_guard<std::mutex> guard(callback_mutex);
                for(auto& callback: callbacks) {
                    local_callbacks.emplace_back(callback);
                }
            }

            for(auto& callback: local_callbacks) {
                std::cout << "RUNNING CALLBACK" << std::endl;
                callback(this);
            }

            return true;
        } else if(op == nullptr) {
            op = nextOp(value);
            nextOp = nullptr;
        }
    }

    if constexpr(Async) {
        sched->submit([self = this->shared_from_this(), sched] {
            self->resume(sched);
        });
    }

    return true;
}

template <class T, class E>
FiberState Fiber<T,E>::getState() {
    return state;
}

template <class T, class E>
const FiberValue& Fiber<T,E>::getRawValue() {
    return value;
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

            std::vector<ShutdownCallback> local_callbacks;

            {
                std::lock_guard<std::mutex> guard(callback_mutex);
                for(auto& callback: callbacks) {
                    local_callbacks.emplace_back(callback);
                }
            }

            for(auto& callback: local_callbacks) {
                std::cout << "RUNNING CALLBACK" << std::endl;
                callback(this);
            }
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

            std::vector<ShutdownCallback> local_callbacks;

            {
                std::lock_guard<std::mutex> guard(callback_mutex);
                for(auto& callback: callbacks) {
                    local_callbacks.emplace_back(callback);
                }
            }

            for(auto& callback: local_callbacks) {
                std::cout << "RUNNING CALLBACK" << std::endl;
                callback(this);
            }
        }
    }
}

template <class T, class E>
void Fiber<T,E>::asyncCancel() {
    if(state.load() == WAITING) {
        value.setCanceled();
        this->waitingOn = nullptr;

        if(nextOp) {
            op = nextOp(value);
            nextOp = nullptr;
            state.store(READY);
        } else {
            state.store(CANCELED);
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
bool Fiber<T,E>::racerFinished(const std::shared_ptr<Fiber<Erased,Erased>> racer) {
    if(state.load() == RACING && awaiting_first_racer.exchange(false)) {
        std::cout << "FIRST RACER" << std::endl;

        this->value = racer->getRawValue();

        for(auto& other_racer: racing_fibers) {
            other_racer->cancel();
        }

        racing_fibers.clear();

        if(nextOp) {
            delayedBy = nullptr;
            op = nextOp(value);
            nextOp = nullptr;
            state.store(READY);
        } else if(value.isCanceled()) {
            state.store(CANCELED);
        } else {
            state.store(COMPLETED);
        }

        return true;
    } else {
        std::cout << "OTHER RACER" << std::endl;
        return false;
    }
}

template <class T, class E>
void Fiber<T,E>::cancel() {
    attempting_cancel.store(true);
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