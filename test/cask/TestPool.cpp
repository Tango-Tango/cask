//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Erased.hpp"
#include "cask/Pool.hpp"

using cask::Pool;

TEST(Pool, Constructs) {
    Pool<128> pool;
}

TEST(Pool, AllocatesAndFrees) {
    Pool<128> pool;

    int* thing = pool.allocate<int>();
    pool.deallocate<int>(thing);
}

TEST(Pool, AllocatesLIFO) {
    Pool<128> pool;

    int* thing1 = pool.allocate<int>();
    pool.deallocate<int>(thing1);

    int* thing2 = pool.allocate<int>();
    pool.deallocate<int>(thing2);

    EXPECT_EQ(thing1, thing2);
}

TEST(Pool, AllocatesLargeObjectOnHeap) {
    Pool<1> pool;

    int* thing1 = pool.allocate<int>();
    pool.deallocate<int>(thing1);

    int* thing2 = pool.allocate<int>();
    pool.deallocate<int>(thing2);

    EXPECT_NE(thing1, thing2);
}

TEST(Pool, Preallocates) {
    Pool<128> pool(32);

    int* thing1 = pool.allocate<int>();
    pool.deallocate<int>(thing1);
}

TEST(Pool, RepeatedlyAllocates) {
    Pool<128> pool;

    int* thing1 = pool.allocate<int>();
    int* thing2 = pool.allocate<int>();
    
    EXPECT_NE(thing1, thing2);

    pool.deallocate<int>(thing1);
    pool.deallocate<int>(thing2);
}

