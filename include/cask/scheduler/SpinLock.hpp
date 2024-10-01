//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SCHEDULER_SPIN_LOCK_H_
#define _CASK_SCHEDULER_SPIN_LOCK_H_

#include <atomic>

namespace cask::scheduler {

class SpinLockGuard;

/**
 * In some (very rare) cases we would prefer to use a spinlock instead of a mutex. This is
 * typically true deep in scheduler code where, for latency and timer accuracy reasons, we'd
 * prefer to spin on a lock rather than sleep.
 * 
 * You shouldn't use this lock unless you're sure you need it. It's not as fair as a mutex
 * and can cause a lot of contention if used inappropriately.
 * 
 * This lock implementation exposes no methods directly to manipulate it. Instead it's expected
 * that the SpinLockGuard is used to lock and unlock the lock in a way that ensures safety.
 */
class SpinLock {
public:
    SpinLock() = default;
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) volatile = delete;
private:
    friend SpinLockGuard;
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

class SpinLockGuard {
public:
    explicit SpinLockGuard(SpinLock& lock); // NOLINT(google-runtime-references)
    ~SpinLockGuard();
private:
    SpinLock& lock;
};

} // namespace cask::scheduler

#endif