//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_LIST_ENTRY_H_
#define _CASK_LIST_ENTRY_H_

#include "../List.hpp"

namespace cask::list {

/**
 * Represents a list that has a value and a tail (which may be empty).
 */
template <class T>
class ListEntry final : public List<T>, public std::enable_shared_from_this<ListEntry<T>> {
public:
    ListEntry(const T& head, ListRef<T> tail);
    static ListRef<T> create(const T& head, ListRef<T> tail);

    ListRef<T> prepend(const T& elem) override;
    ListRef<T> append(const T& elem) override;
    bool is_empty() const override;
    std::optional<T> head() const override;
    ListRef<T> tail() override;

private:
    T headValue;
    ListRef<T> tailRef;
};

template <class T>
ListRef<T> ListEntry<T>::create(const T& head, ListRef<T> tail) {
    return std::make_shared<ListEntry<T>>(head, tail);
}

template <class T>
ListEntry<T>::ListEntry(const T& head, ListRef<T> tail)
    : headValue(head)
    , tailRef(tail)
{}

template <class T>
ListRef<T> ListEntry<T>::prepend(const T& elem) {
    return ListEntry<T>::create(elem, this->shared_from_this());
}

template <class T>
ListRef<T> ListEntry<T>::append(const T& elem) {
    return ListEntry<T>::create(headValue, tailRef->append(elem));
}

template <class T>
bool ListEntry<T>::is_empty() const {
    return false;
}

template <class T>
std::optional<T> ListEntry<T>::head() const {
    return headValue;
}

template <class T>
ListRef<T> ListEntry<T>::tail() {
    return tailRef;
}

}

#endif
