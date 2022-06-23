//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)


#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/scheduler/ThreadPoolScheduler.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"

using cask::Observable;
using cask::scheduler::ThreadPoolScheduler;
using cask::scheduler::SingleThreadScheduler;

class ObservableMergeAllTest : public ::testing::TestWithParam<std::shared_ptr<cask::Scheduler>> {
protected:
    std::shared_ptr<cask::Scheduler> sched;
    
    void SetUp() override {
        sched = GetParam();
    }
};

TEST_P(ObservableMergeAllTest,Empty) {
    auto fiber = Observable<int,std::string>::mergeAll({
            Observable<int,std::string>::empty(),
            Observable<int,std::string>::empty()
        })
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableMergeAllTest,StackedEmpties) {
    auto fiber = Observable<int,std::string>::mergeAll({
            Observable<int,std::string>::empty(),
            Observable<int,std::string>::empty(),
            Observable<int,std::string>::empty()
        })
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableMergeAllTest,LeftValue) {
    auto fiber = Observable<int,std::string>::mergeAll({
            Observable<int,std::string>::pure(123),
            Observable<int,std::string>::empty()
        })
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST_P(ObservableMergeAllTest,RightValue) {
    auto fiber = Observable<int,std::string>::mergeAll({
            Observable<int,std::string>::empty(),
            Observable<int,std::string>::pure(123)
        })
        ->last()
        .run(sched);

    auto result = fiber->await();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST_P(ObservableMergeAllTest,Never) {
    auto fiber = Observable<int,std::string>::mergeAll({
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
        })
        ->last()
        .run(sched);

    fiber->cancel();

    try {
        fiber->await();
        FAIL() << "Expected fiber to cancel";
    } catch(std::runtime_error&) {}
}

INSTANTIATE_TEST_SUITE_P(ObservableMergeAllSingleThread, ObservableMergeAllTest, ::testing::Values(
    std::make_shared<SingleThreadScheduler>()
));

INSTANTIATE_TEST_SUITE_P(ObservableMergeAllThreaded, ObservableMergeAllTest, ::testing::Values(
    std::make_shared<ThreadPoolScheduler>(1),
    std::make_shared<ThreadPoolScheduler>(2),
    std::make_shared<ThreadPoolScheduler>(4),
    std::make_shared<ThreadPoolScheduler>(8)
));
