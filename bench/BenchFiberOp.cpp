//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

// Benchmarks targeting the cost of creating ref-counted FiberOp handles.
// Each FiberOp factory pool-allocates the op and returns a FiberOpRef -
// an intrusive refcounted pointer. The handle must not defeat the pool
// with extra heap allocations since this is one of the hottest
// allocation paths in the library.

#include <benchmark/benchmark.h>
#include <vector>
#include "cask/Erased.hpp"
#include "cask/fiber/FiberOp.hpp"
#include "cask/fiber/FiberValue.hpp"
#include "cask/pool/InternalPool.hpp"

using cask::Erased;
using cask::fiber::FiberOp;
using cask::fiber::FiberValue;

// Single value op create + destroy - the most common operation
// (every Task::pure / map / flatMap result goes through this).
static void BM_FiberOp_CreateValue(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        auto op = FiberOp::value(Erased(42));
        benchmark::DoNotOptimize(op);
    }
    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_FiberOp_CreateValue)->ThreadRange(1, 8);

// Error op create + destroy.
static void BM_FiberOp_CreateError(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        auto op = FiberOp::error(Erased(42));
        benchmark::DoNotOptimize(op);
    }
    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_FiberOp_CreateError);

// Thunk op create + destroy.
static void BM_FiberOp_CreateThunk(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        auto op = FiberOp::thunk([]() { return Erased(42); });
        benchmark::DoNotOptimize(op);
    }
    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_FiberOp_CreateThunk);

// Valueless op (cede) create + destroy - isolates the handle and op
// allocation cost since there is no payload allocation at all.
static void BM_FiberOp_CreateCede(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        auto op = FiberOp::cede();
        benchmark::DoNotOptimize(op);
    }
    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_FiberOp_CreateCede);

// Build a flatMap chain - each link creates a VALUE op plus a FLATMAP op,
// mirroring what Task composition does internally.
static void BM_FiberOp_FlatMapChainBuild(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        auto op = FiberOp::value(Erased(0));
        for (int i = 0; i < chain_length; ++i) {
            op = op->flatMap([](FiberValue&& value) {
                return FiberOp::value(std::move(value).getValue().value_or(Erased(0)));
            });
        }
        benchmark::DoNotOptimize(op);
    }
    state.SetItemsProcessed(state.iterations() * (chain_length + 1));
    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_FiberOp_FlatMapChainBuild)->Range(1, 256);

// Copying a FiberOpRef (intrusive refcount churn) - measures the atomic
// increment/decrement cost in the trampoline, which hands these handles
// around constantly.
static void BM_FiberOp_RefCopy(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    auto op = FiberOp::value(Erased(42));
    for (auto _ : state) {
        auto copy = op;
        benchmark::DoNotOptimize(copy);
    }
    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_FiberOp_RefCopy);

// Batch create-then-destroy: stresses both alloc and dealloc paths with
// many ops alive at once, closer to a real fiber's working set.
static void BM_FiberOp_BatchChurn(benchmark::State& state) {
    const int batch = static_cast<int>(state.range(0));
    auto pool = cask::pool::global_pool();
    std::vector<cask::fiber::FiberOpRef> ops;
    ops.reserve(batch);
    for (auto _ : state) {
        for (int i = 0; i < batch; ++i) {
            ops.push_back(FiberOp::value(Erased(i)));
        }
        ops.clear();
    }
    state.SetItemsProcessed(state.iterations() * batch);
    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_FiberOp_BatchChurn)->Range(8, 1024);
