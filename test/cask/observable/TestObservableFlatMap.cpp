//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "gtest/trompeloeil.hpp"
#include "cask/Observable.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"

using cask::Observable;
using cask::ObservableRef;
using cask::Observer;
using cask::Scheduler;
using cask::Task;
using cask::None;
using cask::Ack;
using cask::observable::FlatMapObserver;

class MockFlatMapDownstreamObserver : public trompeloeil::mock_interface<Observer<float,std::string>> {
public:
    IMPLEMENT_MOCK1(onNext);
    IMPLEMENT_MOCK1(onError);
    IMPLEMENT_MOCK0(onComplete);
    IMPLEMENT_MOCK0(onCancel);
};

TEST(ObservableFlatMap, Performance) {
    int counter = 0;

    auto sched = Scheduler::global();

    auto before = std::chrono::steady_clock::now();
    Observable<int>::repeatTask(Task<int>::eval([]{ return 1; }))
        ->flatMap<float>([](auto value) {
            return Observable<float>::pure(value * 1.5);
        })
        ->takeWhile([&counter](auto) {
            return counter++ <= 1000;
        })
        ->last()
        .run(sched)
        ->await();
    auto after = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count();

    std::cout << "Took " << delta << " milliseconds" << std::endl;
}

TEST(ObservableFlatMap, Pure) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::pure(123)
        ->flatMap<float>([](auto value) {
            return Observable<float>::pure(value * 1.5);
        })
        ->last()
        .run(sched)
        ->await();

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 184.5);
}

TEST(ObservableFlatMap, UpstreamError) {
    auto sched = Scheduler::global();
    auto result = Observable<int,std::string>::raiseError("broke")
        ->flatMap<float>([](auto value) {
            return Observable<float,std::string>::pure(value * 1.5);
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST(ObservableFlatMap, ProducesError) {
    auto sched = Scheduler::global();
    auto result = Observable<int,std::string>::pure(123)
        ->flatMap<float>([](auto) {
            return Observable<float,std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST(ObservableFlatMap, ErrorStopsInfiniteUpstream) {
    auto sched = Scheduler::global();
    int counter = 0;
    auto result = Observable<int,std::string>::repeatTask(Task<int,std::string>::pure(123))
        ->flatMap<float>([&counter](auto) {
            counter++;
            return Observable<float,std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
    EXPECT_EQ(counter, 1);
}

TEST(ObservableFlatMap, StopsUpstreamOnDownstreamComplete) {
    auto sched = Scheduler::global();
    int counter = 0;
    auto result = Observable<int,std::string>::repeatTask(Task<int,std::string>::pure(123))
        ->flatMap<float>([&counter](auto) {
            counter++;
            return Observable<float,std::string>::pure(123 * 1.5f);
        })
        ->take(10)
        .run(sched)
        ->await();

    EXPECT_EQ(result.size(), 10);
    EXPECT_EQ(counter, 10);
}

TEST(ObservableFlatMap, IgnoresRepeatedCompletes) {
    int counter = 0;

    auto mockDownstream = std::make_shared<MockFlatMapDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onComplete())
        .LR_SIDE_EFFECT(counter++)
        .RETURN(Task<None,None>::none());
    
    auto observer = std::make_shared<FlatMapObserver<int,float,std::string>>(
        [](auto value) {
            return Observable<float,std::string>::pure(value * 1.5f);
        },
        mockDownstream
    );

    observer->onComplete().run(Scheduler::global())->await();
    observer->onComplete().run(Scheduler::global())->await();

    EXPECT_EQ(counter, 1);
}

TEST(ObservableFlatMap, IgnoresRepeatedErrors) {
    int counter = 0;

    auto mockDownstream = std::make_shared<MockFlatMapDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onError("broke"))
        .LR_SIDE_EFFECT(counter++)
        .RETURN(Task<None,None>::none());
    
    auto observer = std::make_shared<FlatMapObserver<int,float,std::string>>(
        [](auto value) {
            return Observable<float,std::string>::pure(value * 1.5f);
        },
        mockDownstream
    );

    observer->onError("broke").run(Scheduler::global())->await();
    observer->onError("broke").run(Scheduler::global())->await();

    EXPECT_EQ(counter, 1);
}
