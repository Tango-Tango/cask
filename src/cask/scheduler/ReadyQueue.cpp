//          Copyright Tango Tango, Inc. 2020 - 2025.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/ReadyQueue.hpp"

namespace cask::scheduler {

ReadyQueue::ReadyQueue(std::optional<std::size_t> max_queue_size)
    : max_queue_size(max_queue_size.value_or(std::numeric_limits<std::size_t>::max()))
    , mutex()
    , work_available()
    , ready_queue()
    , memoized_queue_size(0)
{}

std::size_t ReadyQueue::size() const {
    return memoized_queue_size.load(std::memory_order_relaxed);
}

bool ReadyQueue::empty() const {
    return memoized_queue_size.load(std::memory_order_relaxed) == 0;
}

std::optional<std::function<void()>> ReadyQueue::push_front(const std::function<void()>& task) {
    std::lock_guard<std::mutex> lock(mutex);

    if (ready_queue.size() + 1 > max_queue_size) {
        // Pop an item from the back of the queue and return it
        // as overflow, then push this item to the front.
        auto overflow = ready_queue.back();
        ready_queue.pop_back();
        ready_queue.emplace_front(task);
        return overflow;
    } else {
        ready_queue.emplace_front(task);
        memoized_queue_size.fetch_add(1, std::memory_order_relaxed);
        work_available.notify_one();
        return std::nullopt;
    }
}

bool ReadyQueue::push_back(const std::function<void()>& task) {
    std::lock_guard<std::mutex> lock(mutex);

    if (ready_queue.size() >= max_queue_size) {
        return false;
    } else {
        ready_queue.emplace_back(task);
        memoized_queue_size.fetch_add(1, std::memory_order_relaxed);
        work_available.notify_one();
        return true;
    }
}

bool ReadyQueue::push_batch_back(const std::vector<std::function<void()>>& batch) {
    std::lock_guard<std::mutex> lock(mutex);

    if (ready_queue.size() + batch.size() > max_queue_size) {
        return false;
    } else {
        for(auto& task: batch) {
            ready_queue.emplace_back(task);
        }
        memoized_queue_size.fetch_add(batch.size(), std::memory_order_relaxed);
        work_available.notify_one();
        return true;
    }
}

std::optional<std::function<void()>> ReadyQueue::pop_front() {
    std::lock_guard<std::mutex> lock(mutex);
    if(ready_queue.empty()) {
        return std::nullopt;
    } else {
        auto task = ready_queue.front();
        ready_queue.pop_front();
        memoized_queue_size.fetch_sub(1, std::memory_order_relaxed);
        return task;
    }
}

std::optional<std::function<void()>> ReadyQueue::pop_back() {
    std::lock_guard<std::mutex> lock(mutex);
    if(ready_queue.empty()) {
        return std::nullopt;
    } else {
        auto task = ready_queue.back();
        ready_queue.pop_back();
        memoized_queue_size.fetch_sub(1, std::memory_order_relaxed);
        return task;
    }
}


bool ReadyQueue::steal_from(ReadyQueue& victim) {
    std::scoped_lock<std::mutex, std::mutex> lock(mutex, victim.mutex);

    if (ready_queue.size() >= max_queue_size || victim.ready_queue.empty()) {
        return false;
    } else {
        auto task = victim.ready_queue.back();
        victim.ready_queue.pop_back();
        ready_queue.emplace_front(task);
        memoized_queue_size.fetch_add(1, std::memory_order_relaxed);
        victim.memoized_queue_size.fetch_sub(1, std::memory_order_relaxed);
        work_available.notify_one();
        return true;
    }
}

void ReadyQueue::wake() {
    std::lock_guard<std::mutex> lock(mutex);
    work_available.notify_all();
}

} // namespace cask::scheduler