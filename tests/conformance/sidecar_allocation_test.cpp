// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "../../tools/tape-inspect/sidecar_time.hpp"
#include <cstdlib>
#include <limits>
#include <new>

namespace {
bool rejectAllocation = false;      ///< Single-threaded fault scope in this isolated test executable.
std::size_t allocationAttempts = 0; ///< Attempts observed only in this executable.
} // namespace

/** Inject allocation failure without changing production allocator policy.
 * @param count Requested allocation size.
 * @return Owned malloc-backed storage, or throws bad_alloc.
 */
void *operator new(std::size_t count) {
    ++allocationAttempts;

    if (rejectAllocation) {
        throw std::bad_alloc{};
    }

    if (void *result = std::malloc(count == 0 ? 1 : count)) {
        return result;
    }

    throw std::bad_alloc{};
}

/** Release test-owned allocation.
 * @param storage Pointer returned by this executable's operator new.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release a sized test-owned allocation.
 * @param storage Pointer returned by this executable's operator new.
 * @param count Unused allocation size.
 */
void operator delete(void *storage, std::size_t count) noexcept {
    static_cast<void>(count);
    std::free(storage);
}

/** Verify allocation ordering and typed failure without a test framework allocating in the fault scope.
 * @return Zero on all checks; distinct nonzero checkpoints identify a failed guarantee.
 */
int main() {
    namespace ti = org::ttldtor::qdcxx::tape_inspect;
    const auto before = allocationAttempts;
    ti::SidecarTime invalid(static_cast<ti::SidecarFormat>(99));
    ti::SidecarLimits huge;
    huge.lineBytes = std::numeric_limits<std::size_t>::max();
    ti::SidecarTime oversized(ti::SidecarFormat::LONG, huge);
    ti::SidecarTime valid(ti::SidecarFormat::LONG);

    if (allocationAttempts != before || invalid.diagnostic().error != ti::SidecarError::INVALID_ARGUMENT ||
        oversized.diagnostic().error != ti::SidecarError::INVALID_ARGUMENT) {
        return 1;
    }

    const char bytes[] = "0:0\n";
    rejectAllocation = true;
    const auto failed = valid.feed(std::as_bytes(std::span(bytes, sizeof(bytes) - 1)));
    rejectAllocation = false;

    if (failed.status != ti::SidecarStatus::FAILED || valid.diagnostic().error != ti::SidecarError::ALLOCATION) {
        return 2;
    }

    const auto afterFailure = allocationAttempts;

    if (valid.feed({}).accepted != 0 || valid.endInput().status != ti::SidecarStatus::FAILED ||
        valid.finish(0) != ti::SidecarError::ALLOCATION || allocationAttempts != afterFailure) {
        return 3;
    }

    ti::SidecarLimits zero;
    zero.retainedBytes = 0;
    ti::SidecarTime noStorage(ti::SidecarFormat::LONG, zero);
    const auto refused = noStorage.feed(std::as_bytes(std::span(bytes, sizeof(bytes) - 1)));

    return refused.accepted == 0 && noStorage.counters().inputBytes == 0 && noStorage.counters().workBytes == 0 &&
                   allocationAttempts == afterFailure
               ? 0
               : 4;
}
