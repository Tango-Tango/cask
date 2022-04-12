#ifndef _CASK_POOL_H_
#define _CASK_POOL_H_

#include <atomic>
#include <cstdint>
#include <stack>
#include <map>

namespace cask {

class Pool {
public:
    Pool();
    ~Pool();

    template <class T, class... Args>
    T* allocate(Args&&... args);

    template <class T>
    void deallocate(T* ptr);

private:
    const std::uint32_t block_size = 1024;
    const std::uint32_t num_blocks = 1024;
    std::uint8_t** memory_pool;
    std::map<void*,std::size_t> allocated_blocks;
    std::stack<std::size_t> free_list;
    std::atomic_flag memory_pool_lock = ATOMIC_FLAG_INIT;
};

template <class T, class... Args>
T* Pool::allocate(Args&&... args) {
    while(memory_pool_lock.test_and_set(std::memory_order_acquire));

    if(free_list.empty() || sizeof(T) > block_size) {
        memory_pool_lock.clear(std::memory_order_release);
        return new T(std::forward<Args>(args)...);
    } else {
        auto block = free_list.top();
        free_list.pop();
        allocated_blocks.emplace(static_cast<void*>(memory_pool[block]), block);
        memory_pool_lock.clear(std::memory_order_release);
        return new (memory_pool[block]) T(std::forward<Args>(args)...);   
    }
}

template <class T>
void Pool::deallocate(T* ptr) {
    bool is_from_pool = false;

    while(memory_pool_lock.test_and_set(std::memory_order_acquire));

    auto search = allocated_blocks.find(ptr);
    if (search != allocated_blocks.end()) {
        const auto& block = search->second;
        free_list.push(block);
        allocated_blocks.erase(search);
        is_from_pool = true;
    }

    memory_pool_lock.clear(std::memory_order_release);

    if(is_from_pool) {
        std::destroy_at<T>(ptr);
    } else {
        delete ptr;
    }
}

}

#endif