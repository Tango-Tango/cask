//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include <exception>

using cask::None;
using cask::Scheduler;
using cask::Task;

TEST(TaskGuarantee, RunsOnComplete) {
    auto counter = 0;
    auto result = Task<int>::pure(123)
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, 123);
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, RunsOnError) {
    auto counter = 0;
    auto result = Task<int, std::string>::raiseError("broke")
        .guarantee(Task<None, std::string>::eval([&counter] {
            counter++;
            return None();
        }))
        .failed()
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, "broke");
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, RunsOnCancel) {
    auto counter = 0;
    auto deferred = Task<int>::never()
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(Scheduler::global());

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 1);
}
