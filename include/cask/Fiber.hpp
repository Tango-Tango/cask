//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_H_
#define _CASK_FIBER_H_

#include "cask/Cancelable.hpp"
#include "cask/Either.hpp"
#include "cask/Scheduler.hpp"
#include "cask/fiber/FiberOp.hpp"

namespace cask {

template <class T, class E>
class Fiber;

template <class T, class E>
using FiberRef = std::shared_ptr<Fiber<T,E>>;

template <class T, class E>
class Fiber : public Cancelable, public std::enable_shared_from_this<Fiber<T,E>> {
public:
    static FiberRef<T,E> create(const std::shared_ptr<const fiber::FiberOp>& op);
    static FiberRef<T,E> run(const std::shared_ptr<const fiber::FiberOp>& op, const std::shared_ptr<Scheduler>& sched);
    static std::optional<Either<T,E>> runSync(const std::shared_ptr<const fiber::FiberOp>& op);

    virtual int getId() = 0;
    virtual const fiber::FiberValue& getRawValue() = 0;
    virtual std::optional<T> getValue() = 0;
    virtual std::optional<E> getError() = 0;
    virtual bool isCanceled() = 0;

    virtual void onFiberShutdown(const std::function<void(Fiber<T,E>*)>& callback) = 0;
    virtual T await() = 0;

    template <class T2, class E2>
    FiberRef<T2,E2> mapBoth(
        const std::function<T2(const T&)>& value_transform,
        const std::function<E2(const E&)>& error_transform
    );
};

}

#include "cask/fiber/FiberImpl.hpp"
#include "cask/fiber/FiberMap.hpp"

namespace cask {

template <class T, class E>
FiberRef<T,E> Fiber<T,E>::create(const std::shared_ptr<const fiber::FiberOp>& op) {
    return std::make_shared<Fiber<T,E>>(op);
}

template <class T, class E>
FiberRef<T,E> Fiber<T,E>::run(const std::shared_ptr<const fiber::FiberOp>& op, const std::shared_ptr<Scheduler>& sched) {
    auto fiber = std::make_shared<fiber::FiberImpl<T,E>>(op);
    fiber->reschedule(sched);
    return fiber;
}

template <class T, class E>
std::optional<Either<T,E>> Fiber<T,E>::runSync(const std::shared_ptr<const fiber::FiberOp>& op) {
    auto fiber = std::make_shared<fiber::FiberImpl<T,E>>(op);
    fiber->resumeSync();

    if(auto value_opt = fiber->getValue()) {
        return Either<T,E>::left(*value_opt);
    } else if(auto error_opt = fiber->getError()) {
        return Either<T,E>::right(*error_opt);
    } else {
        return {};
    }
}

template <class T, class E>
template <class T2, class E2>
FiberRef<T2,E2> Fiber<T,E>::mapBoth(
    const std::function<T2(const T&)>& value_transform,
    const std::function<E2(const E&)>& error_transform
) {
    return std::make_shared<fiber::FiberMap<T,T2,E,E2>>(this->shared_from_this(), value_transform, error_transform);
}

}

#endif