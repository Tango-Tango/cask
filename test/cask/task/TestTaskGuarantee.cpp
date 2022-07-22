//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Task.hpp"
#include "cask/scheduler/BenchScheduler.hpp"
#include <exception>

using cask::None;
using cask::Scheduler;
using cask::Task;
using cask::scheduler::BenchScheduler;

TEST(TaskGuarantee, RunsOnComplete) {
    auto counter = 0;
    auto result = Task<int>::pure(123)
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, 123);
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, RunsOnError) {
    auto counter = 0;
    auto result = Task<int, std::string>::raiseError("broke")
        .guarantee(Task<None, std::string>::eval([&counter] {
            counter++;
            return None();
        }))
        .failed()
        .run(Scheduler::global())
        ->await();
    
    EXPECT_EQ(result, "broke");
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, RunsOnCancelAfterWaiting) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;
    auto deferred = Task<int>::never()
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(sched);

    sched->run_ready_tasks();
    deferred->cancel();
    sched->run_ready_tasks();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, AlwaysRuns) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;
    auto deferred = Task<int>::never()
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(sched);

    deferred->cancel();
    sched->run_ready_tasks();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, StackedGuarantees) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;
    auto deferred = Task<int>::never()
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(sched);

    deferred->cancel();
    sched->run_ready_tasks();

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 2);
}

TEST(TaskGuarantee, StackedAsyncGuarantee) {
    auto sched = std::make_shared<BenchScheduler>();
    auto promise = cask::Promise<None>::create(sched);
    auto counter = 0;
    auto deferred = Task<int>::never()
        .guarantee(Task<None>::deferAction([&counter, promise](auto) {
            counter++;
            return cask::Deferred<None>::forPromise(promise);
        }))
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(sched);

    deferred->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(counter, 1);

    promise->success(None());
    sched->run_ready_tasks();
    EXPECT_EQ(counter, 2);

    try {
        deferred->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 2);
}

TEST(TaskGuarantee, RacedGuarantees) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;
    auto first_task = Task<int>::never()
        .guarantee(Task<None>::eval([&counter] {
            std::cout << "First Task Guarantee!" << std::endl;
            counter++;
            return None();
        }));

    auto second_task = Task<int>::never()
        .guarantee(Task<None>::eval([&counter] {
            std::cout << "Second Task Guarantee!" << std::endl;
            counter++;
            return None();
        }));

    auto third_task = Task<int>::never()
        .guarantee(Task<None>::eval([&counter] {
            std::cout << "Third Task Guarantee!" << std::endl;
            counter++;
            return None();
        }));

    auto fiber = first_task
        .raceWith(second_task)
        .raceWith(third_task)
        .guarantee(Task<None>::eval([&counter] {
            std::cout << "Fiber Guarantee!" << std::endl;
            counter++;
            return None();
        }))
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(counter, 4);

    try {
        fiber->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 4);
}

TEST(TaskGuarantee, GuaranteePureValueRace) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;

    auto fiber = Task<int>::pure(1)
        .raceWith(Task<int>::pure(2))
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(sched);

    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(counter, 1);

    try {
        fiber->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, GuaranteePureNeverRace) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;

    auto fiber = Task<int>::pure(1)
        .raceWith(Task<int>::never())
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(sched);

    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(counter, 1);

    try {
        fiber->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, GuaranteeNeverPureRace) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;

    auto fiber = Task<int>::never()
        .raceWith(Task<int>::pure(1))
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(sched);

    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(counter, 1);

    try {
        fiber->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, GuaranteeNeverNeverRace) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;

    auto fiber = Task<int>::never()
        .raceWith(Task<int>::never())
        .guarantee(Task<None>::eval([&counter] {
            counter++;
            return None();
        }))
        .run(sched);

    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(counter, 1);

    try {
        fiber->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, RaceGuaranteedInnerTask) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;

    auto fiber = Task<int>::never()
        .raceWith(
            Task<int>::never().guarantee(Task<None>::eval([&counter] {
                counter++;
                return None();
            })
        ))
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(counter, 1);

    try {
        fiber->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 1);
}

TEST(TaskGuarantee, RaceDoOnCancelInnerTask) {
    auto sched = std::make_shared<BenchScheduler>();
    auto counter = 0;

    auto fiber = Task<int>::never()
        .raceWith(
            Task<int>::never().doOnCancel(Task<None,None>::eval([&counter] {
                counter++;
                return None();
            })
        ))
        .run(sched);

    sched->run_ready_tasks();
    fiber->cancel();
    sched->run_ready_tasks();

    EXPECT_EQ(counter, 1);

    try {
        fiber->await();
        FAIL() << "Expected method to throw";
    } catch(std::runtime_error&) {}
    
    EXPECT_EQ(counter, 1);
}
