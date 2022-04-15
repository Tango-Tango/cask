#ifndef _CASK_FIXED_SIZE_POOL_H_
#define _CASK_FIXED_SIZE_POOL_H_

#include <atomic>
#include <cassert>
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
    static constexpr std::size_t TotalBlockSize = BlockSize + sizeof(void*) + sizeof(bool) + sizeof(uint32_t);
    static constexpr std::size_t PadSize = (Alignment - (TotalBlockSize % Alignment)) % Alignment;

    explicit BlockPool();
    ~BlockPool();

    template <class T, class... Args>
    T* allocate(Args&&... args);

    template <class T>
    void deallocate(T* ptr);

    struct Block {
        uint8_t memory[BlockSize];
        uint32_t sentinel;
        Block* next;
        bool allocated;
        std::uint8_t padding[PadSize];
        Block();
    };

    struct Chunk {
        Block blocks[ChunkSize];
        Chunk* next;
        Chunk();
    };

private:
    void allocate_chunk();
    
    std::atomic<Block*> free_blocks;
    std::atomic<Chunk*> allocated_chunks;
    std::atomic_bool allocating_chunk;
};

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
BlockPool<BlockSize,ChunkSize,Alignment>::BlockPool()
    : free_blocks(nullptr)
    , allocated_chunks(nullptr)
    , allocating_chunk(false)
{}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
BlockPool<BlockSize,ChunkSize,Alignment>::~BlockPool() {
    auto current = allocated_chunks.load(std::memory_order_acquire);

    while(current) {
#if CASK_ASAN_ENABLED
        // un-poison the memory for asan
        for(std::size_t i = 0; i < ChunkSize; i++) {
            __asan_unpoison_memory_region(&(current->blocks[i].memory), BlockSize);
        }
#endif

        auto next = current->next;
        delete current;
        current = next;
    }
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
template <class T, class... Args>
T* BlockPool<BlockSize,ChunkSize,Alignment>::allocate(Args&&... args) {
    static_assert(sizeof(T) <= BlockSize);

    while(true) {
        Block* head = free_blocks.load(std::memory_order_relaxed);
        while(head && !free_blocks.compare_exchange_weak(head, head->next, std::memory_order_acquire, std::memory_order_relaxed));

        if(head) {
            assert(head->sentinel == 0xDEADBEEF);
#if CASK_ASAN_ENABLED
            // Un-poison the memory for asan
            if(!head->allocated) {
                __asan_unpoison_memory_region(head->memory, BlockSize);
            }
#else
            assert(!head->allocated);
#endif
            head->allocated = true;
            return new (head->memory) T(std::forward<Args>(args)...);
        } else {
            allocate_chunk();
        }
    }
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
template <class T>
void BlockPool<BlockSize,ChunkSize,Alignment>::deallocate(T* ptr) {
    static_assert(sizeof(T) <= BlockSize);
    auto block = reinterpret_cast<Block*>(ptr);
    assert(block->sentinel == 0xDEADBEEF);
    assert(block->allocated);

    std::destroy_at<T>(ptr);

#if CASK_ASAN_ENABLED
    // Poison the memory for asan
    __asan_poison_memory_region(block->memory, BlockSize);
#endif

    block->allocated = false;
    block->next = free_blocks.load(std::memory_order_relaxed);
    while(!free_blocks.compare_exchange_weak(block->next,block,std::memory_order_release,std::memory_order_relaxed));
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
void BlockPool<BlockSize,ChunkSize,Alignment>::allocate_chunk() {
    if (!allocating_chunk.exchange(true)) {
        // Allocate a chunk and add it to the allocated list
        Chunk* chunk = new Chunk();
        chunk->next = allocated_chunks.load(std::memory_order_relaxed);
        while(!allocated_chunks.compare_exchange_weak(chunk->next,chunk,std::memory_order_release,std::memory_order_relaxed));

        // Load the blocks for this chunk into the free list
        for(std::size_t i = 0; i < ChunkSize; i++) {
            Block* block = &(chunk->blocks[i]);
#if CASK_ASAN_ENABLED
            __asan_poison_memory_region(block->memory, BlockSize);
#endif
            block->allocated = false;
            block->next = free_blocks.load(std::memory_order_relaxed);
            while(!free_blocks.compare_exchange_weak(block->next,block,std::memory_order_release,std::memory_order_relaxed));
        }

        allocating_chunk.store(false);
    }
}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
BlockPool<BlockSize,ChunkSize,Alignment>::Block::Block()
    : memory()
    , sentinel(0xDEADBEEF)
    , next(nullptr)
    , allocated(false)
    , padding()
{}

template <std::size_t BlockSize, std::size_t ChunkSize, std::size_t Alignment>
BlockPool<BlockSize,ChunkSize,Alignment>::Chunk::Chunk()
    : blocks()
    , next(nullptr)
{}

} // namespace cask

#endif