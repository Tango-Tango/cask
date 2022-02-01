//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_SWITCH_MAP_STATE_H_
#define _CASK_SWITCH_MAP_STATE_H_

#include "../../Fiber.hpp"
#include "../../Observer.hpp"

namespace cask::observable::switchmap {

struct SwitchMapState {
    FiberRef<None,None> subscription;
    Ack downstream_ack;
    bool upstream_completed;
    bool subscription_completed;

    SwitchMapState()
        : subscription(nullptr)
        , downstream_ack(Continue)
        , upstream_completed(false)
        , subscription_completed(true)
    {}
};

} // namespace cask::observable::switchmap

#endif