#ifndef _CASK_ERASED_H_
#define _CASK_ERASED_H_

#include <stdexcept>
#include <functional>
#include <type_traits>
#include <typeinfo>

namespace cask {

class Erased {
public:
    Erased() noexcept;
    Erased(const Erased& other) noexcept;
    Erased(Erased&& other) noexcept;

    template <typename T,
              std::enable_if_t<!std::is_same<T,Erased>::value, bool> = true>
    Erased(const T& value) noexcept;

    template <typename T,
              typename T2 = typename std::remove_const<typename std::remove_reference<T>::type>::type,
              std::enable_if_t<!std::is_same<T2,Erased>::value, bool> = true>
    Erased(T&& value) noexcept;

    Erased& operator=(const Erased& other) noexcept;
    Erased& operator=(Erased&& other) noexcept;

    template <typename T,
              std::enable_if_t<!std::is_same<T,Erased>::value, bool> = true>
    Erased& operator=(const T& value) noexcept;

    template <typename T,
              typename T2 = typename std::remove_const<typename std::remove_reference<T>::type>::type,
              std::enable_if_t<!std::is_same<T2,Erased>::value, bool> = true>
    Erased& operator=(T&& value) noexcept;

    bool has_value() const noexcept;

    template <typename T>
    T& get() const;

    void reset() noexcept;

    ~Erased();
private:
    void* data;
    void (*deleter)(void*);
    void* (*copier)(void*);
    const std::type_info * info;
};

template <typename T, std::enable_if_t<!std::is_same<T,Erased>::value, bool>>
inline Erased::Erased(const T& value) noexcept
    : data(new T(value))
    , deleter([](void* ptr) -> void { delete static_cast<T*>(ptr);})
    , copier([](void* ptr) -> void* { return new T(*(static_cast<T*>(ptr))); })
    , info(&typeid(T))
{}

template <typename T, typename T2, std::enable_if_t<!std::is_same<T2,Erased>::value, bool>>
inline Erased::Erased(T&& value) noexcept {
    data = new T2(value);
    deleter = [](void* ptr) -> void { delete static_cast<T2*>(ptr);};
    copier = [](void* ptr) -> void* { return new T2(*(static_cast<T2*>(ptr))); };
    info = &typeid(T2);
}

template <typename T, std::enable_if_t<!std::is_same<T,Erased>::value, bool>>
inline Erased& Erased::operator=(const T& value) noexcept {
    if(data == nullptr) {
        data = new T(value);
        deleter = [](void* ptr) -> void { delete static_cast<T*>(ptr);};
        copier = [](void* ptr) -> void* { return new T(*(static_cast<T*>(ptr))); };
        info = &typeid(T);
    } else if(typeid(T) == *info) {
        *static_cast<T*>(data) = value;
    } else {
        deleter(data);
        data = new T(value);
        deleter = [](void* ptr) -> void { delete static_cast<T*>(ptr);};
        copier = [](void* ptr) -> void* { return new T(*(static_cast<T*>(ptr))); };
        info = &typeid(T);
    }

    return *this;
}

template <typename T, typename T2, std::enable_if_t<!std::is_same<T2,Erased>::value, bool>>
inline Erased& Erased::operator=(T&& value) noexcept {
    if(data == nullptr) {
        data = new T(value);
        deleter = [](void* ptr) -> void { delete static_cast<T*>(ptr);};
        copier = [](void* ptr) -> void* { return new T(*(static_cast<T*>(ptr))); };
        info = &typeid(T);
    } else if(typeid(T) == *info) {
        *static_cast<T*>(data) = value;
    } else {
        deleter(data);
        data = new T(value);
        deleter = [](void* ptr) -> void { delete static_cast<T*>(ptr);};
        copier = [](void* ptr) -> void* { return new T(*(static_cast<T*>(ptr))); };
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

inline Erased& Erased::operator=(const Erased& other) noexcept  {
    reset();
    if(other.data != nullptr) {
        this->deleter = other.deleter;
        this->copier = other.copier;
        this->data = other.copier(other.data);
        this->info = other.info;
    }
    return *this;
}

inline Erased& Erased::operator=(Erased&& other) noexcept  {
    reset();
    if(other.data != nullptr) {
        this->deleter = other.deleter;
        this->copier = other.copier;
        this->data = other.data;
        this->info = other.info;
        other.data = nullptr;
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

}

#endif