//          Copyright Tango Tango, Inc. 2020 - 2022.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_MERGE_OBSERVER_H_
#define _CASK_MERGE_OBSERVER_H_

#include "../Observer.hpp"

namespace cask::observable {

template <class T, class E>
class MergeObserver final
    : public Observer<ObservableConstRef<T,E>,E>
    , public Observer<T,E>
    , public std::enable_shared_from_this<MergeObserver<T,E>>
{
public:
    MergeObserver(const std::shared_ptr<Observer<T,E>>& downstream, const std::shared_ptr<Scheduler>& sched);
    Task<Ack,None> onNext(const ObservableConstRef<T,E>& upstream) override;
    Task<Ack,None> onNext(const T& value) override;
    Task<None,None> onError(const E& error) override;
    Task<None,None> onComplete() override;
    Task<None,None> onCancel() override;
private:
    std::shared_ptr<Observer<T,E>> downstream;
    std::shared_ptr<Scheduler> sched;
    std::map<uint64_t,std::shared_ptr<Fiber<None,None>>> fibers;
    bool upstream_completed;
    bool awaiting_cancel;
    std::optional<E> cached_error;
    std::mutex mutex;
};

template <class T, class E>
MergeObserver<T,E>::MergeObserver(const std::shared_ptr<Observer<T,E>>& downstream, const std::shared_ptr<Scheduler>& sched)
    : downstream(downstream)
    , sched(sched)
    , fibers()
    , upstream_completed(false)
    , awaiting_cancel(false)
    , cached_error()
    , mutex()
{}

template <class T, class E>
Task<Ack,None> MergeObserver<T,E>::onNext(const ObservableConstRef<T,E>& upstream) {
    std::lock_guard<std::mutex> guard(mutex);

    std::cout << "onNext (upstream)" << std::endl;

    if (awaiting_cancel) {
        return Task<Ack,None>::pure(Stop);
    } else {
        auto self = this->shared_from_this();
        auto fiber = upstream->subscribe(sched, self);
        fibers[fiber->getId()] = fiber;
        return Task<Ack,None>::pure(Continue);
    }
    
}

template <class T, class E>
Task<Ack,None> MergeObserver<T,E>::onNext(const T& value) {
    std::lock_guard<std::mutex> guard(mutex);

    std::cout << "onNext (value)" << std::endl;

    if (awaiting_cancel) {
        return Task<Ack,None>::pure(Stop);
    } else {
        return downstream->onNext(value);
    }
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onError(const E& error) {
    std::lock_guard<std::mutex> guard(mutex);

    std::cout << "onError" << std::endl;

    if (auto current_id = Fiber<None,None>::currentFiberId()) {
        auto iter = fibers.find(*current_id);
        if (iter != fibers.end()) {
            std::cout << "onError - Erased Fiber" << std::endl;
            fibers.erase(iter);
        }
    }

    if (fibers.empty()) {
        std::cout << "onError - pushing downstream" << std::endl;
        return downstream->onError(error);
    } else {
        std::cout << "onError - canceling others" << std::endl;
        cached_error = error;
        awaiting_cancel = true;

        std::vector<uint64_t> fibers_to_erase;

        for (const auto& [id, fiber] : fibers) {
            fiber->cancel();

            if (fiber->isCanceled()) {
                fibers_to_erase.push_back(id);
            }
        }

        for (const auto& id : fibers_to_erase) {
            fibers.erase(id);
        }

        if (fibers.empty()) {
            std::cout << "onError - pushing downstream after sync cancelations" << std::endl;
            return downstream->onError(error);
        } else {
            std::cout << "onError - ignoring for now" << std::endl;
            return Task<None,None>::none();
        }
    }
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onComplete() {
    std::lock_guard<std::mutex> guard(mutex);

    std::cout << "onComplete" << std::endl;

    if (auto current_id = Fiber<None,None>::currentFiberId()) {
        auto iter = fibers.find(*current_id);
        if (iter != fibers.end()) {
            std::cout << "onComplete - erased fiber" << std::endl;
            fibers.erase(iter);
        } else {
            std::cout << "onComplete - upstream" << std::endl;
            upstream_completed = true;
        }
    }

    if (fibers.empty() && upstream_completed) {
        std::cout << "onComplete - completing downstream" << std::endl;
        return downstream->onComplete();
    } else {
        std::cout << "onComplete - waiting to complete downstream" << std::endl;
        return Task<None,None>::none();
    }
}

template <class T, class E>
Task<None,None> MergeObserver<T,E>::onCancel() {
    std::lock_guard<std::mutex> guard(mutex);

    std::cout << "onCancel" << std::endl;

    if (auto current_id = Fiber<None,None>::currentFiberId()) {
        auto iter = fibers.find(*current_id);
        if (iter != fibers.end()) {
            fibers.erase(iter);
        }
    }

    if (fibers.empty()) {
        if (cached_error.has_value()) {
            return downstream->onError(*cached_error);
        } else {
            return downstream->onCancel();
        }
    } else {
        if (!awaiting_cancel) {
            awaiting_cancel = true;
            for (const auto& fiber_entry : fibers) {
                const auto& fiber = std::get<1>(fiber_entry);
                fiber->cancel();
            }
        }

        return Task<None,None>::none();
    }
}

}

#endif
