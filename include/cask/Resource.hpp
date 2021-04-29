//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_RESOURCE_H_
#define _CASK_RESOURCE_H_

#include "Task.hpp"

namespace cask {

/**
 * A Resource automatically acquires and releases some managed resource - like
 * a server that needs to be properly started and stopped. Rather than putting
 * these side effects in class constructors and destructors - this class allows
 * makes such effects referentially transparent and easily managed.
 */
template <class T = None, class E = std::any>
class Resource {
public:
    using ReleaseTask = Task<None,E>;
    using ReleaseFunction = std::function<ReleaseTask(T)>;
    using AllocatedResource = std::tuple<T,ReleaseTask>;
    using AllocatedResourceTask = Task<AllocatedResource,E>;

    /**
     * Create a resource which uses the given tasks to automatically acquire
     * and release when used.
     *
     * @param acquire The task which returns an acquired and ready to use resource.
     * @param release The task which releases this resource when the user has completed their work.
     * @return A resource wrapping these acquire and release methods.
     */
    constexpr static Resource<T,E> make(const Task<T,E>& acquire, ReleaseFunction release) noexcept;

    /**
     * Create a resource wrapping the given allocation task.
     */
    constexpr explicit Resource(const AllocatedResourceTask& allocated) noexcept;

    /**
     * Acquire and use this resource. The acquired resource will be supplied to
     * the given function. Once the given completes the resource will be released
     * regardless of success or error on behalf of the user task.
     *
     * @param userTask The task which will use the acquired resource.
     * @return A Task which represents the acquisition, usage, and release of this resource.
     */ 
    template <class T2>
    constexpr Task<T2,E> use(std::function<Task<T2,E>(T)> userTask) const noexcept;

    /**
     * Create a new resource which transforms the resource to some new value after
     * acquisition and then otherwise releases the resource when the usage is complete
     * as normal.
     * 
     * @param predicate The transformation function to apply to this resource.
     * @return A resource which performs the given transform after acquisition.
     */
    template <class T2>
    constexpr Resource<T2,E> map(std::function<T2(T)> predicate) const noexcept;

    /**
     * Create a new resource which transforms any errors to some new error type.
     * 
     * @param predicate The transformation function to apply to any errors..
     * @return A resource which transforms errors.
     */
    template <class E2>
    constexpr Resource<T,E2> mapError(std::function<E2(E)> predicate) const noexcept;

    /**
     * Construct a new inner resource from this one. The result is a new resource
     * which, at usage time, acquires the original resource - generates and
     * acquries the inner resource - calls the user task - releases the inner
     * resource - and the releases the original resource. This can be useful
     * to safely compose dependent startup and shutdown procedures between
     * multiple resources.
     * 
     * @param predicate A resource generating function.
     * @return A resource which acquries and releases all resources properly.
     */
    template <class T2>
    constexpr Resource<T2,E> flatMap(std::function<Resource<T2,E>(T)> predicate) const noexcept;

    AllocatedResourceTask allocated;
};

template <class T, class E>
constexpr Resource<T,E> Resource<T,E>::make(const Task<T,E> &acquire, ReleaseFunction release) noexcept {
    return Resource<T,E>(
        acquire.template map<AllocatedResource>([release](T thing) {
            auto deferredRelease = ReleaseTask::defer([thing, release] {
                return release(thing);
            });

            return std::tuple<T,ReleaseTask>(thing, deferredRelease);
        })
    );
}

template <class T, class E>
constexpr Resource<T,E>::Resource(const AllocatedResourceTask& allocated) noexcept
    : allocated(allocated)
{}

template <class T, class E>
template <class T2>
constexpr Task<T2,E> Resource<T,E>::use(std::function<Task<T2,E>(T)> userTask) const noexcept {
    return allocated.template flatMap<T2>([userTask](auto result) {
        auto [value, release] = result;
        return userTask(value).template guarantee<None>(release);
    });
}

template <class T, class E>
template <class T2>
constexpr Resource<T2,E> Resource<T,E>::map(std::function<T2(T)> predicate) const noexcept {
    return Resource<T2,E>(
        allocated.template map<typename Resource<T2,E>::AllocatedResource>([predicate](auto result) {
            auto [value, release] = result;
            auto newValue = predicate(value);
            return std::make_tuple(newValue, release);
        })
    );
}

template <class T, class E>
template <class E2>
constexpr Resource<T,E2> Resource<T,E>::mapError(std::function<E2(E)> predicate) const noexcept {
    return Resource<T,E2>(
        allocated.template map<typename Resource<T,E2>::AllocatedResource>([predicate](auto result) {
            auto [value, release] = result;
            return std::make_tuple(
                value,
                release.template mapError<E2>(predicate)
            );
        })
        .template mapError<E2>(predicate)
    );
}

template <class T, class E>
template <class T2>
constexpr Resource<T2,E> Resource<T,E>::flatMap(std::function<Resource<T2,E>(T)> predicate) const noexcept {
    using SourceResource = typename Resource<T,E>::AllocatedResource;
    using TargetResource = typename Resource<T2,E>::AllocatedResource;
    using TargetResourceTask = Task<TargetResource,E>;

    std::function<TargetResourceTask(SourceResource)> composeResources =
        [predicate](SourceResource result) {
            T outerValue = std::get<0>(result);
            ReleaseTask outerRelease = std::get<1>(result);
            Resource<T2,E> innerResource = predicate(outerValue);

            std::function<TargetResource(TargetResource)> innerCompose =
                [outerRelease](auto innerAllocated) {
                    auto [innerValue, innerRelease] = innerAllocated;
                    Task<None,E> fullRelease = innerRelease.template flatMap<None>([outerRelease](auto) { return outerRelease; });
                    return std::make_tuple(innerValue, fullRelease);
                };

            TargetResourceTask composedAllocation = innerResource
            .allocated
            .template map<TargetResource>(innerCompose);

            return composedAllocation;
        };


    TargetResourceTask result = allocated.template flatMap<TargetResource>(composeResources);
    return Resource<T2,E>(result);
}

}

#endif