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

#include "immer/vector.hpp"
#include "immer/map.hpp"

TEST(Ref,Creates) {
    auto ref = Ref<immer::vector<int>>::create(immer::vector<int>());
    auto result = ref->get().run(Scheduler::global());
    auto vector = result->await();
    EXPECT_TRUE(vector.empty());
}

TEST(Ref,Updates) {
    auto ref = Ref<immer::vector<int>>::create(immer::vector<int>());
    auto result = ref->update([](auto vector) {
        return vector.push_back(0).push_back(1);
    })
    .flatMap<immer::vector<int>>([ref](auto) {
        return ref->get();
    })
    .run(Scheduler::global());

    auto vector = result->await();

    EXPECT_FALSE(vector.empty());
    EXPECT_EQ(vector.size(), 2);
    EXPECT_EQ(vector[0], 0);
    EXPECT_EQ(vector[1], 1);
}

TEST(Ref,ContendedUpdates) {
    auto ref = Ref<immer::map<std::string, int>>::create(immer::map<std::string, int>());

    std::vector<DeferredRef<None,std::any>> deferreds;

    for(int i = 0; i < 1000; i++) {
        auto first = ref->update([](auto map) {
            if(map.count("hello")) {
                auto counter = map["hello"];
                return map.set("hello", counter + 1);
            } else {
                return map.set("hello", 1);
            }
        })
        .asyncBoundary()
        .run(Scheduler::global());

        auto second = ref->update([](auto map) {
            if(map.count("world")) {
                auto counter = map["world"];
                return map.set("world", counter + 1);
            } else {
                return map.set("world", 1);
            }
        })
        .asyncBoundary()
        .run(Scheduler::global());

        deferreds.push_back(first);
        deferreds.push_back(second);
    }

    for(auto& deferred : deferreds) {
        deferred->await();
    }

    auto map = ref->get()
    .asyncBoundary()
    .run(Scheduler::global())
    ->await();

    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map.size(), 2);
    EXPECT_EQ(map["hello"], 1000);
    EXPECT_EQ(map["world"], 1000);
}
