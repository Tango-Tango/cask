//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "cask/scheduler/ThreadPoolScheduler.hpp"
#include <chrono>
#include <cstdlib>

namespace cask::scheduler {

ThreadPoolScheduler::ThreadPoolScheduler(unsigned int poolSize)
    : should_run(true)
    , readyQueueMutex()
    , dataInQueue()
    , readyQueue()
    , idleThreads(poolSize)
    , timerMutex()
    , timers()
    , threadStatus()
    , timerThreadStatus(false)
    , next_id(0)
    , last_execution_ms(current_time_ms())
{
    threadStatus.reserve(poolSize);

#if defined(__linux__) && defined(_GNU_SOURCE)
    int thread_offset = std::rand();
#endif

    for(unsigned int i = 0; i < poolSize; i++) {
        threadStatus.push_back(new std::atomic_bool(false));
        std::thread poolThread(std::bind(&ThreadPoolScheduler::run, this, i));

#if defined(__linux__) && defined(_GNU_SOURCE)
        auto handle = poolThread.native_handle();

        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        int core_id = (i + thread_offset) % std::thread::hardware_concurrency();

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
#endif

        poolThread.detach();
    }

    std::thread timerThread(std::bind(&ThreadPoolScheduler::timer, this));
    timerThread.detach();

    for(unsigned int i = 0; i < poolSize; i++) {
        while(!threadStatus[i]->load());
    }

    while(!timerThreadStatus.load());
}

ThreadPoolScheduler::~ThreadPoolScheduler() {
    should_run.store(false);
    timerCondition.notify_one();

    for(auto& t : threadStatus) {
        while(t->load());
        delete t;
    }

    while(timerThreadStatus.load());
}

void ThreadPoolScheduler::submit(const std::function<void()>& task) {
    {
        std::lock_guard<std::mutex> guard(readyQueueMutex);
        readyQueue.emplace(task);
    }
    dataInQueue.notify_one();
}

void ThreadPoolScheduler::submitBulk(const std::vector<std::function<void()>>& tasks) {
    std::lock_guard<std::mutex> guard(readyQueueMutex);
    for(auto& task: tasks) {
        readyQueue.emplace(task);
        dataInQueue.notify_one();
    }
}

CancelableRef ThreadPoolScheduler::submitAfter(int64_t milliseconds, const std::function<void()>& task) {
    auto id = next_id++;
    auto executionTick = current_time_ms() + milliseconds;

    {
        std::lock_guard<std::mutex> guard(timerMutex);

        auto tasks = timers.find(executionTick);
        auto entry = std::make_tuple(id, task);
        
        if(tasks == timers.end()) {
            std::vector<TimerEntry> taskVector = {entry};
            timers[executionTick] = taskVector;
        } else {
            tasks->second.push_back(entry);
        }
    }

    auto cancelable = std::make_shared<CancelableTimer>(
        this->shared_from_this(),
        executionTick,
        id
    );

    timerCondition.notify_one();

    return cancelable;
}

bool ThreadPoolScheduler::isIdle() const {
    return idleThreads.load() == threadStatus.size() && readyQueue.empty();
}

void ThreadPoolScheduler::run(unsigned int thread_index) {
    std::unique_lock<std::mutex> readyQueueLock(readyQueueMutex, std::defer_lock);
    std::chrono::milliseconds max_wait_time(10);
    std::function<void()> task;
    bool idling = true;

    threadStatus[thread_index]->store(true);

    while(should_run.load()) {
        readyQueueLock.lock();

        if(!idling && readyQueue.empty()) {
            idling = true;
            idleThreads++;
        }

        if(dataInQueue.wait_for(readyQueueLock, max_wait_time, [this](){return !readyQueue.empty(); })) {
            if(idling) {
                idling = false;
                idleThreads--;
            }
            
            task = readyQueue.front();
            readyQueue.pop();
            readyQueueLock.unlock();
            task();
        } else {
            readyQueueLock.unlock();
        }
    }

    threadStatus[thread_index]->store(false);
}

void ThreadPoolScheduler::timer() {
    timerThreadStatus.store(true, std::memory_order_relaxed);

    auto dormant_sleep_time = std::chrono::milliseconds(1000);
    auto sleep_time = dormant_sleep_time;

    while(true) {
        auto current_time = current_time_ms();
        std::unique_lock<std::mutex> lock(timerMutex);
        timerCondition.wait_for(lock, sleep_time);

        if (should_run.load(std::memory_order_relaxed)) {
            std::vector<int64_t> expired;

            // Submit any tasks associated with an expired
            // timer to the scheduler
            for (auto& [timer_time, entries] : timers) {
                if (timer_time <= current_time) {
                    std::vector<std::function<void()>> submittedTasks;

                    submittedTasks.reserve(entries.size());

                    for(auto& entry : entries) {
                        submittedTasks.emplace_back(std::get<1>(entry));
                    }

                    submitBulk(submittedTasks);
                    expired.push_back(timer_time);
                } else {
                    break;
                }
            }

            // Erase those timers from timer storage
            for (auto& timer_time : expired) {
                timers.erase(timer_time);
            }

            // Compute the next sleep time
            if (timers.empty()) {
                sleep_time = dormant_sleep_time;
            } else {
                auto next_timer = timers.begin();
                auto next_timer_time = next_timer->first;
                auto next_timer_sleep_time = std::chrono::milliseconds(next_timer_time - current_time);

                next_timer_sleep_time = std::min(next_timer_sleep_time, dormant_sleep_time);
                next_timer_sleep_time = std::max(next_timer_sleep_time, std::chrono::milliseconds(0));

                sleep_time = std::chrono::milliseconds(next_timer_sleep_time);
            }
        } else {
            break;
        }
    }

    timerThreadStatus.store(false, std::memory_order_relaxed);
}

int64_t ThreadPoolScheduler::current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

ThreadPoolScheduler::CancelableTimer::CancelableTimer(
    const std::shared_ptr<ThreadPoolScheduler>& parent,
    int64_t time_slot,
    int64_t id
)   : parent(parent)
    , time_slot(time_slot)
    , id(id)
    , callbacks()
    , callback_mutex()
    , canceled(false)
{}

void ThreadPoolScheduler::CancelableTimer::cancel() {
    if(canceled) {
        return;
    } else {
        std::lock_guard<std::mutex> parent_guard(parent->timerMutex);
        std::lock_guard<std::mutex> self_guard(callback_mutex);
        auto tasks = parent->timers.find(time_slot);
        if(tasks != parent->timers.end()) {
            std::vector<TimerEntry> filteredEntries;
            auto entries = tasks->second;

            for(auto& entry : entries) {
                auto entry_id = std::get<0>(entry);
                if(entry_id != id) {
                    filteredEntries.emplace_back(entry);
                } else {
                    canceled = true;
                }
            }

            if(filteredEntries.size() > 0) {
                parent->timers[time_slot] = filteredEntries;
            } else {
                parent->timers.erase(time_slot);
            }
        }
    }

    if(canceled) {
        for(auto& cb : callbacks) {
            cb();
        }
    }
}

void ThreadPoolScheduler::CancelableTimer::onCancel(const std::function<void()>& callback) {
    if(canceled) {
        callback();
    } else {
        std::lock_guard<std::mutex> guard(callback_mutex);
        callbacks.emplace_back(callback);
    }
}

void ThreadPoolScheduler::CancelableTimer::onShutdown(const std::function<void()>& shutdownCallback) {
    bool found = false;

    {
        std::lock_guard<std::mutex> parent_guard(parent->timerMutex);
        std::lock_guard<std::mutex> self_guard(callback_mutex);
        auto tasks = parent->timers.find(time_slot);

        if(tasks != parent->timers.end()) {
            std::vector<TimerEntry> newEntries;
            auto entries = tasks->second;

            for(auto& entry : entries) {
                auto entryId = std::get<0>(entry);
                auto entryCallback = std::get<1>(entry);
                if(entryId != id) {
                    newEntries.emplace_back(entry);
                } else {
                    found = true;
                    newEntries.emplace_back(entryId, [entryCallback, shutdownCallback]() {
                        entryCallback();
                        shutdownCallback();
                    });
                }
            }

            parent->timers[time_slot] = newEntries;
        }
    }

    if(!found) {
        shutdownCallback();
    }
}

} // namespace cask::scheduler