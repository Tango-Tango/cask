//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include "SchedulerTestBench.hpp"

using cask::None;
using cask::Observable;
using cask::Scheduler;
using cask::Task;
using cask::scheduler::BenchScheduler;

INSTANTIATE_SCHEDULER_TEST_BENCH_SUITE(ObservableTakeTest);

TEST_P(ObservableTakeTest, ErrorTakeNothing) {
    auto result = Observable<int,float>::raiseError(1.23)
        ->take(0)
        .run(sched)
        ->await();

    EXPECT_TRUE(result.empty());
}

TEST_P(ObservableTakeTest, ErrorTakeOne) {
    auto result = Observable<int,float>::raiseError(1.23)
        ->take(1)
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, 1.23f);
}

TEST_P(ObservableTakeTest, PureTakeNothing) {
    auto result = Observable<int>::pure(123)
        ->take(0)
        .run(sched)
        ->await();

    EXPECT_TRUE(result.empty());
}

TEST_P(ObservableTakeTest, PureTakeOne) {
    auto result = Observable<int>::pure(123)
        ->take(1)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST_P(ObservableTakeTest, PureTakeMultiple) {
    auto result = Observable<int>::pure(123)
        ->take(2)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST_P(ObservableTakeTest, VectorTakeNothing) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(0)
        .run(sched)
        ->await();

    EXPECT_TRUE(result.empty());
}

TEST_P(ObservableTakeTest, VectorTakeOne) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(1)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 1);
}

TEST_P(ObservableTakeTest, VectorTakeMulti) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(3)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
}


TEST_P(ObservableTakeTest, VectorTakeAll) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(5)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 4);
    EXPECT_EQ(result[4], 5);
}

TEST_P(ObservableTakeTest, VectorTakeMoreThanAll) {
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->take(6)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 4);
    EXPECT_EQ(result[4], 5);
}

TEST_P(ObservableTakeTest, VectorTakeMergedNone) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->take(0)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 0);
}

TEST_P(ObservableTakeTest, VectorTakeMergedMulti) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->take(6)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 6);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 4);
    EXPECT_EQ(result[4], 5);
    EXPECT_EQ(result[5], 6);
}

TEST_P(ObservableTakeTest, VectorTakeMergedAll) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->take(15)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 15);
    for(unsigned int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], i+1);
    }
}

TEST_P(ObservableTakeTest, VectorTakeMergedMoreThanAll) {
    std::vector<int> first_values = {1,2,3,4,5};
    std::vector<int> second_values = {6,7,8,9,10};
    std::vector<int> third_values = {11,12,13,14,15};
    std::vector<std::vector<int>> all_values = {first_values, second_values, third_values};

    auto result = Observable<std::vector<int>>::fromVector(all_values)
        ->template flatMap<int>([](auto vector) { return Observable<int>::fromVector(vector); })
        ->take(16)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 15);
    for(unsigned int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], i+1);
    }
}

TEST_P(ObservableTakeTest, CompletesGuaranteedEffects) {
    bool completed = false;
    std::vector<int> values = {1,2,3,4,5};
    auto result = Observable<int>::fromVector(values)
        ->guarantee(
            Task<None,None>::eval([&completed] {
                completed = true;
                return None();
            }).delay(100)
        )
        ->take(1)
        .run(sched)
        ->await();

    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(completed);
}


TEST(ObservableTakeTest, RunsCancelCallbacks) {
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
        ->take(10)
        .failed()
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
