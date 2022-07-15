//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/None.hpp"
#include <cstring>

using cask::Deferred;
using cask::None;
using cask::Promise;
using cask::Scheduler;
using cask::Task;

TEST(Task, NoneAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,std::optional<int>>::none();
    auto result = task.run(sched)->await();
    EXPECT_EQ(result, None());
}

TEST(Task, PureAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123);
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, PureWithoutErrorType) {
    auto sched = Scheduler::global();
    auto task = Task<int>::pure(123);
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, RaiseErrorAsync) {
    auto sched = Scheduler::global();
    auto task = Task<None,int>::raiseError(123);
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, RaiseErrorWithoutErrorType) {
    auto sched = Scheduler::global();
    auto task = Task<None, std::optional<int>>::raiseError(123);
    auto result = task.failed().run(sched);
    EXPECT_EQ(*(result->await()), 123);
}

TEST(Task, EvalAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::eval([](){return 123;});
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, EvalWithoutErrorType) {
    auto sched = Scheduler::global();
    auto task = Task<int>::eval([](){return 123;});
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, DeferAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::deferAction([](auto){
        return Deferred<int,None>::pure(123);
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, DeferWithoutErrorType) {
    auto sched = Scheduler::global();
    auto task = Task<int>::deferAction([](auto){
        return Deferred<int>::pure(123);
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, DeferError) {
    auto sched = Scheduler::global();
    auto task = Task<None,int>::deferAction([](auto){
        return Deferred<None,int>::raiseError(123);
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, PureMapAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123).map<float>([](auto value){
        return value * 1.5;
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST(Task, PureMapThrowsError) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123).map<float>([](auto value){
        throw None();
        return value * 1.5;
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST(Task, ErrorMapAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::raiseError(None()).map<float>([](auto value){
        return value * 1.5;
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST(Task, EvalMapAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::eval([](){return 123;}).map<float>([](auto value){
        return value * 1.5;
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST(Task, DeferMapAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::deferAction([](auto){
            return Deferred<int,None>::pure(123);
        })
        .map<float>([](auto value){
            return value * 1.5;
        });
    
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST(Task, PureFlatMapAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123).flatMap<float>([](auto value) {
        return Task<float,None>::pure(value * 1.5);
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST(Task, PureFlatMapError) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123).flatMap<float>([](auto) {
        return Task<float,None>::raiseError(None());
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST(Task, PureFlatMapThrowsError) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123).flatMap<float>([](auto value) {
        throw None();
        return Task<float,None>::pure(value * 1.5);
    });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST(Task, EvalFlatMap) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::eval([](){return 123;}).flatMap<float>([](auto value) {
        return Task<float,None>::pure(value * 1.5);
    });
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 184.5);
}

TEST(Task, ErrorFlatMap) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::raiseError(None())
        .flatMap<float>([](auto value) {
            return Task<float,None>::pure(value * 1.5);
        });
    auto result = task.failed().run(sched);
    EXPECT_EQ(result->await(), None());
}

TEST(Task, MaterializeValueAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,std::string>::pure(123).materialize();
    auto result = task.run(sched)->await();
    EXPECT_EQ(result.get_left(), 123);
}

TEST(Task, MaterializeErrorAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,std::string>::raiseError("broke").materialize();
    auto result = task.run(sched)->await();
    EXPECT_EQ(result.get_right(), "broke");
}

TEST(Task, DematerializeValueAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,std::string>::pure(123).materialize().dematerialize<int>();
    auto result = task.run(sched)->await();
    EXPECT_EQ(result, 123);
}

TEST(Task, DematerializeErrorAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,std::string>::raiseError("broke").materialize().dematerialize<int>();
    auto result = task.failed().run(sched)->await();
    EXPECT_EQ(result, "broke");
}

TEST(Task, PureDelayAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123).delay(1);
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, EvalDelayAsync) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::eval([](){return 123;}).delay(1);
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, PureRestartUntil) {
    auto sched = Scheduler::global();
    auto task = Task<int,None>::pure(123).restartUntil([](auto){return true;});
    auto result = task.run(sched);
    EXPECT_EQ(result->await(), 123);
}

TEST(Task, EvalRestartUntilAsync) {
    auto sched = Scheduler::global();
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

TEST(Task, RecurseWithoutExploding) {
    auto sched = Scheduler::global();

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

TEST(Task, OnErrorSuccess) {
    int calls = 0;
    auto result = Task<int,None>::pure(123)
    .onError([&calls](auto) {
        calls++;
    })
    .run(Scheduler::global())
    ->await();

    EXPECT_EQ(result, 123);
    EXPECT_EQ(calls, 0);
}

TEST(Task, OnErrorNormal) {
    int calls = 0;
    auto result = Task<int,None>::raiseError(None())
    .onError([&calls](auto) {
        calls++;
    })
    .failed()
    .run(Scheduler::global())
    ->await();

    EXPECT_EQ(result, None());
    EXPECT_EQ(calls, 1);
}


TEST(Task, OnErrorThrows) {
    int calls = 0;
    auto result = Task<int,float>::raiseError(1.5f)
    .onError([&calls](auto) {
        calls++;
        throw 2.5f;
    })
    .failed()
    .run(Scheduler::global())
    ->await();

    EXPECT_EQ(result, 2.5f);
    EXPECT_EQ(calls, 1);
}

TEST(Task, MapError) {
    auto result = Task<int,int>::raiseError(123)
    .mapError<float>([](auto input) { return input *1.5f; })
    .failed()
    .run(Scheduler::global())
    ->await();

    EXPECT_EQ(result, 184.5);
}