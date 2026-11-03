// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal caller: event_time_routing_tests; count only explicitly armed creator-thread string allocations.
#include <cstdlib>
#include <new>

namespace routing_allocations {
thread_local bool armed = false;    ///< No worker or unrelated thread participates in this measurement.
thread_local std::size_t calls = 0; ///< Ordinary allocation requests during one selected routing operation.

/** Start one allocation-call witness after all schema, input and profile setup has completed. */
void begin() noexcept {
    calls = 0;
    armed = true;
}

/** End the witness before test diagnostics allocate.
 * @return Number of ordinary allocation requests made by the selected operation.
 */
std::size_t finish() noexcept {
    armed = false;

    return calls;
}
} // namespace routing_allocations

/** Allocate ordinary test storage while counting an explicitly armed operation.
 * @param bytes Requested bytes; zero receives one malloc byte.
 * @return Owning ordinary storage, or bad_alloc on exhaustion.
 */
void *operator new(std::size_t bytes) {
    if (routing_allocations::armed) {
        ++routing_allocations::calls;
    }

    if (auto *result = std::malloc(bytes ? bytes : 1)) {
        return result;
    }

    throw std::bad_alloc();
}

/** Allocate arrays through the same ordinary witness.
 * @param bytes Requested byte count.
 * @return Owning ordinary storage, or bad_alloc.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** Release matching ordinary storage.
 * @param address Original allocation, or null.
 */
void operator delete(void *address) noexcept {
    std::free(address);
}

/** Release matching array storage.
 * @param address Original allocation, or null.
 */
void operator delete[](void *address) noexcept {
    std::free(address);
}

/** Release ordinary storage with a compiler-supplied size.
 * @param address Original allocation, or null.
 * @param bytes Unused request size.
 */
void operator delete(void *address, std::size_t bytes) noexcept {
    static_cast<void>(bytes);
    std::free(address);
}

/** Release array storage with a compiler-supplied size.
 * @param address Original allocation, or null.
 * @param bytes Unused request size.
 */
void operator delete[](void *address, std::size_t bytes) noexcept {
    static_cast<void>(bytes);
    std::free(address);
}
