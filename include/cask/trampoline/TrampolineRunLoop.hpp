//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _TRAMPOLINE_RUN_LOOP_
#define _TRAMPOLINE_RUN_LOOP_

#include "TrampolineOp.hpp"

namespace cask::trampoline {

using TrampolineResult = std::variant<
    Either<Erased,Erased>,
    DeferredRef<Erased,Erased>
>;

using OpResult = Either<
    TrampolineResult,
    std::shared_ptr<TrampolineOp>
>;

using AsyncBoundary = std::tuple<
    std::shared_ptr<const TrampolineOp>,
    TrampolineOp::FlatMapPredicate
>;

using TrampolineSyncResult = std::variant<
    Either<Erased,Erased>,
    AsyncBoundary
>;

/**
 * The executor for a trampolined program. Trampolined programs run synchronously until they
 * encounter an asynchronous boundary. The result provided by the execution is either a success
 * value, an error, or a `Deferred` holding the remaining computation.
 */
class TrampolineRunLoop {
public:
    /**
     * Execute a trampolined program by starting with the given operation.
     * 
     * @param initialOp The program's starting point.
     * @return The result of the program execution being either a value, an
     *         error, or a deferred representing some pending async operations.
     */
    static TrampolineResult execute(const std::shared_ptr<const TrampolineOp>& initialOp, const std::shared_ptr<Scheduler>& sched);

    /**
     * Execute a trampolined program by starting with the given operation. This
     * execution will proced either until a synchronous result is provided or
     * an asynchronous boundary is encountered.
     * 
     * @param initialOp The program's starting point.
     * @return The result of the program execution being either a value or
     *         an asynchronous boundary representing the remaining required
     *         computation to provide a complete result.
     */
    static TrampolineSyncResult executeSync(const std::shared_ptr<const TrampolineOp>& initialOp);

    /**
     * Execute the given asynchronous boundary on the given scheduler. Can be used
     * to continue asynchronous evaluation of an attempted synchronous execution.
     * 
     * @param boundary The asynchronous boundary to execute.
     * @param sched The scheduler to execute the asynchronous operation on.
     * @return The result of the asynchronous evaluation.
     */
    static DeferredRef<Erased,Erased> executeAsyncBoundary(const AsyncBoundary& boundary, const std::shared_ptr<Scheduler>& sched);
};

}

#endif