#include "cask/pool/InternalPool.hpp"

std::shared_ptr<cask::Pool> cask::pool::global_pool() {
    static thread_local std::weak_ptr<cask::Pool> pool_weak;

    if (auto pool = pool_weak.lock()) {
        return pool;
    } else {
        pool = std::make_shared<cask::Pool>();
        pool_weak = pool;
        return pool;
    }
}
