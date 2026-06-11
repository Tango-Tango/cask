//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <atomic>
#include <chrono>
#include "gtest/gtest.h"
#include "cask/Deferred.hpp"
#include "cask/Task.hpp"
#include "cask/Scheduler.hpp"
#include "SchedulerTestBench.hpp"

using cask::Deferred;
using cask::Task;
using cask::Scheduler;

INSTANTIATE_SCHEDULER_TEST_BENCH_SUITE(SchedulerTest);

TEST_P(SchedulerTest, IdlesAtStart) {
    EXPECT_TRUE(sched->isIdle());
}

TEST_P(SchedulerTest, SubmitSingle) {
    TestSignal signal;

    sched->submit([&signal] {
        signal.notify();
    });

    signal.wait();
    
    awaitIdle();
}

TEST_P(SchedulerTest, SubmitBulk) {
    const static int num_tasks = 100;
    int num_exec_retries = 1000;

    std::atomic_int num_executed(0);
    std::vector<std::function<void()>> tasks;

    tasks.reserve(num_tasks);
    for(int i = 0; i < num_tasks; i++) {
        tasks.push_back([&num_executed] {
            num_executed++;
        });
    }

    sched->submitBulk(tasks);

    
    while(num_exec_retries > 0) {
        if(num_executed.load() == num_tasks) {
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            num_exec_retries--;
        }
    }

    EXPECT_EQ(num_executed.load(), num_tasks);
    awaitIdle();
}

TEST_P(SchedulerTest, SubmitAfter) {
    TestSignal signal;

    auto before = std::chrono::high_resolution_clock::now();
    sched->submitAfter(25, [&signal] {
        signal.notify();
    });
    signal.wait();
    auto after = std::chrono::high_resolution_clock::now();

    auto delta = after - before;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();

    EXPECT_GE(milliseconds, 24);
    
    awaitIdle();
}

TEST_P(SchedulerTest, SubmitAfterCancel) {
    TestSignal signal;

    int cancel_counter = 0;
    auto firstHandle = sched->submitAfter(25, []{});
    auto secondHandle = sched->submitAfter(25, [&signal] { signal.notify(); });

    firstHandle->onCancel([&cancel_counter]{ cancel_counter++; });
    secondHandle->onCancel([&cancel_counter]{ cancel_counter++; });

    firstHandle->cancel();
    firstHandle->cancel();
    firstHandle->cancel();

    signal.wait();

    EXPECT_EQ(cancel_counter, 1);
    awaitIdle();
}

TEST_P(SchedulerTest, RegistersCallbackAfterCancelled) {
    TestSignal signal;

    int cancel_counter = 0;
    auto firstHandle = sched->submitAfter(25, []{});
    auto secondHandle = sched->submitAfter(25, [&signal] { signal.notify(); });

    firstHandle->cancel();
    firstHandle->onCancel([&cancel_counter]{ cancel_counter++; });
    secondHandle->onCancel([&cancel_counter]{ cancel_counter++; });

    signal.wait();

    EXPECT_EQ(cancel_counter, 1);
    awaitIdle();
}

TEST_P(SchedulerTest, RunsShutdownCallbackAfterTimerTaskCompletion) {
    bool shutdown = false;
    TestSignal shutdown_signal;

    auto before = std::chrono::high_resolution_clock::now();
    auto cancelable = sched->submitAfter(25, [] {});

    cancelable->onShutdown([&shutdown, &shutdown_signal] {
        shutdown = true;
        shutdown_signal.notify();
    });

    shutdown_signal.wait();
    auto after = std::chrono::high_resolution_clock::now();

    auto delta = after - before;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();

    EXPECT_GE(milliseconds, 24);
    EXPECT_TRUE(shutdown);
    
    awaitIdle();
}

TEST_P(SchedulerTest, RunsShutdownImmediatelyCallbackIfTimerAlreadyFired) {
    bool shutdown = false;
    TestSignal signal;

    auto before = std::chrono::high_resolution_clock::now();
    auto cancelable = sched->submitAfter(25, [&signal] {
        signal.notify();
    });

    signal.wait();
    auto after = std::chrono::high_resolution_clock::now();

    auto delta = after - before;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();

    EXPECT_GE(milliseconds, 24);
    EXPECT_FALSE(shutdown);

    cancelable->onShutdown([&shutdown] {
        shutdown = true;
    });

    EXPECT_TRUE(shutdown);
    
    awaitIdle();
}

TEST_P(SchedulerTest, AwaitTaskOnScheduler) {
    auto result = Task<int>::deferFiber([](auto sched) {
        auto result = Task<int>::eval([] {
                return 42;
            })
            .asyncBoundary()
            .run(sched)
            ->await();
        
        return Task<int>::pure(result).asyncBoundary().run(sched);
    }).run(sched)->await();

    EXPECT_EQ(result, 42);
}
