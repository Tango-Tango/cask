#ifndef _CASK_ERASED_H_
#define _CASK_ERASED_H_

#include <functional>

namespace cask {

class Erased {
public:
    Erased() noexcept;
    Erased(const Erased& other) noexcept;
    Erased(Erased&& other) noexcept;

    template <class T>
    Erased(const T& value) noexcept;

    Erased& operator=(const Erased& other) noexcept;
    Erased& operator=(Erased&& other) noexcept;

    template <class T>
    Erased& operator=(const T& value) noexcept;

    bool has_value() const noexcept;

    template <class T>
    T& get() const;

    void reset() noexcept;

    ~Erased();
private:
    void* data;
    void (*deleter)(void*);
    void* (*copier)(void*);
};

template <class T>
Erased::Erased(const T& value) noexcept
    : data(new T(value))
    , deleter([](void* ptr) -> void { delete static_cast<T*>(ptr);})
    , copier([](void* ptr) -> void* { return new T(*(static_cast<T*>(ptr))); })
{}

template <class T>
Erased& Erased::operator=(const T& value) noexcept {
    reset();
    data = new T(value);
    deleter = [](void* ptr) -> void { delete static_cast<T*>(ptr);};
    copier = [](void* ptr) -> void* { return new T(*(static_cast<T*>(ptr))); };
    return *this;
}

template <class T>
T& Erased::get() const {
    return *(static_cast<T*>(data));
}

}

#endif