//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <random>
#include <thread>
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

    for(std::size_t i = 0; i < 100000; i++) {
        int* thing = pool.allocate<int>();
        pool.deallocate<int>(thing);
    }
}

TEST(Pool, AllocatesLotsOfSmallObjects) {
    Pool pool;

    for(std::size_t i = 0; i < 10000; i++) {
        std::vector<int*> allocations;

        for(std::size_t i = 0; i < 32; i++) {
            allocations.push_back(pool.allocate<int>());
        }

        std::shuffle(allocations.begin(), allocations.end(), std::default_random_engine());

        for(auto& ptr : allocations) {
            pool.deallocate<int>(ptr);
        }
    }
}

TEST(Pool, RepeatedlyAllocatesParallel) {
    Pool pool;
    std::vector<std::thread> threads;

    for(std::size_t i = 0; i < 32; i++) {
        threads.emplace_back([&pool] {
            for(std::size_t i = 0; i < 10000; i++) {
                int* thing = pool.allocate<int>();
                pool.deallocate<int>(thing);
            }
        });
    }

    for(auto& thread : threads) {
        thread.join();
    }
}
