#include "cask/pool/InternalPool.hpp"

std::shared_ptr<cask::Pool> cask::pool::global_pool() {
    thread_local static std::shared_ptr<cask::Pool> pool = std::make_shared<cask::Pool>();
    return pool;
}
