//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Task.hpp"
#include "gtest/gtest.h"

using cask::Scheduler;
using cask::Task;

TEST(TaskRaiseError, EvalutesSync) {
    auto result = Task<int, float>::raiseError(1.23).runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());
    EXPECT_EQ(result->get_right(), 1.23f);
}

TEST(TaskRaiseError, EvaluatesAsync) {
    try {
        Task<int, float>::raiseError(1.23).run(Scheduler::global())->await();

        FAIL() << "Excepted operation to throw.";
    } catch (float& error) {
        EXPECT_EQ(error, 1.23f);
    }
}
