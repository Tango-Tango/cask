//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/fiber/FiberValue.hpp"

namespace cask::fiber {

FiberValue::FiberValue()
    : value()
    , error(false)
    , canceled(false)
{}

FiberValue::FiberValue(const Erased& value, bool error, bool canceled)
    : value(value)
    , error(error)
    , canceled(canceled)
{}

FiberValue::FiberValue(Erased&& value, bool error, bool canceled)
    : value(value)
    , error(error)
    , canceled(canceled)
{}

bool FiberValue::isValue() const {
    return !error && !canceled && value.has_value();
}

bool FiberValue::isError() const {
    return error;
}

bool FiberValue::isCanceled() const {
    return canceled;
}

void FiberValue::setValue(const Erased& value) {
    this->value = value;
    this->error = false;
    this->canceled = false;
}

void FiberValue::setValue(Erased&& value) {
    this->value = value;
    this->error = false;
    this->canceled = false;
}

void FiberValue::setError(const Erased& value) {
    this->value = value;
    this->error = true;
    this->canceled = false;
}

void FiberValue::setError(Erased&& value) {
    this->value = value;
    this->error = true;
    this->canceled = false;
}

void FiberValue::setCanceled() {
    this->value.reset();
    this->error = false;
    this->canceled = true;
}

std::optional<Erased> FiberValue::getValue() const {
    if(isValue()) {
        return value;
    } else {
        return {};
    }
}

std::optional<Erased> FiberValue::getError() const {
    if(isError()) {
        return value;
    } else {
        return {};
    }
}

const Erased& FiberValue::underlying() const {
    return value;
}


} // namespace cask::fiber