//          Copyright Tango Tango, Inc. 2020 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef _CASK_ERASED_H_
#define _CASK_ERASED_H_

#include "cask/Config.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "cask/pool/InternalPool.hpp"

namespace cask {

namespace erased {

// The inline buffer is pointer-aligned. Types with stricter alignment
// requirements (e.g. 16-byte-aligned SIMD or `long double` types) simply
// fail `fits_sbo` below and are pool-allocated instead - keeping the
// buffer from forcing padding that would push Erased past one cache line.
constexpr std::size_t sbo_align = alignof(void*);

// Reference held to the pool while a pool-allocated value is alive.
using PoolRef = std::shared_ptr<Pool>;

// Storage for a pool-allocated value: the value pointer and the pool
// reference that keeps the pool alive while the value exists.
struct HeapValue {
    void* ptr;
    PoolRef pool;
};

// Non-buffer overhead of an Erased instance: just the tagged
// operations-table pointer. The value pointer and pool reference are NOT
// overhead - they share storage with the inline buffer, since a value is
// held in exactly one of the two.
constexpr std::size_t erased_overhead = sizeof(std::uintptr_t);

// Size the inline buffer so a complete Erased fills exactly one cache line
// on the configured architecture (e.g. 56 bytes of buffer for 64-byte
// lines, 120 for 128-byte lines). For pathologically small configured
// line sizes the buffer still needs to be at least as large as the heap
// value bookkeeping it shares storage with.
constexpr std::size_t sbo_size =
    config::cache_line_size > erased_overhead + sizeof(HeapValue)
        ? config::cache_line_size - erased_overhead
        : sizeof(HeapValue);

// A type is stored inline when it fits the buffer and can be moved
// without throwing (Erased's move operations are noexcept and must
// relocate inline values).
template <typename T>
constexpr bool fits_sbo =
    sizeof(T) <= sbo_size &&
    alignof(T) <= sbo_align &&
    std::is_nothrow_move_constructible<T>::value;

// A trivial inline type needs no operations table at all: copies and
// moves are a memcpy of the buffer and destruction is a no-op. No
// per-type code or data is generated for these types.
template <typename T>
constexpr bool trivial_sbo =
    fits_sbo<T> &&
    std::is_trivially_copyable<T>::value &&
    std::is_trivially_destructible<T>::value;

// In-place operations used for non-trivial values stored in the inline buffer.
struct InlineOps {
    void (*destroy)(void* ptr) noexcept;
    void (*copy_construct)(void* dest, const void* src);
    void (*move_construct)(void* dest, void* src);
};

// Pool-backed operations used for values too large for the inline buffer.
struct PoolOps {
    void* (*copy)(const void* src, const PoolRef& pool);
    void (*destroy)(void* ptr, const PoolRef& pool);
};

/**
 * A static per-type operations table describing how to destroy, copy, and
 * move a type-erased value when its static type is not available (Erased's
 * own special members). Everything that CAN be derived from the caller's T
 * (`get`, construction, the assignment fast path) is resolved at compile
 * time and never touches this table.
 * 
 * A given type only ever uses one storage strategy - inline or pool - so
 * the two operation sets share a union and only the relevant set is
 * instantiated for each type. Which member is active (and whether the type
 * is trivial) is encoded in the low bits of the pointer to this table
 * carried by each Erased - see Erased::State below.
 */
// Alignment for TypeOps tables: at least 4 so the two low bits of a
// table pointer are guaranteed free for tagging on every supported
// architecture (x86_64/aarch64 naturally align to 8; armv7l and mipsel
// align function pointers to 4). alignas may not weaken natural
// alignment, so take the max.
constexpr std::size_t type_ops_align = alignof(PoolOps) > 4 ? alignof(PoolOps) : 4;

union alignas(type_ops_align) TypeOps {
    constexpr explicit TypeOps(InlineOps ops) noexcept : inline_ops(ops) {}
    constexpr explicit TypeOps(PoolOps ops) noexcept : pool_ops(ops) {}

    InlineOps inline_ops;
    PoolOps pool_ops;
};

// `constexpr` (rather than `const`) guarantees constant-initialization, so
// the table is valid even for Erased values constructed during the dynamic
// initialization of other globals (no static-initialization-order hazard).
template <typename T>
inline constexpr TypeOps ops_for = [] {
    static_assert(!trivial_sbo<T>, "trivial inline types need no operations table");
    if constexpr (fits_sbo<T>) {
        return TypeOps(InlineOps{
            [](void* ptr) noexcept { static_cast<T*>(ptr)->~T(); },
            [](void* dest, const void* src) { ::new (dest) T(*static_cast<const T*>(src)); },
            [](void* dest, void* src) { ::new (dest) T(std::move(*static_cast<T*>(src))); }
        });
    } else {
        return TypeOps(PoolOps{
            [](const void* src, const PoolRef& pool) -> void* { return pool->allocate<T>(*static_cast<const T*>(src)); },
            [](void* ptr, const PoolRef& pool) { pool->deallocate<T>(static_cast<T*>(ptr)); }
        });
    }
}();

} // namespace erased

/**
 * A holder for a type-erased value. This type can hold any other type and will
 * properly construct/destruct the value based on its real type. In many ways it
 * is similiar to `std::any` except it is _far_ less safe. Rather than checking
 * and validating type information at runtime, this type assumes that the caller
 * _really_ knows what they are doing. In the context of cask, it is used by
 * `Task` since its template layer validates these types at compile time.
 * 
 * That "caller knows the type" contract is leaned on hard as an optimization:
 * no RTTI is stored or compared, and every operation where the caller supplies
 * T (construction, `get`, assignment) resolves its storage strategy entirely at
 * compile time. Small values (the common case on the fiber hot path - ints,
 * `None`, `shared_ptr`s and the like) are stored in an inline buffer, requiring
 * no allocation and no interaction with the global memory pool at all -
 * trivially-copyable ones generate no per-type code or data whatsoever. Larger
 * values are allocated from the pool, with a reference to the pool held for
 * exactly as long as the value lives - the pool is reference counted and is
 * destroyed deterministically when its last user releases it.
 * 
 * Does the idea of a type blowing up on you because you don't pass correct
 * type arguments to it later scare you? Good. Don't use this. It serves a very
 * specific purpose for cask - and beyond that it has behavior that might not
 * even be considered a good idea.
 */
class Erased {
public:
    Erased() noexcept;
    Erased(const Erased& other) noexcept;
    Erased(Erased&& other) noexcept;

    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>,Erased>::value>>
    Erased(const T& value) noexcept; // NOLINT(google-explicit-constructor)

    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>,Erased>::value>>
    Erased(T&& value) noexcept; // NOLINT(google-explicit-constructor)

    Erased& operator=(const Erased& other) noexcept;
    Erased& operator=(Erased&& other) noexcept;

    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>,Erased>::value>>
    Erased& operator=(const T& value) noexcept;

    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>,Erased>::value>>
    Erased& operator=(T&& value) noexcept;

    /**
     * Check if this instance is currently holding a value.
     * 
     * @return true iff this instance is currently holding a value.
     */
    bool has_value() const noexcept;

    /**
     * Get the value held by this instance - casting it to the
     * proper type. The behavior of casting to the wrong type
     * is undefined (it's a blind cast under the hood) so be
     * _sure_ that this type is correct. This method throws
     * an exception if the user attempts to obtain a value but
     * no value is available.
     * 
     * @return The casted value.
     */
    template <typename T>
    T& get() const;

    /**
     * If this instance is currently holding a value then free it.
     * Afterwards this instance will not hold a value. If the instance
     * is already not holding a value - then nothing is done.
     */
    void reset() noexcept;

    ~Erased();
private:
    static constexpr std::size_t sbo_size = erased::sbo_size;
    static constexpr std::size_t sbo_align = erased::sbo_align;

    using PoolRef = erased::PoolRef;
    using HeapValue = erased::HeapValue;

    // The entire per-instance bookkeeping is a single tagged pointer:
    //
    //   0                            - empty, no value held
    //   (size << 2) | trivial_tag    - trivial value of `size` bytes in the
    //                                  inline buffer (no operations table
    //                                  exists or is needed; the value's size
    //                                  rides in the bits a table pointer
    //                                  would otherwise occupy)
    //   TypeOps*                     - non-trivial value in the inline buffer
    //   TypeOps* | pooled_flag       - pool-allocated value
    //
    // TypeOps holds function pointers so its alignment guarantees the low
    // bits of a real table pointer are zero. Trivial states never set
    // pooled_flag because the size is shifted past it.
    using State = std::uintptr_t;

    static constexpr State trivial_tag = 0x1;
    static constexpr State pooled_flag = 0x2;
    static constexpr State ptr_mask = ~static_cast<State>(0x3);
    static constexpr State size_shift = 2;

    static_assert(alignof(erased::TypeOps) >= 4, "TypeOps alignment must leave two low tag bits free");

    // The state a value of type T is always stored with - fully resolved
    // at compile time (the table address folds to a link-time constant).
    template <typename T>
    static State state_for() noexcept {
        if constexpr (erased::trivial_sbo<T>) {
            return trivial_tag | (static_cast<State>(sizeof(T)) << size_shift);
        } else if constexpr (erased::fits_sbo<T>) {
            return reinterpret_cast<State>(&erased::ops_for<T>);
        } else {
            return reinterpret_cast<State>(&erased::ops_for<T>) | pooled_flag;
        }
    }

    const erased::TypeOps* table() const noexcept {
        return reinterpret_cast<const erased::TypeOps*>(state & ptr_mask); // NOLINT(performance-no-int-to-ptr)
    }

    // The byte size of a held trivial value. Only meaningful when
    // `state & trivial_tag` is set.
    std::size_t trivial_size() const noexcept {
        return static_cast<std::size_t>(state >> size_shift);
    }

    // A value lives in exactly one of these: small values are constructed
    // directly in the inline buffer, while large values live in the pool
    // with the value pointer and pool reference held here for the value's
    // lifetime. Which member is active is encoded in `state` - no
    // per-instance discriminator or value pointer is needed. The heap
    // member's lifetime is managed manually with placement-new / explicit
    // destroy.
    union Storage {
        alignas(sbo_align) unsigned char buffer[sbo_size];
        HeapValue heap;

        Storage() noexcept {}
        ~Storage() {}
    };

    void* inline_ptr() const noexcept {
        return const_cast<void*>(static_cast<const void*>(&storage.buffer)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

    // Activate / deactivate the heap member of the union.
    void emplace_heap_copy(const Erased& other) noexcept {
        HeapValue& heap = *(::new (static_cast<void*>(&storage.heap)) HeapValue{nullptr, other.storage.heap.pool});
        heap.ptr = table()->pool_ops.copy(other.storage.heap.ptr, heap.pool);
    }

    void emplace_heap_move(Erased& other) noexcept {
        ::new (static_cast<void*>(&storage.heap)) HeapValue{other.storage.heap.ptr, std::move(other.storage.heap.pool)};
        other.destroy_heap();
    }

    void destroy_heap() noexcept {
        storage.heap.~HeapValue();
    }

    // Type-erased copy/move used by Erased's own special members, where no
    // static type information is available. `state` must already be set.
    void copy_value_from(const Erased& other) noexcept {
        if(state & trivial_tag) {
            std::memcpy(&storage.buffer, &other.storage.buffer, trivial_size());
        } else if(state & pooled_flag) {
            emplace_heap_copy(other);
        } else {
            table()->inline_ops.copy_construct(inline_ptr(), other.inline_ptr());
        }
    }

    void move_value_from(Erased& other) noexcept {
        if(state & trivial_tag) {
            std::memcpy(&storage.buffer, &other.storage.buffer, trivial_size());
        } else if(state & pooled_flag) {
            emplace_heap_move(other);
        } else {
            table()->inline_ops.move_construct(inline_ptr(), other.inline_ptr());
            table()->inline_ops.destroy(other.inline_ptr());
        }
    }

    template <typename T, typename Arg>
    void construct(Arg&& value) noexcept;

    Storage storage;
    State state;
};

template <typename T, typename Arg>
inline void Erased::construct(Arg&& value) noexcept {
    state = state_for<T>();
    if constexpr (erased::fits_sbo<T>) {
        ::new (inline_ptr()) T(std::forward<Arg>(value));
    } else {
        HeapValue& heap = *(::new (static_cast<void*>(&storage.heap)) HeapValue{nullptr, cask::pool::global_pool()});
        heap.ptr = heap.pool->allocate<T>(std::forward<Arg>(value));
    }
}

template <typename T, typename>
inline Erased::Erased(const T& value) noexcept
    : state(0)
{
    construct<T>(value);
}

template <typename T, typename>
inline Erased::Erased(T&& value) noexcept
    : state(0)
{
    construct<std::decay_t<T>>(std::forward<T>(value));
}

template <typename T, typename>
inline Erased& Erased::operator=(const T& value) noexcept {
    if(state == state_for<T>()) {
        if constexpr (erased::trivial_sbo<T>) {
            // Same-size trivial types share a state, so the buffer may hold a
            // different (but equally trivial) type. Reconstruct to ensure the
            // correct object's lifetime is active in C++17.
            ::new (inline_ptr()) T(value);
        } else if constexpr (erased::fits_sbo<T>) {
            *static_cast<T*>(inline_ptr()) = value;
        } else {
            *static_cast<T*>(storage.heap.ptr) = value;
        }
    } else {
        reset();
        construct<T>(value);
    }
    return *this;
}

template <typename T, typename>
inline Erased& Erased::operator=(T&& value) noexcept {
    using DecayedT = std::decay_t<T>;
    if(state == state_for<DecayedT>()) {
        if constexpr (erased::trivial_sbo<DecayedT>) {
            std::memcpy(inline_ptr(), &value, sizeof(DecayedT));
        } else if constexpr (erased::fits_sbo<DecayedT>) {
            *static_cast<DecayedT*>(inline_ptr()) = std::forward<T>(value);
        } else {
            *static_cast<DecayedT*>(storage.heap.ptr) = std::forward<T>(value);
        }
    } else {
        reset();
        construct<DecayedT>(std::forward<T>(value));
    }
    return *this;
}

template <typename T>
inline T& Erased::get() const {
    if(state != 0) {
        // The caller's T is trusted to match the stored type (see the class
        // docs), so the storage strategy is selected at compile time.
        if constexpr (erased::fits_sbo<T>) {
            return *static_cast<T*>(inline_ptr());
        } else {
            return *static_cast<T*>(storage.heap.ptr);
        }
    } else {
        throw std::runtime_error("Tried to obtain value for empty Erased container.");
    }
}

inline Erased::Erased() noexcept
    : state(0)
{}

inline Erased::Erased(const Erased& other) noexcept
    : state(other.state)
{
    if(state != 0) {
        copy_value_from(other);
    }
}

inline Erased::Erased(Erased&& other) noexcept
    : state(other.state)
{
    if(state != 0) {
        move_value_from(other);
        other.state = 0;
    }
}

inline Erased& Erased::operator=(const Erased& other) noexcept {
    if(this != &other) {
        reset();
        state = other.state;
        if(state != 0) {
            copy_value_from(other);
        }
    }
    return *this;
}

inline Erased& Erased::operator=(Erased&& other) noexcept {
    if(this != &other) {
        reset();
        state = other.state;
        if(state != 0) {
            move_value_from(other);
            other.state = 0;
        }
    }
    return *this;
}

inline bool Erased::has_value() const noexcept {
    return state != 0;
}

inline void Erased::reset() noexcept {
    if(state != 0) {
        if(state & pooled_flag) {
            table()->pool_ops.destroy(storage.heap.ptr, storage.heap.pool);
            destroy_heap();
        } else if(!(state & trivial_tag)) {
            table()->inline_ops.destroy(inline_ptr());
        }
        state = 0;
    }
}

inline Erased::~Erased() {
    reset();
}

} // namespace cask

#endif
