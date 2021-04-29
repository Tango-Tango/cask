//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Resource.hpp"

using cask::Task;
using cask::Resource;
using cask::None;
using cask::Scheduler;

TEST(Resource,BasicUsage) {
    auto resource = Resource<int>::make(
        Task<int>::pure(123),
        [](int){
            return Task<int>::none();
        }
    );

    auto sched = std::make_shared<cask::Scheduler>();
    auto result = resource.template use<float>([](int value) {
        return Task<float>::pure(value * 1.5);
    });

    EXPECT_EQ(result.run(sched)->await(), 184.5);
}

TEST(Resource,CallsAcquireRelease) {
    int calls = 0;
    int openResourceCount = 0;

    auto resource = Resource<int>::make(
        Task<int>::eval([&calls, &openResourceCount]() {
            calls++;
            openResourceCount++;
            return 123;
        }),
        [&calls, &openResourceCount](int){
            calls++;
            openResourceCount--;
            return Task<int>::none();
        }
    );

    auto sched = std::make_shared<cask::Scheduler>();
    auto result = resource.template use<float>([](int value) {
        return Task<float>::pure(value * 1.5);
    });

    EXPECT_EQ(result.run(sched)->await(), 184.5);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(openResourceCount, 0);
}

TEST(Resource,ReleasesAfterUsage) {
    bool released = false;

    auto resource = Resource<int>::make(
        Task<int>::pure(123),
        [&released](int){
            released = true;
            return Task<int>::none();
        }
    );

    auto sched = std::make_shared<cask::Scheduler>();
    auto releasedBeforeUse = resource
        .template use<bool>([&released](int) {
            return Task<bool>::pure(released);
        })
        .run(Scheduler::global())
        ->await();

    EXPECT_FALSE(releasedBeforeUse);
    EXPECT_TRUE(released);
}

TEST(Resource,CallsReleaseOnError) {
    int calls = 0;
    int openResourceCount = 0;

    auto resource = Resource<int, std::string>::make(
        Task<int, std::string>::eval([&calls, &openResourceCount]() {
            calls++;
            openResourceCount++;
            return 123;
        }),
        [&calls, &openResourceCount](int){
            calls++;
            openResourceCount--;
            return Task<None, std::string>::none();
        }
    );

    auto sched = std::make_shared<cask::Scheduler>();
    auto result = resource.template use<float>([](int) {
        return Task<float, std::string>::raiseError("something went wrong");
    });

    auto failure = result.failed().run(sched)->await();

    EXPECT_EQ(std::any_cast<std::string>(failure), "something went wrong");
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(openResourceCount, 0);
}

TEST(Resource,ReleaseRaisesError) {
    auto resource = Resource<int, std::string>::make(
        Task<int, std::string>::eval([]() {
            return 123;
        }),
        [](int){
            return Task<None, std::string>::raiseError("it broke");
        }
    );

    auto sched = std::make_shared<cask::Scheduler>();
    auto result = resource.template use<float>([](int value) {
        return Task<float,std::string>::pure(value * 1.5);
    });

    auto failure = result.failed().run(sched)->await();
    EXPECT_EQ(failure, "it broke");
}

TEST(Resource,AcquireRaisesError) {
    auto resource = Resource<int, std::string>::make(
        Task<int, std::string>::raiseError("acquire broke"),
        [](int){
            return Task<None, std::string>::none();
        }
    );

    auto sched = std::make_shared<cask::Scheduler>();
    auto result = resource.template use<float>([](int value) {
        return Task<float, std::string>::pure(value * 1.5);
    });

    auto failure = result.failed().run(sched)->await();
    EXPECT_EQ(failure, "acquire broke");
}

TEST(Resource, Map) {
    auto resource = Resource<int>::make(
        Task<int>::pure(123),
        [](int){
            return Task<int>::none();
        }
    )
    .map<float>([](auto value) {
        return value * 1.5;
    });

    auto sched = std::make_shared<cask::Scheduler>();
    auto result = resource.template use<float>([](float value) {
        return Task<float>::pure(value);
    });

    EXPECT_EQ(result.run(sched)->await(), 184.5);
}

TEST(Resource, MapAcquireError) {
    auto resource = Resource<int,int>::make(
        Task<int,int>::raiseError(123),
        [](int){
            return Task<int,int>::none();
        }
    )
    .mapError<float>([](auto value) {
        return value * 1.5;
    });

    auto sched = std::make_shared<cask::Scheduler>();
    auto result = resource.template use<float>([](float value) {
        return Task<float,float>::pure(value);
    });

    EXPECT_EQ(result.failed().run(sched)->await(), 184.5);
}


TEST(Resource, MapReleaseError) {
    auto resource = Resource<int,int>::make(
        Task<int,int>::pure(678),
        [](int){
            return Task<None,int>::raiseError(123);
        }
    )
    .mapError<float>([](auto value) {
        return value * 1.5;
    });

    auto sched = std::make_shared<cask::Scheduler>();
    auto result = resource.template use<float>([](float value) {
        return Task<float,float>::pure(value);
    });

    EXPECT_EQ(result.failed().run(sched)->await(), 184.5);
}


TEST(Resource, FlatMapAcquiresAndReleasesBoth) {
    int calls = 0;
    int openResourceCount = 0;

    auto resource = Resource<int>::make(
        Task<int>::eval([&calls, &openResourceCount]() {
            calls++;
            openResourceCount++;
            return 123;
        }),
        [&calls, &openResourceCount](int){
            calls++;
            openResourceCount--;
            return Task<int>::none();
        }
    ).flatMap<float>([&calls, &openResourceCount](auto value) {
        return Resource<float>::make(
            Task<float>::eval([&calls, &openResourceCount, value]() {
                calls++;
                openResourceCount++;
                return value * 1.5f;
            }),
            [&calls, &openResourceCount](float){
                calls++;
                openResourceCount--;
                return Task<int>::none();
            }
        );
    });

    auto sched = std::make_shared<cask::Scheduler>();
    auto result = resource.template use<float>([](float value) {
        return Task<float>::pure(value);
    });

    EXPECT_EQ(result.run(sched)->await(), 184.5);
    EXPECT_EQ(calls, 4);
    EXPECT_EQ(openResourceCount, 0);
}
