//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_MAP_H_
#define _CASK_FIBER_MAP_H_

#include <atomic>
#include "cask/Fiber.hpp"

namespace cask::fiber {

template <class T1, class T2, class E1, class E2>
class FiberMap final : public Fiber<T2,E2> {
public:
    FiberMap(
        const FiberRef<T1,E1>& parent,
        const std::function<T2(const T1&)>& value_transform,
        const std::function<E2(const E1&)>& error_transform
    );

    int getId() override;
    const FiberValue& getRawValue() override;
    std::optional<T2> getValue() override;
    std::optional<E2> getError() override;
    bool isCanceled() override;
    void cancel() override;
    void onCancel(const std::function<void()>& callback) override;
    void onShutdown(const std::function<void()>& callback) override;
    void onFiberShutdown(const std::function<void(Fiber<T2,E2>*)>& callback) override;
    T2 await() override;

private:
    FiberRef<T1,E1> parent;
    std::function<T2(const T1&)> value_transform;
    std::function<E2(const E1&)> error_transform;
};

template <class T1, class T2, class E1, class E2>
FiberMap<T1,T2,E1,E2>::FiberMap(
    const FiberRef<T1,E1>& parent,
    const std::function<T2(const T1&)>& value_transform,
    const std::function<E2(const E1&)>& error_transform
)
    : parent(parent)
    , value_transform(value_transform)
    , error_transform(error_transform)
{}

template <class T1, class T2, class E1, class E2>
int FiberMap<T1,T2,E1,E2>::getId() {
    return parent->getId();
}

template <class T1, class T2, class E1, class E2>
const FiberValue& FiberMap<T1,T2,E1,E2>::getRawValue() {
    return parent->getRawValue();
}

template <class T1, class T2, class E1, class E2>
std::optional<T2> FiberMap<T1,T2,E1,E2>::getValue() {
    if(auto value_opt = parent->getValue()) {
        return value_transform(*value_opt);
    } else {
        return {};
    }
}

template <class T1, class T2, class E1, class E2>
std::optional<E2> FiberMap<T1,T2,E1,E2>::getError() {
    if(auto error_opt = parent->getError()) {
        return error_transform(*error_opt);
    } else {
        return {};
    }
}

template <class T1, class T2, class E1, class E2>
bool FiberMap<T1,T2,E1,E2>::isCanceled() {
    return parent->isCanceled();
}

template <class T1, class T2, class E1, class E2>
void FiberMap<T1,T2,E1,E2>::cancel() {
    parent->cancel();
}

template <class T1, class T2, class E1, class E2>
void FiberMap<T1,T2,E1,E2>::onCancel(const std::function<void()>& callback) {
    parent->onCancel(callback);
}

template <class T1, class T2, class E1, class E2>
void FiberMap<T1,T2,E1,E2>::onShutdown(const std::function<void()>& callback) {
    parent->onShutdown(callback);
}

template <class T1, class T2, class E1, class E2>
void FiberMap<T1,T2,E1,E2>::onFiberShutdown(const std::function<void(Fiber<T2,E2>*)>& callback) {
    parent->onFiberShutdown([self_weak = this->weak_from_this(), callback](auto) {
        if(auto self = self_weak.lock()) {
            callback(self.get());
        }
    });
}

template <class T1, class T2, class E1, class E2>
T2 FiberMap<T1,T2,E1,E2>::await() {
    try {
        return value_transform(parent->await());
    } catch(E1& error) {
        throw error_transform(error);
    }
}

} // namespace cask::fiber

#endif