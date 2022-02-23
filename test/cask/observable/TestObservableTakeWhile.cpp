//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/None.hpp"
#include "cask/Observable.hpp"
#include "gtest/gtest.h"

using cask::None;
using cask::Observable;
using cask::Scheduler;
using cask::Task;

TEST(ObservableTakeWhile, Error) {
    auto result = Observable<int, float>::raiseError(1.23)
                      ->takeWhile([](auto) {
                          return true;
                      })
                      ->last()
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, 1.23f);
}

TEST(ObservableTakeWhile, Empty) {
    auto result = Observable<int>::empty()
                      ->takeWhile([](auto) {
                          return true;
                      })
                      ->last()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableTakeWhile, Infinite) {
    int counter = 0;
    auto result = Observable<int>::repeatTask(Task<int>::eval([&counter] {
                      counter++;
                      return counter;
                  }))
                      ->takeWhile([](auto value) {
                          return value < 10;
                      })
                      ->take(100)
                      .run(Scheduler::global())
                      ->await();

    ASSERT_EQ(result.size(), 9);
    for (unsigned int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], i + 1);
    }
}

TEST(ObservableTakeWhile, Finite) {
    auto result = Observable<int>::pure(123)
                      ->takeWhile([](auto) {
                          return true;
                      })
                      ->take(100)
                      .run(Scheduler::global())
                      ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST(ObservableTakeWhile, CompletesOnlyOnce) {
    auto result = Observable<int>::deferTask([] {
                      return Task<int>::pure(123);
                  })
                      ->takeWhile([](auto) {
                          return false;
                      })
                      ->last()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableTakeWhile, RunsCompletionTask) {
    auto shutdown_counter = 0;

    auto result = Observable<int>::pure(123)
                      ->takeWhile([](auto) {
                          return false;
                      })
                      ->guarantee(Task<None, None>::eval([&shutdown_counter] {
                          shutdown_counter++;
                          return None();
                      }))
                      ->take(100)
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result.size(), 0);
    EXPECT_EQ(shutdown_counter, 1);
}

TEST(ObservableTakeWhile, RunsErrorTask) {
    auto shutdown_counter = 0;

    auto result = Observable<int, std::string>::raiseError("broke")
                      ->takeWhile([](auto) {
                          return false;
                      })
                      ->guarantee(Task<None, None>::eval([&shutdown_counter] {
                          shutdown_counter++;
                          return None();
                      }))
                      ->take(100)
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, "broke");
    EXPECT_EQ(shutdown_counter, 1);
}
