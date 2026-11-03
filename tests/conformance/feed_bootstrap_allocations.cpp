// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal caller: feed_bootstrap_tests --bootstrap-allocation-failures, registered by CTest.
// One-shot creator allocations fail after caller options preparation; no worker or network starts.
#include "qd/api/feed.hpp"

#include "bootstrap_lease.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <utility>

namespace bootstrap_allocations {
thread_local bool armed = false;        ///< Only the owned test thread can consume an armed fault.
thread_local std::size_t remaining = 0; ///< Successful creator allocations before one refusal.

/** Refuse exactly one allocation, disarming before exception/error representation allocates. */
void beforeAllocation() {
    if (armed && remaining == 0) {
        armed = false;
        throw std::bad_alloc();
    }

    if (armed) {
        --remaining;
    }
}
} // namespace bootstrap_allocations

/** Allocate scalar test storage with the explicitly armed one-shot refusal.
 * @param bytes Requested bytes; zero gets one ordinary malloc byte.
 * @return Owning storage; throws bad_alloc on refusal or exhaustion.
 */
void *operator new(std::size_t bytes) {
    bootstrap_allocations::beforeAllocation();

    if (auto *result = std::malloc(bytes ? bytes : 1)) {
        return result;
    }

    throw std::bad_alloc();
}

/** Allocate test array storage through the same refusal boundary.
 * @param bytes Requested byte count.
 * @return Owning storage, or bad_alloc.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** Release matching scalar storage.
 * @param address Owning allocation, or null.
 */
void operator delete(void *address) noexcept {
    std::free(address);
}

/** Release matching array storage.
 * @param address Owning allocation, or null.
 */
void operator delete[](void *address) noexcept {
    std::free(address);
}

/** Release scalar storage with a compiler-supplied size.
 * @param address Owning allocation, or null.
 * @param bytes Unused allocation size.
 */
void operator delete(void *address, std::size_t bytes) noexcept {
    (void)bytes;
    std::free(address);
}

/** Release array storage with a compiler-supplied size.
 * @param address Owning allocation, or null.
 * @param bytes Unused allocation size.
 */
void operator delete[](void *address, std::size_t bytes) noexcept {
    (void)bytes;
    std::free(address);
}

/** Allocate nothrow scalar storage through the same test boundary.
 * @param bytes Requested bytes.
 * @param tag Standard nothrow selection.
 * @return Owning storage, or null.
 */
void *operator new(std::size_t bytes, const std::nothrow_t &tag) noexcept {
    (void)tag;
    try {
        return ::operator new(bytes);
    } catch (...) {
        return nullptr;
    }
}

/** Allocate nothrow array storage through the same test boundary.
 * @param bytes Requested bytes.
 * @param tag Standard nothrow selection.
 * @return Owning storage, or null.
 */
void *operator new[](std::size_t bytes, const std::nothrow_t &tag) noexcept {
    return ::operator new(bytes, tag);
}

/** Release matching nothrow scalar storage after constructor refusal.
 * @param address Owning allocation, or null.
 * @param tag Standard nothrow selection.
 */
void operator delete(void *address, const std::nothrow_t &tag) noexcept {
    (void)tag;
    std::free(address);
}

/** Release matching nothrow array storage after constructor refusal.
 * @param address Owning allocation, or null.
 * @param tag Standard nothrow selection.
 */
void operator delete[](void *address, const std::nothrow_t &tag) noexcept {
    (void)tag;
    std::free(address);
}

namespace bootstrap_allocations {
/** Sweep creator allocations for owned and borrowed bootstrap construction only.
 * @return Zero after typed one-shot refusals and intact host leases; nonzero after a mismatch.
 */
int run() {
    namespace q = org::ttldtor::qdcxx;
    constexpr std::size_t MAXIMUM_CREATOR_ALLOCATION_SITES = 16;
    auto host = q::transport::RuntimeBootstrap::create();

    if (!host) {
        return 1;
    }

    auto probe = q::transport::detail::BootstrapLease::acquire(*host);

    if (!probe) {
        return 2;
    }

    for (const bool borrowed : {false, true}) {
        std::size_t failures = 0;
        bool completed = false;

        for (std::size_t site = 0; site != MAXIMUM_CREATOR_ALLOCATION_SITES; ++site) {
            q::api::RuntimeOptions options;
            options.dnsServers.push_back({{127, 0, 0, 1}});
            remaining = site;
            armed = true;
            try {
                if (borrowed) {
                    q::api::ApiRuntime runtime(*host, std::move(options));
                } else {
                    q::api::ApiRuntime runtime(std::move(options));
                }

                completed = true;
            } catch (const q::api::Error &error) {
                armed = false;
                const auto failure = error.diagnostic();
                const bool expected =
                    (failure.code == q::api::ErrorCode::ALLOCATION_FAILURE && failure.stage == "api-allocation") ||
                    (!borrowed && failure.code == q::api::ErrorCode::BOOTSTRAP_FAILURE &&
                     failure.stage == "bootstrap-storage" && failure.nativeCode == 0 &&
                     failure.detailCode == static_cast<std::uint32_t>(q::transport::TcpError::ALLOCATION_FAILURE));
                std::cout << (borrowed ? "borrowed" : "owned") << " refusal site=" << site << " stage=" << failure.stage
                          << " code=" << static_cast<int>(failure.code) << " detail=" << failure.detailCode
                          << std::endl;

                if (!expected) {
                    return 3;
                }

                ++failures;
            } catch (...) {
                armed = false;

                return 4;
            }
            armed = false;

            if (probe->state()->borrowers.load() != 1) {
                return 5;
            }

            if (completed) {
                break;
            }
        }

        if (!completed || failures == 0) {
            return 6;
        }

        std::cout << (borrowed ? "borrowed" : "owned") << " one-shot allocation refusals=" << failures
                  << "; subsequent construction/destruction passed; host leases=1\n";
    }

    return 0;
}
} // namespace bootstrap_allocations
