// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace org::ttldtor::qdcxx::core {

/** Base serialization identities used by the implemented field subset, without representation bits.
 * A shared base does not imply equal value width, interpretation or profile support.
 */
enum class SerialType : std::int32_t {
    UTF_CHAR = 2,       ///< One UTF-encoded code point.
    COMPACT_INT = 8,    ///< Compact integer encoding; the representation determines the logical value.
    UTF_CHAR_ARRAY = 10 ///< Nullable character-counted array of UTF-16 units.
};

/** Encoded representation selectors, not independently combinable flags; intentionally incomplete. */
enum class FieldRepresentation : std::int32_t {
    PLAIN = 0x00,        ///< No additional representation selector.
    SHORT_STRING = 0x20, ///< Packed short-string integer bits.
    TIME_SECONDS = 0x30, ///< Signed seconds.
    SEQUENCE = 0x40,     ///< Packed sequence integer bits.
    DATE = 0x50,         ///< Signed day identifier.
    WIDE_DECIMAL = 0x70  ///< Wide-decimal bits requiring a 64-bit value.
};

/// The four bits that encode the field representation selector.
inline constexpr std::int32_t REPRESENTATION_MASK = 0xf0;
/// All non-representation bits, including unknown high bits; deliberately not just the low nibble.
inline constexpr std::int32_t SERIAL_TYPE_MASK = ~REPRESENTATION_MASK;

/** Compose named components without validating their combination or authorizing profile support.
 * @param type Base serialization identity.
 * @param representation One representation selector, or PLAIN for an unadorned type.
 * @return Full numeric identity; callers must still validate it against their local profile.
 */
[[nodiscard]] constexpr std::int32_t
composeSerialTypeId(SerialType type, FieldRepresentation representation = FieldRepresentation::PLAIN) noexcept {
    return std::to_underlying(type) | std::to_underlying(representation);
}

/** Extract base serialization bits without discarding unknown high bits or validating the input.
 * @param typeId Full raw numeric identity, including unsupported or invalid inputs.
 * @return Every bit outside REPRESENTATION_MASK; not a claim of a known serialization type.
 */
[[nodiscard]] constexpr std::int32_t serialTypeBits(std::int32_t typeId) noexcept {
    return typeId & SERIAL_TYPE_MASK;
}

/** Extract a representation selector without validating the full identity.
 * @param typeId Full raw numeric identity, including unsupported or invalid inputs.
 * @return Representation bits, including selectors absent from FieldRepresentation.
 */
[[nodiscard]] constexpr std::int32_t representationBits(std::int32_t typeId) noexcept {
    return typeId & REPRESENTATION_MASK;
}

/** Named full serial type identities, composed from base serialization and representation.
 * See spec/encoding/FIELD-TYPES.md for decomposition, supported identities and upstream provenance.
 * This enum is intentionally incomplete. Raw inputs retain numeric IDs so that
 * unsupported identities can be validated without masking representation bits.
 */
enum class SerialFieldType : std::int32_t {
    /// Code point, including accepted surrogates and supplementary points.
    UTF_CHAR = composeSerialTypeId(SerialType::UTF_CHAR),
    /// Ordinary signed int32 logical value.
    COMPACT_INT = composeSerialTypeId(SerialType::COMPACT_INT),
    /// Nullable owned exact UTF-16 array.
    UTF_CHAR_ARRAY = composeSerialTypeId(SerialType::UTF_CHAR_ARRAY),
    /// Raw signed int32 bits; no text conversion is implied.
    SHORT_STRING = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::SHORT_STRING),
    /// Signed seconds, distinct from ordinary integers.
    TIME_SECONDS = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::TIME_SECONDS),
    /// Raw signed int32, independent of event-time metadata.
    SEQUENCE = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::SEQUENCE),
    /// Raw signed int32, without calendar conversion.
    DATE = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::DATE),
    /// All signed64 decimal bits, without a physical VOID tail.
    WIDE_DECIMAL = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::WIDE_DECIMAL)
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
    if (profile != LocalProfile::COMPACT_INT_V1 && profile != LocalProfile::QUOTE_FIELDS_V1 &&
        profile != LocalProfile::EXTENDED_FIELDS_V1) {
        return false;
    }

    // Match the FULL identity: decomposition must never widen an explicit policy.
    switch (static_cast<SerialFieldType>(type)) {
    case SerialFieldType::COMPACT_INT:
        return true;
    case SerialFieldType::UTF_CHAR:
    case SerialFieldType::TIME_SECONDS:
    case SerialFieldType::WIDE_DECIMAL:
        return profile == LocalProfile::QUOTE_FIELDS_V1 || profile == LocalProfile::EXTENDED_FIELDS_V1;
    case SerialFieldType::UTF_CHAR_ARRAY:
    case SerialFieldType::SHORT_STRING:
    case SerialFieldType::SEQUENCE:
    case SerialFieldType::DATE:
        return profile == LocalProfile::EXTENDED_FIELDS_V1;
    default:
        return false;
    }
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
