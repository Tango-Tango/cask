//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/None.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include <cstring>

using cask::Deferred;
using cask::None;
using cask::Promise;
using cask::Scheduler;
using cask::Task;
using cask::scheduler::SingleThreadScheduler;
using cask::scheduler::WorkStealingScheduler;

class TaskTest : public ::testing::TestWithParam<std::shared_ptr<Scheduler>> {
protected:

    void SetUp() override {
        sched = GetParam();
    }

    std::shared_ptr<Scheduler> sched;
};

TEST_P(TaskTest, NoneAsync) {
    auto task = Task<int,std::optional<int>>::none();
    auto result = task.run(sched)->await();
    EXPECT_EQ(result, None());
}

TEST_P(TaskTest, PureAsync) {
    auto task = Task<int,None>::pure(123);
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, PureWithoutErrorType) {
    auto task = Task<int>::pure(123);
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, RaiseErrorAsync) {
    auto task = Task<None,int>::raiseError(123);
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, RaiseErrorWithoutErrorType) {
    auto task = Task<None, std::optional<int>>::raiseError(123);
    auto result = task.failed().run(sched);
    EXPECT_EQ(*(result->await()), 123);
}

TEST_P(TaskTest, EvalAsync) {
    auto task = Task<int,None>::eval([](){return 123;});
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, EvalWithoutErrorType) {
    auto task = Task<int>::eval([](){return 123;});
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, DeferAsync) {
    auto task = Task<int,None>::deferAction([](auto){
        return Deferred<int,None>::pure(123);
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, DeferWithoutErrorType) {
    auto task = Task<int>::deferAction([](auto){
        return Deferred<int>::pure(123);
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, DeferError) {
    auto task = Task<None,int>::deferAction([](auto){
        return Deferred<None,int>::raiseError(123);
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, PureMapAsync) {
    auto task = Task<int,None>::pure(123).map<float>([](auto&& value){
        return value * 1.5;
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST_P(TaskTest, PureMapThrowsError) {
    auto task = Task<int,None>::pure(123).map<float>([](auto value){
        throw None();
        return value * 1.5;
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST_P(TaskTest, ErrorMapAsync) {
    auto task = Task<int,None>::raiseError(None()).map<float>([](auto value){
        return value * 1.5;
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST_P(TaskTest, EvalMapAsync) {
    auto task = Task<int,None>::eval([](){return 123;}).map<float>([](auto value){
        return value * 1.5;
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST_P(TaskTest, DeferMapAsync) {
    auto task = Task<int,None>::deferAction([](auto){
            return Deferred<int,None>::pure(123);
        })
        .map<float>([](auto value){
            return value * 1.5;
        });
    
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST_P(TaskTest, PureFlatMapAsync) {
    auto task = Task<int,None>::pure(123).flatMap<float>([](auto value) {
        return Task<float,None>::pure(value * 1.5);
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST_P(TaskTest, PureFlatMapError) {
    auto task = Task<int,None>::pure(123).flatMap<float>([](auto) {
        return Task<float,None>::raiseError(None());
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST_P(TaskTest, PureFlatMapThrowsError) {
    auto task = Task<int,None>::pure(123).flatMap<float>([](auto value) {
        throw None();
        return Task<float,None>::pure(value * 1.5);
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST_P(TaskTest, EvalFlatMap) {
    auto task = Task<int,None>::eval([](){return 123;}).flatMap<float>([](auto value) {
        return Task<float,None>::pure(value * 1.5);
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST_P(TaskTest, ErrorFlatMap) {
    auto task = Task<int,None>::raiseError(None())
        .flatMap<float>([](auto value) {
            return Task<float,None>::pure(value * 1.5);
        });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST_P(TaskTest, MaterializeValueAsync) {
    auto task = Task<int,std::string>::pure(123).materialize();
    auto result = task.run(sched)->await();
    EXPECT_EQ(result.get_left(), 123);
}

TEST_P(TaskTest, MaterializeErrorAsync) {
    auto task = Task<int,std::string>::raiseError("broke").materialize();
    auto result = task.run(sched)->await();
    EXPECT_EQ(result.get_right(), "broke");
}

TEST_P(TaskTest, DematerializeValueAsync) {
    auto task = Task<int,std::string>::pure(123).materialize().dematerialize<int>();
    auto result = task.run(sched)->await();
    EXPECT_EQ(result, 123);
}

TEST_P(TaskTest, DematerializeErrorAsync) {
    auto task = Task<int,std::string>::raiseError("broke").materialize().dematerialize<int>();
    auto result = task.failed().run(sched)->await();
    EXPECT_EQ(result, "broke");
}

TEST_P(TaskTest, PureDelayAsync) {
    auto task = Task<int,None>::pure(123).delay(1);
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, EvalDelayAsync) {
    auto task = Task<int,None>::eval([](){return 123;}).delay(1);
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, PureRestartUntil) {
    auto task = Task<int,None>::pure(123).restartUntil([](auto){return true;});
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST_P(TaskTest, EvalRestartUntilAsync) {
    auto value = 0;

    auto task = Task<int,None>::eval([&value]() {
            value++;
            return value;
        }).restartUntil([](auto currentValue){
            return currentValue >= 10;
        });
    
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 10);
    EXPECT_EQ(value, 10);
}

TEST_P(TaskTest, RecurseWithoutExploding) {
    std::function<Task<int>(const int&)> recurse = [&recurse](auto counter) {
        return Task<int>::defer([counter, &recurse]() {
            char yuge[8192];
            memset(yuge, 0, 8192);

            if(counter > 0) {
                return recurse(counter - 1);
            } else  {
                return Task<int>::pure(counter);
            }
        });
    };

    auto task = Task<int>::pure(1000).flatMap<int>(recurse);
    auto result = task.run(sched)->await();
    EXPECT_EQ(result, 0);
}

TEST_P(TaskTest, OnErrorSuccess) {
    int calls = 0;
    auto result = Task<int,None>::pure(123)
    .onError([&calls](auto) {
        calls++;
    })
    .run(sched)
    ->await();

    EXPECT_EQ(result, 123);
    EXPECT_EQ(calls, 0);
}

TEST_P(TaskTest, OnErrorNormal) {
    int calls = 0;
    auto result = Task<int,None>::raiseError(None())
    .onError([&calls](auto) {
        calls++;
    })
    .failed()
    .run(sched)
    ->await();

    EXPECT_EQ(result, None());
    EXPECT_EQ(calls, 1);
}


TEST_P(TaskTest, OnErrorThrows) {
    int calls = 0;
    auto result = Task<int,float>::raiseError(1.5f)
    .onError([&calls](auto) {
        calls++;
        throw 2.5f;
    })
    .failed()
    .run(sched)
    ->await();

    EXPECT_EQ(result, 2.5f);
    EXPECT_EQ(calls, 1);
}

TEST_P(TaskTest, MapError) {
    auto result = Task<int,int>::raiseError(123)
    .mapError<float>([](auto input) { return input *1.5f; })
    .failed()
    .run(sched)
    ->await();

    EXPECT_EQ(result, 184.5);
}

INSTANTIATE_TEST_SUITE_P(TaskTest, TaskTest,
    ::testing::Values(
        std::make_shared<SingleThreadScheduler>(),
        std::make_shared<WorkStealingScheduler>(1),
        std::make_shared<WorkStealingScheduler>(2),
        std::make_shared<WorkStealingScheduler>(4),
        std::make_shared<WorkStealingScheduler>(8)
    ),
    [](const ::testing::TestParamInfo<TaskTest::ParamType>& info) {
        return info.param->toString();
    }
);