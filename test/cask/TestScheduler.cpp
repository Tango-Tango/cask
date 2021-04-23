//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <atomic>
#include "gtest/gtest.h"
#include "cask/Scheduler.hpp"

using cask::Scheduler;

const static std::chrono::milliseconds sleep_time(1);

class SchedulerTest : public ::testing::TestWithParam<int> {
protected:

    void SetUp() override {
        sched = std::make_shared<Scheduler>(GetParam());
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

    EXPECT_GT(milliseconds, 25);
    EXPECT_LT(milliseconds, 50);
    
    awaitIdle();
}

INSTANTIATE_TEST_SUITE_P(Scheduler, SchedulerTest, ::testing::Values(1, 2, 4, 16));
