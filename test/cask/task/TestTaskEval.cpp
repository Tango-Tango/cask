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

TEST(TaskEval,ThrownErrorBecomesSyncFailure) {
    auto result = Task<int, std::string>::eval([]() -> int {
        throw std::string("broke");
    }).runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());  // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->get_right(), "broke");  // NOLINT(bugprone-unchecked-optional-access)
}

TEST(TaskEval,ThrownErrorBecomesAsyncFailure) {
    try {
        Task<int, std::string>::eval([]() -> int {
            throw std::string("broke");
        })
            .run(Scheduler::global())
            ->await();

        FAIL() << "Expected task error to be thrown.";
    } catch(const std::string& error) {
        EXPECT_EQ(error, "broke");
    }
}

TEST(TaskEval,ThrownErrorSurvivesMapErrorRetype) {
    auto result = Task<int, std::string>::eval([]() -> int {
        throw std::string("broke");
    })
        .template mapError<int>([](std::string&& error) {
            return static_cast<int>(error.size());
        })
        .runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());  // NOLINT(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->get_right(), 5);  // NOLINT(bugprone-unchecked-optional-access)
}
