//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_CURRENT_FIBER_H_
#define _CASK_CURRENT_FIBER_H_

#include <atomic>
#include <cstdint>
#include <optional>

namespace cask::fiber {

template <class T, class E>
class FiberImpl;

class CurrentFiber {
public:
    static std::optional<uint64_t> id();
private:
    template <class T, class E>
    friend class FiberImpl;

    static uint64_t acquireId();
    static void setId(uint64_t id);
    static void clear();
    static std::atomic_uint64_t next_id;
    static thread_local std::optional<uint64_t> current_id;
};

} // namespace cask::fiber

#endif
