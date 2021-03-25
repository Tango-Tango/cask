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
    ASSERT_TRUE(result.is_left());
    
    auto value = result.get_left();
    ASSERT_TRUE(value.is_left());
    EXPECT_EQ(value.get_left(), 123);
}

TEST(TaskEval,EvaluatesAsync) {
    auto result = Task<int>::eval([]{ return 123; })
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, 123);
}
