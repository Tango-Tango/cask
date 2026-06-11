//          Copyright Tango Tango, Inc. 2020 - 2025.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <benchmark/benchmark.h>
#include "cask/Task.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"

using cask::Task;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::WorkStealingScheduler;

// These benchmarks mirror BenchFlatMap but execute on a real scheduler,
// with periodic asyncBoundary() steps so the fiber cedes and is re-queued
// through the scheduler's ReadyQueue rather than running synchronously.

namespace {

Task<int> buildCedingChain(int chain_length, int cede_every) {
    Task<int> task = Task<int>::pure(0);
    for (int i = 0; i < chain_length; ++i) {
        task = task.template flatMap<int>([](int value) {
            return Task<int>::pure(value + 1);
        });
        if (cede_every > 0 && (i + 1) % cede_every == 0) {
            task = task.asyncBoundary();
        }
    }
    return task;
}

void runChainOnScheduler(benchmark::State& state, const std::shared_ptr<cask::Scheduler>& sched, int cede_every) {
    const int chain_length = static_cast<int>(state.range(0));
    auto task = buildCedingChain(chain_length, cede_every);

    for (auto _ : state) {
        auto result = task.run(sched)->await();
        benchmark::DoNotOptimize(result);
    }
}

} // namespace

// Pure flatMap chain on a real scheduler with no cedes — the synchronous
// fast path. ReadyQueue is touched only once for the initial submit.
static void BM_SchedFlatMap_SingleThread_NoCede(benchmark::State& state) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    runChainOnScheduler(state, sched, 0);
}
BENCHMARK(BM_SchedFlatMap_SingleThread_NoCede)->Arg(64)->Arg(256);

// Cede after every operation — maximum ReadyQueue pressure. Every step of
// the chain round-trips through push_back/pop_front.
static void BM_SchedFlatMap_SingleThread_CedeEvery1(benchmark::State& state) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    runChainOnScheduler(state, sched, 1);
}
BENCHMARK(BM_SchedFlatMap_SingleThread_CedeEvery1)->Arg(64)->Arg(256);

// Cede every 16 operations — a mixed workload closer to real usage where
// most work is synchronous with occasional fairness yields.
static void BM_SchedFlatMap_SingleThread_CedeEvery16(benchmark::State& state) {
    auto sched = std::make_shared<SingleThreadScheduler>();
    runChainOnScheduler(state, sched, 16);
}
BENCHMARK(BM_SchedFlatMap_SingleThread_CedeEvery16)->Arg(64)->Arg(256);

// Same shapes on the work stealing scheduler, where requeues may also
// involve steal_from between worker queues.
static void BM_SchedFlatMap_WorkStealing_NoCede(benchmark::State& state) {
    auto sched = std::make_shared<WorkStealingScheduler>(4);
    runChainOnScheduler(state, sched, 0);
}
BENCHMARK(BM_SchedFlatMap_WorkStealing_NoCede)->Arg(64)->Arg(256);

static void BM_SchedFlatMap_WorkStealing_CedeEvery1(benchmark::State& state) {
    auto sched = std::make_shared<WorkStealingScheduler>(4);
    runChainOnScheduler(state, sched, 1);
}
BENCHMARK(BM_SchedFlatMap_WorkStealing_CedeEvery1)->Arg(64)->Arg(256);

static void BM_SchedFlatMap_WorkStealing_CedeEvery16(benchmark::State& state) {
    auto sched = std::make_shared<WorkStealingScheduler>(4);
    runChainOnScheduler(state, sched, 16);
}
BENCHMARK(BM_SchedFlatMap_WorkStealing_CedeEvery16)->Arg(64)->Arg(256);

// Many concurrent fibers all ceding — keeps the ReadyQueue populated so
// pops happen under load rather than ping-ponging an empty queue.
static void BM_SchedFlatMap_SingleThread_ManyFibers(benchmark::State& state) {
    const int num_fibers = static_cast<int>(state.range(0));
    auto sched = std::make_shared<SingleThreadScheduler>();
    auto task = buildCedingChain(32, 1);

    for (auto _ : state) {
        std::vector<cask::FiberRef<int, std::any>> fibers;
        fibers.reserve(num_fibers);
        for (int i = 0; i < num_fibers; ++i) {
            fibers.push_back(task.run(sched));
        }
        for (auto& fiber : fibers) {
            auto result = fiber->await();
            benchmark::DoNotOptimize(result);
        }
    }
}
BENCHMARK(BM_SchedFlatMap_SingleThread_ManyFibers)->Arg(8);
