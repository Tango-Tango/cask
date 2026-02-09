//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <benchmark/benchmark.h>
#include "cask/Task.hpp"
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
}
BENCHMARK(BM_FlatMap_FullCost)->Range(1, 256);

