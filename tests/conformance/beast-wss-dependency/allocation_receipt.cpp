// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "allocation_receipt.hpp"

#include <atomic>
#include <cstdlib>
#include <limits>
#include <openssl/crypto.h>

namespace org::ttldtor::qdcxx::test::beast_wss {
namespace {

/** Size prefix with alignment sufficient for malloc's returned payload. */
struct alignas(std::max_align_t) Allocation {
    std::size_t size;
};

constinit std::atomic_size_t outstanding{};
constinit std::atomic_size_t peak{};
constinit std::atomic_size_t allocations{};

/** Record additional live requested bytes and update the observation peak.
 * @param bytes Newly live payload bytes.
 */
void increase(std::size_t bytes) noexcept {
    const auto total = outstanding.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    auto previous = peak.load(std::memory_order_relaxed);

    while (previous < total && !peak.compare_exchange_weak(previous, total, std::memory_order_relaxed)) {
    }
}

/** Check arithmetic for metadata plus a single positive payload request.
 * @param bytes Requested OpenSSL allocator payload size.
 * @return True when the corresponding malloc argument fits.
 */
bool fits(std::size_t bytes) noexcept {
    return bytes != 0 && bytes <= std::numeric_limits<std::size_t>::max() - sizeof(Allocation);
}

/** Observe a normal malloc-backed request before returning it to OpenSSL.
 * @param bytes Requested payload size.
 * @param file OpenSSL call-site label, ignored.
 * @param line OpenSSL call-site line, ignored.
 * @return Aligned payload or nullptr on failure.
 */
void *allocate(std::size_t bytes, [[maybe_unused]] const char *file, [[maybe_unused]] int line) noexcept {
    if (!fits(bytes)) {
        return nullptr;
    }

    auto *entry = static_cast<Allocation *>(std::malloc(sizeof(Allocation) + bytes));

    if (entry == nullptr) {
        return nullptr;
    }

    entry->size = bytes;
    allocations.fetch_add(1, std::memory_order_relaxed);
    increase(bytes);

    return entry + 1;
}

/** Release one measurement allocation.
 * @param pointer Previously returned pointer, or nullptr.
 * @param file OpenSSL call-site label, ignored.
 * @param line OpenSSL call-site line, ignored.
 */
void release(void *pointer, [[maybe_unused]] const char *file, [[maybe_unused]] int line) noexcept {
    if (pointer != nullptr) {
        auto *entry = static_cast<Allocation *>(pointer) - 1;
        outstanding.fetch_sub(entry->size, std::memory_order_relaxed);
        std::free(entry);
    }
}

/** Observe the requested size after successful libc realloc, without estimating transient allocator storage.
 * @param pointer Previously returned pointer, or nullptr.
 * @param bytes Requested replacement payload size.
 * @param file OpenSSL call-site label.
 * @param line OpenSSL call-site line.
 * @return Replacement payload or nullptr. Failure with nonzero size retains the old allocation.
 */
void *resize(void *pointer, std::size_t bytes, const char *file, int line) noexcept {
    if (pointer == nullptr) {
        return allocate(bytes, file, line);
    }

    if (bytes == 0) {
        release(pointer, file, line);

        return nullptr;
    }

    if (!fits(bytes)) {
        return nullptr;
    }

    auto *entry = static_cast<Allocation *>(pointer) - 1;
    const auto previousSize = entry->size;
    auto *resized = static_cast<Allocation *>(std::realloc(entry, sizeof(Allocation) + bytes));

    if (resized == nullptr) {
        return nullptr;
    }

    resized->size = bytes;

    if (bytes >= previousSize) {
        increase(bytes - previousSize);
    } else {
        outstanding.fetch_sub(previousSize - bytes, std::memory_order_relaxed);
    }

    return resized + 1;
}

} // namespace

bool enableAllocationReceipt() noexcept {
    return CRYPTO_set_mem_functions(allocate, resize, release) == 1;
}

AllocationReceipt getAllocationReceipt() noexcept {
    return {peak.load(std::memory_order_relaxed), outstanding.load(std::memory_order_relaxed),
            allocations.load(std::memory_order_relaxed)};
}

} // namespace org::ttldtor::qdcxx::test::beast_wss
