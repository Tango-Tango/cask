//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_LIST_NIL_H_
#define _CASK_LIST_NIL_H_

#include "../List.hpp"

namespace cask::list {

/**
 * Represents the empty list that holds no values and has no tail.
 */
template <class T>
class Nil final : public List<T>, public std::enable_shared_from_this<Nil<T>> {
public:
    static ListRef<T> create();

    ListRef<T> prepend(const T& elem) override;
    ListRef<T> append(const T& elem) override;
    bool is_empty() const override;
    std::optional<T> head() const override;
    ListRef<T> tail() override;
    ListRef<T> dropWhile(const std::function<bool(const T&)>& predicate) override;
};

}

#include "ListEntry.hpp"

namespace cask::list {

template <class T>
ListRef<T> Nil<T>::create() {
    return std::make_shared<Nil<T>>();
}

template <class T>
ListRef<T> Nil<T>::prepend(const T& elem) {
    return ListEntry<T>::create(elem, this->shared_from_this());
}

template <class T>
ListRef<T> Nil<T>::append(const T& elem) {
    return ListEntry<T>::create(elem, this->shared_from_this());
}

template <class T>
bool Nil<T>::is_empty() const {
    return true;
}

template <class T>
std::optional<T> Nil<T>::head() const {
    return {};
}

template <class T>
ListRef<T> Nil<T>::tail() {
    return this->shared_from_this();
}

template <class T>
ListRef<T> Nil<T>::dropWhile(const std::function<bool(const T&)>&) {
    return this->shared_from_this();
}

}

#endif