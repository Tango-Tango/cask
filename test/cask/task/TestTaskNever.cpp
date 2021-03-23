//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"

using cask::Task;
using cask::Scheduler;

TEST(TaskNever,EvalutesSync) {
    auto result = Task<int,float>::never().runSync();
    ASSERT_TRUE(result.is_right());

    auto continuation = result.get_right();
    auto deferred = continuation.run(Scheduler::global());

    deferred->cancel(1.23);

    try {
        deferred->await();
        FAIL() << "Excepted operation to throw.";
    } catch(float& error) {
        EXPECT_EQ(error, 1.23f);
    }
}

TEST(TaskNever,EvalutesAsync) {
    auto deferred = Task<int,float>::never().run(Scheduler::global());
    deferred->cancel(1.23);

    try {
        
        deferred->await();
        FAIL() << "Excepted operation to throw.";
    } catch(float& error) {
        EXPECT_EQ(error, 1.23f);
    }
}

