//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/None.hpp"
#include "cask/Observable.hpp"
#include "gtest/gtest.h"

using cask::Observable;
using cask::Scheduler;
using cask::Task;

TEST(ObservableFilter, FilterMatch) {
    auto result = Observable<int>::pure(123)
                      ->filter([](auto value) {
                          return value == 123;
                      })
                      ->last()
                      .run(Scheduler::global())
                      ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST(ObservableFilter, FilterDoesntMatch) {
    auto result = Observable<int>::pure(123)
                      ->filter([](auto value) {
                          return value != 123;
                      })
                      ->last()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableFilter, FilterError) {
    auto result = Observable<int, std::string>::raiseError("broked")
                      ->filter([](auto value) {
                          return value == 123;
                      })
                      ->last()
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, "broked");
}

TEST(ObservableFilter, Cancelled) {
    auto deferred = Observable<int>::deferTask([] {
                        return Task<int>::never();
                    })
                        ->filter([](auto value) {
                            return value == 123;
                        })
                        ->last()
                        .run(Scheduler::global());

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch (std::runtime_error&) {
    }
}

TEST(ObservableFilter, StopsUpstream) {
    auto counter = 0;
    auto result = Observable<int>::repeatTask(Task<int>::eval([&counter] {
                      return counter++;
                  }))
                      ->filter([](auto value) {
                          return value >= 10 && value < 20;
                      })
                      ->take(10)
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(counter, 20);
    ASSERT_EQ(result.size(), 10);
    for (unsigned int i = 0; i < 10; i++) {
        EXPECT_EQ(result[i], i + 10);
    }
}
