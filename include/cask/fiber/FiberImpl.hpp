//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_IMPL_H_
#define _CASK_FIBER_IMPL_H_

#include <atomic>
#include <climits>
#include <mutex>
#include <map>
#include "cask/Config.hpp"
#include "cask/Deferred.hpp"
#include "cask/Fiber.hpp"
#include "cask/fiber/CurrentFiber.hpp"

namespace cask::fiber {

enum FiberState { READY, RUNNING, WAITING, DELAYED, RACING, COMPLETED, CANCELED };

template <class T, class E>
class FiberImpl final : public Fiber<T,E> {
public:
    explicit FiberImpl(const std::shared_ptr<const FiberOp>& op);
    ~FiberImpl();

    FiberState getState();
    uint64_t getId() override;
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
    friend class Fiber<T,E>;

    template <class TT, class EE>
    friend class FiberImpl;

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

    void setDeferredCallbacks(
        const DeferredRef<Erased,Erased>& deferred,
        const std::shared_ptr<Scheduler>& sched
    );

    uint64_t id;
    std::shared_ptr<const FiberOp> op;
    std::weak_ptr<Scheduler> last_used_scheduler;
    FiberValue value;
    FiberOp::FlatMapPredicate nextOp;
    std::atomic<FiberState> state;
    DeferredRef<Erased,Erased> waitingOn;
    CancelableRef delayedBy;
    std::mutex callback_mutex;
    std::mutex racing_fibers_mutex;
    std::vector<std::function<void(Fiber<T,E>*)>> callbacks;
    std::atomic_bool attempting_cancel;
    std::map<int, std::shared_ptr<Fiber<Erased,Erased>>> racing_fibers;
};

template <class T, class E>
FiberImpl<T,E>::FiberImpl(const std::shared_ptr<const FiberOp>& op)
    : id(CurrentFiber::acquireId())
    , op(op)
    , last_used_scheduler()
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
    CurrentFiber::setId(id);
    auto result = resumeUnsafe<false>(nullptr, UINT_MAX);
    CurrentFiber::clear();
    return result;
}

template <class T, class E>
bool FiberImpl<T,E>::resume(const std::shared_ptr<Scheduler>& sched) {
    CurrentFiber::setId(id);
    auto result = resumeUnsafe<true>(sched, batch_size);
    CurrentFiber::clear();
    return result;
}

template <class T, class E>
template <bool Async>
bool FiberImpl<T,E>::resumeUnsafe(const std::shared_ptr<Scheduler>& sched, unsigned int batch_size) {
    const auto forced_cede_disabled = batch_size == 0;

    FiberState current_state = state.load(std::memory_order_relaxed);

    if(state != READY || (state == READY && !state.compare_exchange_strong(current_state, RUNNING, std::memory_order_acquire, std::memory_order_relaxed))) {
        return false;
    }

    last_used_scheduler = std::weak_ptr<Scheduler>(sched);

    while(forced_cede_disabled || batch_size-- > 0) {
        if(this->template evaluateOp<Async>(sched)) {
            return true;
        }

        if( (nextOp == nullptr || op == nullptr) && finishIteration()) {
            return true;
        }
    }

    state.store(READY, std::memory_order_release);

    if constexpr(Async) {
        reschedule(sched);
    }

    return true;
}

template <class T, class E>
void FiberImpl<T,E>::setDeferredCallbacks(const DeferredRef<Erased,Erased>& deferred, const std::shared_ptr<Scheduler>& sched) {
    deferred->onSuccess([self_weak = this->weak_from_this(), sched_weak = std::weak_ptr<Scheduler>(sched)](auto value) {
        if(auto self = self_weak.lock()) {
            auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
            if(auto sched = sched_weak.lock()) {
                casted_self->asyncSuccess(value);
                casted_self->reschedule(sched);
            }
        }
    });

    deferred->onError([self_weak = this->weak_from_this(), sched_weak = std::weak_ptr<Scheduler>(sched)](auto error) {
        if(auto self = self_weak.lock()) {
            auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
            if(auto sched = sched_weak.lock()) {
                casted_self->asyncError(error);
                casted_self->reschedule(sched);
            }
        }
    });

    deferred->onCancel([self_weak = this->weak_from_this(), sched_weak = std::weak_ptr<Scheduler>(sched)]() {
        if(auto self = self_weak.lock()) {
            if(auto sched = sched_weak.lock()) {
                auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
                casted_self->asyncCancel();
                casted_self->reschedule(sched);
            }
        }
    });
}

template <class T, class E>
FiberState FiberImpl<T,E>::getState() {
    return state;
}

template <class T, class E>
uint64_t FiberImpl<T,E>::getId() {
    return id;
}

template <class T, class E>
const FiberValue& FiberImpl<T,E>::getRawValue() {
    return value;
}

template <class T, class E>
std::optional<T> FiberImpl<T,E>::getValue() {
    std::optional<T> result;

    if(state.load(std::memory_order_acquire) == COMPLETED && value.isValue()) {
        result = value.underlying().template get<T>();
    }

    return result;
}

template <class T, class E>
std::optional<E> FiberImpl<T,E>::getError() {
    std::optional<E> result;

    if(state.load(std::memory_order_acquire) == COMPLETED && value.isError()) {
        result = value.underlying().template get<E>();
    }

    return result;
}

template <class T, class E>
bool FiberImpl<T,E>::isCanceled() {
    return state.load(std::memory_order_relaxed) == CANCELED;
}

template <class T, class E>
void FiberImpl<T,E>::asyncError(const Erased& error) {
    FiberState expected = WAITING;
    if(state.compare_exchange_strong(expected, RUNNING, std::memory_order_acquire, std::memory_order_relaxed)) {
        value.setError(error);
        waitingOn = nullptr;

        if(!finishIteration()) {
            state.store(READY, std::memory_order_release);
        }
    }
}

template <class T, class E>
void FiberImpl<T,E>::asyncSuccess(const Erased& new_value) {
    FiberState expected = WAITING;
    if(state.compare_exchange_strong(expected, RUNNING, std::memory_order_acquire, std::memory_order_relaxed)) {
        value.setValue(new_value);
        waitingOn = nullptr;

        if(!finishIteration()) {
            state.store(READY, std::memory_order_release);
        }
    }
}

template <class T, class E>
void FiberImpl<T,E>::asyncCancel() {
    FiberState expected = WAITING;
    if(state.compare_exchange_strong(expected, RUNNING, std::memory_order_acquire, std::memory_order_relaxed)) {
        value.setCanceled();
        waitingOn = nullptr;

        if(!finishIteration()) {
            state.store(READY, std::memory_order_release);
        }
    }
}

template <class T, class E>
void FiberImpl<T,E>::delayFinished() {
    FiberState expected = DELAYED;
    if(state.compare_exchange_strong(expected, RUNNING, std::memory_order_acquire, std::memory_order_relaxed)) {
        delayedBy = nullptr;
        if(!finishIteration()) {
            state.store(READY, std::memory_order_release);
        }
    }
}

template <class T, class E>
bool FiberImpl<T,E>::racerFinished(const std::shared_ptr<Fiber<Erased,Erased>>& racer) {
    bool no_more_fibers = false;

    {
        std::lock_guard<std::mutex> guard(racing_fibers_mutex);
        racing_fibers.erase(racer->getId());
        no_more_fibers = racing_fibers.empty();
    }

    FiberState expected = RACING;
    if(state.compare_exchange_strong(expected, RUNNING, std::memory_order_acquire, std::memory_order_relaxed)) {
        if(!value.isCanceled()) value = racer->getRawValue();

        std::vector<FiberRef<Erased,Erased>> local_racers;

        {
            std::lock_guard<std::mutex> guard(racing_fibers_mutex);
            for(const auto& entry: racing_fibers) {
                const auto& fiber = std::get<1>(entry);
                local_racers.emplace_back(fiber);
            }
        }

        for(auto& fiber : local_racers) {
            fiber->cancel();
        }

    }

    if(no_more_fibers) {
        if(!finishIteration()) {
            state.store(READY, std::memory_order_release);
        }

        return true;
    } else {
        return false;
    }
}

template <class T, class E>
void FiberImpl<T,E>::reschedule(const std::shared_ptr<Scheduler>& sched) {
    last_used_scheduler = sched;
    sched->submit([self_weak = this->weak_from_this(), sched_weak = std::weak_ptr<Scheduler>(sched)] {
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
    case CEDE:// NOLINT(bugprone-branch-clone): When not async the linter picks this and ASYNC up as duplicate branches
    {
        if constexpr(Async) {
            auto promise = Promise<Erased,Erased>::create(sched);
            auto deferred = Deferred<Erased,Erased>::forPromise(promise);

            suspended = true;
            waitingOn = deferred;            
            state.store(WAITING, std::memory_order_release);
            setDeferredCallbacks(deferred, sched);

            sched->submit([promise_weak = std::weak_ptr<Promise<Erased,Erased>>(promise)] {
                if (auto promise = promise_weak.lock()) {
                    promise->success(None());
                }
            });
            
        } else {
            suspended = true;
            state.store(READY, std::memory_order_release);
        }
    }
    break;
    case ASYNC:
    {
        if constexpr(Async) {
            const FiberOp::AsyncData* data = op->data.asyncData;
            auto deferred = (*data)(sched);

            if (auto result_opt = deferred->get()) {
                if(result_opt->is_left()) {
                    value.setValue(result_opt->get_left());
                    op = nullptr;
                } else {
                    value.setError(result_opt->get_right());
                    op = nullptr;
                }
            } else {
                suspended = true;
                waitingOn = deferred;            
                state.store(WAITING, std::memory_order_release);
                setDeferredCallbacks(deferred, sched);
            }
            
        } else {
            suspended = true;
            state.store(READY, std::memory_order_release);
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
    case DELAY: // NOLINT(bugprone-branch-clone): When not async the linter picks this and RACE up as duplicate branches
    {
        suspended = true;
        if constexpr(Async) {
            const FiberOp::DelayData* data = op->data.delayData;
            delayedBy = sched->submitAfter(*data, [self_weak = this->weak_from_this(), sched_weak = std::weak_ptr<Scheduler>(sched)] {
                if(auto self = self_weak.lock()) {
                    if(auto sched = sched_weak.lock()) {
                        auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
                        casted_self->delayFinished();
                        casted_self->resume(sched);
                    }
                }
            });
            state.store(DELAYED, std::memory_order_release);
        } else {
            state.store(READY, std::memory_order_release);
        }
    }
    break;
    case RACE:
    {
        suspended = true;
        if constexpr(Async) {
            state.store(RACING, std::memory_order_release);
            const FiberOp::RaceData* data = op->data.raceData;
            std::vector<std::shared_ptr<FiberImpl<Erased,Erased>>> local_racing_fibers;

            {
                std::lock_guard<std::mutex> guard(racing_fibers_mutex);
                for(auto& racer: *data) {
                    auto fiber = std::make_shared<FiberImpl<Erased,Erased>>(racer);
                    fiber->onFiberShutdown([
                        self_weak = this->weak_from_this(),
                        fiber_weak = std::weak_ptr<FiberImpl<Erased,Erased>>(fiber),
                        sched_weak = std::weak_ptr<Scheduler>(sched)](auto){
                        if(auto self = self_weak.lock()) {
                            if(auto fiber = fiber_weak.lock()) {
                                if(auto sched = sched_weak.lock()) {
                                    auto casted_self = std::static_pointer_cast<FiberImpl<T,E>>(self);
                                    if(casted_self->racerFinished(fiber)) {
                                        casted_self->reschedule(sched);
                                    }
                                }
                            }
                        }
                    });
                    racing_fibers[fiber->getId()] = fiber;
                    local_racing_fibers.emplace_back(fiber);
                }
            }

            for(auto& racer: local_racing_fibers) {
                racer->reschedule(sched);
            }
            
        } else {
            state.store(READY, std::memory_order_release);
        }
    }
    break;
    }

    return suspended;
}

template <class T, class E>
bool FiberImpl<T,E>::finishIteration() {
    if(nextOp) {
        op = nextOp(std::move(value));
        nextOp = nullptr;
        return false;
    } else {
        if(value.isCanceled()) {
            state.store(CANCELED, std::memory_order_release);
        } else {
            state.store(COMPLETED, std::memory_order_release);
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
    FiberState current_state;

    while(true) {
        current_state = state.load(std::memory_order_relaxed);
        if(state == COMPLETED || state == CANCELED) {
            return;
        } else if (state != RUNNING) {
            if(state.compare_exchange_weak(current_state, RUNNING, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        } else if (auto sched = last_used_scheduler.lock()) {
            sched->submit([self = this->shared_from_this()] {
                self->cancel();
            });
            return;
        }
    }

    value.setCanceled();

    if(current_state == WAITING) {
        state.store(WAITING, std::memory_order_release);
        waitingOn->cancel();
        return;
    } else if(current_state == DELAYED) {
        delayedBy->cancel();
        delayedBy = nullptr;
    } else if(current_state == RACING) {
        state.store(RACING, std::memory_order_release);

        std::vector<FiberRef<Erased,Erased>> local_racers;

        {
            std::lock_guard<std::mutex> guard(racing_fibers_mutex);
            for(const auto& entry: racing_fibers) {
                const auto& fiber = std::get<1>(entry);
                local_racers.emplace_back(fiber);
            }
        }

        if (!local_racers.empty()) {
            local_racers[0]->cancel();
        }
        
        return;
    }
    
    if(!finishIteration()) {
        state.store(READY, std::memory_order_release);

        if(auto sched = last_used_scheduler.lock()) {
            reschedule(sched);
        } else {
            resumeSync();
            if(state.load(std::memory_order_relaxed) == READY) {
                throw std::runtime_error("Cannot finish processing async cancel without a scheduler.");
            }
        }
    }
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
        auto current_state = state.load(std::memory_order_acquire);
        if(current_state != COMPLETED && current_state != CANCELED) {
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
    auto current_state = state.load(std::memory_order_acquire);

    if(current_state != COMPLETED && current_state != CANCELED) {
        std::shared_ptr<std::mutex> mutex = std::make_shared<std::mutex>();
        mutex->lock();

        onFiberShutdown([mutex](auto){
            mutex->unlock();
        });

        mutex->lock();
    }

    if(auto value_opt = value.getValue()) {
        return value_opt->template get<T>();
    } else if(auto error_opt = value.getError()) {
        throw error_opt->template get<E>();
    } else {
        throw std::runtime_error("Awaiting a fiber which was canceled");
    }
}

} // namespace cask::fiber

#endif