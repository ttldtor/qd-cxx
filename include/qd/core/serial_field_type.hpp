// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace org::ttldtor::qdcxx::core {

/** Named full serial type identities supported by the local-schema profile.
 * See spec/schema/JSON-CONTRACT.md for the supported identities and upstream provenance.
 * This enum is intentionally incomplete. Raw inputs retain numeric IDs so that
 * unsupported identities can be validated without masking representation bits.
 */
enum class SerialFieldType : std::int32_t {
    UTF_CHAR = 2,        ///< One code point, including accepted surrogate and supplementary points.
    COMPACT_INT = 8,     ///< Ordinary compact integer with no representation flags; one logical int field.
    UTF_CHAR_ARRAY = 10, ///< Nullable owned UTF-16 character array with a compact unit count.
    SHORT_STRING = 40,   ///< Exact signed compact int bits; no text conversion is implied.
    TIME_SECONDS = 56,   ///< Signed compact seconds, distinct from ordinary integers.
    SEQUENCE = 72,       ///< Signed compact int field, independent of event-time metadata.
    DATE = 88,           ///< Signed compact int field, without calendar interpretation.
    WIDE_DECIMAL = 120   ///< Exact compact-long decimal bits, without a physical VOID tail.
};

/** Explicit exact-type policy; the default preserves the original bounded subset. */
enum class LocalProfile {
    COMPACT_INT_V1,    ///< Only ordinary full type 8.
    QUOTE_FIELDS_V1,   ///< Exactly 2, 8, 56 and 120; no representation conversions.
    EXTENDED_FIELDS_V1 ///< Exactly 2, 8, 10, 40, 56, 72, 88 and 120; ordinary fields only.
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

    if (profile != LocalProfile::QUOTE_FIELDS_V1 && profile != LocalProfile::EXTENDED_FIELDS_V1) {
        return false;
    }

    return type == static_cast<std::int32_t>(SerialFieldType::COMPACT_INT) ||
           type == static_cast<std::int32_t>(SerialFieldType::UTF_CHAR) ||
           type == static_cast<std::int32_t>(SerialFieldType::TIME_SECONDS) ||
           type == static_cast<std::int32_t>(SerialFieldType::WIDE_DECIMAL) ||
           (profile == LocalProfile::EXTENDED_FIELDS_V1 &&
            (type == static_cast<std::int32_t>(SerialFieldType::UTF_CHAR_ARRAY) ||
             type == static_cast<std::int32_t>(SerialFieldType::SHORT_STRING) ||
             type == static_cast<std::int32_t>(SerialFieldType::SEQUENCE) ||
             type == static_cast<std::int32_t>(SerialFieldType::DATE)));
}

/** Owned exact logical field, independent of Java slot storage.
 * Decoder values obey spec/encoding/FIELD-TYPES.md: integer fields are sign-extended int32,
 * char is a code point, decimal preserves all signed 64 raw bits, and arrays retain exact units.
 */
struct FieldValue {
    SerialFieldType type;                  ///< Full identity, retained even when primitives are shared.
    std::int64_t raw;                      ///< Raw integer, code point or decimal bits according to type.
    std::optional<std::u16string> units{}; ///< Array null/empty/content; absent for every scalar tag.

    /** Compare the identity and complete raw representation, including array nullability.
     * @param other Owned value to compare.
     * @return True only when type, raw bits and nullable units all match.
     */
    bool operator==(const FieldValue &other) const = default;
};

/** Validate a manually constructed exact field value independently of schema policy.
 * @param value Borrowed tagged value; never modified or retained.
 * @return Whether the known tag, scalar range and nullable payload obey FIELD-TYPES.
 */
[[nodiscard]] constexpr bool validFieldValue(const FieldValue &value) noexcept {
    if (value.type == SerialFieldType::UTF_CHAR_ARRAY) {
        return value.raw == 0;
    }

    if (value.units) {
        return false;
    }

    switch (value.type) {
    case SerialFieldType::COMPACT_INT:
    case SerialFieldType::SHORT_STRING:
    case SerialFieldType::TIME_SECONDS:
    case SerialFieldType::SEQUENCE:
    case SerialFieldType::DATE:
        return value.raw >= std::numeric_limits<std::int32_t>::min() &&
               value.raw <= std::numeric_limits<std::int32_t>::max();
    case SerialFieldType::UTF_CHAR: {
        constexpr std::int64_t MAX_CODE_POINT = 0x10ffff;

        return value.raw >= 0 && value.raw <= MAX_CODE_POINT;
    }
    case SerialFieldType::WIDE_DECIMAL:
        return true;
    default:
        return false;
    }
}

} // namespace org::ttldtor::qdcxx::core
