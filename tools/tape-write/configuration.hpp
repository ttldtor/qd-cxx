// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/tape_admission.hpp"
#include "qd/schema/json_schema.hpp"

#include <array>
#include <filesystem>
#include <iosfwd>

namespace org::ttldtor::qdcxx::tape_write {
/** Owned command-line policy resolved before file operations. */
struct Configuration {
    std::filesystem::path input;           ///< Literal nonempty native source path.
    std::filesystem::path output;          ///< Literal nonempty native fresh destination path.
    std::filesystem::path schemaPath;      ///< Explicit optional native JSON document.
    std::string profile;                   ///< Explicit built-in selector, absent for JSON.
    schema::JsonSchemaLimits schemaLimits; ///< Unchanged native loading bounds.
    std::array<std::size_t, 31> limits;    ///< Native tape/admission/writer counts in documented option order.
};
/** Resolution outcome, independent of I/O and library validation. */
enum class ArgumentStatus {
    RUN,        ///< Complete explicit configuration.
    HELP,       ///< No arguments or standalone help.
    INVALID,    ///< Invalid spelling, duplicate, conflict or missing argument.
    UNSUPPORTED ///< Explicit unsupported mode or profile.
};
/** Resolve separate option/value tokens once without opening files.
 * @param argc Native process argument count.
 * @param argv Borrowed process argument vector.
 * @param result Receives owned paths, source and finite limits.
 * @return Resolution category; allocations may throw to the process boundary.
 */
ArgumentStatus parseArguments(int argc, char **argv, Configuration &result);
/** Emit complete help including every exact finite default.
 * @param output Borrowed output stream.
 */
void writeHelp(std::ostream &output);
/** Translate every CLI limit into independent reader/admission/layout bounds.
 * @param configuration Explicit resolved configuration.
 * @return Required library bounds or arithmetic refusal.
 */
std::expected<protocol::AdmissionLimits, int> admissionLimits(const Configuration &configuration);
/** Translate every writer-related CLI limit into required writer bounds.
 * @param configuration Explicit resolved configuration.
 * @return Independent writer bounds.
 */
protocol::WriterLimits writerLimits(const Configuration &configuration);
/** Build a validated explicit built-in without discovery or event projection.
 * @param configuration Resolved built-in and schema bounds.
 * @return Owned schema or typed core failure.
 */
std::expected<core::LocalSchema, core::SchemaError> builtinSchema(const Configuration &configuration);
} // namespace org::ttldtor::qdcxx::tape_write
