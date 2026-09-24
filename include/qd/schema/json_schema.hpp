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
    UNSUPPORTED_PROFILE, ///< Profile identity outside qdcxx-native.
    UNSUPPORTED_VERSION, ///< Format or native profile version outside version 1.
    UNSUPPORTED_TYPE,    ///< Full field identity outside the extended exact-type subset.
    UNSUPPORTED_INDEXED, ///< Indexed record semantics requested through hasTime:true.
    RESOURCE_LIMIT,      ///< Input, nesting, logical schema or parser-size budget exceeded.
    ALLOCATION_FAILURE   ///< Required parser or owned schema storage could not be allocated.
};

/** Finite input and logical bounds; zero is meaningful and equality is admitted. */
struct JsonSchemaLimits {
    std::size_t documentBytes = 1048576; ///< Entire supplied document, including whitespace, in bytes.
    std::size_t depth = 16;              ///< Nested objects/arrays, including the root at depth one.
    core::SchemaLimits schema{1024, 16384, 32768, 1024, 1048576}; ///< Counts and exact UTF-16 name units.
};

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
