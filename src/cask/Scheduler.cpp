//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Scheduler.hpp"
#include "cask/scheduler/ThreadPoolScheduler.hpp"

namespace cask {

std::shared_ptr<Scheduler> Scheduler::global() {
    static std::shared_ptr<scheduler::ThreadPoolScheduler> sched = std::make_shared<scheduler::ThreadPoolScheduler>();
    return sched;
}

} // namespace cask