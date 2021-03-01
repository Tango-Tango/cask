//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _TRAMPOLINE_RUN_LOOP_
#define _TRAMPOLINE_RUN_LOOP_

#include "TrampolineOp.hpp"

namespace cask::trampoline {

using TrampolineResult = std::variant<
    Either<std::any,std::any>,
    DeferredRef<std::any,std::any>
>;

using OpResult = Either<
    TrampolineResult,
    std::shared_ptr<TrampolineOp>
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
};

}

#endif