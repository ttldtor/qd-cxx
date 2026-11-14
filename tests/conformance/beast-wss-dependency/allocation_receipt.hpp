// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>

namespace org::ttldtor::qdcxx::test::beast_wss {

/** Requested payload bytes tracked by instrumentation in a dedicated test process. */
struct AllocationReceipt {
    std::size_t requestedPeak;          ///< Maximum simultaneous observed bytes requested through OpenSSL.
    std::size_t requestedAtObservation; ///< Live observed requests when the caller takes the receipt.
    std::size_t successfulAllocations;  ///< Successful wrapper-level allocations, excluding reallocations.
};

/** Install measurement hooks before any OpenSSL operation in a process reserved for this test.
 * @return True when OpenSSL accepted the hooks.
 */
bool enableAllocationReceipt() noexcept;

/** Snapshot observed OpenSSL allocator requests.
 * @return Request sizes and count, excluding allocator overhead and non-OpenSSL users.
 */
AllocationReceipt getAllocationReceipt() noexcept;

} // namespace org::ttldtor::qdcxx::test::beast_wss
