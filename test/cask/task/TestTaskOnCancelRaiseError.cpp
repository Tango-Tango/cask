//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"

using cask::Task;
using cask::scheduler::BenchScheduler;

namespace {
struct MoveTrackingError {
    std::string message;
    bool moved_from = false;

    explicit MoveTrackingError(std::string message)
        : message(std::move(message)) {}

    MoveTrackingError(const MoveTrackingError&) = default;
    MoveTrackingError& operator=(const MoveTrackingError&) = default;

    MoveTrackingError(MoveTrackingError&& other) noexcept
        : message(std::move(other.message)) {
        other.moved_from = true;
    }

    MoveTrackingError& operator=(MoveTrackingError&& other) noexcept {
        if(this != &other) {
            message = std::move(other.message);
            moved_from = other.moved_from;
            other.moved_from = true;
        }

        return *this;
    }
};
}  // namespace

TEST(TaskOnCancelRaiseError,ConvertsToError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int, std::string>::never()
        .onCancelRaiseError("cancel happened")
        .failed()
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(fiber->await(), "cancel happened");
}

TEST(TaskOnCancelRaiseError,IgnoresValue) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int, std::string>::pure(123)
        .onCancelRaiseError("cancel happened")
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(fiber->await(), 123);
}

TEST(TaskOnCancelRaiseError,IgnoresError) {
    auto sched = std::make_shared<BenchScheduler>();
    auto fiber = Task<int, std::string>::raiseError("broke")
        .onCancelRaiseError("cancel happened")
        .failed()
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(fiber->await(), "broke");
}

TEST(TaskOnCancelRaiseError,DoesNotMoveFromLvalueError) {
    auto sched = std::make_shared<BenchScheduler>();
    MoveTrackingError error("cancel happened");

    auto fiber = Task<int, MoveTrackingError>::never()
        .onCancelRaiseError(error)
        .run(sched);

    EXPECT_FALSE(error.moved_from);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    ASSERT_TRUE(fiber->getError().has_value());
    EXPECT_EQ(fiber->getError()->message, "cancel happened");
    EXPECT_FALSE(error.moved_from);
}
