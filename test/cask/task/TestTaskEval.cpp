//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"

using cask::Task;
using cask::Scheduler;

TEST(TaskEval,EvalutesSync) {
    auto result = Task<int>::eval([]{ return 123; }).runSync();
    
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_left());  // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->get_left(), 123);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST(TaskEval,EvaluatesAsync) {
    auto result = Task<int>::eval([]{ return 123; })
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, 123);
}
