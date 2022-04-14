#include "cask/pool/InternalPool.hpp"

cask::Pool pool;

cask::Pool& cask::pool::global_pool() {
    return ::pool;
}
