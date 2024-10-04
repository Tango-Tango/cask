//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::None;
using cask::Observable;
using cask::Scheduler;
using cask::Task;
using cask::scheduler::BenchScheduler;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::WorkStealingScheduler;

class ObservableLastTest : public ::testing::TestWithParam<std::shared_ptr<Scheduler>> {
protected:

    void SetUp() override {
        sched = GetParam();
    }

    std::shared_ptr<Scheduler> sched;
};

TEST_P(ObservableLastTest, Pure) {
    auto result = Observable<int>::pure(123)
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST_P(ObservableLastTest, Error) {
    auto result = Observable<int,float>::raiseError(1.23)
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, 1.23f);
}

TEST_P(ObservableLastTest, Empty) {
    auto result = Observable<int>::empty()
        ->last()
        .run(sched)
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableLastTest, FiniteValues) {
    std::vector<int> values = {1,2,3,4,5};

    auto result = Observable<int>::fromVector(values)
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 5);
}

TEST_P(ObservableLastTest, MergedFiniteValues) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 15);
}

TEST_P(ObservableLastTest, CompletesGuaranteedEffects) {
    bool completed = false;
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->guarantee(
            Task<None,None>::eval([&completed] {
                completed = true;
                return None();
            }).delay(100)
        )
        ->last()
        .run(sched)
        ->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(completed);
}

TEST(ObservableLastTest, RunsCancelCallbacks) {
    auto sched = std::make_shared<BenchScheduler>();
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto fiber = Observable<int,float>::deferTask([]{
            return Task<int,float>::never();
        })
        ->guarantee(task)
        ->last()
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();
    
    try {
        fiber->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {
        EXPECT_EQ(run_count, 1);
    }
}

INSTANTIATE_TEST_SUITE_P(ObservableLastTest, ObservableLastTest,
    ::testing::Values(
        std::make_shared<SingleThreadScheduler>(),
        std::make_shared<WorkStealingScheduler>(1),
        std::make_shared<WorkStealingScheduler>(2),
        std::make_shared<WorkStealingScheduler>(4),
        std::make_shared<WorkStealingScheduler>(8)
    ),
    [](const ::testing::TestParamInfo<ObservableLastTest::ParamType>& info) {
        return info.param->toString();
    }
);
