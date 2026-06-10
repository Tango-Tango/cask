#include "cask/pool/InternalPool.hpp"

std::shared_ptr<cask::Pool> cask::pool::global_pool() {
    static std::atomic_flag initializing_pool = ATOMIC_FLAG_INIT;
    static std::weak_ptr<cask::Pool> pool_weak;

    // Fast path: a thread-local cache of the global weak pointer. Only this
    // thread touches local_pool_weak so calling lock() on it cannot race. It
    // also doesn't extend the pool's lifetime - when the pool is destroyed
    // this cache expires and we fall through to the slow path below.
    thread_local std::weak_ptr<cask::Pool> local_pool_weak;

    if (auto pool = local_pool_weak.lock()) {
        return pool;
    }

    // Slow path: the spinlock must be held for *all* access to pool_weak.
    // Calling weak_ptr::lock() concurrently with an assignment to the same
    // weak_ptr is a data race - so an unguarded read of it is not safe.
    while(initializing_pool.test_and_set(std::memory_order_acquire)) {
        #if defined(__cpp_lib_atomic_flag_test)
        while (initializing_pool.test(std::memory_order_relaxed))
        #endif
        ;
    }

    // RAII guard ensures the spinlock is released on every exit path,
    // including if Pool construction throws below.
    struct SpinlockGuard {
        std::atomic_flag& flag;
        ~SpinlockGuard() { flag.clear(std::memory_order_release); }
    } guard{initializing_pool};

    auto pool = pool_weak.lock();
    if (!pool) {
        pool = std::make_shared<cask::Pool>();
        pool_weak = pool;
    }

    local_pool_weak = pool;
    return pool;
}
