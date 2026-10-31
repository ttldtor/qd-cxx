// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>

namespace org::ttldtor::qdcxx::encoding {
/** Outcomes of a complete-payload symbol read, distinct from stream framing. */
enum class SymbolStatus {
    OK,                  ///< One owned symbol is available.
    TRUNCATED_INPUT,     ///< The supplied complete payload ends inside a symbol.
    INVALID_ENCODING,    ///< Reserved prefix, undefined repeat, duplicate flags or malformed text.
    INVALID_LENGTH,      ///< String length is outside zero through Java INT_MAX.
    RESOURCE_LIMIT,      ///< An explicit input, output or retained-state budget was exceeded.
    UNSUPPORTED_PROFILE, ///< An option mask other than EMPTY_SET0 or history-snapshot1 was requested.
    ALLOCATION_FAILURE,  ///< Allocation failed; no tentative state or value was published.
    FAILED               ///< A previous failure requires beginMessage before another read.
};

/** Mandatory finite logical-element budgets, excluding allocator overhead and caller-held results. */
struct SymbolLimits {
    std::size_t inputBytes; ///< Maximum supplied remaining payload bytes per call.
    std::size_t codeUnits;  ///< Maximum decoded UTF-16 units per result.
    std::size_t stateUnits; ///< Maximum retained UTF-16 units for the previous symbol.
};

/** Independent owned symbol and raw Java encoding metadata; no event semantics are applied. */
struct SymbolValue {
    std::u16string units;            ///< Exact units, including NUL and isolated surrogates; never a null symbol.
    std::int32_t cipher = 0;         ///< Java cipher; zero means string representation, not an empty symbol.
    std::int32_t eventFlags = 0;     ///< Raw signed low-32-bit flags, zero without a prefix.
    std::size_t eventFlagsBytes = 0; ///< Java count: one for F8, compact integer bytes only for F9.
};

/** Read result; failure never exposes a partial symbol. */
struct SymbolResult {
    std::optional<SymbolValue> value; ///< Engaged only on OK; independent of reader/input lifetime.
    std::size_t consumed;             ///< Examined bytes of this call, including failure bytes.
    SymbolStatus status;              ///< Success or explicit failure classification.
};

/** Allocator-aware symbol; every copy retains the same explicit allocation resource. */
struct PmrSymbolValue {
    std::pmr::u16string units;       ///< Exact owned UTF-16, never a null symbol.
    std::int32_t cipher = 0;         ///< Raw Java cipher metadata.
    std::int32_t eventFlags = 0;     ///< Raw signed low-32-bit event flags.
    std::size_t eventFlagsBytes = 0; ///< Original flag primitive byte count.

    /** Select the allocation resource before any symbol storage is created.
     * @param resource Resource which must outlive this value and every copy.
     */
    explicit PmrSymbolValue(std::pmr::memory_resource &resource) : units(&resource) {
    }

    /** Copy exact text through the source's explicit resource.
     * @param other Immutable source, borrowed during the copy.
     */
    PmrSymbolValue(const PmrSymbolValue &other)
        : units(other.units, other.units.get_allocator()), cipher(other.cipher), eventFlags(other.eventFlags),
          eventFlagsBytes(other.eventFlagsBytes) {
    }

    /** Transfer owned storage without allocation.
     * @param other Source value.
     */
    PmrSymbolValue(PmrSymbolValue &&other) noexcept = default;
    /** Assign exact text through this value's existing allocator.
     * @param other Source value.
     * @return This value.
     */
    PmrSymbolValue &operator=(const PmrSymbolValue &other) = default;
    /** Transfer storage when allocator identity permits; otherwise copy into this allocator.
     * @param other Source value.
     * @return This value.
     */
    PmrSymbolValue &operator=(PmrSymbolValue &&other) = default;
};

/** Allocator-aware symbol result; the explicit resource outlives every successful value. */
struct PmrSymbolResult {
    std::optional<PmrSymbolValue> value; ///< Independent successful symbol, absent on failure.
    std::size_t consumed;                ///< Exact examined input bytes.
    SymbolStatus status;                 ///< Primitive, profile, work or allocation result.
};

/** Message-local symbol decoder sharing the ordinary grammar with explicit allocation. */
class PmrSymbolReader {
    SymbolLimits limits_;                    ///< Required primitive budgets.
    std::pmr::memory_resource *resource_;    ///< Borrowed allocator, outliving reader and all results.
    std::optional<PmrSymbolValue> previous_; ///< Message-local previous exact symbol.
    std::int32_t mruFlags_ = 1;              ///< Message-local MRU flags.
    SymbolStatus state_ = SymbolStatus::OK;  ///< Terminal until beginMessage after failure.

    public:
    /** Initialize message-local state without allocating.
     * @param limits Required finite primitive bounds.
     * @param optionMask Zero or history-snapshot bit one.
     * @param resource Explicit allocator which outlives this reader and all results.
     */
    PmrSymbolReader(SymbolLimits limits, std::uint32_t optionMask, std::pmr::memory_resource &resource) noexcept;
    /** Start fresh message-local previous-symbol and flags state.
     * @param optionMask Zero or history-snapshot bit one.
     * @return OK for the selected sets, otherwise UNSUPPORTED_PROFILE.
     */
    [[nodiscard]] SymbolStatus beginMessage(std::uint32_t optionMask) noexcept;
    /** Decode through the ordinary shared symbol grammar using explicit allocation.
     * @param input Borrowed remaining message payload, never retained.
     * @return Exact owned symbol or failure; custom resource exceptions propagate.
     */
    [[nodiscard]] PmrSymbolResult read(std::span<const std::byte> input);
};

/** Single-threaded message-local reader, independent of schema, framing and transport.
 * All failures are terminal until beginMessage; successful state publication is atomic.
 */
class SymbolReader {
    SymbolLimits limits_;
    std::optional<SymbolValue> previous_;
    std::int32_t mruFlags_ = 1;
    SymbolStatus state_ = SymbolStatus::OK;

    public:
    /** Initialize an empty message state without allocating.
     * @param limits Mandatory finite input/output/state budgets.
     * @param optionMask Zero for EMPTY_SET or one for hs; other bits make reads UNSUPPORTED_PROFILE.
     */
    explicit SymbolReader(SymbolLimits limits, std::uint32_t optionMask) noexcept;
    /** Clear prior symbol, restore MRU=1 and begin a new complete message.
     * @param optionMask Zero for EMPTY_SET or one for hs; no wire form depends on this mask.
     * @return OK for either known set, otherwise UNSUPPORTED_PROFILE; no input is examined.
     */
    [[nodiscard]] SymbolStatus beginMessage(std::uint32_t optionMask) noexcept;
    /** Read one symbol from a borrowed remaining span inside one complete message payload.
     * @param input Caller-owned bytes borrowed only for this call, never across frames.
     * @return Owned exact units/metadata and consumed count, or terminal failure without value.
     * Allocation exceptions become ALLOCATION_FAILURE; earlier returned values remain unchanged.
     */
    [[nodiscard]] SymbolResult read(std::span<const std::byte> input);
};
} // namespace org::ttldtor::qdcxx::encoding
