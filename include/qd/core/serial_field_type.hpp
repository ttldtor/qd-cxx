#pragma once

#include <cstdint>

namespace qd::core {

/** Named full serial type identities supported by the local-schema profile.
 * See spec/schema/JSON-CONTRACT.md for the supported identities and upstream provenance.
 * This enum is intentionally incomplete. Raw inputs retain numeric IDs so that
 * unsupported identities can be validated without masking representation bits.
 */
enum class SerialFieldType : std::int32_t {
    COMPACT_INT = 8 ///< Ordinary compact integer with no representation flags; one logical int field.
};

} // namespace qd::core
