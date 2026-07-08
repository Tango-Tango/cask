//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <benchmark/benchmark.h>
#include <string>
#include <vector>
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Either;
using cask::None;
using cask::Task;
using cask::scheduler::BenchScheduler;

// Benchmarks for per-operator payload traffic. Each stage of a task
// composition hands its value/error payload to the next stage through
// FiberValue/Erased - these benchmarks use copy-expensive, move-cheap
// payloads (vectors/strings) so that a payload copy at any stage shows
// up directly in the measured time.

namespace {

constexpr std::size_t payload_elems = 512;

std::vector<int> make_payload() {
    return std::vector<int>(payload_elems, 42);
}

using VectorTask = Task<std::vector<int>, std::vector<int>>;

} // namespace

// A single eval thunk on the non-throwing fast path - isolates the overhead
// of entering the THUNK branch in the fiber loop.
static void BM_Eval_RunSync(benchmark::State& state) {
    Task<int, std::string> task = Task<int, std::string>::eval([]() {
        return 42;
    });

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_RunSync);

// Fast path for deferAction: the callback does not throw and returns an
// already-completed Deferred. This exercises the new typed catch boundary.
static void BM_DeferAction_Async(benchmark::State& state) {
    auto sched = std::make_shared<BenchScheduler>();
    auto task = Task<int, std::string>::deferAction([](const auto&) {
        return cask::Deferred<int, std::string>::pure(42);
    });

    for (auto _ : state) {
        auto fiber = task.run(sched);
        sched->run_ready_tasks();
        auto result = fiber->await();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_DeferAction_Async);

// Fast path for deferFiber: the callback does not throw and returns a fiber.
static void BM_DeferFiber_Async(benchmark::State& state) {
    auto sched = std::make_shared<BenchScheduler>();
    auto task = Task<int, std::string>::deferFiber([](const auto& sched) {
        return Task<int, std::string>::pure(42).run(sched);
    });

    for (auto _ : state) {
        auto fiber = task.run(sched);
        sched->run_ready_tasks();
        auto result = fiber->await();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_DeferFiber_Async);

// A chain of map stages moving a vector payload through each stage.
static void BM_Map_VectorChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::pure(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.template map<std::vector<int>>([](std::vector<int>&& value) {
            value[0]++;
            return std::move(value);
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Map_VectorChain)->Range(1, 256);

// A success payload passing untouched through a chain of error-side
// operators (mapError) - exercises the value passthrough branch.
static void BM_MapError_ValuePassthrough(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::pure(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.template mapError<std::vector<int>>([](std::vector<int>&& error) {
            return std::move(error);
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_MapError_ValuePassthrough)->Range(1, 256);

// An error payload flowing through a chain of mapError stages.
static void BM_MapError_ErrorChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::raiseError(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.template mapError<std::vector<int>>([](std::vector<int>&& error) {
            error[0]++;
            return std::move(error);
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_MapError_ErrorChain)->Range(1, 256);

// A success payload passing untouched through a chain of flatMapError stages.
static void BM_FlatMapError_ValuePassthrough(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::pure(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.template flatMapError<std::vector<int>>([](std::vector<int>&& error) {
            return VectorTask::raiseError(std::move(error));
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FlatMapError_ValuePassthrough)->Range(1, 256);

// A vector payload moved through a chain of flatMap stages.
static void BM_FlatMap_VectorChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::pure(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.template flatMap<std::vector<int>>([](std::vector<int>&& value) {
            value[0]++;
            return VectorTask::pure(std::move(value));
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FlatMap_VectorChain)->Range(1, 256);

// Repeated materialize/dematerialize round trips - each round trip wraps
// the payload into an Either and unwraps it again.
static void BM_MaterializeDematerialize_Roundtrip(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::pure(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.materialize().template dematerialize<std::vector<int>>();
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_MaterializeDematerialize_Roundtrip)->Range(1, 128);

// Repeated failed() transpositions - each pair of stages moves the payload
// from the value channel to the error channel and back.
static void BM_Failed_Roundtrip(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::pure(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.failed().failed();
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Failed_Roundtrip)->Range(1, 128);

// An error payload consumed by a recover stage after passing through a
// chain of onError side-effect stages (which only observe the error).
static void BM_OnError_ErrorChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::raiseError(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.onError([](const std::vector<int>& error) {
            benchmark::DoNotOptimize(error.data());
        });
    }
    task = task.recover([](std::vector<int>&& error) {
        return std::move(error);
    });

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_OnError_ErrorChain)->Range(1, 256);

// A success payload passing through a chain of guarantee finalizers.
static void BM_Guarantee_ValuePassthrough(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::pure(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.guarantee(Task<None, std::vector<int>>::none());
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Guarantee_ValuePassthrough)->Range(1, 128);

// A success payload passing through a chain of doOnCancel stages.
static void BM_DoOnCancel_ValuePassthrough(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    VectorTask task = VectorTask::pure(make_payload());
    for (int i = 0; i < chain_length; ++i) {
        task = task.doOnCancel(Task<None, None>::none());
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_DoOnCancel_ValuePassthrough)->Range(1, 128);

// String payloads through map - a smaller but heap-backed payload that is
// common in practice.
static void BM_Map_StringChain(benchmark::State& state) {
    const int chain_length = static_cast<int>(state.range(0));

    Task<std::string> task = Task<std::string>::pure(std::string(256, 'x'));
    for (int i = 0; i < chain_length; ++i) {
        task = task.template map<std::string>([](std::string&& value) {
            value[0] = 'y';
            return std::move(value);
        });
    }

    for (auto _ : state) {
        auto result = task.runSync();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Map_StringChain)->Range(1, 256);
