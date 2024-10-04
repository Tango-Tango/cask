//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"
#include "cask/None.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"

using cask::Observable;
using cask::Scheduler;
using cask::Task;
using cask::None;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::WorkStealingScheduler;

class ObservableTakeWhileTest : public ::testing::TestWithParam<std::shared_ptr<Scheduler>> {
protected:

    void SetUp() override {
        sched = GetParam();
    }

    std::shared_ptr<Scheduler> sched;
};

TEST_P(ObservableTakeWhileTest, Error) {
    auto result = Observable<int,float>::raiseError(1.23)
        ->takeWhile([](auto) {
            return true;
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, 1.23f);
}

TEST_P(ObservableTakeWhileTest, Empty) {
    auto result = Observable<int>::empty()
        ->takeWhile([](auto) {
            return true;
        })
        ->last()
        .run(sched)
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableTakeWhileTest, Infinite) {
    int counter = 0;
    auto result = Observable<int>::repeatTask(Task<int>::eval([&counter] {
            counter++;
            return counter;
        }))
        ->takeWhile([](auto value) {
            return value < 10;
        })
        ->take(100)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 9);
    for(unsigned int i = 0; i < result.size(); i++) {
        EXPECT_EQ(result[i], i+1);
    }
}

TEST_P(ObservableTakeWhileTest, Finite) {
    auto result = Observable<int>::pure(123)
        ->takeWhile([](auto) {
            return true;
        })
        ->take(100)
        .run(sched)
        ->await();

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 123);
}

TEST_P(ObservableTakeWhileTest, CompletesOnlyOnce) {
    auto result = Observable<int>::deferTask([]{
            return Task<int>::pure(123);
        })
        ->takeWhile([](auto) {
            return false;
        })
        ->last()
        .run(sched)
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST_P(ObservableTakeWhileTest, RunsCompletionTask) {
    auto shutdown_counter = 0;

    auto result = Observable<int>::pure(123)
        ->takeWhile([](auto) {
            return false;
        })
        ->guarantee(Task<None,None>::eval([&shutdown_counter] {
            shutdown_counter++;
            return None();

        }))
        ->take(100)
        .run(sched)
        ->await();

    EXPECT_EQ(result.size(), 0);
    EXPECT_EQ(shutdown_counter, 1);
}

TEST_P(ObservableTakeWhileTest, RunsErrorTask) {
    auto shutdown_counter = 0;

    auto result = Observable<int,std::string>::raiseError("broke")
        ->takeWhile([](auto) {
            return false;
        })
        ->guarantee(Task<None,None>::eval([&shutdown_counter] {
            shutdown_counter++;
            return None();

        }))
        ->take(100)
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
    EXPECT_EQ(shutdown_counter, 1);
}

INSTANTIATE_TEST_SUITE_P(ObservableTakeWhileTest, ObservableTakeWhileTest,
    ::testing::Values(
        std::make_shared<SingleThreadScheduler>(),
        std::make_shared<WorkStealingScheduler>(1),
        std::make_shared<WorkStealingScheduler>(2),
        std::make_shared<WorkStealingScheduler>(4),
        std::make_shared<WorkStealingScheduler>(8)
    ),
    [](const ::testing::TestParamInfo<ObservableTakeWhileTest::ParamType>& info) {
        return info.param->toString();
    }
);
