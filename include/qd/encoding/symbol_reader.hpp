// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace qd::encoding {
/** Outcomes of a complete-payload symbol read, distinct from stream framing. */
enum class SymbolStatus {
    OK,                  ///< One owned symbol is available.
    TRUNCATED_INPUT,     ///< The supplied complete payload ends inside a symbol.
    INVALID_ENCODING,    ///< Reserved prefix, undefined repeat, duplicate flags or malformed text.
    INVALID_LENGTH,      ///< String length is outside zero through Java INT_MAX.
    RESOURCE_LIMIT,      ///< An explicit input, output or retained-state budget was exceeded.
    UNSUPPORTED_PROFILE, ///< Only option mask zero is implemented.
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
    std::u16string units;        ///< Exact units, including NUL and isolated surrogates; never a null symbol.
    std::int32_t cipher;         ///< Java cipher; zero means string representation, not an empty symbol.
    std::int32_t eventFlags;     ///< Raw signed low-32-bit flags, zero without a prefix.
    std::size_t eventFlagsBytes; ///< Java count: one for F8, compact integer bytes only for F9.
};

/** Read result; failure never exposes a partial symbol. */
struct SymbolResult {
    std::optional<SymbolValue> value; ///< Engaged only on OK; independent of reader/input lifetime.
    std::size_t consumed;             ///< Examined bytes of this call, including failure bytes.
    SymbolStatus status;              ///< Success or explicit failure classification.
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
     * @param optionMask Zero for EMPTY_SET; other values make reads UNSUPPORTED_PROFILE.
     */
    explicit SymbolReader(SymbolLimits limits, std::uint32_t optionMask) noexcept;
    /** Clear prior symbol, restore MRU=1 and begin a new complete message.
     * @param optionMask Zero for EMPTY_SET; every other value is explicitly unsupported.
     * @return OK for EMPTY_SET, otherwise UNSUPPORTED_PROFILE; no input is examined.
     */
    [[nodiscard]] SymbolStatus beginMessage(std::uint32_t optionMask) noexcept;
    /** Read one symbol from a borrowed remaining span inside one complete message payload.
     * @param input Caller-owned bytes borrowed only for this call, never across frames.
     * @return Owned exact units/metadata and consumed count, or terminal failure without value.
     * Allocation exceptions become ALLOCATION_FAILURE; earlier returned values remain unchanged.
     */
    [[nodiscard]] SymbolResult read(std::span<const std::byte> input);
};
} // namespace qd::encoding
