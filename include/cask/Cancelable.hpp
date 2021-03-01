//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_CANCELABLE_H_
#define _CASK_CANCELABLE_H_

#include <any>
#include <memory>

namespace cask {

template <class E>
class Cancelable;

template <class E = std::any>
using CancelableRef = std::shared_ptr<Cancelable<E>>;

/**
 * Represents a computation that can be canceled.
 */
template <class E = std::any>
class Cancelable {
public:
    /**
     * Cancel an ongoing and uncompleted background computation.
     * Cancel may be called multiple times without error - the
     * cancellation will only be attempted once. When this method
     * returns the computation may not yet be cancelled - users
     * must observe the computation itself for an indication of
     * if and when it as cancelled.
     */
    virtual void cancel(const E& error) = 0;

    virtual ~Cancelable() {};
};


template <class E>
class IgnoreCancelation final : public Cancelable<E> {
public:
    void cancel(const E&) override {}
};

}

#endif