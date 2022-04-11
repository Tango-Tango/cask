//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Erased.hpp"
#include "cask/Pool.hpp"

using cask::Pool;

TEST(Pool, Constructs) {
    Pool pool;
}

TEST(Pool, AllocatesAndFrees) {
    Pool pool;

    int* thing = pool.allocate<int>();
    pool.deallocate<int>(thing);
}