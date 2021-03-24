//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include "gtest/gtest.h"
#include "cask/Deferred.hpp"
#include "cask/None.hpp"
#include <chrono>
#include <thread>
#include <semaphore.h>

using cask::Promise;
using cask::Deferred;
using cask::None;
using cask::Scheduler;
using cask::Either;

TEST(Deferred, PureOnComplete) {
    Either<int,float> result = Either<int,float>::left(0);
    auto deferred = Deferred<int,float>::pure(123);

    deferred->onComplete([&result](auto value) {
        result = value;
    });

    EXPECT_EQ(result.get_left(), 123);
}

TEST(Deferred, PureOnSuccess) {
    int result = 0;
    auto deferred = Deferred<int,float>::pure(123);

    deferred->onSuccess([&result](auto value) {
        result = value;
    });

    EXPECT_EQ(result, 123);
}

TEST(Deferred, PureOnError) {
    int result = 0;
    auto deferred = Deferred<int,float>::pure(123);

    deferred->onError([&result](auto) {
        result = 123;
    });

    EXPECT_EQ(result, 0);
}

TEST(Deferred, PureAwait) {
    auto deferred = Deferred<int>::pure(123);
    EXPECT_EQ(deferred->await(), 123);
}

TEST(Deferred, PureIgnoresCancel) {
    auto deferred = Deferred<int, std::string>::pure(123);
    deferred->cancel();
    EXPECT_EQ(deferred->await(), 123);
}

TEST(Deferred, ErrorOnComplete) {
    Either<int,std::string> result = Either<int,std::string>::right("not broke");
    auto deferred = Deferred<int,std::string>::raiseError("broke");

    deferred->onComplete([&result](auto value) {
        result = value;
    });

    EXPECT_EQ(result.get_right(), "broke");
}

TEST(Deferred, ErrorOnSuccess) {
    std::string result = "not called";
    auto deferred = Deferred<int,std::string>::raiseError("broke");

    deferred->onSuccess([&result](auto) {
        result = "success?";
    });

    EXPECT_EQ(result, "not called");
}

TEST(Deferred, ErrorOnError) {
    std::string result = "not called";
    auto deferred = Deferred<int,std::string>::raiseError("broke");

    deferred->onError([&result](auto value) {
        result = value;
    });

    EXPECT_EQ(result, "broke");
}

TEST(Deferred, ErrorAwait) {
    auto deferred = Deferred<int,std::string>::raiseError("broke");
    try {
        deferred->await();
        FAIL() << "Expected operation to throw.";
    } catch(std::string& value) {
        EXPECT_EQ(value, "broke");
    }
}

TEST(Deferred, ErrorIgnoresCancel) {
    auto deferred = Deferred<int,std::string>::raiseError("broke");
    deferred->cancel();
    try {
        deferred->await();
        FAIL() << "Expected operation to throw.";
    } catch(std::string& value) {
        EXPECT_EQ(value, "broke");
    }
}

TEST(Deferred, PromiseOnCompleteSuccess) {
    sem_t semaphore;
    sem_init(&semaphore, 0, 0);

    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    Either<int,std::string> result = Either<int,std::string>::left(0);
    deferred->onComplete([&result, &semaphore](auto value) {
        result = value;
        sem_post(&semaphore);
    });

    promise->success(123);
    sem_wait(&semaphore);

    EXPECT_EQ(result.get_left(), 123);
    sem_destroy(&semaphore);
}

TEST(Deferred, PromiseOnCompleteError) {
    sem_t semaphore;
    sem_init(&semaphore, 0, 0);

    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    Either<int,std::string> result = Either<int,std::string>::left(0);
    deferred->onComplete([&result, &semaphore](auto value) {
        result = value;
        sem_post(&semaphore);
    });

    promise->error("broke");
    sem_wait(&semaphore);

    EXPECT_EQ(result.get_right(), "broke");
    sem_destroy(&semaphore);
}

TEST(Deferred, PromiseOnSuccess) {
    sem_t semaphore;
    sem_init(&semaphore, 0, 0);

    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    int result = 0;
    deferred->onSuccess([&result, &semaphore](auto value) {
        result = value;
        sem_post(&semaphore);
    });

    promise->success(123);
    sem_wait(&semaphore);

    EXPECT_EQ(result, 123);
    sem_destroy(&semaphore);
}

TEST(Deferred, PromiseOnError) {
    sem_t semaphore;
    sem_init(&semaphore, 0, 0);

    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    std::string result = "not called";
    deferred->onError([&result, &semaphore](auto value) {
        result = value;
        sem_post(&semaphore);
    });

    promise->error("broke");
    sem_wait(&semaphore);

    EXPECT_EQ(result, "broke");
    sem_destroy(&semaphore);
}

TEST(Deferred, PromiseAwaitSyncSuccess) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    promise->success(123);

    EXPECT_EQ(deferred->await(), 123);
}

TEST(Deferred, PromiseAwaitSyncError) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    promise->error("broke");

    try {
        deferred->await();
        FAIL() << "Expected operation to throw.";
    } catch(std::string& value) {
        EXPECT_EQ(value, "broke");
    }
}

TEST(Deferred, PromiseAwaitAsync) {
    sem_t semaphore;
    sem_init(&semaphore, 0, 0);

    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);
    int value;

    std::thread backgroundAwait([&value, &semaphore, &deferred]() {
        sem_post(&semaphore);
        value = deferred->await();
    });

    // Wait for background thread to get started
    sem_wait(&semaphore);

    // Complete the promise after a small sleep (to be triple sure that
    // the await is running).
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    promise->success(123);

    // Join the thread to make sure a value has been set. Can spuriously throw
    // if the thread stops before we try to join it.
    try {
        backgroundAwait.join();
    } catch(std::system_error& error) {}

    // Finally assert that the value as set properly.
    EXPECT_EQ(deferred->await(), 123);
}

TEST(Deferred, PromiseCancel) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    deferred->cancel();

    try {
        deferred->await();
        FAIL() << "Expected operation to throw.";
    } catch(std::runtime_error&) {}
}

TEST(Deferred, PromiseSuccessIgnoresCancel) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    promise->success(123);
    promise->cancel();

    EXPECT_EQ(deferred->await(), 123);
}

TEST(Deferred, PromiseErrorIgnoresCancel) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    promise->error("broke");
    promise->cancel();

    try {
        deferred->await();
        FAIL() << "Expected operation to throw.";
    } catch(std::string& value) {
        EXPECT_EQ(value, "broke");
    }
}

TEST(Deferred, PromiseCancelIgnoresSuccess) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    promise->cancel();
    promise->success(123);

    try {
        deferred->await();
        FAIL() << "Expected operation to throw.";
    } catch(std::runtime_error&) {}
}

TEST(Deferred, PromiseCancelIgnoresError) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    promise->cancel();
    promise->error("broke");

    try {
        deferred->await();
        FAIL() << "Expected operation to throw.";
    } catch(std::runtime_error&) {}
}

TEST(Deferred, PromiseCancelIgnoresSubsequentCancel) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);

    promise->cancel();
    promise->cancel();

    try {
        deferred->await();
        FAIL() << "Expected operation to throw.";
    } catch(std::runtime_error&) {}
}

TEST(Deferred, PromiseCancelAffectsPeers) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    auto deferred = Deferred<int,std::string>::forPromise(promise);
    auto sibling = Deferred<int,std::string>::forPromise(promise);

    deferred->cancel();

    try {
        sibling->await();
        FAIL() << "Expected operation to throw.";
    } catch(std::runtime_error&) {}
}

TEST(Deferred, DoesntAllowMultipleSuccesses) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    promise->success(123);
    
    try {
        promise->success(456);
        FAIL() << "Excpeted method to throw";
    } catch(std::runtime_error& error) {
        std::string message = error.what();
        EXPECT_EQ(message, "Promise already successfully completed.");
    }
}

TEST(Deferred, DoesntAllowMultipleErrors) {
    auto promise = Promise<int,std::string>::create(Scheduler::global());
    promise->error("fail");
    
    try {
        promise->error("fail2");
        FAIL() << "Excpeted method to throw";
    } catch(std::runtime_error& error) {
        std::string message = error.what();
        EXPECT_EQ(message, "Promise already completed with an error.");
    }
}
