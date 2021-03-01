//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_REF_H_
#define _CASK_REF_H_

#include "Task.hpp"
#include <atomic>

namespace cask {

/**
 * A Ref is a data holder type that allows fast lockless access to an
 * underlying piece of state. The stored state _must_ represent an 
 * immutable value or immutable persistent datastore. Do not use
 * STL or other mutable data strutures with Ref.
 * 
 * Updates to the store are handled by providing mutator methods via
 * update. Updates are evaluated optimistically and in the event
 * that an update could not be achieved due to a parallel update
 * happening during the read-modify-write cycle then update
 * operation will be retried. Under contention, this means that
 * supplied updater methods may be called multiple times (hence
 * the need to avoid mutable side-effecting structures).
 * 
 * Under lightly contended load the result is a synchronized type
 * which provides extremely low overhead.
 * 
 * If you need to use a mutable structure look to MVar instead. Its
 * more pessimistic (and correspondingly more heavyweight) approach
 * works better with those structures.
 */
template <class T, class E = std::any>
class Ref : public std::enable_shared_from_this<Ref<T,E>> {
public:
    /**
     * Create a new Ref instance which stores the given initial value.
     *
     * @param initialValue The initial value of the ref.
     * @return A new ref storing this value.
     */
    static std::shared_ptr<Ref<T,E>> create(const T& initialValue);

    /**
     * Retrieve the currently stored value.
     * 
     * @return A task which, when run, will provide the value.
     */
    Task<T,E> get();

    /**
     * Update the stored value using the given mutator function.
     * 
     * @return A task which, when run, will update the stored value.
     */
    Task<None,E> update(std::function<T(T&)> predicate);

    /**
     * Modify the stored value using the given mutator function which
     * also provides a return value for the original caller.
     *
     * @param predicate The mutator method which returns updated state
     *                  as the first element of a tuple and a value
     *                  for the caller as the second element.
     * @return A task which will update the stored value and then provide
     *         the return value provided by the predicate function.
     */
    template <typename U>
    Task<U,E> modify(std::function<std::tuple<T,U>(T&)> predicate);
private:
    Ref(const T& initialValue);
    std::shared_ptr<T> data;
};

template <class T, class E>
std::shared_ptr<Ref<T,E>> Ref<T,E>::create(const T& initialValue) {
    return std::shared_ptr<Ref<T,E>>(new Ref<T,E>(initialValue));
}

template <class T, class E>
Ref<T,E>::Ref(const T& initialValue)
    : data(std::make_shared<T>(initialValue))
{}

template <class T, class E>
Task<T,E> Ref<T,E>::get() {
    auto self = this->shared_from_this();
    return Task<T,E>::eval([self]() {
        return *(self->data);
    });
}

template <class T, class E>
Task<None,E> Ref<T,E>::update(std::function<T(T&)> predicate) {
    auto self = this->shared_from_this();
    return Task<bool,E>::eval([self, predicate]() {
        auto initial = std::atomic_load(&(self->data));
        auto updated = std::make_shared<T>(predicate(*initial));
        return std::atomic_compare_exchange_weak(&(self->data), &initial, updated);
    }).restartUntil([](auto exchangeCompleted) {
        return exchangeCompleted;
    }).template map<None>([](auto) {
        return None();
    });
}

template <class T, class E>
template <typename U>
Task<U,E> Ref<T,E>::modify(std::function<std::tuple<T,U>(T&)> predicate) {
    using InternalResult = std::tuple<bool,U>;

    auto self = this->shared_from_this();
    return Task<InternalResult,E>::eval([self, predicate]() {
        auto initial = std::atomic_load(&(self->data));
        auto [updated, result] = predicate(*initial);
        auto updatedRef = std::make_shared<T>(updated);
        auto exchangeCompleted = std::atomic_compare_exchange_weak(&(self->data), &initial, updatedRef);
        return std::make_tuple(exchangeCompleted, result);
    }).restartUntil([](InternalResult resultPair) {
        auto [exchangeCompleted, result] = resultPair;
        return exchangeCompleted;
    }).template map<U>([](InternalResult resultPair) {
        auto [exchangeCompleted, result] = resultPair;
        return result;
    });
}

}

#endif