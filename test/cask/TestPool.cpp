//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Erased.hpp"
#include "cask/Pool.hpp"

using cask::Pool;
using cask::pool::BlockPool;

TEST(Pool, Constructs) {
    Pool pool;
}

TEST(Pool, AllocatesAndFrees) {
    Pool pool;

    int* thing = pool.allocate<int>();
    pool.deallocate<int>(thing);
}

TEST(Pool, AllocatesLIFO) {
    Pool pool;

    int* thing1 = pool.allocate<int>();
    pool.deallocate<int>(thing1);

    int* thing2 = pool.allocate<int>();
    pool.deallocate<int>(thing2);

    EXPECT_EQ(thing1, thing2);
}

TEST(Pool, RepeatedlyAllocates) {
    Pool pool;

    int* thing1 = pool.allocate<int>();
    int* thing2 = pool.allocate<int>();
    
    EXPECT_NE(thing1, thing2);

    pool.deallocate<int>(thing1);
    pool.deallocate<int>(thing2);
}

TEST(Pool, AllocatesLotsOfSmallObjects) {
    Pool pool;
    std::deque<int*> allocations;

    for(std::size_t i = 0; i < 2048*1024; i++) {
        allocations.push_back(pool.allocate<int>());
    }

    for(auto& ptr : allocations) {
        pool.deallocate<int>(ptr);
    }
}

