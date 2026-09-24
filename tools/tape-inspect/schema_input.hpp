// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <expected>
#include <iosfwd>
#include <string>

namespace org::ttldtor::qdcxx::tape_inspect {

/** Source-acquisition errors, separate from JSON and protocol diagnostics. */
enum class SchemaInputError {
    IO_ERROR,          ///< Failed source read, including a non-file stream failure.
    RESOURCE_LIMIT,    ///< More than the admitted document bytes were present.
    ALLOCATION_FAILURE ///< The bounded document could not be allocated.
};

/** Read one document without trusting a prior file size or retaining a partial result.
 * @param input Borrowed binary stream, read from its current position through EOF.
 * @param maximumBytes Maximum admitted document bytes; one extra byte may be probed for EOF.
 * @return Owned exact bytes or a source error; input growth cannot bypass the bound.
 */
[[nodiscard]] std::expected<std::string, SchemaInputError> readSchemaDocument(std::istream &input,
                                                                              std::size_t maximumBytes);

} // namespace org::ttldtor::qdcxx::tape_inspect
