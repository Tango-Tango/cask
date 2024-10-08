//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/Scheduler.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"

namespace cask {

std::shared_ptr<Scheduler> Scheduler::global() {
    static std::shared_ptr<scheduler::WorkStealingScheduler> sched = std::make_shared<scheduler::WorkStealingScheduler>();
    return sched;
}

} // namespace cask