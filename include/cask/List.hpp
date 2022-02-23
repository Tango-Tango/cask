//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_LIST_H_
#define _CASK_LIST_H_

#include <functional>
#include <memory>
#include <optional>

namespace cask {

template <class T>
class List;

template <class T>
using ListRef = std::shared_ptr<const List<T>>;

/**
 * A simple immutable and persistent list supporting constant time prepend and
 * head access operations - along with linear time append operations.
 */
template <class T>
class List {
public:
    /**
     * Create an empty list.
     *
     * @return An empty list.
     */
    static ListRef<T> empty();

    /**
     * Prepend a value to this list. Executes in O(1)
     * time.
     *
     * @return The list with the item prepended.
     */
    virtual ListRef<T> prepend(const T& elem) const = 0;

    /**
     * Append a value to this list. Executes in O(n)
     * time.
     *
     * @return The list with the value appended.
     */
    virtual ListRef<T> append(const T& elem) const = 0;

    /**
     * Check if this list is empty. Executes in O(1)
     * time.
     *
     * @return true iff this list is empty.
     */
    virtual bool is_empty() const = 0;

    /**
     * Retrieve the size of this list. The size is
     * memoized and checking it executes in O(1) time.
     *
     * @return The number of elements contained in
     *         this list.
     */
    virtual std::size_t size() const = 0;

    /**
     * Retrieve the head value of the list. Executes in
     * O(1) time.
     *
     * @return The head value or nothing if the list
     *         is empty.
     */
    virtual std::optional<T> head() const = 0;

    /**
     * Retrieve the tail items of the list (which is
     * the whole list not including the head). Executes
     * in O(1) time.
     *
     * @return The tail of the list. If the list is empty
     *         then the empty list will be returned as the
     *         tail.
     */
    virtual ListRef<T> tail() const = 0;

    /**
     * Drop elements from the head of the list until the given
     * predicate returns false.
     *
     * @param predicate The function to evaluate and continue
     *                  dropping while it returns true.
     * @return A list with the head elements matching the predicate
     *         dropped.
     */
    virtual ListRef<T> dropWhile(const std::function<bool(const T&)>& predicate) const = 0;

    virtual ~List() = default;
};

} // namespace cask

#include "list/Nil.hpp"

namespace cask {

template <class T>
ListRef<T> List<T>::empty() {
    return list::Nil<T>::create();
}

} // namespace cask

#endif