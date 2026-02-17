#ifndef _CASK_FIXED_SIZE_POOL_H_
#define _CASK_FIXED_SIZE_POOL_H_

#include <atomic>
#include <cstdint>
#include <type_traits>

#if defined(__SANITIZE_ADDRESS__)
#define CASK_ASAN_ENABLED 1
#include "sanitizer/asan_interface.h"
#elif defined(__has_feature)
#if __has_feature(__SANITIZE_ADDRESS__)
#define CASK_ASAN_ENABLED 1
#include "sanitizer/asan_interface.h"
#endif
#endif

#if defined(CASK_ASAN_ENABLED)
#define POISON_BLOCK(__block) __asan_poison_memory_region(__block->memory, BlockSize)
#define UNPOISON_BLOCK(__block) __asan_unpoison_memory_region(__block->memory, BlockSize)
#else
#define POISON_BLOCK(__block) do {} while(0)
#define UNPOISON_BLOCK(__block) do {} while(0)
#endif

namespace cask::pool {

template <std::size_t BlockSize, std::size_t NumBlocksInChunk, std::size_t Alignment>
class BlockPool {
public:
    explicit BlockPool();
    ~BlockPool();

    template <class T, class... Args>
    T* allocate(Args&&... args);

    template <class T>
    void deallocate(T* ptr);

private:
    static constexpr std::size_t TotalBlockSize = BlockSize + sizeof(void*);
    static constexpr std::size_t PadSize = (Alignment - (TotalBlockSize % Alignment)) % Alignment;

    struct Chunk;

    // A block which as at least one byte of padding.
    struct BlockWithPad {
        BlockWithPad() : next(nullptr) {}

        uint8_t memory[BlockSize];
        BlockWithPad* next;
        std::uint8_t padding[PadSize == 0 ? 1 : PadSize];
    };

    // A block with no padding.
    struct BlockWithoutPad {
        BlockWithoutPad() : next(nullptr) {}

        uint8_t memory[BlockSize];
        BlockWithoutPad* next;
    };

    // Choose the block type. Most of the time at least some padding is required, but
    // occasionally no padding is required and we must account for that to avoid
    // compile errors about 0-sized arrays.
    typedef typename std::conditional<PadSize == 0, BlockWithoutPad, BlockWithPad>::type Block;

    static_assert(sizeof(Block) == TotalBlockSize + PadSize);
    static_assert(offsetof(Block, memory) == 0);

    struct Chunk {
        Block blocks[NumBlocksInChunk];
        Chunk* next;
        Chunk();
    };

    template <class T>
    struct Head {
        T* ptr;
        unsigned int counter;
        Head();
    };

    void allocate_chunk();
    
    std::atomic<Head<Block>> free_blocks;
    std::atomic<Head<Chunk>> allocated_chunks;
    std::atomic_bool allocating_chunk;
};

template <std::size_t BlockSize, std::size_t NumBlocksInChunk, std::size_t Alignment>
template <class T>
BlockPool<BlockSize,NumBlocksInChunk,Alignment>::Head<T>::Head()
    : ptr(nullptr)
    , counter(0)
{}

template <std::size_t BlockSize, std::size_t NumBlocksInChunk, std::size_t Alignment>
BlockPool<BlockSize,NumBlocksInChunk,Alignment>::BlockPool()
    : free_blocks(Head<Block>())
    , allocated_chunks(Head<Chunk>())
    , allocating_chunk(false)
{}

template <std::size_t BlockSize, std::size_t NumBlocksInChunk, std::size_t Alignment>
BlockPool<BlockSize,NumBlocksInChunk,Alignment>::~BlockPool() {
    auto current_head = allocated_chunks.load(std::memory_order_acquire);
    auto current = current_head.ptr;

    while(current) {

#if CASK_ASAN_ENABLED
        for(std::size_t i = 0; i < NumBlocksInChunk; i++) {
            auto block = &(current->blocks[i]);
            UNPOISON_BLOCK(block);
        }
#endif

        auto next = current->next;
        delete current;
        current = next;
    }
}

template <std::size_t BlockSize, std::size_t NumBlocksInChunk, std::size_t Alignment>
template <class T, class... Args>
T* BlockPool<BlockSize,NumBlocksInChunk,Alignment>::allocate(Args&&... args) {
    static_assert(sizeof(T) <= BlockSize);

    while(true) {
        Head<Block> old_head = free_blocks.load(std::memory_order_relaxed);

        if(old_head.ptr) {
            Head<Block> new_head;
            new_head.ptr = old_head.ptr->next;
            new_head.counter = old_head.counter + 1;
            if(free_blocks.compare_exchange_weak(old_head, new_head, std::memory_order_acquire, std::memory_order_relaxed)) {
                UNPOISON_BLOCK(old_head.ptr);
                old_head.ptr->next = nullptr;
                return new (old_head.ptr->memory) T(std::forward<Args>(args)...);
            }

        } else {
            allocate_chunk();
        }
    }
}

template <std::size_t BlockSize, std::size_t NumBlocksInChunk, std::size_t Alignment>
template <class T>
void BlockPool<BlockSize,NumBlocksInChunk,Alignment>::deallocate(T* ptr) {
    static_assert(sizeof(T) <= BlockSize);

    ptr->~T();
    
    Block* block = reinterpret_cast<Block*>(ptr);
    POISON_BLOCK(block);

    Head<Block> old_head = free_blocks.load(std::memory_order_relaxed);
    Head<Block> new_head;

    do {
        new_head.ptr = block;
        new_head.ptr->next = old_head.ptr;
        new_head.counter = old_head.counter + 1;
    } while(!free_blocks.compare_exchange_weak(old_head,new_head, std::memory_order_release, std::memory_order_relaxed));
}

template <std::size_t BlockSize, std::size_t NumBlocksInChunk, std::size_t Alignment>
void BlockPool<BlockSize,NumBlocksInChunk,Alignment>::allocate_chunk() {
    if (!allocating_chunk.exchange(true, std::memory_order_relaxed)) {
        // Allocate a chunk and add it to the allocated list
        Chunk* chunk = new Chunk();

        {
            Head<Chunk> old_head = allocated_chunks.load(std::memory_order_relaxed);
            Head<Chunk> new_head;

            do {
                new_head.ptr = chunk;
                new_head.ptr->next = old_head.ptr;
                new_head.counter = old_head.counter + 1;
            } while(!allocated_chunks.compare_exchange_weak(old_head,new_head,std::memory_order_release,std::memory_order_relaxed));
        }

        // String the chunk into a list of blocks that can be pushed onto
        // the free list
        for (std::size_t i = 0; i < NumBlocksInChunk; i++) {
            Block* block = &(chunk->blocks[i]);
            block->next = (i < NumBlocksInChunk - 1) ? &(chunk->blocks[i + 1]) : nullptr;
            POISON_BLOCK(block);
        }

        // Push the entire chunk onto the free list with a single CAS
        Head<Block> old_head = free_blocks.load(std::memory_order_relaxed);
        Head<Block> new_head;
        do {
            chunk->blocks[NumBlocksInChunk - 1].next = old_head.ptr;
            new_head.ptr = &(chunk->blocks[0]);
            new_head.counter = old_head.counter + 1;
        } while (!free_blocks.compare_exchange_weak(old_head, new_head,
            std::memory_order_release, std::memory_order_relaxed));

        allocating_chunk.store(false, std::memory_order_relaxed);
    }
}

template <std::size_t BlockSize, std::size_t NumBlocksInChunk, std::size_t Alignment>
BlockPool<BlockSize,NumBlocksInChunk,Alignment>::Chunk::Chunk()
    : blocks()
    , next(nullptr)
{}

} // namespace cask::pool

#endif
