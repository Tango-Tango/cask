//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Either.hpp"

using cask::Either;

TEST(Either, Left) {
    auto either = Either<int,float>::left(123);

    EXPECT_TRUE(either.is_left());
    EXPECT_FALSE(either.is_right());
    EXPECT_EQ(either.get_left(), 123);
}

TEST(Either, Right) {
    auto either = Either<int,double>::right(4.56);

    EXPECT_FALSE(either.is_left());
    EXPECT_TRUE(either.is_right());
    EXPECT_EQ(either.get_right(), 4.56);
}
