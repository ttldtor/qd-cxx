// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <new>

namespace org::ttldtor::qdcxx::transport::detail {

/** Fixed backing with a bounded reusable allocation table; no fallback allocation is possible. */
class FixedArena {
    public:
    static constexpr std::size_t MAX_BLOCKS = 128; ///< Finite allocator metadata and first-fit scan bound.

    private:
    /** One live requested interval; intervals remain ordered by their backing offset. */
    struct Block {
        std::size_t offset; ///< Beginning relative to the fixed backing.
        std::size_t bytes;  ///< Whole requested interval, including alignment padding supplied by callers.
    };

    std::unique_ptr<std::byte[]> backing_;   ///< One fully counted backing allocation.
    std::size_t capacity_;                   ///< Whole requested backing, retained until arena destruction.
    std::array<Block, MAX_BLOCKS> blocks_{}; ///< Ordered live intervals, without dynamic metadata.
    std::size_t count_ = 0;                  ///< Live allocation count.

    public:
    /** Allocate fixed backing before constructing any SDK objects that borrow it.
     * @param capacity Whole requested byte allowance; allocation failure propagates to the factory.
     */
    explicit FixedArena(std::size_t capacity) : backing_(std::make_unique<std::byte[]>(capacity)), capacity_(capacity) {
    }

    /** Find one aligned free interval using at most MAX_BLOCKS metadata visits.
     * @param bytes Requested payload bytes, nonzero.
     * @param alignment Required power-of-two alignment.
     * @return Stable owned storage; throws bad_alloc when backing or metadata is exhausted.
     */
    void *allocate(std::size_t bytes, std::size_t alignment) {
        if (bytes == 0 || count_ == MAX_BLOCKS || alignment == 0) {
            throw std::bad_alloc();
        }

        std::size_t offset = 0;

        for (std::size_t i = 0; i <= count_; ++i) {
            const auto remainder = reinterpret_cast<std::uintptr_t>(backing_.get() + offset) % alignment;
            const auto padding = remainder == 0 ? 0 : alignment - remainder;

            if (padding > capacity_ - offset) {
                throw std::bad_alloc();
            }

            offset += padding;
            const auto end = i == count_ ? capacity_ : blocks_[i].offset;

            if (offset <= end && bytes <= end - offset) {
                for (std::size_t j = count_; j > i; --j) {
                    blocks_[j] = blocks_[j - 1];
                }

                blocks_[i] = {offset, bytes};
                ++count_;

                return backing_.get() + offset;
            }

            if (i != count_) {
                offset = blocks_[i].offset + blocks_[i].bytes;
            }
        }

        throw std::bad_alloc();
    }

    /** Return an exact previously allocated interval; only the serialized SDK owner calls this.
     * @param pointer Beginning of a live interval from this arena.
     */
    void deallocate(void *pointer) noexcept {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        const auto beginning = reinterpret_cast<std::uintptr_t>(backing_.get());

        if (address < beginning || address - beginning >= capacity_) {
            std::terminate();
        }

        const auto offset = address - beginning;

        for (std::size_t i = 0; i < count_; ++i) {
            if (blocks_[i].offset == static_cast<std::size_t>(offset)) {
                for (std::size_t j = i + 1; j < count_; ++j) {
                    blocks_[j - 1] = blocks_[j];
                }

                --count_;

                return;
            }
        }

        // Unknown pointers indicate an internal allocator/lifetime contract violation.
        std::terminate();
    }

    /** Inspect the number of unreleased intervals.
     * @return Live allocations, bounded by MAX_BLOCKS.
     */
    [[nodiscard]] std::size_t allocations() const noexcept {
        return count_;
    }
};

/** Instance-local standard allocator for Asio context services and associated operation storage.
 * @tparam T Rebound element type requested by the SDK.
 */
template <class T> class ArenaAllocator {
    FixedArena *arena_; ///< Borrowed arena outlives every SDK allocation and allocator copy.
    template <class> friend class ArenaAllocator;

    public:
    using value_type = T; ///< Required standard allocator element type.

    /** Borrow one fixed instance arena.
     * @param arena Serialized stable backing owner.
     */
    explicit ArenaAllocator(FixedArena &arena) noexcept : arena_(&arena) {
    }

    /** Rebind without allocating or changing the owning arena.
     * @tparam U Original allocator element type.
     * @param other Existing allocator for the same lifetime domain.
     */
    template <class U> ArenaAllocator(const ArenaAllocator<U> &other) noexcept : arena_(other.arena_) {
    }

    /** Allocate a finite element array from the fixed backing.
     * @param count Requested element count, checked for multiplication overflow.
     * @return Stable aligned storage or bad_alloc; no heap fallback.
     */
    [[nodiscard]] T *allocate(std::size_t count) {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }

        return static_cast<T *>(arena_->allocate(count * sizeof(T), alignof(T)));
    }

    /** Release a live interval after its SDK object has been destroyed.
     * @param pointer Original allocation beginning.
     * @param count Original element count, unused because the arena records whole intervals.
     */
    void deallocate(T *pointer, [[maybe_unused]] std::size_t count) noexcept {
        arena_->deallocate(pointer);
    }

    /** Compare allocation lifetime domains.
     * @tparam U Other allocator element type.
     * @param other Allocator to compare.
     * @return True when either allocator can release the other's allocations.
     */
    template <class U> bool operator==(const ArenaAllocator<U> &other) const noexcept {
        return arena_ == other.arena_;
    }
};

} // namespace org::ttldtor::qdcxx::transport::detail
