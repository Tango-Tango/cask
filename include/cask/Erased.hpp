//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_ERASED_H_
#define _CASK_ERASED_H_

#include "cask/Config.hpp"

#include <atomic>
#include <stdexcept>
#include <functional>
#include <type_traits>
#include <typeinfo>
#include <stack>
#include "cask/pool/InternalPool.hpp"

namespace cask {

/**
 * A holder for a type-erased value. This type can hold any other type and will
 * properly construct/destruct the value based on its real type. In many ways it
 * is similiar to `std::any` except it is _far_ less safe. Rather than checking
 * and validating type information at runtime, this type assumes that the caller
 * _really_ knows what they are doing. In the context of cask, it is used by
 * `Task` since its template layer validates these types at compile time.
 * 
 * Does the idea of a type blowing up on you because you don't pass correct
 * type arguments to it later scare you? Good. Don't use this. It serves a very
 * specific purpose for cask - and beyond that it has behavior that might not
 * even be considered a good idea.
 */
class Erased {
public:
    Erased() noexcept;
    Erased(const Erased& other) noexcept;
    Erased(Erased&& other) noexcept;

    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>,Erased>::value>>
    Erased(const T& value) noexcept; // NOLINT(google-explicit-constructor)

    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>,Erased>::value>>
    Erased(T&& value) noexcept; // NOLINT(google-explicit-constructor)

    Erased& operator=(const Erased& other) noexcept;
    Erased& operator=(Erased&& other) noexcept;

    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>,Erased>::value>>
    Erased& operator=(const T& value) noexcept;

    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>,Erased>::value>>
    Erased& operator=(T&& value) noexcept;

    /**
     * Check if this instance is currently holding a value.
     * 
     * @return true iff this instance is currently holding a value.
     */
    bool has_value() const noexcept;

    /**
     * Get the value held by this instance - casting it to the
     * proper type. The behavior of casting to the wrong type
     * is undefined (it's a blind cast under the hood) so be
     * _sure_ that this type is correct. This method throws
     * an exception if the user attempts to obtain a value but
     * no value is available.
     * 
     * @return The casted value.
     */
    template <typename T>
    T& get() const;

    /**
     * If this instance is currently holding a value then free it.
     * Afterwards this instance will not hold a value. If the instance
     * is already not holding a value - then nothing is done.
     */
    void reset() noexcept;

    ~Erased();
private:
    std::shared_ptr<Pool> pool;
    void* data;
    void (*deleter)(void*, const std::shared_ptr<Pool>& pool);
    void* (*copier)(void*, const std::shared_ptr<Pool>& pool);
    const std::type_info * info;
};

template <typename T, typename>
inline Erased::Erased(const T& value) noexcept
    : pool(cask::pool::global_pool())
    , data(pool->allocate<T>(value))
    , deleter([](void* ptr, const std::shared_ptr<Pool>& pool) -> void { pool->deallocate<T>(static_cast<T*>(ptr)); })
    , copier([](void* ptr, const std::shared_ptr<Pool>& pool) -> void* { return pool->allocate<T>(*(static_cast<T*>(ptr))); })
    , info(&typeid(T))
{}

template <typename T, typename>
inline Erased::Erased(T&& value) noexcept
    : pool(cask::pool::global_pool())
    , data(pool->allocate<std::decay_t<T>>(std::forward<T>(value)))
    , deleter([](void* ptr, const std::shared_ptr<Pool>& pool) -> void { pool->deallocate<std::decay_t<T>>(static_cast<std::decay_t<T>*>(ptr)); })
    , copier([](void* ptr, const std::shared_ptr<Pool>& pool) -> void* { return pool->allocate<std::decay_t<T>>(*(static_cast<std::decay_t<T>*>(ptr))); })
    , info(&typeid(std::decay_t<T>))
{}

template <typename T, typename>
inline Erased& Erased::operator=(const T& value) noexcept {
    if(data == nullptr) {
        pool = cask::pool::global_pool();
        data = pool->allocate<T>(value);
        deleter = [](void* ptr, const std::shared_ptr<Pool>& pool) -> void { pool->deallocate<T>(static_cast<T*>(ptr)); };
        copier = [](void* ptr, const std::shared_ptr<Pool>& pool) -> void* { return pool->allocate<T>(*static_cast<T*>(ptr)); };
        info = &typeid(T);
    } else if(typeid(T) == *info) {
        *static_cast<T*>(data) = value;
    } else {
        deleter(data, pool);
        pool = cask::pool::global_pool();
        data = pool->allocate<T>(value);
        deleter = [](void* ptr, const std::shared_ptr<Pool>& pool) -> void { pool->deallocate<T>(static_cast<T*>(ptr)); };
        copier = [](void* ptr, const std::shared_ptr<Pool>& pool) -> void* { return pool->allocate<T>(*static_cast<T*>(ptr)); };
        info = &typeid(T);
    }

    return *this;
}

template <typename T, typename>
inline Erased& Erased::operator=(T&& value) noexcept {
    using DecayedT = std::decay_t<T>;
    if(data == nullptr) {
        pool = cask::pool::global_pool();
        data = pool->allocate<DecayedT>(std::forward<T>(value));
        deleter = [](void* ptr, const std::shared_ptr<Pool>& pool) -> void { pool->deallocate<DecayedT>(static_cast<DecayedT*>(ptr)); };
        copier = [](void* ptr, const std::shared_ptr<Pool>& pool) -> void* { return pool->allocate<DecayedT>(*static_cast<DecayedT*>(ptr)); };
        info = &typeid(DecayedT);
    } else if(typeid(DecayedT) == *info) {
        *static_cast<DecayedT*>(data) = std::forward<T>(value);
    } else {
        deleter(data, pool);
        pool = cask::pool::global_pool();
        data = pool->allocate<DecayedT>(std::forward<T>(value));
        deleter = [](void* ptr, const std::shared_ptr<Pool>& pool) -> void { pool->deallocate<DecayedT>(static_cast<DecayedT*>(ptr)); };
        copier = [](void* ptr, const std::shared_ptr<Pool>& pool) -> void* { return pool->allocate<DecayedT>(*static_cast<DecayedT*>(ptr)); };
        info = &typeid(DecayedT);
    }

    return *this;
}

template <typename T>
inline T& Erased::get() const {
    if(data != nullptr) {
        return *(static_cast<T*>(data));
    } else {
        throw std::runtime_error("Tried to obtain value for empty Erased container.");
    }
}

inline Erased::Erased() noexcept
    : pool()
    , data(nullptr)
    , deleter()
    , copier()
    , info(nullptr)
{}

inline Erased::Erased(const Erased& other) noexcept
    : pool()
    , data(nullptr)
    , deleter()
    , copier()
    , info(other.info)
{
    if(other.data != nullptr) {
        this->pool = other.pool;
        this->deleter = other.deleter;
        this->copier = other.copier;
        this->data = other.copier(other.data, other.pool);
    }
}

inline Erased::Erased(Erased&& other) noexcept
    : pool(std::move(other.pool))
    , data(other.data)
    , deleter(other.deleter)
    , copier(other.copier)
    , info(other.info)
{
    other.data = nullptr;
    other.deleter = nullptr;
    other.copier = nullptr;
    other.info = nullptr;
}

inline Erased& Erased::operator=(const Erased& other) noexcept {
    if(this != &other) {
        reset();
        if(other.data != nullptr) {
            this->pool = other.pool;
            this->deleter = other.deleter;
            this->copier = other.copier;
            this->data = other.copier(other.data, other.pool);
            this->info = other.info;
        }
    }
    return *this;
}

inline Erased& Erased::operator=(Erased&& other) noexcept {
    if(this != &other) {
        reset();
        this->pool = std::move(other.pool);
        this->data = other.data;
        this->deleter = other.deleter;
        this->copier = other.copier;
        this->info = other.info;
        other.data = nullptr;
        other.deleter = nullptr;
        other.copier = nullptr;
        other.info = nullptr;
    }
    return *this;
}

inline bool Erased::has_value() const noexcept {
    return data != nullptr;
}

inline void Erased::reset() noexcept {
    if(data != nullptr) {
        deleter(data, pool);
        data = nullptr;
        pool = nullptr;
    }
}

inline Erased::~Erased() { 
    if(data != nullptr) {
        deleter(data, pool);
        data = nullptr;
        pool = nullptr;
    }
} // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks): delete is happening within the deleter lambda

} // namespace cask

#endif