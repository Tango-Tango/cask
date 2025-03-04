//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"

using cask::Observable;
using cask::scheduler::WorkStealingScheduler;
using cask::scheduler::SingleThreadScheduler;

class ObservableMergeAllTest : public ::testing::TestWithParam<int> {
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

TEST_P(ObservableMergeAllTest,Empty) {
    auto fiber = Observable<int,std::string>::mergeAll(
            Observable<int,std::string>::empty(),
            Observable<int,std::string>::empty()
        )
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableMergeAllTest,StackedEmpties) {
    auto fiber = Observable<int,std::string>::mergeAll(
            Observable<int,std::string>::empty(),
            Observable<int,std::string>::empty(),
            Observable<int,std::string>::empty()
        )
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableMergeAllTest,LeftValue) {
    auto fiber = Observable<int,std::string>::mergeAll(
            Observable<int,std::string>::pure(123),
            Observable<int,std::string>::empty()
        )
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST_P(ObservableMergeAllTest,RightValue) {
    auto fiber = Observable<int,std::string>::mergeAll(
            Observable<int,std::string>::empty(),
            Observable<int,std::string>::pure(123)
        )
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST_P(ObservableMergeAllTest,Never) {
    auto fiber = Observable<int,std::string>::mergeAll(
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never(),
            Observable<int,std::string>::never()
        )
        ->last()
        .run(sched);

    fiber->cancel();

    try {
        fiber->await();
        FAIL() << "Expected fiber to cancel";
    } catch(std::runtime_error&) {}  // NOLINT(bugprone-empty-catch)
}

INSTANTIATE_TEST_SUITE_P(ObservableMergeAll, ObservableMergeAllTest, ::testing::Values(
    1,2,3,8
));

