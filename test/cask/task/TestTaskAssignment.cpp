//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"

using cask::Task;

TEST(TaskAssignment,CopyAssignment) {
    Task<int> original = Task<int>::pure(123);
    Task<int> other = original; // NOLINT(performance-unnecessary-copy-initialization)
    EXPECT_EQ(original.op, other.op);
}

TEST(TaskAssignment,MoveAssignment) {
    Task<int> outer = Task<int>::pure(123);

    auto beforeResult = outer.runSync();
    EXPECT_EQ(beforeResult->get_left(), 123); // NOLINT(bugprone-unchecked-optional-access)

    {
        Task<int> inner = Task<int>::pure(456);
        outer = inner;
    }

    auto afterResult = outer.runSync();
    EXPECT_EQ(afterResult->get_left(), 456);  // NOLINT(bugprone-unchecked-optional-access)
}
