//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <atomic>
#include "gtest/gtest.h"
#include "cask/Deferred.hpp"
#include "cask/Task.hpp"
#include "cask/Scheduler.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"

using cask::Deferred;
using cask::Task;
using cask::Scheduler;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::WorkStealingScheduler;

const static std::chrono::milliseconds sleep_time(1);

class SchedulerTest : public ::testing::TestWithParam<std::shared_ptr<Scheduler>> {
protected:

    void SetUp() override {
        sched = GetParam();
    }

    void awaitIdle() {
        int num_retries = 1000;
        while(num_retries > 0) {
            if(sched->isIdle()) {
                return;
            } else {
                std::this_thread::sleep_for(sleep_time);
                num_retries--;
            }
        }

        FAIL() << "Expected scheduler to return to idle within 1 second.";
    }
    
    std::shared_ptr<Scheduler> sched;
};

TEST_P(SchedulerTest, IdlesAtStart) {
    EXPECT_TRUE(sched->isIdle());
}

TEST_P(SchedulerTest, SubmitSingle) {
    std::mutex mutex;
    mutex.lock();

    sched->submit([&mutex] {
        mutex.unlock();
    });

    mutex.lock();
    
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
            std::this_thread::sleep_for(sleep_time);
            num_exec_retries--;
        }
    }

    EXPECT_EQ(num_executed.load(), num_tasks);
    awaitIdle();
}

TEST_P(SchedulerTest, SubmitAfter) {
    std::mutex mutex;
    mutex.lock();

    auto before = std::chrono::high_resolution_clock::now();
    sched->submitAfter(25, [&mutex] {
        mutex.unlock();
    });
    mutex.lock();
    auto after = std::chrono::high_resolution_clock::now();

    auto delta = after - before;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();

    EXPECT_GE(milliseconds, 24);
    
    awaitIdle();
}

TEST_P(SchedulerTest, SubmitAfterCancel) {
    std::mutex mutex;
    mutex.lock();

    int cancel_counter = 0;
    auto firstHandle = sched->submitAfter(25, []{});
    auto secondHandle = sched->submitAfter(25, [&mutex] { mutex.unlock(); });

    firstHandle->onCancel([&cancel_counter]{ cancel_counter++; });
    secondHandle->onCancel([&cancel_counter]{ cancel_counter++; });

    firstHandle->cancel();
    firstHandle->cancel();
    firstHandle->cancel();

    mutex.lock();

    EXPECT_EQ(cancel_counter, 1);
    awaitIdle();
}

TEST_P(SchedulerTest, RegistersCallbackAfterCancelled) {
    std::mutex mutex;
    mutex.lock();

    int cancel_counter = 0;
    auto firstHandle = sched->submitAfter(25, []{});
    auto secondHandle = sched->submitAfter(25, [&mutex] { mutex.unlock(); });

    firstHandle->cancel();
    firstHandle->onCancel([&cancel_counter]{ cancel_counter++; });
    secondHandle->onCancel([&cancel_counter]{ cancel_counter++; });

    mutex.lock();

    EXPECT_EQ(cancel_counter, 1);
    awaitIdle();
}

TEST_P(SchedulerTest, RunsShutdownCallbackAfterTimerTaskCompletion) {
    bool shutdown = false;
    std::mutex shutdown_mutex;

    shutdown_mutex.lock();

    auto before = std::chrono::high_resolution_clock::now();
    auto cancelable = sched->submitAfter(25, [] {});

    cancelable->onShutdown([&shutdown, &shutdown_mutex] {
        shutdown = true;
        shutdown_mutex.unlock();
    });

    shutdown_mutex.lock();
    auto after = std::chrono::high_resolution_clock::now();

    auto delta = after - before;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();

    EXPECT_GE(milliseconds, 24);
    EXPECT_TRUE(shutdown);
    
    awaitIdle();
}

TEST_P(SchedulerTest, RunsShutdownImmediatelyCallbackIfTimerAlreadyFired) {
    bool shutdown = false;
    std::mutex mutex;
    mutex.lock();

    auto before = std::chrono::high_resolution_clock::now();
    auto cancelable = sched->submitAfter(25, [&mutex] {
        mutex.unlock();
    });

    mutex.lock();
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

INSTANTIATE_TEST_SUITE_P(Scheduler, SchedulerTest,
    ::testing::Values(
        std::make_shared<SingleThreadScheduler>(),
        std::make_shared<WorkStealingScheduler>(1),
        std::make_shared<WorkStealingScheduler>(2),
        std::make_shared<WorkStealingScheduler>(4),
        std::make_shared<WorkStealingScheduler>(8)
    ),
    [](const ::testing::TestParamInfo<SchedulerTest::ParamType>& info) {
        return info.param->toString();
    }
);
