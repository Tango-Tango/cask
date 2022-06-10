#ifndef _CASK_INTERNAL_POOL_H_
#define _CASK_INTERNAL_POOL_H_

#include "../Pool.hpp"
#include <memory>

namespace cask::pool {

std::shared_ptr<Pool> global_pool();

} // namespace cask::pool
 
#endif