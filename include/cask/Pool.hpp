#ifndef _CASK_POOL_H_
#define _CASK_POOL_H_

#include <cstdint>
#include <cstddef>
#include <new>
#include "cask/Config.hpp"
#include "pool/BlockPool.hpp"

namespace cask {

class Pool {
public:
    template <class T, class... Args>
    T* allocate(Args&&... args);

    template <class T>
    void deallocate(T* ptr);

private:
    static constexpr std::size_t smallest_block_num_entries = initial_blocks_per_pool;
    static constexpr std::size_t smallest_block_size = cache_line_size * smallest_block_num_entries;

    pool::BlockPool<cache_line_size, smallest_block_size, alignof(std::max_align_t)> small_pool;
    pool::BlockPool<cache_line_size*2UL, smallest_block_size / 2, alignof(std::max_align_t)> medium_pool;
    pool::BlockPool<cache_line_size*4UL, smallest_block_size / 4, alignof(std::max_align_t)> large_pool;
    pool::BlockPool<cache_line_size*8UL, smallest_block_size / 8, alignof(std::max_align_t)> xlarge_pool;
    pool::BlockPool<cache_line_size*16UL, smallest_block_size / 16, alignof(std::max_align_t)> xxlarge_pool;
    pool::BlockPool<cache_line_size*32UL, smallest_block_size / 32, alignof(std::max_align_t)> xxxlarge_pool;
    pool::BlockPool<cache_line_size*64UL, smallest_block_size / 64, alignof(std::max_align_t)> xxxxlarge_pool;
};

template <class T, class... Args>
T* Pool::allocate(Args&&... args) {
    if constexpr (sizeof(T) <= cache_line_size) {
        return small_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= cache_line_size * 2UL) {
        return medium_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= cache_line_size * 4UL) {
        return large_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= cache_line_size * 8UL) {
        return xlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= cache_line_size * 16UL) {
        return xxlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= cache_line_size * 32UL) {
        return xxxlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= cache_line_size * 64UL)  {
        return xxxxlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else {
        return new T(std::forward<Args>(args)...);
    }
}

template <class T>
void Pool::deallocate(T* ptr) {
    if constexpr (sizeof(T) <= cache_line_size) {
        return small_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= cache_line_size * 2UL) {
        return medium_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= cache_line_size * 4UL) {
        return large_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= cache_line_size * 8UL) {
        return xlarge_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= cache_line_size * 16UL) {
        return xxlarge_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= cache_line_size * 32UL) {
        return xxxlarge_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= cache_line_size * 64UL)  {
        return xxxxlarge_pool.template deallocate<T>(ptr);
    } else {
        delete ptr;
    }
}

} // namespace cask

#endif