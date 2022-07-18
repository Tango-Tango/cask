//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_REF_H_
#define _CASK_REF_H_

#include "Task.hpp"
#include "cask/Config.hpp"
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
    template <class... Args>
    static std::shared_ptr<Ref<T,E>> create(Args&&... args)  {
        auto value = T(std::forward<Args>(args)...);
        return std::make_shared<Ref<T,E>>(std::move(value));
    }

    /**
     * Retrieve the currently stored value.
     * 
     * @return A task which, when run, will provide the value.
     */
    Task<T,E> get()  {
        auto self = this->shared_from_this();
        return Task<T,E>::eval([self = std::move(self)]() {
            return *(self->data);
        });
    }

    /**
     * Update the stored value using the given mutator function.
     * 
     * @return A task which, when run, will update the stored value.
     */
    template <typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<T(const T&)>
        >::value
    >>
    Task<None,E> update(Predicate&& predicate)  {
        auto self = this->shared_from_this();

        if constexpr (ref_uses_atomics) {
            return Task<None,E>::eval([self = std::move(self), predicate = std::forward<Predicate>(predicate)]() {
                while(true) {
                    auto initial = std::atomic_load_explicit(&(self->data), std::memory_order_acquire);
                    auto updated = std::make_shared<T>(predicate(*initial));
                    if(std::atomic_compare_exchange_weak_explicit(&(self->data), &initial, updated, std::memory_order_release, std::memory_order_relaxed)) {
                        return None();
                    }
                }
            });
        } else {
            return Task<None,E>::eval([self = std::move(self), predicate = std::forward<Predicate>(predicate)]() {
                std::lock_guard guard(self->mutex);
                self->data = std::make_shared<T>(predicate(*(self->data)));
                return None();
            });
        }
    }

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
    template <typename U, typename Predicate, typename = std::enable_if_t<
        std::is_convertible<
            std::remove_reference_t<Predicate>,
            std::function<std::tuple<T,U>(const T&)>
        >::value
    >>
    Task<U,E> modify(Predicate&& predicate)  {
        auto self = this->shared_from_this();

        if constexpr (ref_uses_atomics) {
            return Task<U,E>::eval([self = std::move(self), predicate = std::forward<Predicate>(predicate)]() {
                while(true) {
                    auto initial = std::atomic_load_explicit(&(self->data), std::memory_order_acquire);
                    auto&& [updated, result] = predicate(*initial);
                    auto updatedRef = std::make_shared<T>(std::move(updated));
                    if(std::atomic_compare_exchange_weak_explicit(&(self->data), &initial, updatedRef, std::memory_order_release, std::memory_order_relaxed)) {
                        return std::move(result);
                    }
                }
            });
        } else {
            return Task<U,E>::eval([self = std::move(self), predicate = std::forward<Predicate>(predicate)]() {
                std::lock_guard guard(self->mutex);
                auto&& [updated, result] = predicate(*(self->data));
                self->data = std::make_shared<T>(std::move(updated));
                return std::move(result);
            });
        }
    }

    template <class... Args>
    explicit Ref(Args&&... args)
        : data(std::make_shared<T>(std::forward<Args>(args)...))
        , mutex()
    {}

private:
    std::shared_ptr<T> data;
    std::mutex mutex;
};

} // namespace cask

#endif