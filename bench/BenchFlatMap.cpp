//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <benchmark/benchmark.h>
#include <array>
#include "cask/Task.hpp"
#include "cask/Erased.hpp"
#include "cask/fiber/FiberOp.hpp"
#include "cask/pool/InternalPool.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Task;
using cask::scheduler::BenchScheduler;

// Benchmark a chain of flatMap operations with pure values
// Pre-build the task chain outside the timing loop
static void BM_FlatMap_PureChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    // Build the task chain once
    Task<int> task = Task<int>::pure(0);
    for (int i = 0; i < chain_length; ++i) {
        task = task.template flatMap<int>([](int value) {
            return Task<int>::pure(value + 1);
        });
    }

    // Only measure execution time
    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FlatMap_PureChain)->Range(1, 1024);

// Benchmark a chain of flatMap operations with eval (thunk) values
static void BM_FlatMap_EvalChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    Task<int> task = Task<int>::eval([]() { return 0; });
    for (int i = 0; i < chain_length; ++i) {
        task = task.template flatMap<int>([](int value) {
            return Task<int>::eval([value]() { return value + 1; });
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FlatMap_EvalChain)->Range(1, 1024);

// Benchmark flatMap with string values (heap allocated)
static void BM_FlatMap_StringChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    Task<std::string> task = Task<std::string>::pure(std::string("start"));
    for (int i = 0; i < chain_length; ++i) {
        task = task.template flatMap<std::string>([](std::string value) {
            return Task<std::string>::pure(value + "x");
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FlatMap_StringChain)->Range(1, 256);

// Benchmark map operations (simpler than flatMap)
static void BM_Map_PureChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    Task<int> task = Task<int>::pure(0);
    for (int i = 0; i < chain_length; ++i) {
        task = task.template map<int>([](int value) {
            return value + 1;
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Map_PureChain)->Range(1, 1024);

// Benchmark async execution with BenchScheduler
static void BM_FlatMap_AsyncChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));
    auto sched = std::make_shared<BenchScheduler>();

    Task<int> task = Task<int>::pure(0);
    for (int i = 0; i < chain_length; ++i) {
        task = task.template flatMap<int>([](int value) {
            return Task<int>::pure(value + 1);
        });
    }

    for (auto _ : state) {
        auto fiber = task.run(sched);
        sched->run_ready_tasks();
        auto result = fiber->await();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FlatMap_AsyncChain)->Range(1, 1024);

// Benchmark deeply nested flatMap (tests reassociation)
static void BM_FlatMap_DeepNested(benchmark::State& state) {
    const int depth = static_cast<int>(state.range(0));

    // Create a deeply nested structure
    Task<int> task = Task<int>::pure(0);
    for (int i = 0; i < depth; ++i) {
        task = task.template flatMap<int>([](int value) {
            return Task<int>::pure(value + 1).template flatMap<int>([](int v) {
                return Task<int>::pure(v);
            });
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FlatMap_DeepNested)->Range(1, 256);

// Benchmark with larger value types
static void BM_FlatMap_LargeValue(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    struct LargeValue {
        std::array<int, 64> data;
        int counter;
    };

    Task<LargeValue> task = Task<LargeValue>::pure(LargeValue{{}, 0});
    for (int i = 0; i < chain_length; ++i) {
        task = task.template flatMap<LargeValue>([](LargeValue value) {
            value.counter++;
            return Task<LargeValue>::pure(value);
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FlatMap_LargeValue)->Range(1, 256);

// Benchmark task construction + execution (full cost)
static void BM_FlatMap_FullCost(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    // Keep the pool alive by holding a reference throughout the benchmark
    auto pool = cask::pool::global_pool();

    for (auto _ : state) {
        Task<int> task = Task<int>::pure(0);
        for (int i = 0; i < chain_length; ++i) {
            task = task.template flatMap<int>([](int value) {
                return Task<int>::pure(value + 1);
            });
        }
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }

    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_FlatMap_FullCost)->Range(1, 256);

// Benchmark just Task::pure construction (no flatMap)
static void BM_Task_PureConstruction(benchmark::State& state) {
    // Keep the pool alive by holding a reference throughout the benchmark
    auto pool = cask::pool::global_pool();

    for (auto _ : state) {
        auto task = Task<int>::pure(42);
        benchmark::DoNotOptimize(task);
    }

    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_Task_PureConstruction);

// Benchmark just FiberOp::value construction
static void BM_FiberOp_ValueConstruction(benchmark::State& state) {
    // Keep the pool alive by holding a reference throughout the benchmark
    auto pool = cask::pool::global_pool();

    for (auto _ : state) {
        auto erased = cask::Erased(42);
        auto op = cask::fiber::FiberOp::value(std::move(erased));
        benchmark::DoNotOptimize(op);
    }

    benchmark::DoNotOptimize(pool);
}
BENCHMARK(BM_FiberOp_ValueConstruction);

// Benchmark global_pool() call (realistic: pool stays alive)
static void BM_GlobalPool(benchmark::State& state) {
    // Keep a holder to simulate realistic usage where the pool stays alive
    auto holder = cask::pool::global_pool();
    for (auto _ : state) {
        auto pool = cask::pool::global_pool();
        benchmark::DoNotOptimize(pool);
    }
}
BENCHMARK(BM_GlobalPool);

// Benchmark pool allocation
static void BM_PoolAllocate(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        int* ptr = pool->allocate<int>(42);
        benchmark::DoNotOptimize(ptr);
        pool->deallocate(ptr);
    }
}
BENCHMARK(BM_PoolAllocate);

// Benchmark shared_ptr creation with custom deleter
static void BM_SharedPtrCustomDeleter(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        int* ptr = pool->allocate<int>(42);
        auto sp = std::shared_ptr<int>(ptr, [pool](int* p) { pool->deallocate(p); });
        benchmark::DoNotOptimize(sp);
    }
}
BENCHMARK(BM_SharedPtrCustomDeleter);

// Benchmark std::function creation with small lambda
static void BM_StdFunction_Small(benchmark::State& state) {
    for (auto _ : state) {
        std::function<int(int)> f = [](int x) { return x + 1; };
        benchmark::DoNotOptimize(f);
    }
}
BENCHMARK(BM_StdFunction_Small);

// Benchmark std::function creation with capturing lambda
static void BM_StdFunction_Capturing(benchmark::State& state) {
    int capture1 = 1, capture2 = 2, capture3 = 3, capture4 = 4;
    for (auto _ : state) {
        std::function<int(int)> f = [=](int x) { return x + capture1 + capture2 + capture3 + capture4; };
        benchmark::DoNotOptimize(f);
    }
}
BENCHMARK(BM_StdFunction_Capturing);

// Benchmark Erased construction
static void BM_Erased_Construction(benchmark::State& state) {
    for (auto _ : state) {
        cask::Erased erased(42);
        benchmark::DoNotOptimize(erased);
    }
}
BENCHMARK(BM_Erased_Construction);

// Benchmark Erased construction with pre-warmed pool
static void BM_Erased_Construction_Warmed(benchmark::State& state) {
    // Warm up the pool
    auto pool = cask::pool::global_pool();
    {
        cask::Erased warmup(42);
    }

    for (auto _ : state) {
        cask::Erased erased(42);
        benchmark::DoNotOptimize(erased);
    }
}
BENCHMARK(BM_Erased_Construction_Warmed);

// Benchmark just the pool allocation part of Erased
static void BM_Erased_PoolOnly(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        int* ptr = pool->allocate<int>(42);
        benchmark::DoNotOptimize(ptr);
        pool->deallocate(ptr);
    }
}
BENCHMARK(BM_Erased_PoolOnly);

// Benchmark allocating an Either<Erased,Erased>
static void BM_EitherErased_Construction(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        cask::Erased erased(42);
        auto either = cask::Either<cask::Erased,cask::Erased>::left(std::move(erased));
        benchmark::DoNotOptimize(either);
    }
}
BENCHMARK(BM_EitherErased_Construction);

// Benchmark allocating ConstantData via pool (now just Erased, not Either<Erased,Erased>)
static void BM_ConstantData_PoolAllocation(benchmark::State& state) {
    using ConstantData = cask::fiber::FiberOp::ConstantData;  // Now just Erased
    auto pool = cask::pool::global_pool();
    for (auto _ : state) {
        auto* constant = pool->allocate<ConstantData>(42);
        benchmark::DoNotOptimize(constant);
        pool->deallocate(constant);
    }
}
BENCHMARK(BM_ConstantData_PoolAllocation);

// Benchmark FiberOp construction without shared_ptr wrapping
static void BM_FiberOp_RawConstruction(benchmark::State& state) {
    auto pool = cask::pool::global_pool();
    using ConstantData = cask::fiber::FiberOp::ConstantData;
    for (auto _ : state) {
        auto* constant = pool->allocate<ConstantData>(42);
        auto* op = pool->allocate<cask::fiber::FiberOp>(constant, pool, cask::fiber::VALUE);
        benchmark::DoNotOptimize(op);
        pool->deallocate(op);
    }
}
BENCHMARK(BM_FiberOp_RawConstruction);

