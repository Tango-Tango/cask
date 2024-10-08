//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"

using cask::Observable;
using cask::scheduler::WorkStealingScheduler;
using cask::scheduler::SingleThreadScheduler;

class ObservableMergeTest : public ::testing::TestWithParam<int> {
protected:
    std::shared_ptr<cask::Scheduler> sched;
    
    void SetUp() override {
        auto num_threads = GetParam();
        if (num_threads <= 1) {
            sched = std::make_shared<SingleThreadScheduler>();
        } else {
            sched = std::make_shared<WorkStealingScheduler>(num_threads);
        }
    }
};

TEST_P(ObservableMergeTest,Empty) {
    auto fiber = Observable<int,std::string>::empty()
        ->merge(Observable<int,std::string>::empty())
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableMergeTest,StackedEmpties) {
    auto fiber = Observable<int,std::string>::empty()
        ->merge(Observable<int,std::string>::empty())
        ->merge(Observable<int,std::string>::empty())
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableMergeTest,LeftValue) {
    auto fiber = Observable<int,std::string>::pure(123)
        ->merge(Observable<int,std::string>::empty())
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST_P(ObservableMergeTest,RightValue) {
    auto fiber = Observable<int,std::string>::empty()
        ->merge(Observable<int,std::string>::pure(123))
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST_P(ObservableMergeTest,LeftError) {
    auto fiber = Observable<int,std::string>::raiseError("broke")
        ->merge(Observable<int,std::string>::empty())
        ->last()
        .failed()
        .run(sched);

    auto result = fiber->await();
    EXPECT_EQ(result, "broke");
}

TEST_P(ObservableMergeTest,RightError) {
    auto fiber = Observable<int,std::string>::empty()
        ->merge(Observable<int,std::string>::raiseError("broke"))
        ->last()
        .failed()
        .run(sched);

    auto result = fiber->await();
    EXPECT_EQ(result, "broke");
}

TEST_P(ObservableMergeTest,Never) {
    auto fiber = Observable<int,std::string>::never()
        ->merge(Observable<int,std::string>::never())
        ->last()
        .run(sched);

    fiber->cancel();

    try {
        fiber->await();
        FAIL() << "Expected fiber to cancel";
    } catch(std::runtime_error&) {}
}

TEST_P(ObservableMergeTest,DownstreamLeftStop) {
    auto fiber = Observable<int,std::string>::pure(123)
        ->merge(Observable<int,std::string>::never())
        ->take(1)
        .run(sched);
    
    auto result = fiber->await();
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST_P(ObservableMergeTest,DownstreamRightStop) {
    auto fiber = Observable<int,std::string>::never()
        ->merge(Observable<int,std::string>::pure(123))
        ->take(1)
        .run(sched);
    
    auto result = fiber->await();
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST_P(ObservableMergeTest,DownstreamBothStop) {
    auto fiber = Observable<int,std::string>::sequence(1,2,3)
        ->merge(Observable<int,std::string>::sequence(4,5,6))
        ->take(4)
        .run(sched);
    
    auto result = fiber->await();
    EXPECT_EQ(result.size(), 4);
}

TEST_P(ObservableMergeTest,DownstreamManyStop) {
    auto fiber = Observable<int,std::string>::sequence(1,2,3)
        ->merge(Observable<int,std::string>::sequence(4,5,6))
        ->merge(Observable<int,std::string>::sequence(7,8,9))
        ->take(7)
        .run(sched);
    
    auto result = fiber->await();
    EXPECT_EQ(result.size(), 7);
}

TEST_P(ObservableMergeTest,DownstreamManyTakeAll) {
    auto fiber = Observable<int,std::string>::sequence(1,2,3)
        ->merge(Observable<int,std::string>::sequence(4,5,6))
        ->merge(Observable<int,std::string>::sequence(7,8,9))
        ->take(10)
        .run(sched);
    
    auto result = fiber->await();
    EXPECT_EQ(result.size(), 9);
}

TEST_P(ObservableMergeTest,EmptyNeverValue) {
    auto fiber = Observable<int,std::string>::empty()
        ->merge(Observable<int,std::string>::never())
        ->merge(Observable<int,std::string>::pure(123))
        ->take(1)
        .run(sched);

    auto result = fiber->await();
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

INSTANTIATE_TEST_SUITE_P(ObservableMerge, ObservableMergeTest, ::testing::Values(
    1,2,3,8
));

