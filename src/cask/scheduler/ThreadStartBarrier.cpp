//          Copyright Tango Tango, Inc. 2020 - 2025.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/ThreadStartBarrier.hpp"

namespace cask::scheduler {

ThreadStartBarrier::ThreadStartBarrier()
    : mutex()
    , cv()
    , started(false)
{}

void ThreadStartBarrier::wait() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] { return started; });
}

void ThreadStartBarrier::notify() {
    std::lock_guard<std::mutex> lock(mutex);
    started = true;
    cv.notify_all();
}
    
} // namespace cask::scheduler
