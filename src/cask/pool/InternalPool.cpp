#include "cask/pool/InternalPool.hpp"

std::shared_ptr<cask::Pool> cask::pool::global_pool() {
    static std::atomic_flag initializing_pool = ATOMIC_FLAG_INIT;
    static std::weak_ptr<cask::Pool> pool_weak;

    if (auto pool = pool_weak.lock()) {
        return pool;
    } else {
        while(initializing_pool.test_and_set(std::memory_order_acquire)) {
            #if defined(__cpp_lib_atomic_flag_test)
            while (initializing_pool.test(std::memory_order_relaxed))
            #endif
            ;
        }

        if (auto pool = pool_weak.lock()) {
            initializing_pool.clear(std::memory_order_release);
            return pool;
        } else {
            pool = std::make_shared<cask::Pool>();
            pool_weak = pool;
            initializing_pool.clear(std::memory_order_release);
            return pool;
        }
    }
}
