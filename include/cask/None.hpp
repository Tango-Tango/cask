//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_NIL_H_
#define _CASK_NIL_H_

namespace cask {

/**
 * Represents a type that holds now value. It is similiar to `void`
 * but cricitally can also be a value - which is important for
 * referential transparency. A value of None still represents the
 * absence of a value (in the same way nullptr represents the absence
 * of a pointer).
 */
class None {};

constexpr inline bool operator==(const None&, const None&) {
    return true;
}

} // namespace cask

#endif
