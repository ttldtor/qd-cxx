// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/core/local_schema.hpp"

#include <cstddef>
#include <expected>
#include <string_view>

namespace org::ttldtor::qdcxx::schema {

/** Stable failures at the native JSON source boundary; no input or parser-owned text escapes. */
enum class JsonSchemaError {
    MALFORMED_JSON,      ///< Invalid JSON syntax, encoding, escapes, BOM or trailing document.
    INVALID_SCHEMA,      ///< Invalid native object members, numeric range or local invariants.
    UNSUPPORTED_PROFILE, ///< Profile identity outside the explicitly selected loader entry.
    UNSUPPORTED_VERSION, ///< Format or native profile version outside version 1.
    UNSUPPORTED_TYPE,    ///< Full field identity outside the selected exact-type subset.
    UNSUPPORTED_INDEXED, ///< Unsupported indexed layout/type, or legacy hasTime:true.
    RESOURCE_LIMIT,      ///< Input, nesting, logical schema or parser-size budget exceeded.
    ALLOCATION_FAILURE   ///< Required parser or owned schema storage could not be allocated.
};

/** Finite input and logical bounds; zero is meaningful and equality is admitted. */
struct JsonSchemaLimits {
    std::size_t documentBytes = 1048576; ///< Entire supplied document, including whitespace, in bytes.
    std::size_t depth = 16;              ///< Nested objects/arrays, including the root at depth one.
    core::SchemaLimits schema{1024, 16384, 32768, 1024, 1048576}; ///< Counts and exact UTF-16 name units.
};

inline constexpr std::size_t DEFAULT_BINARY_JSON_PARSER_BYTES = 67108864; ///< Pinned parser request allowance.
inline constexpr std::size_t DEFAULT_BINARY_JSON_WORK_UNITS = 67108864;   ///< Independent logical work allowance.
inline constexpr std::size_t DEFAULT_BINARY_JSON_MODEL_BYTES = 67108864;  ///< Independent logical two-copy allowance.

/** Independent budgets for the explicitly selected native binary JSON profile.
 * These limits intersect: logical schema storage is not an allocation-request or process RSS cap.
 */
struct BinaryJsonSchemaLimits {
    JsonSchemaLimits loading{}; ///< Entire input/depth and original core count/name budgets.
    std::size_t parserBytes =
        DEFAULT_BINARY_JSON_PARSER_BYTES;                   ///< Fresh-parser requests, excluding allocator overhead.
    std::size_t workUnits = DEFAULT_BINARY_JSON_WORK_UNITS; ///< Logical byte/visit/comparison credits, not CPU/bytes.
    std::size_t schemaModelBytes = DEFAULT_BINARY_JSON_MODEL_BYTES; ///< Logical staging/result, not STL capacities.
};

/** Load only format 1, qdcxx-native-binary profile 1, from supplied bytes and explicit limits.
 * The loader performs no file IO or application property/source resolution. Parser-request
 * qualification covers the pinned SDK's built-in backends and ordinary SDK selection, including
 * SIMDJSON_FORCE_IMPLEMENTATION selecting a built-in backend. Foreign active-implementation
 * replacement or concurrent SDK-global mutation is outside this resource contract.
 * Input, DOM and parser storage never escape. Separate calls are independent; the caller must keep
 * input unchanged during this synchronous call. Zero budgets are literal and equality is admitted.
 * Caller input, old schemas and downstream copies remain outside these per-load budgets.
 * @param input Borrowed UTF-8 document, with no padding requirement.
 * @param limits Independent finite input, parser-request, logical work and two-copy schema-model bounds.
 * @return Complete owned immutable BINARY_FIELDS_V1 schema with independent provenance, or a typed
 * error without partial publication. Allocation failures are translated; caller-owned state is unchanged.
 */
[[nodiscard]] std::expected<core::LocalSchema, JsonSchemaError>
loadBinaryJsonSchema(std::string_view input, BinaryJsonSchemaLimits limits = {});

/** Load the complete native version-1 document without file IO or ambient configuration.
 * @param input Borrowed UTF-8 bytes, unchanged for the call; no padding requirement.
 * @param limits Finite loading budgets, checked before proportional owned allocations.
 * @return Fully owned immutable extended-profile schema with independent identity, or a typed error.
 */
[[nodiscard]] std::expected<core::LocalSchema, JsonSchemaError> loadJsonSchema(std::string_view input,
                                                                               JsonSchemaLimits limits = {});

/** Obtain a bounded diagnostic token independent of parser and input lifetime.
 * @param error Typed library failure.
 * @return Static uppercase token, or UNKNOWN for an invalid enum value.
 */
[[nodiscard]] std::string_view jsonSchemaErrorName(JsonSchemaError error) noexcept;

} // namespace org::ttldtor::qdcxx::schema
