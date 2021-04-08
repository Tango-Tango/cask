//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Scheduler.hpp"
#include "cask/Task.hpp"
#include <exception>

using cask::Scheduler;
using cask::Task;

TEST(TaskMapBoth, ValuesAndReturnSameType) {
    auto result = Task<int,std::string>::mapBoth<int, int>(
        Task<int, std::string>::pure(2),
        Task<int, std::string>::pure(3),
        [](int a, int b) -> int {
            return a * b;
        },
        Scheduler::global()
    )
    .runSync();

    ASSERT_TRUE(result.is_left());

    auto syncResult = result.get_left();
    ASSERT_TRUE(syncResult.is_left());
    EXPECT_EQ(syncResult.get_left(), 6);
}

TEST(TaskMapBoth, ValuesSameTypeReturnTypeDifferent) {
    auto result = Task<float, std::string>::mapBoth<int, int>(
        Task<int, std::string>::pure(3),
        Task<int, std::string>::pure(5),
        [](int a, int b) -> float {
            return static_cast<float>(a * b * 1.5);
        },
        Scheduler::global()
    )
    .runSync();

    ASSERT_TRUE(result.is_left());

    auto syncResult = result.get_left();
    ASSERT_TRUE(syncResult.is_left());
    EXPECT_EQ(syncResult.get_left(), 22.5f);
}

TEST(TaskMapBoth, ValuesDifferentType) {
    auto result = Task<float, std::string>::mapBoth<float, int>(
        Task<float, std::string>::pure(1.5f),
        Task<int, std::string>::pure(3),
        [](float a, int b) -> float {
            return a * static_cast<float>(b);
        },
        Scheduler::global()
    )
    .runSync();

    ASSERT_TRUE(result.is_left());

    auto syncResult = result.get_left();
    ASSERT_TRUE(syncResult.is_left());
    EXPECT_EQ(syncResult.get_left(), 4.5f);
}

TEST(TaskMapBoth, AllDifferentTypes) {
    auto result = Task<std::string, std::string>::mapBoth<float, int>(
        Task<float, std::string>::pure(1.5f),
        Task<int, std::string>::pure(3),
        [](float a, int b) -> std::string {
            // The output of the product is converted to int so the output string has a reliable length
            return std::to_string(static_cast<int>(a * static_cast<float>(b)));
        },
        Scheduler::global()
    )
    .runSync();

    ASSERT_TRUE(result.is_left());

    auto syncResult = result.get_left();
    ASSERT_TRUE(syncResult.is_left());
    EXPECT_EQ(syncResult.get_left(), "4");
}