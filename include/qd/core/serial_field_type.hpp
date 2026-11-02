// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::core {

/** Base serialization identities used by the implemented field subset, without representation bits.
 * A shared base does not imply equal value width, interpretation or profile support.
 */
enum class SerialType : std::int32_t {
    VOID_FIELD = 0,     ///< Explicit zero-width wire field; never a manufactured storage tail.
    BYTE = 1,           ///< One signed byte.
    UTF_CHAR = 2,       ///< One UTF-encoded code point.
    SHORT = 3,          ///< Signed 16-bit big-endian integer.
    INT = 4,            ///< Signed 32-bit big-endian integer.
    COMPACT_INT = 8,    ///< Compact integer encoding; the representation determines the logical value.
    BYTE_ARRAY = 9,     ///< Nullable byte-counted body; a representation may select a different length grammar.
    UTF_CHAR_ARRAY = 10 ///< Nullable character-counted array of UTF-16 units.
};

/** Encoded representation selectors, not independently combinable flags; intentionally incomplete. */
enum class FieldRepresentation : std::int32_t {
    PLAIN = 0x00,         ///< No additional representation selector.
    DECIMAL = 0x10,       ///< Raw decimal bits in a signed 32-bit value.
    SHORT_STRING = 0x20,  ///< Packed short-string integer bits.
    TIME_SECONDS = 0x30,  ///< Signed seconds.
    SEQUENCE = 0x40,      ///< Packed sequence integer bits.
    DATE = 0x50,          ///< Signed day identifier.
    LONG = 0x60,          ///< Signed 64-bit logical value.
    WIDE_DECIMAL = 0x70,  ///< Wide-decimal bits requiring a 64-bit value.
    STRING = 0x80,        ///< UTFString byte length followed by exact UTF-16 text.
    TIME_MILLIS = 0x90,   ///< Signed 64-bit milliseconds, without timing conversion.
    CUSTOM_OBJECT = 0xe0, ///< Opaque byte-array content without execution or conversion.
    SERIAL_OBJECT = 0xf0  ///< Opaque marshalled content with projected compact-int length.
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
    /// Explicit zero-width generic projection; raw zero is a local default, not a delivered Java field.
    VOID_FIELD = composeSerialTypeId(SerialType::VOID_FIELD),
    /// Signed 8-bit value, sign-extended in FieldValue.
    BYTE = composeSerialTypeId(SerialType::BYTE),
    /// Code point, including accepted surrogates and supplementary points.
    UTF_CHAR = composeSerialTypeId(SerialType::UTF_CHAR),
    /// Signed 16-bit big-endian value.
    SHORT = composeSerialTypeId(SerialType::SHORT),
    /// Signed 32-bit big-endian value.
    INT = composeSerialTypeId(SerialType::INT),
    /// Ordinary signed int32 logical value.
    COMPACT_INT = composeSerialTypeId(SerialType::COMPACT_INT),
    /// Nullable owned byte-array body, with validated compact-long length.
    BYTE_ARRAY = composeSerialTypeId(SerialType::BYTE_ARRAY),
    /// Nullable owned exact UTF-16 array.
    UTF_CHAR_ARRAY = composeSerialTypeId(SerialType::UTF_CHAR_ARRAY),
    /// Signed low 32 raw decimal bits, without numeric conversion.
    DECIMAL = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::DECIMAL),
    /// Raw signed int32 bits; no text conversion is implied.
    SHORT_STRING = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::SHORT_STRING),
    /// Signed seconds, distinct from ordinary integers.
    TIME_SECONDS = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::TIME_SECONDS),
    /// Raw signed int32, independent of event-time metadata.
    SEQUENCE = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::SEQUENCE),
    /// Raw signed int32, without calendar conversion.
    DATE = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::DATE),
    /// All signed 64 bits, independent of physical Java storage slots.
    LONG = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::LONG),
    /// All signed64 decimal bits, without a physical VOID tail.
    WIDE_DECIMAL = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::WIDE_DECIMAL),
    /// All signed 64 raw milliseconds, without event-time conversion.
    TIME_MILLIS = composeSerialTypeId(SerialType::COMPACT_INT, FieldRepresentation::TIME_MILLIS),
    /// Nullable UTFString preserving exact UTF-16 units, not character-counted type10.
    STRING = composeSerialTypeId(SerialType::BYTE_ARRAY, FieldRepresentation::STRING),
    /// Nullable opaque custom bytes, with no tag or class interpretation.
    CUSTOM_OBJECT = composeSerialTypeId(SerialType::BYTE_ARRAY, FieldRepresentation::CUSTOM_OBJECT),
    /// Nullable opaque marshalled bytes, with projected compact-int length and no deserialization.
    SERIAL_OBJECT = composeSerialTypeId(SerialType::BYTE_ARRAY, FieldRepresentation::SERIAL_OBJECT)
};

/** Explicit exact-type policy; the default preserves the original bounded subset. */
enum class LocalProfile {
    COMPACT_INT_V1,     ///< Only ordinary full type 8.
    QUOTE_FIELDS_V1,    ///< Exactly 2, 8, 56 and 120; no representation conversions.
    EXTENDED_FIELDS_V1, ///< Exactly 2, 8, 10, 40, 56, 72, 88 and 120; ordinary fields only.
    BINARY_FIELDS_V1    ///< Read-only ordinary exact fields of the finite binary catalog; explicit opt-in.
};

/** Test an exact full identity against a selected local policy.
 * @param type Full numeric serial identity, never masked.
 * @param profile Explicit supported policy.
 * @return True only for identities supported by that policy; false for invalid profiles.
 */
[[nodiscard]] constexpr bool supportsSerialType(std::int32_t type, LocalProfile profile) noexcept {
    if (profile != LocalProfile::COMPACT_INT_V1 && profile != LocalProfile::QUOTE_FIELDS_V1 &&
        profile != LocalProfile::EXTENDED_FIELDS_V1 && profile != LocalProfile::BINARY_FIELDS_V1) {
        return false;
    }

    // Match the FULL identity: decomposition must never widen an explicit policy.
    switch (static_cast<SerialFieldType>(type)) {
    case SerialFieldType::COMPACT_INT:
        return true;
    case SerialFieldType::UTF_CHAR:
    case SerialFieldType::TIME_SECONDS:
    case SerialFieldType::WIDE_DECIMAL:
        return profile != LocalProfile::COMPACT_INT_V1;
    case SerialFieldType::UTF_CHAR_ARRAY:
    case SerialFieldType::SHORT_STRING:
    case SerialFieldType::SEQUENCE:
    case SerialFieldType::DATE:
        return profile == LocalProfile::EXTENDED_FIELDS_V1 || profile == LocalProfile::BINARY_FIELDS_V1;
    case SerialFieldType::VOID_FIELD:
    case SerialFieldType::BYTE:
    case SerialFieldType::SHORT:
    case SerialFieldType::INT:
    case SerialFieldType::DECIMAL:
    case SerialFieldType::LONG:
    case SerialFieldType::TIME_MILLIS:
    case SerialFieldType::BYTE_ARRAY:
    case SerialFieldType::STRING:
    case SerialFieldType::CUSTOM_OBJECT:
    case SerialFieldType::SERIAL_OBJECT:
        return profile == LocalProfile::BINARY_FIELDS_V1;
    default:
        return false;
    }
}

/** Owned exact logical field, independent of Java slot storage.
 * Decoder values obey spec/encoding/FIELD-TYPES.md: fixed-width integers retain their signed range,
 * decimal is raw signed32, long/wide-decimal/milliseconds retain signed64, and arrays retain exact units.
 */
struct FieldValue {
    SerialFieldType type;                          ///< Full identity, retained even when primitives are shared.
    std::int64_t raw;                              ///< Raw integer, code point or decimal bits according to type.
    std::optional<std::u16string> units{};         ///< Array null/empty/content; absent for every scalar tag.
    std::optional<std::vector<std::byte>> bytes{}; ///< Nullable owned opaque body for byte/object tags only.

    /** Compare the identity and complete raw representation, including array nullability.
     * @param other Owned value to compare.
     * @return True only when type, raw bits and both nullable payloads all match.
     */
    bool operator==(const FieldValue &other) const = default;
};

/** Exact non-owning logical field; payload views borrow their source storage. */
struct FieldValueView {
    SerialFieldType type;                     ///< Full serial identity, never masked to its primitive type.
    std::int64_t raw;                         ///< Exact scalar bits; canonical arrays require zero.
    std::optional<std::u16string_view> units; ///< Null versus engaged empty UTF-16; source must outlive this view.
    std::optional<std::span<const std::byte>>
        bytes; ///< Null versus engaged empty bytes; source must outlive this view.
};

namespace detail {
/** Validate either internal field representation without inspecting or copying payload contents.
 * @tparam Field FieldValue or FieldValueView, exposing the same exact tag and nullable payload members.
 * @param value Borrowed owning value or view; source storage is not retained.
 * @return Whether its tag, range and canonical payload combination obey FIELD-TYPES.
 */
template <typename Field> [[nodiscard]] constexpr bool validFieldValueImpl(const Field &value) noexcept {
    if (value.type == SerialFieldType::UTF_CHAR_ARRAY || value.type == SerialFieldType::STRING) {
        return value.raw == 0 && !value.bytes;
    }

    if (value.units) {
        return false;
    }

    if (value.type == SerialFieldType::BYTE_ARRAY || value.type == SerialFieldType::CUSTOM_OBJECT ||
        value.type == SerialFieldType::SERIAL_OBJECT) {
        return value.raw == 0;
    }

    if (value.bytes) {
        return false;
    }

    switch (value.type) {
    case SerialFieldType::VOID_FIELD:
        return value.raw == 0;
    case SerialFieldType::BYTE:
        return value.raw >= std::numeric_limits<std::int8_t>::min() &&
               value.raw <= std::numeric_limits<std::int8_t>::max();
    case SerialFieldType::SHORT:
        return value.raw >= std::numeric_limits<std::int16_t>::min() &&
               value.raw <= std::numeric_limits<std::int16_t>::max();
    case SerialFieldType::INT:
    case SerialFieldType::DECIMAL:
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
    case SerialFieldType::LONG:
    case SerialFieldType::TIME_MILLIS:
        return true;
    default:
        return false;
    }
}

} // namespace detail

/** Validate a manually constructed exact field value independently of schema policy.
 * @param value Borrowed tagged value; never modified or retained.
 * @return Whether the known tag, scalar range and nullable payload obey FIELD-TYPES.
 */
[[nodiscard]] constexpr bool validFieldValue(const FieldValue &value) noexcept {
    return detail::validFieldValueImpl(value);
}

/** Validate a borrowed exact field without allocating, copying or retaining its payload.
 * @param value Borrowed tagged field whose payload storage remains valid during this call.
 * @return Whether the known tag, scalar range and nullable payload obey FIELD-TYPES.
 */
[[nodiscard]] constexpr bool validFieldValueView(const FieldValueView &value) noexcept {
    return detail::validFieldValueImpl(value);
}

} // namespace org::ttldtor::qdcxx::core
