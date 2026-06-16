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

    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): Testing that moved-from state is empty
    EXPECT_FALSE(first.has_value());
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<int>(), 123);
}

TEST(Erased, MoveAssignment) {
    Erased first(123);
    Erased second;
    second = std::move(first);

    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): Testing that moved-from state is empty
    EXPECT_FALSE(first.has_value());
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<int>(), 123);
}

TEST(Erased, MoveAssignmentOverwrites) {
    Erased first(123);
    Erased second(std::string("hello"));
    second = std::move(first);

    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): Testing that moved-from state is empty
    EXPECT_FALSE(first.has_value());
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<int>(), 123);
}

TEST(Erased, MoveConstructorWithString) {
    std::string original = "hello world";
    Erased first(original);
    Erased second(std::move(first));

    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): Testing that moved-from state is empty
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

// --- Storage-strategy nuances of the small-buffer / tagged-pointer design ---

namespace {

struct Counters {
    int ctor = 0;
    int copy_ctor = 0;
    int move_ctor = 0;
    int copy_assign = 0;
    int move_assign = 0;
    int dtor = 0;

    int live() const { return ctor + copy_ctor + move_ctor - dtor; }
};

// Small non-trivial type: stored inline, managed through the per-type
// operations table.
struct Tracked {
    Counters* counters;
    int payload;

    Tracked(Counters* counters, int payload) : counters(counters), payload(payload) { counters->ctor++; }
    Tracked(const Tracked& other) : counters(other.counters), payload(other.payload) { counters->copy_ctor++; }
    Tracked(Tracked&& other) noexcept : counters(other.counters), payload(other.payload) { counters->move_ctor++; }
    Tracked& operator=(const Tracked& other) {
        if(this != &other) {
            counters = other.counters;
            payload = other.payload;
            counters->copy_assign++;
        }
        return *this;
    }
    Tracked& operator=(Tracked&& other) noexcept {
        counters = other.counters;
        payload = other.payload;
        counters->move_assign++;
        return *this;
    }
    ~Tracked() { counters->dtor++; }
};

// Same semantics but padded past the inline buffer: pool-allocated.
struct BigTracked : Tracked {
    using Tracked::Tracked;
    unsigned char pad[cask::erased::sbo_size] = {};
};

// Trivially-copyable type larger than the primitives the existing tests
// use: stored inline with no operations table at all.
struct TrivialBlob {
    unsigned char bytes[16];
};

// Trivially-copyable but over-aligned past the buffer's pointer
// alignment: must fall back to the pool.
struct alignas(cask::erased::sbo_align * 4) OverAligned {
    double value;
};

// Small, but its move constructor is not noexcept: must fall back to the
// pool (Erased's noexcept move operations cannot risk a throwing
// relocation).
struct ThrowingMove {
    int payload;

    explicit ThrowingMove(int payload) : payload(payload) {}
    ThrowingMove(const ThrowingMove&) = default;
    ThrowingMove(ThrowingMove&& other) : payload(other.payload) {} // NOLINT(performance-noexcept-move-constructor)
    ThrowingMove& operator=(const ThrowingMove&) = default;
    ThrowingMove& operator=(ThrowingMove&& other) { // NOLINT(performance-noexcept-move-constructor)
        payload = other.payload;
        return *this;
    }
    ~ThrowingMove() = default;
};

// Sanity-check that the test types exercise the storage strategies they
// are designed to exercise.
static_assert(cask::erased::fits_sbo<Tracked>, "Tracked must be stored inline");
static_assert(!cask::erased::trivial_sbo<Tracked>, "Tracked must use the operations table");
static_assert(cask::erased::trivial_sbo<TrivialBlob>, "TrivialBlob must be trivially stored inline");
static_assert(!cask::erased::fits_sbo<BigTracked>, "BigTracked must be pool-allocated");
static_assert(!cask::erased::fits_sbo<OverAligned>, "OverAligned must be pool-allocated");
static_assert(!cask::erased::fits_sbo<ThrowingMove>, "ThrowingMove must be pool-allocated");

} // namespace

TEST(Erased, InlineNonTrivialDestroyedExactlyOnce) {
    Counters counters;

    {
        Erased foo((Tracked(&counters, 7)));
        EXPECT_EQ(foo.get<Tracked>().payload, 7);
        EXPECT_EQ(counters.live(), 1);
    }

    EXPECT_EQ(counters.live(), 0);
}

TEST(Erased, InlineNonTrivialMoveEmptiesSourceAndBalancesLifetimes) {
    Counters counters;

    {
        Erased first((Tracked(&counters, 7)));
        Erased second(std::move(first));

        // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): Testing that moved-from state is empty
        EXPECT_FALSE(first.has_value());
        EXPECT_TRUE(second.has_value());
        EXPECT_EQ(second.get<Tracked>().payload, 7);

        // Inline moves relocate the value: exactly one live instance, and
        // the moved-from Erased must not destroy it again.
        EXPECT_EQ(counters.live(), 1);
    }

    EXPECT_EQ(counters.live(), 0);
}

TEST(Erased, InlineNonTrivialCopyIsIndependent) {
    Counters counters;

    Erased first((Tracked(&counters, 7)));
    Erased second(first);

    first.reset();

    EXPECT_FALSE(first.has_value());
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<Tracked>().payload, 7);
    EXPECT_EQ(counters.live(), 1);

    second.reset();
    EXPECT_EQ(counters.live(), 0);
}

TEST(Erased, SameTypeAssignmentReusesStorage) {
    Counters counters;

    Erased foo((Tracked(&counters, 1)));
    int dtors_before = counters.dtor;

    foo = Tracked(&counters, 2);

    // Assigning the same type must hit the in-place fast path: the held
    // value is assigned to, not destroyed and reconstructed.
    EXPECT_EQ(counters.move_assign, 1);
    EXPECT_EQ(counters.dtor, dtors_before + 1); // only the temporary
    EXPECT_EQ(foo.get<Tracked>().payload, 2);
}

TEST(Erased, DifferentTypeAssignmentDestroysOldValue) {
    Counters counters;

    Erased foo((Tracked(&counters, 1)));
    foo = std::string("hello");

    EXPECT_EQ(counters.live(), 0);
    EXPECT_EQ(foo.get<std::string>(), "hello");
}

TEST(Erased, TrivialValuesRoundTrip) {
    TrivialBlob blob;
    for(std::size_t i = 0; i < sizeof(blob.bytes); i++) {
        blob.bytes[i] = static_cast<unsigned char>(i * 3);
    }

    Erased foo(blob);
    Erased copy(foo);
    Erased moved(std::move(foo));

    for(std::size_t i = 0; i < sizeof(blob.bytes); i++) {
        EXPECT_EQ(copy.get<TrivialBlob>().bytes[i], blob.bytes[i]);
        EXPECT_EQ(moved.get<TrivialBlob>().bytes[i], blob.bytes[i]);
    }
}

TEST(Erased, TrivialTypesShareStateAcrossAssignment) {
    // Trivial inline types of the same size share the same (tableless)
    // state, so assigning one over another reuses the buffer - safely,
    // since the fast path copies raw bytes rather than invoking the
    // type's assignment operator. Differently-sized trivial types have
    // distinct states and take the reset-and-reconstruct path.
    Erased foo(123);

    foo = 4.5f; // same size as int: in-place byte copy
    EXPECT_EQ(foo.get<float>(), 4.5f);

    foo = 3.5; // different size: reset + construct
    EXPECT_EQ(foo.get<double>(), 3.5);

    foo = 456;
    EXPECT_EQ(foo.get<int>(), 456);
}

TEST(Erased, PooledValueDestroyedExactlyOnce) {
    Counters counters;

    {
        Erased foo((BigTracked(&counters, 7)));
        EXPECT_EQ(foo.get<BigTracked>().payload, 7);
        EXPECT_EQ(counters.live(), 1);
    }

    EXPECT_EQ(counters.live(), 0);
}

TEST(Erased, PooledMoveStealsPointerWithoutTouchingValue) {
    Counters counters;

    Erased first((BigTracked(&counters, 7)));
    int move_ctors_before = counters.move_ctor;
    int dtors_before = counters.dtor;

    Erased second(std::move(first));

    // A pooled move transfers ownership of the allocation: the held value
    // itself must not be moved or destroyed.
    EXPECT_EQ(counters.move_ctor, move_ctors_before);
    EXPECT_EQ(counters.dtor, dtors_before);

    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): Testing that moved-from state is empty
    EXPECT_FALSE(first.has_value());
    EXPECT_EQ(second.get<BigTracked>().payload, 7);

    second.reset();
    EXPECT_EQ(counters.live(), 0);
}

TEST(Erased, PooledCopyIsIndependent) {
    Counters counters;

    Erased first((BigTracked(&counters, 7)));
    Erased second(first);

    first.reset();

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.get<BigTracked>().payload, 7);
    EXPECT_EQ(counters.live(), 1);

    second.reset();
    EXPECT_EQ(counters.live(), 0);
}

TEST(Erased, OverAlignedValueRoundTrips) {
    Erased foo(OverAligned{3.5});
    Erased copy(foo);
    Erased moved(std::move(foo));

    EXPECT_EQ(copy.get<OverAligned>().value, 3.5);
    EXPECT_EQ(moved.get<OverAligned>().value, 3.5);
}

TEST(Erased, ThrowingMoveValueRoundTrips) {
    Erased foo(ThrowingMove{7});
    Erased copy(foo);
    Erased moved(std::move(foo));

    EXPECT_EQ(copy.get<ThrowingMove>().payload, 7);
    EXPECT_EQ(moved.get<ThrowingMove>().payload, 7);
}

TEST(Erased, SharedPtrRefcountsAcrossCopyAndMove) {
    auto ptr = std::make_shared<int>(42);

    Erased foo(ptr);
    EXPECT_EQ(ptr.use_count(), 2);

    Erased copy(foo);
    EXPECT_EQ(ptr.use_count(), 3);

    Erased moved(std::move(foo));
    EXPECT_EQ(ptr.use_count(), 3);

    copy.reset();
    moved.reset();
    EXPECT_EQ(ptr.use_count(), 1);
    EXPECT_EQ(*ptr, 42);
}

TEST(Erased, MovedFromIsReusable) {
    Erased first(std::string("hello"));
    Erased second(std::move(first));

    // NOLINTNEXTLINE(bugprone-use-after-move): Testing that moved-from state is reusable
    first = std::string("world");

    EXPECT_EQ(first.get<std::string>(), "world");
    EXPECT_EQ(second.get<std::string>(), "hello");
}

TEST(Erased, SelfAssignmentIsSafe) {
    Counters counters;

    Erased foo((Tracked(&counters, 7)));
    Erased& alias = foo;
    foo = alias;

    EXPECT_TRUE(foo.has_value());
    EXPECT_EQ(foo.get<Tracked>().payload, 7);
    EXPECT_EQ(counters.live(), 1);

    foo.reset();
    EXPECT_EQ(counters.live(), 0);
}
