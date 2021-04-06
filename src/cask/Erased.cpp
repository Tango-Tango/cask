#include "cask/Erased.hpp"

namespace cask {

Erased::Erased() noexcept
    : data(nullptr)
    , deleter()
    , copier()
{}

Erased::Erased(const Erased& other) noexcept
    : data(nullptr)
    , deleter()
    , copier()
{
    if(other.data != nullptr) {
        this->deleter = other.deleter;
        this->copier = other.copier;
        this->data = other.copier(other.data);
    }
}

Erased::Erased(Erased&& other) noexcept
    : data(other.data)
    , deleter(other.deleter)
    , copier(other.copier)
{
    other.data = nullptr;
}

Erased& Erased::operator=(const Erased& other) noexcept  {
    reset();
    if(other.data != nullptr) {
        this->deleter = other.deleter;
        this->copier = other.copier;
        this->data = other.copier(other.data);
    }
    return *this;
}

Erased& Erased::operator=(Erased&& other) noexcept  {
    reset();
    if(other.data != nullptr) {
        this->deleter = other.deleter;
        this->copier = other.copier;
        this->data = other.data;
        other.data = nullptr;
    }
    return *this;
}

bool Erased::has_value() const noexcept {
    return data != nullptr;
}

void Erased::reset() noexcept {
    if(data != nullptr) {
        deleter(data);
        data = nullptr;
    }
}

Erased::~Erased() {
    if(data != nullptr) {
        deleter(data);
        data = nullptr;
    }
}

} // namespace cask
