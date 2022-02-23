//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include "gtest/gtest.h"

using cask::None;
using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(TaskRace, LeftValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto task = Task<int, None>::pure(123).raceWith(Task<int, None>::never());
    auto result = task.run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(result->await(), 123);
}

TEST(TaskRace, LeftError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto task = Task<int, std::string>::raiseError("boom").raceWith(Task<int, std::string>::never());
    auto result = task.failed().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(result->await(), "boom");
}

TEST(TaskRace, RightValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto task = Task<int, None>::never().raceWith(Task<int, None>::pure(123));
    auto result = task.run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(result->await(), 123);
}

TEST(TaskRace, RightError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto task = Task<int, std::string>::never().raceWith(Task<int, std::string>::raiseError("boom"));
    auto result = task.failed().run(sched);

    sched->run_ready_tasks();

    EXPECT_EQ(result->await(), "boom");
}

TEST(TaskRace, Cancelled) {
    auto sched = std::make_shared<BenchScheduler>();
    auto task = Task<int, None>::never().raceWith(Task<int, None>::never());
    auto deferred = task.run(sched);

    sched->run_ready_tasks();
    deferred->cancel();
    sched->run_ready_tasks();

    try {
        deferred->await();
        FAIL() << "Expected method to throw.";
    } catch (std::runtime_error&) {
    }
}
