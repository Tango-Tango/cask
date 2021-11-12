//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_FIBER_VALUE_H_
#define _CASK_FIBER_VALUE_H_

#include "Erased.hpp"

namespace cask {

class FiberValue {
public:
    FiberValue();
    FiberValue(const Erased& value, bool error, bool canceled);
    FiberValue(Erased&& value, bool error, bool canceled);

    bool isValue() const;
    bool isError() const;
    bool isCanceled() const;

    void setValue(const Erased& value);
    void setValue(Erased&& value);

    void setError(const Erased& value);
    void setError(Erased&& value);

    void setCanceled();

    std::optional<Erased> getValue() const;
    std::optional<Erased> getError() const;

private:
    Erased value;
    bool error;
    bool canceled;
};

}

#endif