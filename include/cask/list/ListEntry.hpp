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

    ListRef<T> prepend(const T& elem) const override;
    ListRef<T> append(const T& elem) const override;
    bool is_empty() const override;
    std::size_t size() const override;
    std::optional<T> head() const override;
    ListRef<T> tail() const override;
    ListRef<T> dropWhile(const std::function<bool(const T&)>& predicate) const override;
    void foreach(const std::function<void(const T&)>& predicate) const override;

private:
    T headValue;
    ListRef<T> tailRef;
    std::size_t memoizedSize;
};

template <class T>
ListRef<T> ListEntry<T>::create(const T& head, ListRef<T> tail) {
    return std::make_shared<ListEntry<T>>(head, tail);
}

template <class T>
ListEntry<T>::ListEntry(const T& head, ListRef<T> tail)
    : headValue(head)
    , tailRef(tail)
    , memoizedSize(tail == nullptr ? 0 : tail->size() + 1)
{}

template <class T>
ListRef<T> ListEntry<T>::prepend(const T& elem) const {
    return ListEntry<T>::create(elem, this->shared_from_this());
}

template <class T>
ListRef<T> ListEntry<T>::append(const T& elem) const {
    std::shared_ptr<ListEntry<T>> headEntry = std::make_shared<ListEntry<T>>(headValue, nullptr);
    std::shared_ptr<ListEntry<T>> entry = headEntry;
    ListRef<T> original = this->shared_from_this();

    while(true) {
        if(auto next = std::dynamic_pointer_cast<const ListEntry<T>>(original->tail())) {
            auto newNext = std::make_shared<ListEntry<T>>(next->headValue, nullptr);
            entry->tailRef = newNext;
            entry->memoizedSize = original->size() + 1;
            entry = newNext;
            original = original->tail();
        } else {
            entry->tailRef = ListEntry<T>::create(elem, List<T>::empty());
            entry->memoizedSize = 2;
            break;
        }
    }

    return headEntry;
}

template <class T>
bool ListEntry<T>::is_empty() const {
    return false;
}

template <class T>
std::size_t ListEntry<T>::size() const {
    return memoizedSize;
}

template <class T>
std::optional<T> ListEntry<T>::head() const {
    return headValue;
}

template <class T>
ListRef<T> ListEntry<T>::tail() const {
    return tailRef;
}

template <class T>
ListRef<T> ListEntry<T>::dropWhile(const std::function<bool(const T&)>& predicate) const {
    ListRef<T> entry = this->shared_from_this();

    while(true) {
        auto valueOpt = entry->head();
        if(valueOpt.has_value()) {
            if(predicate(*valueOpt)) {
                entry = entry->tail();
            } else {
                break;
            }
        } else {
            break;
        }
    }

    return entry;
}

template <class T>
void ListEntry<T>::foreach(const std::function<void(const T&)>& predicate) const {
    ListRef<T> entry = this->shared_from_this();

    while(true) {
        auto valueOpt = entry->head();
        if(valueOpt.has_value()) {
            predicate(*valueOpt);
            entry = entry->tail();
        } else {
            break;
        }
    }
}

} // namespace cask::list

#endif
