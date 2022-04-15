#ifndef _CASK_FIXED_SIZE_POOL_H_
#define _CASK_FIXED_SIZE_POOL_H_

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <iostream>

#if defined(__SANITIZE_ADDRESS__)
#define CASK_ASAN_ENABLED 1
#include "sanitizer/asan_interface.h"
#elif defined(__has_feature)
#if __has_feature(__SANITIZE_ADDRESS__)
#define CASK_ASAN_ENABLED 1
#include "sanitizer/asan_interface.h"
#endif
#endif

namespace cask::pool {

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
class BlockPool {
public:
    static constexpr std::size_t TotalBlockSize = BlockSize + sizeof(std::atomic<void*>);
    static constexpr std::size_t PadSize = (Alignment - (TotalBlockSize % Alignment)) % Alignment;

    explicit BlockPool();
    ~BlockPool();

    template <class T, class... Args>
    T* allocate(Args&&... args);

    template <class T>
    void deallocate(T* ptr);

    struct Block {
        uint8_t memory[BlockSize];
        std::atomic<Block*> next;
        std::uint8_t padding[PadSize];
        Block();
    };

    struct Chunk {
        Block blocks[ChunkSize];
        std::atomic<Chunk*> next;
        Chunk();
    };

private:
    void allocate_chunk();
    
    std::atomic<Block*> free_blocks;
    std::atomic<Chunk*> allocated_chunks;
    std::atomic_bool allocating_chunk;
};

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
BlockPool<BlockSize,ChunkSize,Alignment>::Block::Block() : memory(), next(nullptr), padding() {
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
BlockPool<BlockSize,ChunkSize,Alignment>::Chunk::Chunk() : blocks(), next(nullptr) {
    // Initially link all of the chunks together
    for(std::size_t i = 0; i < ChunkSize - 1; i++) {
        auto& block = blocks[i];
        auto& next_block = blocks[i+1];
        block.next.store(&next_block);
    }
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
BlockPool<BlockSize,ChunkSize,Alignment>::BlockPool() : free_blocks(nullptr), allocated_chunks(nullptr), allocating_chunk(false) {
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
BlockPool<BlockSize,ChunkSize,Alignment>::~BlockPool() {
    auto current = allocated_chunks.load();

    while(current) {
#if CASK_ASAN_ENABLED
        // un-poison the memory for asan
        for(std::size_t i = 0; i < ChunkSize; i++) {
            __asan_unpoison_memory_region(&(current->blocks[i].memory), BlockSize);
            __asan_unpoison_memory_region(&(current->blocks[i].padding), BlockSize);
        }
#endif

        auto next = current->next.load();

        delete current;
        current = next;
    }
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
template <class T, class... Args>
T* BlockPool<BlockSize,ChunkSize,Alignment>::allocate(Args&&... args) {
    static_assert(sizeof(T) <= BlockSize);

    while (true) {
        auto head = free_blocks.load();

        if(head) {
            auto next = head->next.load();

            if (free_blocks.compare_exchange_weak(head, next)) {
#if CASK_ASAN_ENABLED
                // Un-poison the memory for asan
                __asan_unpoison_memory_region(head->memory, BlockSize);
#endif
                return new (head->memory) T(std::forward<Args>(args)...);
            }
        } else {
            allocate_chunk();
        }
    }
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
template <class T>
void BlockPool<BlockSize,ChunkSize,Alignment>::deallocate(T* ptr) {
    static_assert(sizeof(T) <= BlockSize);

    std::destroy_at<T>(ptr);

    auto block = reinterpret_cast<Block*>(ptr);

#if CASK_ASAN_ENABLED
    // Poison the memory for asan
    __asan_poison_memory_region(block->memory, BlockSize);
#endif

    while (true) {
        auto head = free_blocks.load();
        block->next.store(head);

        if(free_blocks.compare_exchange_weak(head, block)) {
            break;
        }
    }
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
void BlockPool<BlockSize,ChunkSize,Alignment>::allocate_chunk() {
    if (!allocating_chunk.exchange(true)) {
        // Allocate a chunk and add it to the allocated list
        auto chunk = new Chunk();

        while(true) {
            auto allocated_chunks_head = allocated_chunks.load();
            chunk->next.store(allocated_chunks_head);

            if(allocated_chunks.compare_exchange_weak(allocated_chunks_head, chunk)) {
                break;
            }
        }

#if CASK_ASAN_ENABLED
        // Poison the memory for asan
        for(std::size_t i = 0; i < ChunkSize; i++) {
            __asan_poison_memory_region(&(chunk->blocks[i].memory), BlockSize);
            __asan_poison_memory_region(&(chunk->blocks[i].padding), BlockSize);
        }
#endif

        // Load the whole chunk at once into the free list
        auto chunk_head = &(chunk->blocks[0]);
        auto chunk_tail = &(chunk->blocks[ChunkSize - 1]);

        while (true) {
            auto free_list_head = free_blocks.load();
            chunk_tail->next.store(free_list_head);

            if(free_blocks.compare_exchange_weak(free_list_head, chunk_head)) {
                break;
            }
        }

        allocating_chunk.store(false);
    } else {
        // An allocation is already happening. Wait for it rather than running
        // a second one.
        while(allocating_chunk.load());
    }
}

} // namespace cask

#endif