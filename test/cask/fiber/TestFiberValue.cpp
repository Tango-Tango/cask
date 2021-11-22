//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/fiber/FiberValue.hpp"

using cask::Erased;
using cask::fiber::FiberValue;

TEST(FiberValue, Empty) {
    FiberValue value;

    EXPECT_FALSE(value.isValue());
    EXPECT_FALSE(value.isError());
    EXPECT_FALSE(value.isCanceled());
}

TEST(FiberValue, ErasedCopyValue) {
    Erased erased = 123;
    FiberValue value(erased, false, false);

    EXPECT_TRUE(value.isValue());
    EXPECT_FALSE(value.isError());
    EXPECT_FALSE(value.isCanceled());

    EXPECT_EQ(value.getValue()->template get<int>(), 123);
    EXPECT_EQ(value.underlying().template get<int>(), 123);
}

TEST(FiberValue, ErasedCopyError) {
    Erased erased = 123;
    FiberValue value(erased, true, false);

    EXPECT_FALSE(value.isValue());
    EXPECT_TRUE(value.isError());
    EXPECT_FALSE(value.isCanceled());

    EXPECT_EQ(value.getError()->template get<int>(), 123);
    EXPECT_EQ(value.underlying().template get<int>(), 123);
}

TEST(FiberValue, ErasedCopyCancel) {
    Erased erased;
    FiberValue value(erased, false, true);

    EXPECT_FALSE(value.isValue());
    EXPECT_FALSE(value.isError());
    EXPECT_TRUE(value.isCanceled());
}

TEST(FiberValue, ErasedMoveValue) {
    FiberValue value(Erased(123), false, false);

    EXPECT_TRUE(value.isValue());
    EXPECT_FALSE(value.isError());
    EXPECT_FALSE(value.isCanceled());

    EXPECT_EQ(value.getValue()->template get<int>(), 123);
    EXPECT_EQ(value.underlying().template get<int>(), 123);
}

TEST(FiberValue, ErasedMoveError) {
    FiberValue value(Erased(123), true, false);

    EXPECT_FALSE(value.isValue());
    EXPECT_TRUE(value.isError());
    EXPECT_FALSE(value.isCanceled());

    EXPECT_EQ(value.getError()->template get<int>(), 123);
    EXPECT_EQ(value.underlying().template get<int>(), 123);
}

TEST(FiberValue, ErasedMoveCancel) {
    FiberValue value(Erased(), false, true);

    EXPECT_FALSE(value.isValue());
    EXPECT_FALSE(value.isError());
    EXPECT_TRUE(value.isCanceled());
}
