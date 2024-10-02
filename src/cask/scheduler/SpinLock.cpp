//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/SpinLock.hpp"

namespace cask::scheduler {

SpinLockGuard::SpinLockGuard(SpinLock& lock)
    : lock(lock)
{
    while(lock.flag.test_and_set(std::memory_order_acquire));
}

SpinLockGuard::~SpinLockGuard()
{
    lock.flag.clear(std::memory_order_release);
}

} // namespace cask::scheduler
