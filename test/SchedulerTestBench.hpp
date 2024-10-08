#pragma once

#include <functional>
#include <memory>
#include <string>
#include "cask/Scheduler.hpp"
#include "cask/scheduler/SingleThreadScheduler.hpp"
#include "cask/scheduler/WorkStealingScheduler.hpp"

struct SchedulerTestBenchEntry {
    std::function<std::shared_ptr<cask::Scheduler>()> factory;
    std::string name;
};

static const SchedulerTestBenchEntry SchedulerTestEntries[] = {
    { []() { return std::make_shared<cask::scheduler::SingleThreadScheduler>(); }, "SingleThreadScheduler" },
    { []() { return std::make_shared<cask::scheduler::WorkStealingScheduler>(2); }, "WorkStealingScheduler_2threads" },
    { []() { return std::make_shared<cask::scheduler::WorkStealingScheduler>(4); }, "WorkStealingScheduler_4threads" },
    { []() { return std::make_shared<cask::scheduler::WorkStealingScheduler>(8); }, "WorkStealingScheduler_8threads" },
};

class SchedulerTestBench : public ::testing::TestWithParam<SchedulerTestBenchEntry> {
public:
    std::shared_ptr<cask::Scheduler> sched;

    void awaitIdle() {
        const static std::chrono::milliseconds sleep_time(1);

        int num_retries = 60000;
        while(num_retries > 0) {
            if(sched->isIdle()) {
                return;
            } else {
                std::this_thread::sleep_for(sleep_time);
                num_retries--;
            }
        }

        FAIL() << "Expected scheduler to return to idle within 60 seconds.";
    }

protected:
    void SetUp() override {
        sched = GetParam().factory();
    }
};

#define GENERATE_SCHEDULER_TEST_BENCH_FIXTURE(testName) \
    class testName : public SchedulerTestBench {};


#define INSTANTIATE_SCHEDULER_TEST_BENCH_SUITE(testName) \
    GENERATE_SCHEDULER_TEST_BENCH_FIXTURE(testName) \
    INSTANTIATE_TEST_SUITE_P( \
        testName, \
        testName, \
        ::testing::ValuesIn(SchedulerTestEntries), \
        [](const ::testing::TestParamInfo<SchedulerTestBenchEntry>& info) { \
            return info.param.name; \
        })
