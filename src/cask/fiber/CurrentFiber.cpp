#include "cask/fiber/CurrentFiber.hpp"

namespace cask::fiber {

std::atomic_uint64_t CurrentFiber::next_id = 0;
thread_local std::optional<uint64_t> CurrentFiber::current_id {};

std::optional<uint64_t> CurrentFiber::id() {
    return current_id;
}

uint64_t CurrentFiber::acquireId() {
    return next_id++;
}

void CurrentFiber::setId(uint64_t id) {
    current_id = id;
}

void CurrentFiber::clear() {
    current_id.reset();
}

} // namespace cask::fiber 