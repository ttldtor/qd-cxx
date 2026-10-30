// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "formatter.hpp"
#include "qd/protocol/tape_reader.hpp"
#include <iosfwd>

namespace org::ttldtor::qdcxx::tape_inspect {
/** Native utility IO and protocol bounds. */
struct Limits {
    std::size_t chunkSize = 4096;  ///< Bounded read buffer, positive.
    std::size_t output = 67108864; ///< All encoded stdout bytes.
    protocol::TapeLimits tape{{1048576, 1048585},
                              67108864,
                              4096,
                              {16, 8, {4096, 1024}, 8192},
                              {{1024, 65536, 65536, 1024, 1048576}, 1024, {65536, 1024, 1048576}},
                              16777216,
                              {1048576,
                               1048576,
                               4194304,
                               1048576,
                               4194304,
                               4194304,
                               {1048576, 4096, 4096},
                               {65536, 16384},
                               16777216,
                               4194304,
                               4194304},
                              4096};  ///< Explicit finite defaults, including RAW timing attempts.
    std::size_t eventUnits = 4194304; ///< Cumulative copied Profile symbol and string UTF-16 units.
};

/** Construct the fixed diagnostic mixed schema from FIELD-TYPES, without wire discovery.
 * @param limits Finite schema validation bounds.
 * @return Owned ordinary extended-field schema, or explicit validation/allocation failure.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> oracleFieldTypesSchema(core::SchemaLimits limits);

/** Validate IO representability and synchronize derived primitive limits.
 * @param limits Explicit user limits, updated only for derived bounds.
 * @return False when chunk/frame storage arithmetic cannot be represented.
 */
bool prepareLimits(Limits &limits) noexcept;
/** Report context-free native CLI/IO failure.
 * @param output Error destination.
 * @param code Nonzero process code.
 * @param stage Static operation.
 * @param diagnosticsVersion Explicit diagnostic grammar, 1 or 2.
 * @param reason Bounded protocol reason, never input text.
 * @param format TSV or safe human explanation selection.
 * @param mode Selected container policy for the explanation.
 */
void report(std::ostream &output, int code, std::string_view stage, unsigned diagnosticsVersion = 1,
            protocol::TapeRejection reason = protocol::TapeRejection::NONE,
            inspect::OutputFormat format = inspect::OutputFormat::TSV,
            protocol::TapeMode mode = protocol::TapeMode::LEGACY) noexcept;
/** Read one bounded binary stream, retaining completed data prefix on errors.
 * @param input Binary source, borrowed for the call.
 * @param output Binary LF destination.
 * @param diagnostic Error destination.
 * @param schema Validated explicit local schema.
 * @param limits Finite IO/protocol budgets.
 * @param format TSV or safe quoted human output.
 * @param profile Explicit display grammar matching the schema; mismatch fails before input/output work.
 * @param diagnosticsVersion Explicit diagnostic grammar, defaulting to the legacy contract.
 * @param mode Explicit container policy, independent of local schema and presentation.
 * @return Native contract exit code; IO failure cannot be success.
 */
int inspect(std::istream &input, std::ostream &output, std::ostream &diagnostic, const core::LocalSchema &schema,
            Limits limits, inspect::OutputFormat format = inspect::OutputFormat::TSV,
            inspect::DisplayProfile profile = inspect::DisplayProfile::LOCAL_SCHEMA, unsigned diagnosticsVersion = 1,
            protocol::TapeMode mode = protocol::TapeMode::LEGACY);

/** Inspect an explicitly wire-derived ordinary schema with self-describing output version 7.
 * Shares the existing synchronous bounded read/feed/render path; no JSON loader or event adapter.
 * @param input Binary source borrowed only during this call.
 * @param output Bounded LF destination including successful schema generations and exact sender IDs.
 * @param diagnostic Error destination; incomplete output never reports successful completion.
 * @param limits Finite IO, final schema and cumulative construction limits.
 * @param format TSV hex or safe quoted human rendering.
 * @param diagnosticsVersion Explicit existing diagnostic grammar, default 2.
 * @param mode Explicit existing container policy; never inferred from input.
 * @return Native contract exit code; only complete EOF and successful flush return zero.
 */
int inspectWire(std::istream &input, std::ostream &output, std::ostream &diagnostic, Limits limits,
                inspect::OutputFormat format = inspect::OutputFormat::TSV, unsigned diagnosticsVersion = 2,
                protocol::TapeMode mode = protocol::TapeMode::LEGACY);
} // namespace org::ttldtor::qdcxx::tape_inspect
