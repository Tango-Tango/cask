//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"

using cask::None;
using cask::Task;
using cask::Scheduler;

TEST(Task, LeftValue) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123).raceWith(Task<float,None>::never());
    auto result = task.run(sched)->await();
    EXPECT_EQ(result.get_left(), 123);
}

TEST(Task, LeftError) {
    auto sched = Scheduler::global();
    auto task = Task<int,std::string>::raiseError("boom").raceWith(Task<float,std::string>::never());
    auto result = task.failed().run(sched)->await();
    EXPECT_EQ(result, "boom");
}

TEST(Task, RightValue) {
    auto sched = Scheduler::global();
    auto task = Task<float,None>::never().raceWith(Task<int,None>::pure(123));
    auto result = task.run(sched)->await();
    EXPECT_EQ(result.get_right(), 123);
}

TEST(Task, RightError) {
    auto sched = Scheduler::global();
    auto task = Task<float,std::string>::never().raceWith(Task<int,std::string>::raiseError("boom"));
    auto result = task.failed().run(sched)->await();
    EXPECT_EQ(result, "boom");
}

TEST(Task, Cancelled) {
    auto sched = Scheduler::global();
    auto task = Task<float,None>::never().raceWith(Task<float,None>::never());
    auto deferred = task.run(sched);

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw.";
    } catch(std::runtime_error&) {}
}
