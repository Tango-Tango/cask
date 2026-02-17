#include <benchmark/benchmark.h>
#include <thread>
#include <vector>
#include "cask/Pool.hpp"

using cask::Pool;

struct SmallObj { char data[32]; };
struct MediumObj { char data[128]; };
struct LargeObj { char data[256]; };
struct XLargeObj { char data[512]; };
struct XXLargeObj { char data[1024]; };
struct XXXLargeObj { char data[2048]; };
struct XXXXLargeObj { char data[4096]; };

// Steady-state single alloc/dealloc cycle on a pre-warmed pool.
// Parameterized by object size to cover each pool tier.
template <typename T>
static void BM_Pool_AllocDealloc(benchmark::State& state) {
    Pool pool;
    // Pre-warm
    T* warm = pool.allocate<T>();
    pool.deallocate<T>(warm);

    for (auto _ : state) {
        T* ptr = pool.allocate<T>();
        benchmark::DoNotOptimize(ptr);
        pool.deallocate<T>(ptr);
    }
}
BENCHMARK(BM_Pool_AllocDealloc<SmallObj>);
BENCHMARK(BM_Pool_AllocDealloc<MediumObj>);
BENCHMARK(BM_Pool_AllocDealloc<LargeObj>);
BENCHMARK(BM_Pool_AllocDealloc<XLargeObj>);
BENCHMARK(BM_Pool_AllocDealloc<XXLargeObj>);
BENCHMARK(BM_Pool_AllocDealloc<XXXLargeObj>);
BENCHMARK(BM_Pool_AllocDealloc<XXXXLargeObj>);

// Allocate N objects without freeing, then free all.
// Stresses chunk allocation when free list empties.
static void BM_Pool_BurstAlloc(benchmark::State& state) {
    auto N = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        Pool pool;
        std::vector<SmallObj*> ptrs;
        ptrs.reserve(N);

        for (std::size_t i = 0; i < N; i++) {
            ptrs.push_back(pool.allocate<SmallObj>());
        }
        benchmark::DoNotOptimize(ptrs.data());

        for (auto* ptr : ptrs) {
            pool.deallocate<SmallObj>(ptr);
        }
    }
}
BENCHMARK(BM_Pool_BurstAlloc)->Range(64, 8192);

// First allocation from a fresh Pool, measuring chunk allocation cost directly.
static void BM_Pool_ColdStart(benchmark::State& state) {
    for (auto _ : state) {
        Pool pool;
        SmallObj* ptr = pool.allocate<SmallObj>();
        benchmark::DoNotOptimize(ptr);
        pool.deallocate<SmallObj>(ptr);
    }
}
BENCHMARK(BM_Pool_ColdStart);

// Multi-threaded alloc/dealloc on a shared pool.
static void BM_Pool_Contended(benchmark::State& state) {

    // This pool is static because it is intended to be used across all benchmark
    // threads. This is an intentional part of the benchmark.
    static Pool pool;

    // Pre-warm
    if (state.thread_index() == 0) {
        SmallObj* warm = pool.allocate<SmallObj>();
        pool.deallocate<SmallObj>(warm);
    }

    for (auto _ : state) {
        SmallObj* ptr = pool.allocate<SmallObj>();
        benchmark::DoNotOptimize(ptr);
        pool.deallocate<SmallObj>(ptr);
    }
}
BENCHMARK(BM_Pool_Contended)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// new/delete baseline for comparison.
static void BM_Pool_NewDelete_Baseline(benchmark::State& state) {
    for (auto _ : state) {
        auto* ptr = new SmallObj();
        benchmark::DoNotOptimize(ptr);
        delete ptr;
    }
}
BENCHMARK(BM_Pool_NewDelete_Baseline);
