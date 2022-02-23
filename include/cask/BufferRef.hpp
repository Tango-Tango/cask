#ifndef _CASK_BUFFER_REF_H_
#define _CASK_BUFFER_REF_H_

#include <memory>
#include <vector>

namespace cask {

template <class T>
using BufferRef = std::shared_ptr<std::vector<T>>;

} // namespace cask

#endif