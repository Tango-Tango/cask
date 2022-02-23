//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Observable.hpp"
#include "gtest/gtest.h"
#include "gtest/trompeloeil.hpp"
#include <exception>

using cask::Ack;
using cask::None;
using cask::Observable;
using cask::Observer;
using cask::Scheduler;
using cask::Task;
using cask::observable::MapBothTaskObserver;

class MockMapBothTaskDownstreamObserver : public trompeloeil::mock_interface<Observer<double, int>> {
public:
    IMPLEMENT_MOCK1(onNext);
    IMPLEMENT_MOCK1(onError);
    IMPLEMENT_MOCK0(onComplete);
    IMPLEMENT_MOCK0(onCancel);
};

TEST(TestObservableMapBothTask, MapsSuccessToSuccess) {
    auto result = Observable<int, std::string>::pure(123)
                      ->mapBothTask<std::string, std::runtime_error>(
                          [](auto value) {
                              return Task<std::string, std::runtime_error>::pure(std::to_string(value));
                          },
                          [](auto error) {
                              return Task<std::string, std::runtime_error>::raiseError(std::runtime_error(error));
                          })
                      ->last()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, "123");
}

TEST(TestObservableMapBothTask, MapsErrorToError) {
    auto result = Observable<int, std::string>::raiseError("broke")
                      ->mapBothTask<std::string, std::runtime_error>(
                          [](auto value) {
                              return Task<std::string, std::runtime_error>::pure(std::to_string(value));
                          },
                          [](auto error) {
                              return Task<std::string, std::runtime_error>::raiseError(std::runtime_error(error));
                          })
                      ->last()
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result.what(), std::string("broke"));
}

TEST(TestObservableMapBothTask, MapsSuccessToError) {
    auto result = Observable<std::string, int>::pure("working")
                      ->mapBothTask<std::string, std::runtime_error>(
                          [](auto value) {
                              return Task<std::string, std::runtime_error>::raiseError(std::runtime_error(value));
                          },
                          [](auto error) {
                              return Task<std::string, std::runtime_error>::pure(std::to_string(error));
                          })
                      ->last()
                      .failed()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result.what(), std::string("working"));
}

TEST(TestObservableMapBothTask, MapsErrorToSuccess) {
    auto result = Observable<std::string, int>::raiseError(123)
                      ->mapBothTask<std::string, std::runtime_error>(
                          [](auto value) {
                              return Task<std::string, std::runtime_error>::raiseError(std::runtime_error(value));
                          },
                          [](auto error) {
                              return Task<std::string, std::runtime_error>::pure(std::to_string(error));
                          })
                      ->last()
                      .run(Scheduler::global())
                      ->await();

    EXPECT_EQ(result, "123");
}

TEST(TestObservableMapBothTask, SuccessToSuccessPassesDownstreamContinue) {
    auto mockDownstream = std::make_shared<MockMapBothTaskDownstreamObserver>();

    REQUIRE_CALL(*mockDownstream, onNext(1.23)).RETURN(Task<Ack, None>::pure(cask::Continue));

    auto observer = std::make_shared<MapBothTaskObserver<int, double, std::string, int>>(
        [](auto) {
            return Task<double, int>::pure(1.23);
        },
        [](auto) {
            return Task<double, int>::raiseError(456);
        },
        mockDownstream);

    auto result = observer->onNext(123).run(Scheduler::global())->await();
    EXPECT_EQ(result, cask::Continue);
}

TEST(TestObservableMapBothTask, SuccessToSuccessPassesDownstreamStop) {
    auto mockDownstream = std::make_shared<MockMapBothTaskDownstreamObserver>();

    REQUIRE_CALL(*mockDownstream, onNext(1.23)).RETURN(Task<Ack, None>::pure(cask::Stop));

    auto observer = std::make_shared<MapBothTaskObserver<int, double, std::string, int>>(
        [](auto) {
            return Task<double, int>::pure(1.23);
        },
        [](auto) {
            return Task<double, int>::raiseError(456);
        },
        mockDownstream);

    auto result = observer->onNext(123).run(Scheduler::global())->await();
    EXPECT_EQ(result, cask::Stop);
}

TEST(TestObservableMapBothTask, SuccessToErrorPassesErrorDownstreamStopsUpstream) {
    auto mockDownstream = std::make_shared<MockMapBothTaskDownstreamObserver>();

    REQUIRE_CALL(*mockDownstream, onError(456)).RETURN(Task<None, None>::none());

    auto observer = std::make_shared<MapBothTaskObserver<int, double, std::string, int>>(
        [](auto) {
            return Task<double, int>::raiseError(456);
        },
        [](auto) {
            return Task<double, int>::pure(1.23);
        },
        mockDownstream);

    auto result = observer->onNext(123).run(Scheduler::global())->await();
    EXPECT_EQ(result, cask::Stop);
}

TEST(TestObservableMapBothTask, SuccessToErrorPassesErrorDoesntAcceptSubsequentValues) {
    auto counter = 0;
    auto mockDownstream = std::make_shared<MockMapBothTaskDownstreamObserver>();

    REQUIRE_CALL(*mockDownstream, onError(456)).LR_SIDE_EFFECT(counter++).RETURN(Task<None, None>::none());

    auto observer = std::make_shared<MapBothTaskObserver<int, double, std::string, int>>(
        [](auto) {
            return Task<double, int>::raiseError(456);
        },
        [](auto) {
            return Task<double, int>::pure(1.23);
        },
        mockDownstream);

    EXPECT_EQ(observer->onNext(123).run(Scheduler::global())->await(), cask::Stop);
    EXPECT_EQ(observer->onNext(123).run(Scheduler::global())->await(), cask::Stop);
    EXPECT_EQ(counter, 1);
}

TEST(TestObservableMapBothTask, ErrorToErrorPassesError) {
    auto mockDownstream = std::make_shared<MockMapBothTaskDownstreamObserver>();

    REQUIRE_CALL(*mockDownstream, onError(456)).RETURN(Task<None, None>::none());

    auto observer = std::make_shared<MapBothTaskObserver<int, double, std::string, int>>(
        [](auto) {
            return Task<double, int>::pure(1.23);
        },
        [](auto) {
            return Task<double, int>::raiseError(456);
        },
        mockDownstream);

    observer->onError("broke").run(Scheduler::global())->await();
}

TEST(TestObservableMapBothTask, ErrorToSuccessCompletesDownstream) {
    auto mockDownstream = std::make_shared<MockMapBothTaskDownstreamObserver>();

    REQUIRE_CALL(*mockDownstream, onNext(1.23)).RETURN(Task<Ack, None>::pure(cask::Continue));

    REQUIRE_CALL(*mockDownstream, onComplete()).RETURN(Task<None, None>::none());

    auto observer = std::make_shared<MapBothTaskObserver<int, double, std::string, int>>(
        [](auto) {
            return Task<double, int>::raiseError(123);
        },
        [](auto) {
            return Task<double, int>::pure(1.23);
        },
        mockDownstream);

    observer->onError("broke").run(Scheduler::global())->await();
}

TEST(TestObservableMapBothTask, ErrorsOnce) {
    int counter = 0;
    auto mockDownstream = std::make_shared<MockMapBothTaskDownstreamObserver>();

    REQUIRE_CALL(*mockDownstream, onError(456)).LR_SIDE_EFFECT(counter++).RETURN(Task<None, None>::none());

    auto observer = std::make_shared<MapBothTaskObserver<int, double, std::string, int>>(
        [](auto) {
            return Task<double, int>::pure(1.23);
        },
        [](auto) {
            return Task<double, int>::raiseError(456);
        },
        mockDownstream);

    observer->onError("broke").run(Scheduler::global())->await();
    observer->onError("broke").run(Scheduler::global())->await();
    EXPECT_EQ(counter, 1);
}