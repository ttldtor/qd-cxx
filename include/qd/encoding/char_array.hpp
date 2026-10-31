// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/encoding/compact.hpp"

#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::encoding {

/** Mandatory per-value limits; zero is zero allowance, never unlimited. */
struct CharArrayLimits {
    std::size_t encodedBytes; ///< Maximum examined or written prefix and body bytes together.
    std::size_t codeUnits;    ///< Maximum written units or reserved units, including failed decoding attempts.
};

/** Select the observed primitive or enclosing message length check. */
enum class CharArrayBoundary {
    PRIMITIVE, ///< Read characters directly, consuming all supplied incomplete content.
    MESSAGE    ///< Reject a positive unit count larger than all bytes remaining after the prefix.
};

/** Owned exact character-array result; failures expose no partial string. */
struct CharArrayResult {
    std::optional<std::u16string> units; ///< On OK, absent means null; engaged empty means empty.
    std::size_t consumed;                ///< Examined bytes, including failures; not resumable decoder state.
    std::size_t chargedUnits;            ///< Admitted count before allocation; retained on every later failure.
    Status status;                       ///< Success, malformed wire, input exhaustion, resource or allocation failure.
};

/** Allocator-aware exact character-array value, with explicit resource lifetime. */
struct PmrCharArrayResult {
    std::optional<std::pmr::u16string> units; ///< Exact text/null on success, never a failed partial value.
    std::size_t consumed;                     ///< Examined encoded bytes, including failures.
    std::size_t chargedUnits;                 ///< Admitted units, not refunded on later failure.
    Status status;                            ///< Complete or failed primitive outcome.
};

/** Read a compact character count and exact BMP UTFChar units without retaining input.
 * Null is distinct from empty. Surrogates and observed overlong BMP encodings are preserved;
 * supplementary scalar encodings are rejected after their complete tail. This is not UTFString.
 * Length/domain checks precede unit admission; allocation failures are translated to a result.
 * @param input Caller-owned complete available bytes, borrowed only for this call.
 * @param limits Required finite prefix/body-byte and decoded-unit bounds.
 * @param boundary Whether to apply the enclosing message's remaining-byte check before allocation.
 * @return Owned value/null on OK; otherwise no value, exact examined bytes and nonrefunded unit charge.
 */
[[nodiscard]] CharArrayResult readCharArray(std::span<const std::byte> input, CharArrayLimits limits,
                                            CharArrayBoundary boundary = CharArrayBoundary::PRIMITIVE) noexcept;

/** Decode the same character-counted grammar into explicit caller-owned allocation.
 * @param input Borrowed remaining payload, never retained.
 * @param limits Required finite primitive allowances.
 * @param boundary Explicit remaining-message length policy.
 * @param resource Resource which must outlive the result; custom resource exceptions propagate.
 * @return Owning exact text/null or classified failure without a partial value.
 */
[[nodiscard]] PmrCharArrayResult readCharArray(std::span<const std::byte> input, CharArrayLimits limits,
                                               CharArrayBoundary boundary, std::pmr::memory_resource &resource);

/** Write a canonical compact-int count and one UTFChar per exact UTF-16 unit atomically.
 * Null and empty remain distinct; paired and isolated surrogates are never combined.
 * Count-domain checks precede byte/unit budgets, which precede output capacity checks.
 * Sizing and encoding each traverse the units once. No allocation or input retention occurs.
 * @param value Caller-owned nullable units, borrowed only for this call.
 * @param output Caller-owned writable bytes; excess capacity remains unchanged.
 * @param limits Required finite prefix/body-byte and UTF-16-unit bounds, including the count prefix.
 * @return Encoded size on OK; otherwise zero with the whole output unchanged. Invalid counts report
 * INVALID_LENGTH, budget or size overflow reports RESOURCE_LIMIT, and capacity reports INSUFFICIENT_OUTPUT.
 */
[[nodiscard]] WriteResult writeCharArray(std::optional<std::u16string_view> value, std::span<std::byte> output,
                                         CharArrayLimits limits) noexcept;

} // namespace org::ttldtor::qdcxx::encoding
