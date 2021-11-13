//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"

using cask::None;
using cask::Task;
using cask::Scheduler;

TEST(TaskRace, LeftValue) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123).raceWith(Task<int,None>::never());
    auto result = task.run(sched)->await();
    EXPECT_EQ(result, 123);
}

TEST(TaskRace, LeftError) {
    auto sched = Scheduler::global();
    auto task = Task<int,std::string>::raiseError("boom").raceWith(Task<int,std::string>::never());
    auto result = task.failed().run(sched)->await();
    EXPECT_EQ(result, "boom");
}

TEST(TaskRace, RightValue) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::never().raceWith(Task<int,None>::pure(123));
    auto result = task.run(sched)->await();
    EXPECT_EQ(result, 123);
}

TEST(TaskRace, RightError) {
    auto sched = Scheduler::global();
    auto task = Task<int,std::string>::never().raceWith(Task<int,std::string>::raiseError("boom"));
    auto result = task.failed().run(sched)->await();
    EXPECT_EQ(result, "boom");
}

TEST(TaskRace, Cancelled) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::never().raceWith(Task<int,None>::never());
    auto deferred = task.run(sched);

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw.";
    } catch(std::runtime_error&) {}
}
