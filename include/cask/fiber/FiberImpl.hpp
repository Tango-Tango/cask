//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_IMPL_H_
#define _CASK_FIBER_IMPL_H_

#include <atomic>
#include <mutex>
#include "cask/Deferred.hpp"
#include "cask/Fiber.hpp"

namespace cask::fiber {

enum FiberState { READY, RUNNING, WAITING, DELAYED, RACING, COMPLETED, CANCELED };

template <class T, class E>
class FiberImpl final : public Fiber<T,E> {
public:
    FiberImpl(const std::shared_ptr<const FiberOp>& op);
    ~FiberImpl();

    FiberState getState();
    const FiberValue& getRawValue() override;
    std::optional<T> getValue() override;
    std::optional<E> getError() override;
    bool isCanceled() override;
    void cancel() override;
    void onCancel(const std::function<void()>& callback) override;
    void onShutdown(const std::function<void()>& callback) override;
    void onFiberShutdown(const std::function<void(Fiber<T,E>*)>& callback) override;
    T await() override;

private:
    friend Fiber<T,E>;

    bool resumeSync();
    bool resume(const std::shared_ptr<Scheduler>& sched);

    void asyncError(const Erased& error);
    void asyncSuccess(const Erased& value);
    void asyncCancel();
    void delayFinished();
    bool racerFinished(const std::shared_ptr<Fiber<Erased,Erased>>& racer);

    void reschedule(const std::shared_ptr<Scheduler>& sched);

    template <bool Async>
    bool evaluateOp(const std::shared_ptr<Scheduler>& sched);

    bool finishIteration();

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
    std::mutex racing_fibers_mutex;
    std::vector<std::function<void(Fiber<T,E>*)>> callbacks;
    std::atomic_bool attempting_cancel;
    std::vector<std::shared_ptr<Fiber<Erased,Erased>>> racing_fibers;
};

template <class T, class E>
FiberImpl<T,E>::FiberImpl(const std::shared_ptr<const FiberOp>& op)
    : op(op)
    , value()
    , nextOp()
    , state(READY)
    , waitingOn()
    , delayedBy()
    , callback_mutex()
    , racing_fibers_mutex()
    , callbacks()
    , attempting_cancel(false)
    , racing_fibers()
{}

template <class T, class E>
FiberImpl<T,E>::~FiberImpl()
{}

template <class T, class E>
bool FiberImpl<T,E>::resumeSync() {
    return resumeUnsafe<false>(nullptr, 1024);
}

template <class T, class E>
bool FiberImpl<T,E>::resume(const std::shared_ptr<Scheduler>& sched) {
    return resumeUnsafe<true>(sched, 1024);
}

template <class T, class E>
template <bool Async>
bool FiberImpl<T,E>::resumeUnsafe(const std::shared_ptr<Scheduler>& sched, unsigned int batch_size) {
    FiberState current_state = state.load();

    if(attempting_cancel.exchange(false)) {
        while(true) {
            current_state = state.load();
            if(state == COMPLETED || state == CANCELED) {
                return false;
            } else if (state != RUNNING) {
                if(state.compare_exchange_strong(current_state, RUNNING)) {
                    break;
                }
            }
        }

        value.setCanceled();
        op = FiberOp::cancel();

        if(current_state == WAITING) {
            if(waitingOn) {
                waitingOn->cancel();
                waitingOn = nullptr;
            }
        } else if(current_state == DELAYED) {
            if(delayedBy) {
                delayedBy->cancel();
                delayedBy = nullptr;
            }
        } else if(current_state == RACING) {
            std::lock_guard<std::mutex> guard(racing_fibers_mutex);
            for(auto& racer: racing_fibers) {
                racer->cancel();
            }
            racing_fibers.clear();
        }

    } else if(state != READY || (state == READY && !state.compare_exchange_strong(current_state, RUNNING))) {
        return false;
    }

    while(batch_size-- > 0) {
        if(this->template evaluateOp<Async>(sched)) {
            return true;
        }

        if( (nextOp == nullptr || op == nullptr) && finishIteration()) {
            return true;
        }
    }

    state.store(READY);

    if constexpr(Async) {
        reschedule(sched);
    }

    return true;
}

template <class T, class E>
FiberState FiberImpl<T,E>::getState() {
    return state;
}

template <class T, class E>
const FiberValue& FiberImpl<T,E>::getRawValue() {
    return value;
}

template <class T, class E>
std::optional<T> FiberImpl<T,E>::getValue() {
    if(state.load() == COMPLETED && value.isValue()) {
        return value.underlying().template get<T>();
    } else {
        return {};
    }
}

template <class T, class E>
std::optional<E> FiberImpl<T,E>::getError() {
    if(state.load() == COMPLETED && value.isError()) {
        return value.underlying().template get<E>();
    } else {
        return {};
    }
}

template <class T, class E>
bool FiberImpl<T,E>::isCanceled() {
    return state.load() == CANCELED;
}

template <class T, class E>
void FiberImpl<T,E>::asyncError(const Erased& error) {
    FiberState expected = WAITING;
    if(state.compare_exchange_strong(expected, RUNNING)) {
        value.setError(error);
        waitingOn = nullptr;

        if(!finishIteration()) {
            state.store(READY);
        }
    }
}

template <class T, class E>
void FiberImpl<T,E>::asyncSuccess(const Erased& new_value) {
    FiberState expected = WAITING;
    if(state.compare_exchange_strong(expected, RUNNING)) {
        value.setValue(new_value);
        waitingOn = nullptr;

        if(!finishIteration()) {
            state.store(READY);
        }
    }
}

template <class T, class E>
void FiberImpl<T,E>::asyncCancel() {
    FiberState expected = WAITING;
    if(state.compare_exchange_strong(expected, RUNNING)) {
        value.setCanceled();
        waitingOn = nullptr;

        if(!finishIteration()) {
            state.store(READY);
        }
    }
}

template <class T, class E>
void FiberImpl<T,E>::delayFinished() {
    FiberState expected = DELAYED;
    if(state.compare_exchange_strong(expected, RUNNING)) {
        delayedBy = nullptr;
        if(!finishIteration()) {
            state.store(READY);
        }
    }
}

template <class T, class E>
bool FiberImpl<T,E>::racerFinished(const std::shared_ptr<Fiber<Erased,Erased>>& racer) {
    FiberState expected = RACING;
    if(state.compare_exchange_strong(expected, RUNNING)) {
        {
            std::lock_guard<std::mutex> guard(racing_fibers_mutex);

            for(auto& other_racer : racing_fibers) {
                if(other_racer != racer) {
                    other_racer->cancel();
                }
            }

            racing_fibers.clear();
        }
        
        auto racerValue = racer->getRawValue();

        if(racerValue.isValue()) {
            value.setValue(racerValue.underlying());
        } else if(racerValue.isError()) {
            value.setError(racerValue.underlying());
        } else {
            value.setCanceled();
        }

        if(!finishIteration()) {
            state.store(READY);
        }

        return true;
    } else {
        return false;
    }
}

template <class T, class E>
void FiberImpl<T,E>::reschedule(const std::shared_ptr<Scheduler>& sched) {
    sched->submit([self_weak = this->weak_from_this(), sched_weak = std::weak_ptr(sched)] {
        if(auto self = self_weak.lock()) {
            if(auto sched = sched_weak.lock()) {
                std::static_pointer_cast<FiberImpl<T,E>>(self)->resume(sched);
            }
        }
    });
}

template <class T, class E>
template <bool Async>
bool FiberImpl<T,E>::evaluateOp(const std::shared_ptr<Scheduler>& sched) {
    bool suspended = false;

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
    case CANCEL:
    {
        value.setCanceled();
        op = nullptr;
    }
    break;
    case ASYNC:
    {
        suspended = true;
        if constexpr(Async) {
            const FiberOp::AsyncData* data = op->data.asyncData;
            auto deferred = (*data)(sched);
            waitingOn = deferred;            
            state.store(WAITING);          
            
            deferred->onSuccess([self_weak = this->weak_from_this(), sched](auto value) {
                if(auto self = self_weak.lock()) {
                    auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
                    casted_self->asyncSuccess(value);
                    casted_self->reschedule(sched);
                }
            });

            deferred->onError([self_weak = this->weak_from_this(), sched](auto error) {
                if(auto self = self_weak.lock()) {
                    auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
                    casted_self->asyncError(error);
                    casted_self->reschedule(sched);
                }
            });

            deferred->onCancel([self_weak = this->weak_from_this(), sched]() {
                if(auto self = self_weak.lock()) {
                    auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
                    casted_self->asyncCancel();
                    casted_self->reschedule(sched);
                }
            });

            
        } else {
            state.store(READY);
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
        suspended = true;
        if constexpr(Async) {
            const FiberOp::DelayData* data = op->data.delayData;
            delayedBy = sched->submitAfter(*data, [self_weak = this->weak_from_this(), sched_weak = std::weak_ptr(sched)] {
                if(auto self = self_weak.lock()) {
                    auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
                    casted_self->delayFinished();

                    if(auto sched = sched_weak.lock()) {
                        casted_self->resume(sched);
                    }
                }
            });
            state.store(DELAYED);
        } else {
            state.store(READY);
        }
    }
    break;
    case RACE:
    {
        suspended = true;
        if constexpr(Async) {
            std::lock_guard<std::mutex> guard(racing_fibers_mutex);
            state.store(RACING);
            const FiberOp::RaceData* data = op->data.raceData;

            for(auto& racer: *data) {
                auto fiber = Fiber<Erased,Erased>::run(racer, sched);
                fiber->onFiberShutdown([self_weak = this->weak_from_this(), fiber_weak = std::weak_ptr(fiber), sched](auto){
                    if(auto self = self_weak.lock()) {
                        if(auto fiber = fiber_weak.lock()) {
                            auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
                            if(casted_self->racerFinished(fiber)) {
                                casted_self->reschedule(sched);
                            }
                        }
                    }
                });
                racing_fibers.emplace_back(std::move(fiber));
            }
            
        } else {
            state.store(READY);
        }
    }
    break;
    }

    return suspended;
}

template <class T, class E>
bool FiberImpl<T,E>::finishIteration() {
    if(nextOp) {
        op = nextOp(value);
        nextOp = nullptr;
        return false;
    } else {
        if(value.isCanceled()) {
            state.store(CANCELED);
        } else {
            state.store(COMPLETED);
        }
        
        std::vector<std::function<void(Fiber<T,E>*)>> local_callbacks;

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
    }
}

template <class T, class E>
void FiberImpl<T,E>::cancel() {
    attempting_cancel.store(true);
}

template <class T, class E>
void FiberImpl<T,E>::onCancel(const std::function<void()>& callback) {
    onFiberShutdown([callback](auto fiber) {
        if(fiber->isCanceled()) {
            callback();
        }
    });
}

template <class T, class E>
void FiberImpl<T,E>::onShutdown(const std::function<void()>& callback) {
    onFiberShutdown([callback](auto fiber) {
        if(!fiber->isCanceled()) {
            callback();
        }
    });
}

template <class T, class E>
void FiberImpl<T,E>::onFiberShutdown(const std::function<void(Fiber<T,E>*)>& callback) {
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

template <class T, class E>
T FiberImpl<T,E>::await() {
    auto current_state = state.load();

    if(current_state != COMPLETED && current_state != CANCELED) {
        std::mutex mutex;
        mutex.lock();

        onFiberShutdown([&mutex](auto){
            mutex.unlock();
        });

        mutex.lock();
    }

    if(auto value_opt = value.getValue()) {
        return value_opt->template get<T>();
    } else if(auto error_opt = value.getError()) {
        throw error_opt->template get<E>();
    } else {
        throw std::runtime_error("Awaiting a fiber which was canceled");
    }
}

}

#endif