//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/None.hpp"
#include "cask/Observable.hpp"
#include "gtest/gtest.h"

using cask::BufferRef;
using cask::Observable;
using cask::Scheduler;
using cask::Task;

TEST(ObservableConcat, SingleValues) {
    auto result =
        Observable<int>::pure(123)->concat(Observable<int>::pure(456))->take(3).run(Scheduler::global())->await();

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 123);
    EXPECT_EQ(result[1], 456);
}

TEST(ObservableConcat, UpstreamEmpty) {
    auto result =
        Observable<int>::empty()->concat(Observable<int>::pure(456))->take(3).run(Scheduler::global())->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 456);
}

TEST(ObservableConcat, DownstreamEmpty) {
    auto result =
        Observable<int>::pure(123)->concat(Observable<int>::empty())->take(3).run(Scheduler::global())->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST(ObservableConcat, BothEmpty) {
    auto result = Observable<int>::empty()->concat(Observable<int>::empty())->take(3).run(Scheduler::global())->await();

    ASSERT_EQ(result.size(), 0);
}

TEST(ObservableConcat, BothErrors) {
    auto result = Observable<int, std::string>::raiseError("upstream")
                      ->concat(Observable<int, std::string>::raiseError("downstream"))
                      ->completed()
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, "upstream");
}

TEST(ObservableConcat, UpstreamError) {
    auto result = Observable<int, std::string>::raiseError("upstream")
                      ->concat(Observable<int, std::string>::pure(456))
                      ->completed()
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, "upstream");
}

TEST(ObservableConcat, DownstreamError) {
    auto result = Observable<int, std::string>::pure(123)
                      ->concat(Observable<int, std::string>::raiseError("downstream"))
                      ->completed()
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, "downstream");
}

TEST(ObservableConcat, UpstreamCancel) {
    int counter = 0;

    auto deferred = Observable<int>::fromTask(Task<int>::never())
                        ->concat(Observable<int>::pure(456))
                        ->foreach ([&counter](auto) {
                            counter++;
                        })
                        .run(Scheduler::global());

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch (std::runtime_error&) {
        EXPECT_EQ(counter, 0);
    }
}

TEST(ObservableConcat, DownstreamCancel) {
    int counter = 0;

    auto deferred = Observable<int>::empty()
                        ->concat(Observable<int>::fromTask(Task<int>::never()))
                        ->foreach ([&counter](auto) {
                            counter++;
                        })
                        .run(Scheduler::global());

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch (std::runtime_error&) {
        EXPECT_EQ(counter, 0);
    }
}
