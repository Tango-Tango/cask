//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Observable.hpp"
#include "gtest/gtest.h"

using cask::Observable;
using cask::Scheduler;
using cask::Task;

TEST(ObservableFromTask, Value) {
    auto result = Observable<int>::fromTask(Task<int>::pure(123))->last().run(Scheduler::global())->await();

    EXPECT_EQ(result, 123);
}

TEST(ObservableFromTask, Error) {
    auto result = Observable<int, float>::fromTask(Task<int, float>::raiseError(1.23))
                      ->last()
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, 1.23f);
}