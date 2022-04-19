#ifndef _CASK_POOL_H_
#define _CASK_POOL_H_

#include <cstdint>
#include <cstddef>
#include <new>
#include "pool/BlockPool.hpp"

namespace cask {

class Pool {
public:
    template <class T, class... Args>
    T* allocate(Args&&... args);

    template <class T>
    void deallocate(T* ptr);

private:
    #ifdef __cpp_lib_hardware_interference_size
        using std::hardware_destructive_interference_size; 
    #elif __mips__
        static constexpr std::size_t hardware_destructive_interference_size = 32;
    #else
        static constexpr std::size_t hardware_destructive_interference_size = 64;
    #endif

    static constexpr std::size_t smallest_block_num_entries = 2048;
    static constexpr std::size_t smallest_block_size = hardware_destructive_interference_size * smallest_block_num_entries;

    pool::BlockPool<hardware_destructive_interference_size, smallest_block_size, alignof(std::max_align_t)> small_pool;
    pool::BlockPool<hardware_destructive_interference_size*2, smallest_block_size / 2, alignof(std::max_align_t)> medium_pool;
    pool::BlockPool<hardware_destructive_interference_size*4, smallest_block_size / 4, alignof(std::max_align_t)> large_pool;
    pool::BlockPool<hardware_destructive_interference_size*8, smallest_block_size / 8, alignof(std::max_align_t)> xlarge_pool;
    pool::BlockPool<hardware_destructive_interference_size*16, smallest_block_size / 16, alignof(std::max_align_t)> xxlarge_pool;
    pool::BlockPool<hardware_destructive_interference_size*32, smallest_block_size / 32, alignof(std::max_align_t)> xxxlarge_pool;
    pool::BlockPool<hardware_destructive_interference_size*64, smallest_block_size / 64, alignof(std::max_align_t)> xxxxlarge_pool;
};

template <class T, class... Args>
T* Pool::allocate(Args&&... args) {
    if constexpr (sizeof(T) <= hardware_destructive_interference_size) {
        return small_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 2) {
        return medium_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 4) {
        return large_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 8) {
        return xlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 16) {
        return xxlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 32) {
        return xxxlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 64)  {
        return xxxxlarge_pool.template allocate<T,Args...>(std::forward<Args>(args)...);
    } else {
        return new T(std::forward<Args>(args)...);
    }
}

template <class T>
void Pool::deallocate(T* ptr) {
    if constexpr (sizeof(T) <= hardware_destructive_interference_size) {
        return small_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 2) {
        return medium_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 4) {
        return large_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 8) {
        return xlarge_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 16) {
        return xxlarge_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 32) {
        return xxxlarge_pool.template deallocate<T>(ptr);
    } else if constexpr (sizeof(T) <= hardware_destructive_interference_size * 64)  {
        return xxxxlarge_pool.template deallocate<T>(ptr);
    } else {
        delete ptr;
    }
}

} // namespace cask

#endif