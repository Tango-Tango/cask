//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <benchmark/benchmark.h>
#include <string>
#include <vector>
#include "cask/Erased.hpp"

using cask::Erased;

// Benchmark copying an Erased containing an int
static void BM_Erased_CopyInt(benchmark::State& state) {
    Erased source(42);
    for (auto _ : state) {
        Erased dest(source);
        benchmark::DoNotOptimize(dest);
    }
}
BENCHMARK(BM_Erased_CopyInt);

// Benchmark moving an Erased containing an int (using swap pattern to reuse source)
static void BM_Erased_MoveInt(benchmark::State& state) {
    Erased source(42);
    Erased dest;
    for (auto _ : state) {
        dest = std::move(source);
        benchmark::DoNotOptimize(dest);
        // Swap back so source has the value again for next iteration
        source = std::move(dest);
    }
}
BENCHMARK(BM_Erased_MoveInt);

// Benchmark copying an Erased containing a string
static void BM_Erased_CopyString(benchmark::State& state) {
    std::string str(static_cast<size_t>(state.range(0)), 'x');
    Erased source(str);
    for (auto _ : state) {
        Erased dest(source);
        benchmark::DoNotOptimize(dest);
    }
}
BENCHMARK(BM_Erased_CopyString)->Range(8, 8 << 10);

// Benchmark moving an Erased containing a string (using swap pattern)
static void BM_Erased_MoveString(benchmark::State& state) {
    std::string str(static_cast<size_t>(state.range(0)), 'x');
    Erased source(str);
    Erased dest;
    for (auto _ : state) {
        dest = std::move(source);
        benchmark::DoNotOptimize(dest);
        source = std::move(dest);
    }
}
BENCHMARK(BM_Erased_MoveString)->Range(8, 8 << 10);

// Benchmark copy assignment of Erased containing an int
static void BM_Erased_CopyAssignInt(benchmark::State& state) {
    Erased source(42);
    Erased dest;
    for (auto _ : state) {
        dest = source;
        benchmark::DoNotOptimize(dest);
    }
}
BENCHMARK(BM_Erased_CopyAssignInt);

// Benchmark move assignment of Erased containing an int
static void BM_Erased_MoveAssignInt(benchmark::State& state) {
    Erased source(42);
    Erased dest;
    for (auto _ : state) {
        dest = std::move(source);
        benchmark::DoNotOptimize(dest);
        source = std::move(dest);
    }
}
BENCHMARK(BM_Erased_MoveAssignInt);

// Benchmark copy assignment of Erased containing a string
static void BM_Erased_CopyAssignString(benchmark::State& state) {
    std::string str(static_cast<size_t>(state.range(0)), 'x');
    Erased source(str);
    Erased dest;
    for (auto _ : state) {
        dest = source;
        benchmark::DoNotOptimize(dest);
    }
}
BENCHMARK(BM_Erased_CopyAssignString)->Range(8, 8 << 10);

// Benchmark move assignment of Erased containing a string
static void BM_Erased_MoveAssignString(benchmark::State& state) {
    std::string str(static_cast<size_t>(state.range(0)), 'x');
    Erased source(str);
    Erased dest;
    for (auto _ : state) {
        dest = std::move(source);
        benchmark::DoNotOptimize(dest);
        source = std::move(dest);
    }
}
BENCHMARK(BM_Erased_MoveAssignString)->Range(8, 8 << 10);

// Benchmark a chain of copies (simulating value passing through flatMap chain)
static void BM_Erased_CopyChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));
    Erased source(42);
    for (auto _ : state) {
        Erased current = source;
        for (int i = 0; i < chain_length; ++i) {
            Erased next(current);
            current = next;
        }
        benchmark::DoNotOptimize(current);
    }
}
BENCHMARK(BM_Erased_CopyChain)->Range(1, 64);

// Benchmark a chain of moves (simulating value passing through flatMap chain)
// Pre-allocate all Erased objects to avoid measuring construction cost
static void BM_Erased_MoveChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));
    std::vector<Erased> chain(static_cast<size_t>(chain_length + 1));
    for (auto _ : state) {
        // Reset the chain with a value at the start
        chain[0] = 42;
        for (int i = 0; i < chain_length; ++i) {
            chain[static_cast<size_t>(i + 1)] = std::move(chain[static_cast<size_t>(i)]);
        }
        benchmark::DoNotOptimize(chain[static_cast<size_t>(chain_length)]);
    }
}
BENCHMARK(BM_Erased_MoveChain)->Range(1, 64);

// Benchmark copying a vector (larger heap-allocated object)
static void BM_Erased_CopyVector(benchmark::State& state) {
    std::vector<int> vec(static_cast<size_t>(state.range(0)), 42);
    Erased source(vec);
    for (auto _ : state) {
        Erased dest(source);
        benchmark::DoNotOptimize(dest);
    }
}
BENCHMARK(BM_Erased_CopyVector)->Range(8, 8 << 10);

// Benchmark moving a vector (larger heap-allocated object)
static void BM_Erased_MoveVector(benchmark::State& state) {
    std::vector<int> vec(static_cast<size_t>(state.range(0)), 42);
    Erased source(vec);
    Erased dest;
    for (auto _ : state) {
        dest = std::move(source);
        benchmark::DoNotOptimize(dest);
        source = std::move(dest);
    }
}
BENCHMARK(BM_Erased_MoveVector)->Range(8, 8 << 10);

