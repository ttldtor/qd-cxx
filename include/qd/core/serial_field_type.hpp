// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstdint>

namespace org::ttldtor::qdcxx::core {

/** Named full serial type identities supported by the local-schema profile.
 * See spec/schema/JSON-CONTRACT.md for the supported identities and upstream provenance.
 * This enum is intentionally incomplete. Raw inputs retain numeric IDs so that
 * unsupported identities can be validated without masking representation bits.
 */
enum class SerialFieldType : std::int32_t {
    UTF_CHAR = 2,      ///< One code point, including accepted surrogate and supplementary points.
    COMPACT_INT = 8,   ///< Ordinary compact integer with no representation flags; one logical int field.
    TIME_SECONDS = 56, ///< Signed compact seconds, distinct from ordinary integers.
    WIDE_DECIMAL = 120 ///< Exact compact-long decimal bits, without a physical VOID tail.
};

/** Explicit exact-type policy; the default preserves the original bounded subset. */
enum class LocalProfile {
    COMPACT_INT_V1, ///< Only ordinary full type 8.
    QUOTE_FIELDS_V1 ///< Exactly 2, 8, 56 and 120; no representation conversions.
};

/** Test an exact full identity against a selected local policy.
 * @param type Full numeric serial identity, never masked.
 * @param profile Explicit supported policy.
 * @return True only for identities supported by that policy; false for invalid profiles.
 */
[[nodiscard]] constexpr bool supportsSerialType(std::int32_t type, LocalProfile profile) noexcept {
    if (profile == LocalProfile::COMPACT_INT_V1) {
        return type == static_cast<std::int32_t>(SerialFieldType::COMPACT_INT);
    }

    return profile == LocalProfile::QUOTE_FIELDS_V1 &&
           (type == static_cast<std::int32_t>(SerialFieldType::COMPACT_INT) ||
            type == static_cast<std::int32_t>(SerialFieldType::UTF_CHAR) ||
            type == static_cast<std::int32_t>(SerialFieldType::TIME_SECONDS) ||
            type == static_cast<std::int32_t>(SerialFieldType::WIDE_DECIMAL));
}

/** Owned exact logical field, independent of Java slot storage.
 * Decoder values obey spec/encoding/QUOTE-VALUES.md: int/time are sign-extended int32,
 * char is a code point, and decimal preserves all signed 64 raw bits.
 */
struct FieldValue {
    SerialFieldType type; ///< Full identity, retained even when primitives are shared.
    std::int64_t raw;     ///< Raw integer, code point or decimal bits according to type.

    /** Compare both the identity and raw representation.
     * @param other Owned value to compare.
     * @return True only when both type and raw bits match.
     */
    bool operator==(const FieldValue &other) const = default;
};

} // namespace org::ttldtor::qdcxx::core
