//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Ref.hpp"

using cask::DeferredRef;
using cask::Task;
using cask::Ref;
using cask::Scheduler;
using cask::None;

TEST(Ref,Creates) {
    auto ref = Ref<int>::create(0);
    auto result = ref->get().run(Scheduler::global());
    auto value = result->await();
    EXPECT_EQ(value, 0);
}

TEST(Ref,Updates) {
    auto ref = Ref<int>::create(0);
    auto result = ref->update([](auto value) {
        return value + 1;
    })
    .template flatMap<int>([ref](auto) {
        return ref->get();
    })
    .run(Scheduler::global());

    auto value = result->await();
    EXPECT_EQ(value, 1);
}

TEST(Ref,ContendedUpdates) {
    auto ref = Ref<std::tuple<int,int>>::create(std::make_tuple(0,0));

    std::vector<DeferredRef<None,std::any>> deferreds;

    for(int i = 0; i < 1000; i++) {
        auto first = ref->update([](auto value) {
            auto [left, right] = value;
            return std::make_tuple(left + 1, right);
        })
        .asyncBoundary()
        .run(Scheduler::global());

        auto second = ref->update([](auto value) {
            auto [left, right] = value;
            return std::make_tuple(left, right + 1);
        })
        .asyncBoundary()
        .run(Scheduler::global());

        deferreds.push_back(first);
        deferreds.push_back(second);
    }

    for(auto& deferred : deferreds) {
        deferred->await();
    }

    auto [left, right] = ref->get()
        .asyncBoundary()
        .run(Scheduler::global())
        ->await();

    EXPECT_EQ(left, 1000);
    EXPECT_EQ(right, 1000);
}
