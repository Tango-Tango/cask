//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"

using cask::None;
using cask::Task;
using cask::Scheduler;

TEST(TaskTimeout, DoesntTimeoutValue) {
    auto result = Task<int,std::string>::pure(123)
        .timeout(100, "timeout")
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, 123);
}

TEST(TaskTimeout, DoesntTimeoutNormalError) {
    auto result = Task<int,std::string>::raiseError("broke")
        .timeout(100, "timeout")
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST(TaskTimeout, TimesOut) {
    auto result = Task<int, std::string>::never()
        .timeout(1, "timeout")
        .failed()
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, "timeout");
}