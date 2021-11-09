//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Fiber.hpp"

using cask::Fiber;
using cask::Erased;
using cask::FiberOp;
using cask::None;

TEST(TestFiber, Constructs) {
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>(op);

    EXPECT_EQ(fiber.getState(), cask::READY);
    EXPECT_FALSE(fiber.getValue().has_value());
    EXPECT_FALSE(fiber.getError().has_value());
}

TEST(TestFiber, EvaluatesPureValue) {
    auto op = FiberOp::value(123);
    auto fiber = Fiber<int,std::string>(op);

    EXPECT_TRUE(fiber.resume());
    EXPECT_FALSE(fiber.resume());

    EXPECT_EQ(fiber.getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber.getValue().has_value());
    EXPECT_FALSE(fiber.getError().has_value());
    EXPECT_EQ(*(fiber.getValue()), 123);
}

TEST(TestFiber, SuspendsAtAsyncBoundary) {
    auto op = FiberOp::async(None());
    auto fiber = Fiber<int,std::string>(op);

    EXPECT_TRUE(fiber.resume());
    EXPECT_FALSE(fiber.resume());

    EXPECT_EQ(fiber.getState(), cask::WAITING);
    EXPECT_FALSE(fiber.getValue().has_value());
    EXPECT_FALSE(fiber.getError().has_value());
}

TEST(TestFiber, ResumesAfterAsyncBoundary) {
    auto op = FiberOp::async(None());
    auto fiber = Fiber<int,std::string>(op);

    EXPECT_TRUE(fiber.resume());
    EXPECT_FALSE(fiber.resume());
    
    fiber.asyncSuccess(123);

    EXPECT_EQ(fiber.getState(), cask::COMPLETED);
    EXPECT_TRUE(fiber.getValue().has_value());
    EXPECT_FALSE(fiber.getError().has_value());
    EXPECT_EQ(*(fiber.getValue()), 123);
}

