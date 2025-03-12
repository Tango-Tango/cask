//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/ReadyQueue.hpp"
#include "gtest/gtest.h"

#include <thread>

using cask::scheduler::ReadyQueue;

// NOLINTBEGIN(bugprone-unchecked-optional-access)

TEST(ReadyQueue, ConstructsNoParams) {
    ReadyQueue queue;
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(queue.empty());
}

TEST(ReadyQueue, ConstructsWithQueueSize) {
    ReadyQueue queue(10);
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(queue.empty());
}

TEST(ReadyQueue, PushFront) {
    auto executed_task_id = 0;

    ReadyQueue queue(2);
    auto first_task = [&executed_task_id] {
        executed_task_id = 1;
    };
    auto second_task =  [&executed_task_id] {
        executed_task_id = 2;
    };
    auto third_task =  [&executed_task_id] {
        executed_task_id = 3;
    };

    auto first_overflow = queue.push_front(first_task);
    EXPECT_FALSE(first_overflow.has_value());
    EXPECT_EQ(queue.size(), 1);
    EXPECT_FALSE(queue.empty());

    auto second_overflow = queue.push_front(second_task);
    EXPECT_FALSE(second_overflow.has_value());
    EXPECT_EQ(queue.size(), 2);
    EXPECT_FALSE(queue.empty());

    auto third_overflow = queue.push_front(third_task);
    ASSERT_TRUE(third_overflow.has_value());
    EXPECT_EQ(queue.size(), 2);
    EXPECT_FALSE(queue.empty());

    (*third_overflow)();
    EXPECT_EQ(executed_task_id, 1);
}

TEST(ReadyQueue, PushBack) {
    ReadyQueue queue(2);
    auto task = [](){};

    EXPECT_TRUE(queue.push_back(task));
    EXPECT_EQ(queue.size(), 1);
    EXPECT_FALSE(queue.empty());

    EXPECT_TRUE(queue.push_back(task));
    EXPECT_EQ(queue.size(), 2);
    EXPECT_FALSE(queue.empty());

    EXPECT_FALSE(queue.push_back(task));
    EXPECT_EQ(queue.size(), 2);
    EXPECT_FALSE(queue.empty());
}

TEST(ReadyQueue, PushBatchBackFits) {
    ReadyQueue queue(2);
    auto task = [](){};

    EXPECT_TRUE(queue.push_batch_back({task, task}));
    EXPECT_EQ(queue.size(), 2);
    EXPECT_FALSE(queue.empty());

    EXPECT_FALSE(queue.push_batch_back({task, task}));
    EXPECT_EQ(queue.size(), 2);
    EXPECT_FALSE(queue.empty());
}

TEST(ReadyQueue, PushBatchBackDoesntFit) {
    ReadyQueue queue(2);
    auto task = [](){};

    EXPECT_FALSE(queue.push_batch_back({task, task, task}));
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(queue.empty());
}

TEST(ReadyQueue, PopFront) {
    auto executed_task_id = 0;

    ReadyQueue queue(2);
    auto first_task = [&executed_task_id] {
        executed_task_id = 1;
    };
    auto second_task =  [&executed_task_id] {
        executed_task_id = 2;
    };
    auto third_task =  [&executed_task_id] {
        executed_task_id = 3;
    };

    EXPECT_TRUE(queue.push_back(first_task));
    EXPECT_TRUE(queue.push_back(second_task));
    EXPECT_FALSE(queue.push_back(third_task));

    auto first_task_opt = queue.pop_front();
    ASSERT_TRUE(first_task_opt.has_value());
    (*first_task_opt)();
    EXPECT_EQ(executed_task_id, 1);

    auto second_task_opt = queue.pop_front();
    ASSERT_TRUE(second_task_opt.has_value());
    (*second_task_opt)();
    EXPECT_EQ(executed_task_id, 2);

    auto third_task_opt = queue.pop_front();
    EXPECT_FALSE(third_task_opt.has_value());
}

TEST(ReadyQueue, PopBack) {
    auto executed_task_id = 0;

    ReadyQueue queue(2);
    auto first_task = [&executed_task_id] {
        executed_task_id = 1;
    };
    auto second_task =  [&executed_task_id] {
        executed_task_id = 2;
    };
    auto third_task =  [&executed_task_id] {
        executed_task_id = 3;
    };

    EXPECT_TRUE(queue.push_back(first_task));
    EXPECT_TRUE(queue.push_back(second_task));
    EXPECT_FALSE(queue.push_back(third_task));

    auto first_task_opt = queue.pop_back();
    ASSERT_TRUE(first_task_opt.has_value());
    (*first_task_opt)();
    EXPECT_EQ(executed_task_id, 2);

    auto second_task_opt = queue.pop_back();
    ASSERT_TRUE(second_task_opt.has_value());
    (*second_task_opt)();
    EXPECT_EQ(executed_task_id, 1);

    auto third_task_opt = queue.pop_back();
    EXPECT_FALSE(third_task_opt.has_value());
}

TEST(ReadyQueue, StealFrom) {
    ReadyQueue thief(2);
    ReadyQueue victim(2);
    
    auto executed_task_id = 0;
    auto first_task = [&executed_task_id] {
        executed_task_id = 1;
    };
    auto second_task =  [&executed_task_id] {
        executed_task_id = 2;
    };

    EXPECT_TRUE(victim.push_back(first_task));
    EXPECT_TRUE(victim.push_back(second_task));

    EXPECT_TRUE(thief.steal_from(victim));
    EXPECT_EQ(thief.size(), 1);
    EXPECT_EQ(victim.size(), 1);

    auto victim_popped_task = victim.pop_front();
    ASSERT_TRUE(victim_popped_task.has_value());
    (*victim_popped_task)();
    EXPECT_EQ(executed_task_id, 1);


    auto thief_popped_task = thief.pop_front();
    ASSERT_TRUE(thief_popped_task.has_value());
    (*thief_popped_task)();
    EXPECT_EQ(executed_task_id, 2);
}

TEST(ReadyQueue, StealFromEmpty) {
    ReadyQueue thief(2);
    ReadyQueue victim(2);

    EXPECT_FALSE(thief.steal_from(victim));
}

TEST(ReadyQueue, StealWhileFull) {
    ReadyQueue thief(2);
    ReadyQueue victim(2);

    auto task = [](){};
    
    EXPECT_TRUE(victim.push_back(task));
    EXPECT_TRUE(victim.push_back(task));

    EXPECT_TRUE(thief.push_back(task));
    EXPECT_TRUE(thief.push_back(task));

    EXPECT_FALSE(thief.steal_from(victim));
    EXPECT_EQ(thief.size(), 2);
    EXPECT_EQ(victim.size(), 2);
}

// NOLINTEND(bugprone-unchecked-optional-access)
