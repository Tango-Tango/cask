//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"

using cask::Task;
using cask::Scheduler;
using cask::None;

// NOLINTBEGIN(bugprone-unchecked-optional-access)

TEST(TaskNone,EvalutesSync) {
    auto result = Task<>::none().runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_left());
    EXPECT_EQ(result->get_left(), None());
}

TEST(TaskNone,EvaluatesAsync) {
    auto result = Task<>::none()
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, None());
}

// NOLINTEND(bugprone-unchecked-optional-access)
