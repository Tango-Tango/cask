//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_ERASED_H_
#define _CASK_ERASED_H_

#include <atomic>
#include <stdexcept>
#include <functional>
#include <type_traits>
#include <typeinfo>
#include <stack>
#include "Pool.hpp"

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
              std::enable_if_t<!std::is_same<T,Erased>::value, bool> = true>
    Erased(const T& value) noexcept; // NOLINT(google-explicit-constructor)

    template <typename T,
              typename T2 = typename std::remove_const<typename std::remove_reference<T>::type>::type,
              std::enable_if_t<!std::is_same<T2,Erased>::value, bool> = true>
    Erased(T&& value) noexcept; // NOLINT(google-explicit-constructor,bugprone-forwarding-reference-overload)

    Erased& operator=(const Erased& other) noexcept;
    Erased& operator=(Erased&& other) noexcept;

    template <typename T,
              std::enable_if_t<!std::is_same<T,Erased>::value, bool> = true>
    Erased& operator=(const T& value) noexcept;

    template <typename T,
              typename T2 = typename std::remove_const<typename std::remove_reference<T>::type>::type,
              std::enable_if_t<!std::is_same<T2,Erased>::value, bool> = true>
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
    static Pool<128> pool;

    void* data;
    void (*deleter)(void*);
    void* (*copier)(void*);
    const std::type_info * info;
};

template <typename T, std::enable_if_t<!std::is_same<T,Erased>::value, bool>>
inline Erased::Erased(const T& value) noexcept
    : data(Erased::pool.allocate<T>(value))
    , deleter([](void* ptr) -> void { Erased::pool.deallocate<T>(static_cast<T*>(ptr)); })
    , copier([](void* ptr) -> void* { return Erased::pool.allocate<T>(*(static_cast<T*>(ptr))); })
    , info(&typeid(T))
{}

template <typename T, typename T2, std::enable_if_t<!std::is_same<T2,Erased>::value, bool>>
inline Erased::Erased(T&& value) noexcept {  // NOLINT(google-explicit-constructor,bugprone-forwarding-reference-overload)
    data = Erased::pool.allocate<T2>(value);
    deleter = [](void* ptr) -> void { Erased::pool.deallocate<T2>(static_cast<T2*>(ptr)); };
    copier = [](void* ptr) -> void* { return Erased::pool.allocate<T2>(*(static_cast<T2*>(ptr)));};
    info = &typeid(T2);
}

template <typename T, std::enable_if_t<!std::is_same<T,Erased>::value, bool>>
inline Erased& Erased::operator=(const T& value) noexcept {
    if(data == nullptr) {
        data = Erased::pool.allocate<T>(value);
        deleter = [](void* ptr) -> void { Erased::pool.deallocate<T>(static_cast<T*>(ptr)); };
        copier = [](void* ptr) -> void* { return Erased::pool.allocate<T>(*static_cast<T*>(ptr)); };
        info = &typeid(T);
    } else if(typeid(T) == *info) {
        *static_cast<T*>(data) = value;
    } else {
        deleter(data);
        data = Erased::pool.allocate<T>(value);
        deleter = [](void* ptr) -> void { Erased::pool.deallocate<T>(ptr); };
        copier = [](void* ptr) -> void* { return Erased::pool.allocate<T>(*static_cast<T*>(ptr)); };
        info = &typeid(T);
    }

    return *this;
}

template <typename T, typename T2, std::enable_if_t<!std::is_same<T2,Erased>::value, bool>>
inline Erased& Erased::operator=(T&& value) noexcept {
    if(data == nullptr) {
        data = Erased::pool.allocate<T>(value);
        deleter = [](void* ptr) -> void { Erased::pool.deallocate<T>(static_cast<T*>(ptr)); };
        copier = [](void* ptr) -> void* { return Erased::pool.allocate<T>(*static_cast<T*>(ptr)); };
        info = &typeid(T);
    } else if(typeid(T) == *info) {
        *static_cast<T*>(data) = value;
    } else {
        deleter(data);
        data = Erased::pool.allocate<T>(value);
        deleter = [](void* ptr) -> void { Erased::pool.deallocate<T>(static_cast<T*>(ptr)); };
        copier = [](void* ptr) -> void* { return Erased::pool.allocate<T>(*static_cast<T*>(ptr)); };
        info = &typeid(T);
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
    : data(nullptr)
    , deleter()
    , copier()
    , info(nullptr)
{}

inline Erased::Erased(const Erased& other) noexcept
    : data(nullptr)
    , deleter()
    , copier()
    , info(other.info)
{
    if(other.data != nullptr) {
        this->deleter = other.deleter;
        this->copier = other.copier;
        this->data = other.copier(other.data);
    }
}

inline Erased::Erased(Erased&& other) noexcept
    : data(other.data)
    , deleter(other.deleter)
    , copier(other.copier)
    , info(other.info)
{
    other.data = nullptr;
}

inline Erased& Erased::operator=(const Erased& other) noexcept {
    if(this != &other) {
        reset();
        if(other.data != nullptr) {
            this->deleter = other.deleter;
            this->copier = other.copier;
            this->data = other.copier(other.data);
            this->info = other.info;
        }
    }
    return *this;
}

inline Erased& Erased::operator=(Erased&& other) noexcept  {
    if(this != &other) {
        reset();
        if(other.data != nullptr) {
            this->deleter = other.deleter;
            this->copier = other.copier;
            this->data = other.data;
            this->info = other.info;
            other.data = nullptr;
        }
    }
    return *this;
}

inline bool Erased::has_value() const noexcept {
    return data != nullptr;
}

inline void Erased::reset() noexcept {
    if(data != nullptr) {
        deleter(data);
        data = nullptr;
    }
}

inline Erased::~Erased() { 
    if(data != nullptr) {
        deleter(data);
        data = nullptr;
    }
} // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks): delete is happening within the deleter lambda

} // namespace cask

#endif