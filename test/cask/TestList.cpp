//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/List.hpp"

using cask::List;

TEST(List, Empty) {
    auto list = List<int>::empty();

    EXPECT_TRUE(list->is_empty());
    EXPECT_EQ(list->size(), 0);
    EXPECT_FALSE(list->head().has_value());
    EXPECT_TRUE(list->tail()->is_empty());
}

TEST(List, Prepend) {
    auto list = List<int>::empty()
        ->prepend(1)
        ->prepend(2)
        ->prepend(3);

    EXPECT_FALSE(list->is_empty());
    EXPECT_EQ(list->size(), 3);
    EXPECT_EQ(*(list->head()), 3);
    EXPECT_EQ(*(list->tail()->head()), 2);
    EXPECT_EQ(*(list->tail()->tail()->head()), 1);
    EXPECT_TRUE(list->tail()->tail()->tail()->is_empty());
}

TEST(List, Append) {
    auto list = List<int>::empty()
        ->append(1)
        ->append(2)
        ->append(3);

    EXPECT_FALSE(list->is_empty());
    EXPECT_EQ(list->size(), 3);
    EXPECT_EQ(*(list->head()), 1);
    EXPECT_EQ(*(list->tail()->head()), 2);
    EXPECT_EQ(*(list->tail()->tail()->head()), 3);
    EXPECT_TRUE(list->tail()->tail()->tail()->is_empty());
}

TEST(List, DropWhilePartialMatch) {
        auto list = List<int>::empty()
        ->append(1)
        ->append(2)
        ->append(3)
        ->dropWhile([](auto value) { return value <= 2; });
    
    EXPECT_FALSE(list->is_empty());
    EXPECT_EQ(list->size(), 1);
    EXPECT_EQ(*(list->head()), 3);
    EXPECT_TRUE(list->tail()->is_empty());
}

TEST(List, DropWhileMatchesNothing) {
        auto list = List<int>::empty()
        ->append(1)
        ->append(2)
        ->append(3)
        ->dropWhile([](auto value) { return value > 10; });
    
    EXPECT_FALSE(list->is_empty());
    EXPECT_EQ(list->size(), 3);
    EXPECT_EQ(*(list->head()), 1);
    EXPECT_EQ(*(list->tail()->head()), 2);
    EXPECT_EQ(*(list->tail()->tail()->head()), 3);
    EXPECT_TRUE(list->tail()->tail()->tail()->is_empty());
}

TEST(List, DropWhileMatchesEverything) {
        auto list = List<int>::empty()
        ->append(1)
        ->append(2)
        ->append(3)
        ->dropWhile([](auto value) { return value < 10; });
    
    EXPECT_TRUE(list->is_empty());
    EXPECT_EQ(list->size(), 0);
}
