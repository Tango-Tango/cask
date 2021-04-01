//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"

using cask::Observable;
using cask::ObservableRef;
using cask::Scheduler;
using cask::Task;
using cask::None;

TEST(ObservableGuarantee, RunsOnCompletion) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto result = Observable<int,float>::pure(123)
        ->guarantee(task)
        ->last()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(*result, 123);
    EXPECT_EQ(run_count, 1);
}

TEST(ObservableGuarantee, RunsOnDownstreamStop) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto result = Observable<int,float>::eval([]{ return 123; })
        ->guarantee(task)
        ->last()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(*result, 123);
    EXPECT_EQ(run_count, 1);
}



TEST(ObservableGuarantee, RunsOnError) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto result = Observable<int,float>::raiseError(1.23)
        ->guarantee(task)
        ->last()
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, 1.23f);
    EXPECT_EQ(run_count, 1);
}

TEST(ObservableGuarantee, RunsOnSubscriptionCancel) {
    int run_count = 0;
    auto task = Task<None,None>::eval([&run_count]() {
        run_count++;
        return None();
    });

    auto deferred = Observable<int,float>::deferTask([]{
            return Task<int,float>::never();
        })
        ->guarantee(task)
        ->last()
        .failed()
        .run(Scheduler::global());
    
    try {
        deferred->cancel();
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {
        EXPECT_EQ(run_count, 1);
    }
}
