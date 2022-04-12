#ifndef _CASK_POOL_H_
#define _CASK_POOL_H_

#include <atomic>
#include <cstdint>
#include <stack>
#include <unordered_map>

namespace cask {

template <std::size_t BlockSize>
class Pool {
public:
    explicit Pool(std::size_t initial_size = 0);
    ~Pool();

    template <class T, class... Args>
    T* allocate(Args&&... args);

    template <class T>
    void deallocate(T* ptr);

private:
    struct Block {
        uint8_t memory[BlockSize];
        std::atomic<Block*> next;
    };

    std::atomic<Block*> free_blocks;
};

template <std::size_t BlockSize>
Pool<BlockSize>::Pool(std::size_t initial_size) : free_blocks(nullptr) {
    while(initial_size-- > 0) {
        auto block = new Block();
        block->next = free_blocks.load();
        free_blocks.store(block);
    }
}

template <std::size_t BlockSize>
Pool<BlockSize>::~Pool() {
    auto current = free_blocks.load();

    while(current) {
        auto old = current->next.load();
        delete current;
        current = old;
    }
}

template <std::size_t BlockSize>
template <class T, class... Args>
T* Pool<BlockSize>::allocate(Args&&... args) {
    if constexpr (sizeof(T) > BlockSize) {
        return new T(std::forward<Args>(args)...);
    } else {
        while (true) {
            auto head = free_blocks.load(std::memory_order_relaxed);
            if (head && free_blocks.compare_exchange_strong(head, head->next, std::memory_order_relaxed, std::memory_order_relaxed)) {
                return new (head->memory) T(std::forward<Args>(args)...);   
            } else if(!head) {
                auto block = new Block();
                return new (block->memory) T(std::forward<Args>(args)...);   
            }
        }
    }
}

template <std::size_t BlockSize>
template <class T>
void Pool<BlockSize>::deallocate(T* ptr) {
    if constexpr (sizeof(T) > BlockSize) {
        delete ptr;
    } else {
        std::destroy_at<T>(ptr);

        auto block = reinterpret_cast<Block*>(ptr);

        while (true) {
            auto head = free_blocks.load(std::memory_order_relaxed);
            block->next.store(head, std::memory_order_relaxed);

            if(free_blocks.compare_exchange_strong(head, block, std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
    }
}

} // namespace cask

#endif