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

TEST(ObservableBuffer, SingleValue) {
    auto result = Observable<int>::pure(123)->buffer(10)->last().run(Scheduler::global())->await();

    ASSERT_TRUE(result.has_value());

    BufferRef<int> buffer = *result;
    ASSERT_EQ(buffer->size(), 1);
    EXPECT_EQ((*buffer)[0], 123);
}

TEST(ObservableBuffer, Error) {
    auto result = Observable<int, std::string>::raiseError("broke")
                      ->buffer(10)
                      ->last()
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, "broke");
}

TEST(ObservableBuffer, Cancel) {
    auto deferred = Observable<int>::deferTask([] {
                        return Task<int>::never();
                    })
                        ->buffer(10)
                        ->last()
                        .run(Scheduler::global());

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch (std::runtime_error& error) {
    }
}

TEST(ObservableBuffer, Empty) {
    auto result = Observable<int>::empty()->buffer(10)->last().run(Scheduler::global())->await();

    ASSERT_FALSE(result.has_value());
}

TEST(ObservableBuffer, FiniteUpstream) {
    std::vector<int> values = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    auto result = Observable<int>::fromVector(values)->buffer(3)->take(4).run(Scheduler::global())->await();

    ASSERT_EQ(result.size(), 4);
    ASSERT_EQ(result[0]->size(), 3);
    ASSERT_EQ(result[1]->size(), 3);
    ASSERT_EQ(result[2]->size(), 3);
    ASSERT_EQ(result[3]->size(), 1);
}

TEST(ObservableBuffer, InfiniteUpstream) {
    int counter = 0;
    auto result = Observable<int>::repeatTask(Task<int>::eval([&counter] {
                      return counter++;
                  }))
                      ->buffer(3)
                      ->take(4)
                      .run(Scheduler::global())
                      ->await();

    ASSERT_EQ(result.size(), 4);
    ASSERT_EQ(result[0]->size(), 3);
    ASSERT_EQ(result[1]->size(), 3);
    ASSERT_EQ(result[2]->size(), 3);
    ASSERT_EQ(result[3]->size(), 3);
}

TEST(ObservableBuffer, UpstreamExactlyMatchesBufferSize) {
    std::vector<int> values = {0, 1, 2, 3, 4, 5, 6, 7, 8};

    auto result = Observable<int>::fromVector(values)->buffer(3)->take(4).run(Scheduler::global())->await();

    ASSERT_EQ(result.size(), 3);
    ASSERT_EQ(result[0]->size(), 3);
    ASSERT_EQ(result[1]->size(), 3);
    ASSERT_EQ(result[2]->size(), 3);
}
