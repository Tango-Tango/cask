//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_EITHER_H_
#define _CASK_EITHER_H_

#include <optional>

namespace cask {

/**
 * An either holds one of two mutually exclusive values. It differs
 * from std::variant in that it can hold the same type for both the
 * left and right types while retaining explicit knowledge if the
 * value is the left or right result. This behavior is extremely
 * useful for keeping success and error results seperate - for example.
 */
template <typename L, typename R>
class Either {
public:
    /**
     * Construct an either holding a left value.
     *
     * @param left The left value to hold.
     * @return An either holding the left result.
     */
    constexpr static Either<L,R> left(const L& left);

    /**
     * Construct an either holding a left value.
     *
     * @param left The left value to hold.
     * @return An either holding the left result.
     */
    constexpr static Either<L,R> left(const L&& left);

    /**
     * Construct an either holding a right value.
     *
     * @param left The right value to hold.
     * @return An either holding the right result.
     */
    constexpr static Either<L,R> right(const R& right);

    /**
     * Construct an either holding a right value.
     *
     * @param left The right value to hold.
     * @return An either holding the right result.
     */
    constexpr static Either<L,R> right(const R&& right);

    /**
     * Check if this either is holding the left result. If true
     * you can safely assume a call to `get_left` is safe. If
     * false you can safely assume a call to `get_right` is safe.
     *
     * @return True iff this either is holding a left value.
     */
    constexpr bool is_left() const;

    /**
     * Check if this either is holding the right result. If true
     * you can safely assume a call to `get_right` is safe. If
     * false you can safely assume a call to `get_left` is safe.
     *
     * @return True iff this either is holding a right value.
     */
    constexpr bool is_right() const;

    /**
     * Attempt to get the left result. Can throw if this either
     * is holding a right result instead - and thus should be
     * be guarded by a call to `is_left` or `!is_right` to determine
     * the state of this either.
     *
     * @return The left value or throws an exception.
     */
    constexpr L get_left() const;

    /**
     * Attempt to get the right result. Can throw if this either
     * is holding a right result instead - and thus should be
     * be guarded by a call to `is_right` or `!is_left` to determine
     * the state of this either.
     *
     * @return The right value or throws an exception.
     */
    constexpr R get_right() const;

    constexpr Either<L,R>(const Either<L,R>&) = default;
    constexpr Either<L,R>(Either<L,R>&&) = default;
    constexpr Either<L,R>& operator=(const Either<L,R>&) = default;
    constexpr Either<L,R>& operator=(Either<L,R>&&) = default;

private:
    constexpr Either() = default;
    std::optional<L> leftValue;
    std::optional<R> rightValue;
};

template <class L, class R>
constexpr Either<L,R> Either<L,R>::left(const L& left) {
    Either<L,R> either;
    either.leftValue = left;
    return either;
}

template <class L, class R>
constexpr Either<L,R> Either<L,R>::left(const L&& left) {
    Either<L,R> either;
    either.leftValue = std::move(left);
    return either;
}

template <class L, class R>
constexpr Either<L,R> Either<L,R>::right(const R& right) {
    Either<L,R> either;
    either.rightValue = right;
    return either;
}

template <class L, class R>
constexpr Either<L,R> Either<L,R>::right(const R&& right) {
    Either<L,R> either;
    either.rightValue = std::move(right);
    return either;
}

template <class L, class R>
constexpr bool Either<L,R>::is_left() const {
    return leftValue.has_value();
}

template <class L, class R>
constexpr bool Either<L,R>::is_right() const {
    return rightValue.has_value();
}

template <class L, class R>
constexpr L Either<L,R>::get_left() const {
    return *leftValue;
}

template <class L, class R>
constexpr R Either<L,R>::get_right() const {
    return *rightValue;
}

}

#endif