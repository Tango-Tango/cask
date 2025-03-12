//          Copyright Tango Tango, Inc. 2020 - 2025.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace cask::scheduler {

/**
 * A simple thread-safe concurrent queue customized for the needs of the scheduler.
 */
class ReadyQueue {
public:
    explicit ReadyQueue(std::optional<std::size_t> max_queue_size = std::nullopt);

    /**
     * The current size of the queue.
     */
    std::size_t size() const;

    /**
     * Check if the queue is empty.
     */
    bool empty() const;

    /**
     * Wait (block) for work to become available.
     */
    template<class Rep, class Period>
    void await_work(const std::chrono::duration<Rep,Period>& timeout);

    /**
     * Push a task to the front of the queue - optionally pushing
     * a task off the back of the queue to make room. If a task
     * is pushed off the back of the queue it is returned. 
     */
    std::optional<std::function<void()>> push_front(const std::function<void()>& task);

    /**
     * Push a task to the back of the queue if there is room.
     */
    bool push_back(const std::function<void()>& task);

    /**
     * Push a batch of tasks to the back of the queue if there
     * is room for all tasks to fit.
     */
    bool push_batch_back(const std::vector<std::function<void()>>& batch);

    /**
     * Pop a task from the front of the queue.
     */
    std::optional<std::function<void()>> pop_front();

    /**
     * Pop a task from the back of the queue.
     */
    std::optional<std::function<void()>> pop_back();

    /**
     * Steal a task from the back of the victim queue
     * and push it to the front of our queue.
     */
    bool steal_from(ReadyQueue& victim);

    /**
     * Wake any threads waiting for work - regardless
     * of if work is actually available or not.
     */
    void wake();
private:
    const std::size_t max_queue_size;

    std::mutex mutex;
    std::condition_variable work_available;
    std::deque<std::function<void()>> ready_queue;
    std::atomic_size_t memoized_queue_size;
};

template<class Rep, class Period>
void ReadyQueue::await_work(const std::chrono::duration<Rep,Period>& timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    work_available.wait_for(lock, timeout);
}

} // namespace cask::scheduler