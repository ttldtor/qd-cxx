#pragma once

#include <cstdint>

namespace qd::core {

/** Named full serial type identities used by the implemented local-schema profile.
 * Values follow QD 3.355 SerialFieldType and spec/schema/JSON-CONTRACT.md.
 * This is not an exhaustive wire-type enum: raw inputs retain numeric IDs so that
 * unsupported identities can be validated without masking representation bits.
 */
enum class SerialFieldType : std::int32_t {
    COMPACT_INT = 8 ///< Ordinary compact integer with no representation flags; one logical int field.
};

} // namespace qd::core
