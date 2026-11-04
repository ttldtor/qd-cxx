// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>

namespace org::ttldtor::qdcxx::transport::detail {

/** Finite application windows, independent of native read and write chunk sizes. */
struct RawByteLimits {
    std::size_t inputBytes = 0;  ///< Retained application input, consumed explicitly by its owner.
    std::size_t outputBytes = 0; ///< Whole immutable application output slot.
};

/** Stable application backing; its enclosing owner retires the native core before this object.
 * No method calls user code. An output slot cannot be reused until its entire accepted prefix
 * has been accounted; input remains readable after native retirement until explicitly consumed.
 */
class RawByteState {
    RawByteLimits limits_;                ///< Immutable capacities checked before construction.
    std::unique_ptr<std::byte[]> input_;  ///< Whole separately counted input allocation.
    std::unique_ptr<std::byte[]> output_; ///< Whole separately counted output allocation.
    std::size_t inputBegin_ = 0;          ///< Explicitly consumed prefix.
    std::size_t inputEnd_ = 0;            ///< Last retained input byte.
    std::size_t outputBegin_ = 0;         ///< Engine/native accepted application prefix.
    std::size_t outputEnd_ = 0;           ///< End of the immutable published output block.

    public:
    /** Allocate both complete windows before publication.
     * @param limits Positive checked application capacities.
     */
    explicit RawByteState(RawByteLimits limits)
        : limits_(limits), input_(std::make_unique<std::byte[]>(limits.inputBytes)),
          output_(std::make_unique<std::byte[]>(limits.outputBytes)) {
    }

    /** Inspect retained plaintext without changing its lifetime.
     * @return Immutable suffix, valid until consumeInput or owner retirement.
     */
    [[nodiscard]] std::span<const std::byte> input() const noexcept {
        return {input_.get() + inputBegin_, inputEnd_ - inputBegin_};
    }

    /** Copy at most one bounded input prefix; no compaction or caller borrow is retained.
     * @param bytes Verified plaintext or plain native input.
     * @return Exact copied prefix; zero while a previous input block remains retained.
     */
    [[nodiscard]] std::size_t acceptInput(std::span<const std::byte> bytes) noexcept {
        if (!input().empty()) {
            return 0;
        }

        const auto count = (std::min)(limits_.inputBytes, bytes.size());

        if (count != 0) {
            std::memcpy(input_.get(), bytes.data(), count);
        }

        inputBegin_ = 0;
        inputEnd_ = count;

        return count;
    }

    /** Release only an eligible application-consumed input prefix.
     * @param bytes Consumed prefix, at most input().size().
     * @return False without mutation for an ineligible count.
     */
    [[nodiscard]] bool consumeInput(std::size_t bytes) noexcept {
        if (bytes > input().size()) {
            return false;
        }

        inputBegin_ += bytes;

        return true;
    }

    /** Inspect the immutable pending application output.
     * @return Suffix stable through native short writes and exact engine WANT retries.
     */
    [[nodiscard]] std::span<const std::byte> output() const noexcept {
        return {output_.get() + outputBegin_, outputEnd_ - outputBegin_};
    }

    /** Inspect whole output admission capacity without changing backing.
     * @return Maximum bytes in one copied output block.
     */
    [[nodiscard]] std::size_t outputCapacity() const noexcept {
        return limits_.outputBytes;
    }

    /** Publish a complete copied block only when the slot and capacity permit it.
     * @param bytes Borrowed complete output; no address survives this call.
     * @return False without mutation if the slot is busy or the whole block does not fit.
     */
    [[nodiscard]] bool publishOutput(std::span<const std::byte> bytes) noexcept {
        if (!output().empty() || bytes.size() > limits_.outputBytes) {
            return false;
        }

        if (!bytes.empty()) {
            std::memcpy(output_.get(), bytes.data(), bytes.size());
        }

        outputBegin_ = 0;
        outputEnd_ = bytes.size();

        return true;
    }

    /** Credit engine/native application acceptance without entering any SDK.
     * @param bytes Accepted prefix, at most output().size().
     * @return False without mutation for an inconsistent completion.
     */
    [[nodiscard]] bool consumeOutput(std::size_t bytes) noexcept {
        if (bytes > output().size()) {
            return false;
        }

        outputBegin_ += bytes;

        return true;
    }

    /** Retire application output only after native handlers and engine WANT borrowers drain. */
    void retireOutput() noexcept {
        outputBegin_ = outputEnd_ = 0;
    }
};

} // namespace org::ttldtor::qdcxx::transport::detail
