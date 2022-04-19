#include "cask/pool/InternalPool.hpp"



cask::Pool& cask::pool::global_pool() {
    static cask::Pool pool;
    return pool;
}
