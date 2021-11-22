//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include <exception>

using cask::Task;

TEST(TaskFlatMapBoth, ValueSameErrorTypes) {
    auto result = Task<int,std::string>::pure(123)
        .template flatMapBoth<float,std::string>(
            [](auto value) {
                return Task<float,std::string>::pure(value * 1.5);
            },
            [](auto error) {
                return Task<float,std::string>::raiseError(error + "-mapped");
            }
        )
        .runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_left());
    EXPECT_EQ(result->get_left(), 184.5f);
}

TEST(TaskFlatMapBoth, ValueDifferentErrorTypes) {
    auto result = Task<int,std::string>::pure(123)
        .template flatMapBoth<float,std::runtime_error>(
            [](auto value) {
                return Task<float,std::runtime_error>::pure(value * 1.5);
            },
            [](auto error) {
                return Task<float,std::runtime_error>::raiseError(std::runtime_error(error));
            }
        )
        .runSync();
    
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_left());
    EXPECT_EQ(result->get_left(), 184.5f);
}

TEST(TaskFlatMapBoth, ErrorSameErrorTypes) {
    auto result = Task<int,std::string>::raiseError("broke")
        .template flatMapBoth<float,std::string>(
            [](auto value) {
                return Task<float,std::string>::pure(value * 1.5);
            },
            [](auto error) {
                return Task<float,std::string>::raiseError(error + "-mapped");
            }
        )
        .runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());
    EXPECT_EQ(result->get_right(), "broke-mapped");
}

TEST(TaskFlatMapBoth, ErrorDifferentErrorTypes) {
    auto result = Task<int,std::string>::raiseError("broke")
        .template flatMapBoth<float,std::runtime_error>(
            [](auto value) {
                return Task<float,std::runtime_error>::pure(value * 1.5);
            },
            [](auto error) {
                return Task<float,std::runtime_error>::raiseError(std::runtime_error(error));
            }
        )
        .runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());
    EXPECT_EQ(result->get_right().what(), std::string("broke"));
}

TEST(TaskFlatMapBoth, ThrowsSameErrorTypes) {
    auto result = Task<int,std::string>::pure(123)
        .template flatMapBoth<float,std::string>(
            [](auto) -> Task<float,std::string> {
                throw std::string("thrown-error");
            },
            [](auto error)  {
                return Task<float,std::string>::raiseError(error + "-mapped");
            }
        )
        .runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());
    EXPECT_EQ(result->get_right(), "thrown-error-mapped");
}

TEST(TaskFlatMapBoth, ThrowsUpstreamErrorType) {
    auto result = Task<int,std::string>::pure(123)
        .template flatMapBoth<float,std::runtime_error>(
            [](auto) -> Task<float,std::runtime_error> {
                throw std::string("thrown-error");
            },
            [](auto error) {
                return Task<float,std::runtime_error>::raiseError(std::runtime_error(error));
            }
        )
        .runSync();


    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());
    EXPECT_EQ(result->get_right().what(), std::string("thrown-error"));
}

TEST(TaskFlatMapBoth, ThrowsDownstreamErrorType) {
    auto result = Task<int,std::string>::pure(123)
        .template flatMapBoth<float,std::runtime_error>(
            [](auto) -> Task<float,std::runtime_error> {
                throw std::runtime_error("thrown-error");
            },
            [](auto error) {
                return Task<float,std::runtime_error>::raiseError(std::runtime_error(error));
            }
        )
        .runSync();

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_right());
    EXPECT_EQ(result->get_right().what(), std::string("thrown-error"));
}

