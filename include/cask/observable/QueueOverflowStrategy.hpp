//          Copyright Tango Tango, Inc. 2020 - 2023.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>

namespace cask::observable {

enum QueueOverflowStrategy : std::uint8_t {
    TailDrop,
    Backpressure
};

} // namespace cask::observable