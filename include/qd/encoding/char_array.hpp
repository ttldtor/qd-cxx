// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/encoding/compact.hpp"

#include <optional>
#include <string>

namespace org::ttldtor::qdcxx::encoding {

/** Mandatory per-value limits; zero is zero allowance, never unlimited. */
struct CharArrayLimits {
    std::size_t encodedBytes; ///< Maximum examined prefix and body bytes together.
    std::size_t codeUnits;    ///< Maximum reserved UTF-16 units, including failed decoding attempts.
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

} // namespace org::ttldtor::qdcxx::encoding
