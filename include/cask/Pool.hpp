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
    static constexpr std::size_t num_blocks = config::initial_blocks_per_pool;

    pool::BlockPool<config::cache_line_size,      num_blocks,      alignof(std::max_align_t)> small_pool;
    pool::BlockPool<config::cache_line_size*2UL,  num_blocks/2,    alignof(std::max_align_t)> medium_pool;
    pool::BlockPool<config::cache_line_size*4UL,  num_blocks/4,    alignof(std::max_align_t)> large_pool;
    pool::BlockPool<config::cache_line_size*8UL,  num_blocks/8,    alignof(std::max_align_t)> xlarge_pool;
    pool::BlockPool<config::cache_line_size*16UL, num_blocks/16,   alignof(std::max_align_t)> xxlarge_pool;
    pool::BlockPool<config::cache_line_size*32UL, num_blocks/32,   alignof(std::max_align_t)> xxxlarge_pool;
    pool::BlockPool<config::cache_line_size*64UL, num_blocks/64,   alignof(std::max_align_t)> xxxxlarge_pool;
};

template <class T, class... Args>
T* Pool::allocate(Args&&... args) {
    if constexpr (sizeof(T) <= config::cache_line_size) {
        return small_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 2UL) {
        return medium_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 4UL) {
        return large_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 8UL) {
        return xlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 16UL) {
        return xxlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 32UL) {
        return xxxlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 64UL)  {
        return xxxxlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else {
        return new T(std::forward<Args>(args)...);
    }
}

template <class T>
void Pool::deallocate(T* ptr) {
    if constexpr (sizeof(T) <= config::cache_line_size) {
        return small_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 2UL) {
        return medium_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 4UL) {
        return large_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 8UL) {
        return xlarge_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 16UL) {
        return xxlarge_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 32UL) {
        return xxxlarge_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= config::cache_line_size * 64UL)  {
        return xxxxlarge_pool.template deallocate<T>(ptr);
    } else {
        delete ptr;
    }
}

} // namespace cask

#endif