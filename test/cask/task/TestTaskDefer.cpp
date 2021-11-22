//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"

using cask::Task;
using cask::Scheduler;

TEST(TaskDefer,EvalutesSyncThingSync) {
    auto deferred = []{ return Task<int>::pure(123); };
    auto result = Task<int>::defer(deferred).runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_left());
    EXPECT_EQ(result->get_left(), 123);
}

TEST(TaskEval,EvalutesSyncThingAsync) {
    auto deferred = []{ return Task<int>::pure(123); };
    auto result = Task<int>::defer(deferred)
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, 123);
}

TEST(TaskDefer,EvalutesErrorSync) {
    auto deferred = []{ return Task<int,float>::raiseError(1.23); };
    auto result = Task<int,float>::defer(deferred).runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());
    EXPECT_EQ(result->get_right(), 1.23f);
}

TEST(TaskDefer,EvaluatesErrorAsync) {
    auto deferred = []{ return Task<int,float>::raiseError(1.23); };

    try {
        Task<int,float>::defer(deferred)
            .run(Scheduler::global())
            ->await();
        
        FAIL() << "Excepted operation to throw.";
    } catch(float& error) {
        EXPECT_EQ(error, 1.23f);
    }
}

TEST(TaskEval,EvalutesAsyncThingAsync) {
    auto deferred = []{ return Task<int>::pure(123).asyncBoundary(); };
    auto result = Task<int>::defer(deferred)
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, 123);
}
