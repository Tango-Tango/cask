//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"

using cask::Observable;
using cask::Scheduler;
using cask::Task;

TEST(ObservableRepeatTask, Value) {
    auto counter = 0;
    auto task = Task<int>::eval([&counter] {
        return counter++;
    });
    auto result = Observable<int>::repeatTask(task)
        ->take(10)
        .run(Scheduler::global())
        ->await();

    ASSERT_EQ(result.size(), 10);
    for(unsigned int i = 0; i < 10; i++) {
        EXPECT_EQ(result[i], i);
    }
}

TEST(ObservableRepeatTask, Error) {
    auto task = Task<int,std::string>::raiseError("broke");
    auto result = Observable<int,std::string>::repeatTask(task)
        ->take(10)
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, "broke");
}
