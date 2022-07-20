//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "gtest/trompeloeil.hpp"
#include "cask/Observable.hpp"
#include "cask/None.hpp"

using cask::Ack;
using cask::Observable;
using cask::Observer;
using cask::Scheduler;
using cask::Task;
using cask::None;
using cask::observable::MapTaskObserver;

class MockMapTaskDownstreamObserver : public trompeloeil::mock_interface<Observer<float,std::string>> {
public:
    IMPLEMENT_MOCK1(onNext);
    IMPLEMENT_MOCK1(onError);
    IMPLEMENT_MOCK0(onComplete);
    IMPLEMENT_MOCK0(onCancel);
};

TEST(ObservableMapTask, Value) {
    auto sched = Scheduler::global();
    auto result = Observable<int>::pure(123)
        ->mapTask<float>([](auto value) {
            return Task<float>::pure(value * 1.5);
        })
        ->last()
        .run(sched)
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 184.5);
}

TEST(ObservableMapTask, Error) {
    auto sched = Scheduler::global();
    auto result = Observable<int,std::string>::pure(123)
        ->mapTask<float>([](auto) {
            return Task<float,std::string>::raiseError("broke");
        })
        ->last()
        .failed()
        .run(sched)
        ->await();

    EXPECT_EQ(result, "broke");
}

TEST(ObservableMapTask, IgnoresRepeatedCompletes) {
    int counter = 0;

    auto mockDownstream = std::make_shared<MockMapTaskDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onComplete())
        .LR_SIDE_EFFECT(counter++)
        .RETURN(Task<None,None>::none());

    auto observer = std::make_shared<MapTaskObserver<int,float,std::string>>(
        [](int value) {
            return Task<float,std::string>::pure(value * 1.5);
        },
        mockDownstream
    );

    observer->onComplete().run(Scheduler::global())->await();
    observer->onComplete().run(Scheduler::global())->await();

    EXPECT_EQ(counter, 1);
}

TEST(ObservableMapTask, IgnoresRepeatedErrors) {
    int counter = 0;

    auto mockDownstream = std::make_shared<MockMapTaskDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onError("broke"))
        .LR_SIDE_EFFECT(counter++)
        .RETURN(Task<None,None>::none());

    auto observer = std::make_shared<MapTaskObserver<int,float,std::string>>(
        [](int value) {
            return Task<float,std::string>::pure(value * 1.5);
        },
        mockDownstream
    );

    observer->onError("broke").run(Scheduler::global())->await();
    observer->onError("broke").run(Scheduler::global())->await();

    EXPECT_EQ(counter, 1);
}

TEST(ObservableMapTask, TransformsValueContinue) {

    auto mockDownstream = std::make_shared<MockMapTaskDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onNext(184.5f))
        .RETURN(Task<Ack,None>::pure(cask::Continue));

    auto observer = std::make_shared<MapTaskObserver<int,float,std::string>>(
        [](int value) {
            return Task<float,std::string>::pure(value * 1.5);
        },
        mockDownstream
    );

    EXPECT_EQ(observer->onNext(123).run(Scheduler::global())->await(), cask::Continue);
}

TEST(ObservableMapTask, TransformsValueStop) {
    auto mockDownstream = std::make_shared<MockMapTaskDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onNext(184.5f))
        .RETURN(Task<Ack,None>::pure(cask::Stop));

    auto observer = std::make_shared<MapTaskObserver<int,float,std::string>>(
        [](int value) {
            return Task<float,std::string>::pure(value * 1.5);
        },
        mockDownstream
    );

    EXPECT_EQ(observer->onNext(123).run(Scheduler::global())->await(), cask::Stop);
}

TEST(ObservableMapTask, ErrorsUpstreamOnError) {
    auto mockDownstream = std::make_shared<MockMapTaskDownstreamObserver>();
    REQUIRE_CALL(*mockDownstream, onError("broke"))
        .RETURN(Task<None,None>::none());

    auto observer = std::make_shared<MapTaskObserver<int,float,std::string>>(
        [](int) {
            return Task<float,std::string>::raiseError("broke");
        },
        mockDownstream
    );

    EXPECT_EQ(observer->onNext(123).failed().run(Scheduler::global())->await(), None());
}

