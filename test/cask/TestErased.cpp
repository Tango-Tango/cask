//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Erased.hpp"

using cask::Erased;

TEST(Erased, Default) {
    Erased foo;
    EXPECT_FALSE(foo.has_value());
}

TEST(Erased, CopiesValue) {
    int value = 123;
    Erased foo(value);
    EXPECT_TRUE(foo.has_value());
    EXPECT_EQ(foo.get<int>(), value);
}

TEST(Erased, ResetsValue) {
    int value = 123;
    Erased foo(value);
    foo.reset();

    EXPECT_FALSE(foo.has_value());
}

TEST(Erased, ResetsDefault) {
    Erased foo;
    foo.reset();
    EXPECT_FALSE(foo.has_value());
}

TEST(Erased, AssignsDefaultValue) {
    Erased foo;
    foo = 123;
    EXPECT_TRUE(foo.has_value());
    EXPECT_EQ(foo.get<int>(), 123);
}

TEST(Erased, AssignsNewValue) {
    Erased foo(std::string("hello"));
    foo = 123;
    EXPECT_TRUE(foo.has_value());
    EXPECT_EQ(foo.get<int>(), 123);
}

TEST(Erased, AssignsAnotherErased) {
    Erased first(123);
    Erased second = first;

    first = 456;

    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(first.get<int>(), 456);

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<int>(), 123);
}

TEST(Erased, OverwritesDuringAssignment) {
    Erased first(123);
    Erased second(std::string("foo"));
    second = first;

    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(first.get<int>(), 123);

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<int>(), 123);
}

TEST(Erased, ThrowsEmptyGet) {
    try {
        Erased foo;
        foo.get<int>();
        FAIL() << "expected method to throw";
    } catch(std::runtime_error&) {}  // NOLINT(bugprone-empty-catch)
}

TEST(Erased, MoveConstructor) {
    Erased first(123);
    Erased second(std::move(first));

    EXPECT_FALSE(first.has_value());
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<int>(), 123);
}

TEST(Erased, MoveAssignment) {
    Erased first(123);
    Erased second;
    second = std::move(first);

    EXPECT_FALSE(first.has_value());
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<int>(), 123);
}

TEST(Erased, MoveAssignmentOverwrites) {
    Erased first(123);
    Erased second(std::string("hello"));
    second = std::move(first);

    EXPECT_FALSE(first.has_value());
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<int>(), 123);
}

TEST(Erased, MoveConstructorWithString) {
    std::string original = "hello world";
    Erased first(original);
    Erased second(std::move(first));

    EXPECT_FALSE(first.has_value());
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<std::string>(), "hello world");
}

TEST(Erased, RvalueConstruction) {
    Erased foo(std::string("hello"));
    EXPECT_TRUE(foo.has_value());
    EXPECT_EQ(foo.get<std::string>(), "hello");
}

TEST(Erased, RvalueAssignment) {
    Erased foo;
    foo = std::string("hello");
    EXPECT_TRUE(foo.has_value());
    EXPECT_EQ(foo.get<std::string>(), "hello");
}
