//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_DEFERRED_H_
#define _CASK_FIBER_DEFERRED_H_

#include "cask/Deferred.hpp"
#include "cask/Fiber.hpp"

namespace cask::deferred {

template <class T, class E>
class FiberDeferred final : public Deferred<T,E> {
public:
    explicit FiberDeferred(std::shared_ptr<Fiber<T,E>> fiber);
    void onComplete(std::function<void(Either<T,E>)> callback) override;
    void onSuccess(std::function<void(T)> callback) override;
    void onError(std::function<void(E)> callback) override;
    void onCancel(const std::function<void()>& callback) override;
    void onShutdown(const std::function<void()>& callback) override;
    void cancel() override;
    T await() override;

private:
    std::shared_ptr<Fiber<T,E>> fiber;
};

template <class T, class E>
FiberDeferred<T,E>::FiberDeferred(std::shared_ptr<Fiber<T,E>> fiber)
    : fiber(fiber)
{}

template <class T, class E>
void FiberDeferred<T,E>::onComplete(std::function<void(Either<T,E>)> callback) {
    fiber->onFiberShutdown([callback](auto fiber) {
        if(auto value_opt = fiber->getValue()) {
            callback(Either<T,E>::left(*value_opt));
        } else if(auto error_opt = fiber->getError()) {
            callback(Either<T,E>::right(*error_opt));
        }
    });
}

template <class T, class E>
void FiberDeferred<T,E>::onSuccess(std::function<void(T)> callback) {
    fiber->onFiberShutdown([callback](auto fiber) {
        if(auto value_opt = fiber->getValue()) {
            callback(*value_opt);
        }
    });
}

template <class T, class E>
void FiberDeferred<T,E>::onError(std::function<void(E)> callback) {
    fiber->onFiberShutdown([callback](auto fiber) {
        if(auto error_opt = fiber->getError()) {
            callback(*error_opt);
        }
    });
}

template <class T, class E>
void FiberDeferred<T,E>::onCancel(const std::function<void()>& callback) {
    fiber->onCancel(callback);
}

template <class T, class E>
void FiberDeferred<T,E>::onShutdown(const std::function<void()>& callback) {
    fiber->onShutdown(callback);
}

template <class T, class E>
void FiberDeferred<T,E>::cancel() {
    fiber->cancel();
}

template <class T, class E>
T FiberDeferred<T,E>::await() {
    return fiber->await();
}

} // namespace cask::deferred

#endif