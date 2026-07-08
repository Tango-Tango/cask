//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Task;
using cask::Scheduler;

TEST(TaskDefer,EvalutesSyncThingSync) {
    auto deferred = []{ return Task<int>::pure(123); };
    auto result = Task<int>::defer(deferred).runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_left());  // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->get_left(), 123); // NOLINT(bugprone-unchecked-optional-access)
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
    ASSERT_TRUE(result->is_right()); // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->get_right(), 1.23f);  // NOLINT(bugprone-unchecked-optional-access)
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

TEST(TaskDeferAction, ThrownErrorBecomesAsyncFailure) {
    auto sched = Scheduler::global();

    try {
        Task<int, std::string>::deferAction([](const auto&) -> cask::DeferredRef<int,std::string> {
            throw std::string("broke");
        })
            .run(sched)
            ->await();

        FAIL() << "Expected task error to be thrown.";
    } catch(const std::string& error) {
        EXPECT_EQ(error, "broke");
    }
}

TEST(TaskDeferAction, ThrownErrorSurvivesMapErrorRetype) {
    auto sched = std::make_shared<cask::scheduler::BenchScheduler>();

    auto fiber = Task<int, std::string>::deferAction([](const auto&) -> cask::DeferredRef<int,std::string> {
            throw std::string("broke");
        })
        .template mapError<int>([](std::string&& error) {
            return static_cast<int>(error.size());
        })
        .run(sched);

    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->getError().has_value());
    EXPECT_EQ(*(fiber->getError()), 5);  // NOLINT(bugprone-unchecked-optional-access)
}
