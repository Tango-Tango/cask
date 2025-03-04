//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Observable.hpp"

using cask::Observable;
using cask::ObservableRef;
using cask::Scheduler;

TEST(ObservableFlatten, FlattensValue) {
    ObservableRef<ObservableRef<int>> nested = Observable<ObservableRef<int>>::pure(Observable<int>::pure(123));
    ObservableRef<int> flattened = nested->template flatten<int>();

    auto result = flattened
        ->last()
        .run(Scheduler::global())
        ->await();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(ObservableFlatten, FlattensEmpty) {
    ObservableRef<ObservableRef<int>> nested = Observable<ObservableRef<int>>::pure(Observable<int>::empty());
    ObservableRef<int> flattened = nested->template flatten<int>();

    auto result = flattened
        ->last()
        .run(Scheduler::global())
        ->await();

    EXPECT_FALSE(result.has_value());
}

TEST(ObservableFlatten, FlattensError) {
    ObservableRef<ObservableRef<int,float>,float> nested = Observable<ObservableRef<int,float>,float>::pure(
        Observable<int,float>::raiseError(1.23)
    );
    ObservableRef<int,float> flattened = nested->template flatten<int>();

    auto result = flattened
        ->last()
        .failed()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(result, 1.23f);
}
