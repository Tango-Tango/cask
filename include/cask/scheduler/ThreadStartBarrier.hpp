//          Copyright Tango Tango, Inc. 2020 - 2025.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <condition_variable>
#include <mutex>

namespace cask::scheduler {

/**
 * A simple barrier used by the scheduler to indicate when
 * a the background processing thread should start running.
 */
class ThreadStartBarrier final {
public:
    ThreadStartBarrier();

    /**
     * Wait for the barrier to be notified.
     */
    void wait();

    /**
     * Notify the barrier that it may proceed.
     */
    void notify();
private:
    std::mutex mutex;
    std::condition_variable cv;
    bool started;
};

} // namespace cask::scheduler