//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::BufferRef;
using cask::Observable;
using cask::Scheduler;
using cask::Task;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::WorkStealingScheduler;

class ObservableAppendAllTest : public ::testing::TestWithParam<std::shared_ptr<Scheduler>> {
protected:

    void SetUp() override {
        sched = GetParam();
    }

    std::shared_ptr<Scheduler> sched;
};


TEST_P(ObservableAppendAllTest, SingleValues) {
    auto result = Observable<int>::pure(123)
        ->appendAll(Observable<int>::pure(456))
        ->take(3)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 123);
    EXPECT_EQ(result[1], 456);
}

TEST_P(ObservableAppendAllTest, UpstreamEmpty) {
    auto result = Observable<int>::empty()
        ->appendAll(Observable<int>::pure(456))
        ->take(3)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 456);
}

TEST_P(ObservableAppendAllTest, DownstreamEmpty) {
    auto result = Observable<int>::pure(123)
        ->appendAll(Observable<int>::empty())
        ->take(3)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST_P(ObservableAppendAllTest, BothEmpty) {
    auto result = Observable<int>::empty()
        ->appendAll(Observable<int>::empty())
        ->take(3)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 0);
}

TEST_P(ObservableAppendAllTest, BothErrors) {
    auto result = Observable<int, std::string>::raiseError("upstream")
        ->appendAll(Observable<int, std::string>::raiseError("downstream"))
        ->completed()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "upstream");
}

TEST_P(ObservableAppendAllTest, UpstreamError) {
    auto result = Observable<int, std::string>::raiseError("upstream")
        ->appendAll(Observable<int, std::string>::pure(456))
        ->completed()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "upstream");
}

TEST_P(ObservableAppendAllTest, DownstreamError) {
    auto result = Observable<int, std::string>::pure(123)
        ->appendAll(Observable<int, std::string>::raiseError("downstream"))
        ->completed()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "downstream");
}

TEST_P(ObservableAppendAllTest, UpstreamCancel) {
    int counter = 0;

    auto deferred = Observable<int>::fromTask(Task<int>::never())
        ->appendAll(Observable<int>::pure(456))
        ->foreach([&counter](auto) {
            counter++;
        })
        .run(sched);

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {
        EXPECT_EQ(counter, 0);
    }
}

TEST_P(ObservableAppendAllTest, DownstreamCancel) {
    int counter = 0;

    auto deferred = Observable<int>::empty()
        ->appendAll(Observable<int>::fromTask(Task<int>::never()))
        ->foreach([&counter](auto) {
            counter++;
        })
        .run(sched);

    deferred->cancel();
    
    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {
        EXPECT_EQ(counter, 0);
    }
}

INSTANTIATE_TEST_SUITE_P(ObservableAppendAllTest, ObservableAppendAllTest,
    ::testing::Values(
        std::make_shared<SingleThreadScheduler>(),
        std::make_shared<WorkStealingScheduler>(1),
        std::make_shared<WorkStealingScheduler>(2),
        std::make_shared<WorkStealingScheduler>(4),
        std::make_shared<WorkStealingScheduler>(8)
    ),
    [](const ::testing::TestParamInfo<ObservableAppendAllTest::ParamType>& info) {
        return info.param->toString();
    }
);
