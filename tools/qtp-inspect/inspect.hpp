// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "formatter.hpp"
#include "qd/core/local_schema.hpp"

#include <iosfwd>
#include <string_view>

namespace org::ttldtor::qdcxx::inspect {

/** Finite tool policy bounds; counts are logical elements rather than exact RSS. */
struct Limits {
    std::size_t chunkSize = 4096;            ///< Positive input bytes per read.
    std::size_t input = 67108864;            ///< Total accepted input bytes.
    std::size_t frame = 1048576;             ///< Type plus payload bytes per frame.
    std::size_t frames = 4096;               ///< Total frames including empty heartbeats.
    std::size_t descriptions = 1024;         ///< Initial descriptions across frames.
    std::size_t actions = 65536;             ///< Initial binding actions across descriptions.
    std::size_t nameUnits = 1024;            ///< UTF-16 units in one description name.
    std::size_t totalNameUnits = 1048576;    ///< Aggregate description name units.
    std::size_t records = 1048576;           ///< Charged record attempts across data messages.
    std::size_t fields = 4194304;            ///< Charged wire-field attempts across data messages.
    std::size_t values = 4194304;            ///< Completed local output values across messages.
    std::size_t symbolUnits = 4194304;       ///< Owned completed output symbol units across messages.
    std::size_t output = 67108864;           ///< All stdout bytes including header and completion.
    std::size_t messageRecords = 65536;      ///< Charged records per data message.
    std::size_t messageFields = 262144;      ///< Charged wire fields per data message.
    std::size_t messageValues = 262144;      ///< Completed local values per data message.
    std::size_t messageSymbolUnits = 262144; ///< Owned completed symbol units per data message.
    std::size_t symbol = 4096;               ///< Units per current and retained symbol.
};

/** Check representability and compatible storage bounds without allocation.
 * @param limits Explicit tool policy.
 * @return True if chunk and frame storage arithmetic is valid.
 */
bool validLimits(const Limits &limits) noexcept;

/** Build the temporary explicit oracle-sre-v1 fixture adapter.
 * @return Owned validated schema or a schema/resource error.
 */
std::expected<core::LocalSchema, core::SchemaError> oracleSchema();

/** Inspect a raw binary QTP stream with independently supplied immutable local schema.
 * @param input Binary input; no data is retained after this call.
 * @param output Binary UTF-8/LF destination, flushed on completion.
 * @param diagnostic Binary ASCII/LF error destination.
 * @param schema Borrowed validated local schema; never inferred from wire descriptions.
 * @param limits Finite file and message budgets.
 * @param format Output representation, TSV by default.
 * @param profile Explicit display metadata matching the supplied schema.
 * @return Contract exit code; catches allocation and stream failures, never reports failed output as success.
 */
int inspect(std::istream &input, std::ostream &output, std::ostream &diagnostic, const core::LocalSchema &schema,
            Limits limits, OutputFormat format = OutputFormat::TSV,
            DisplayProfile profile = DisplayProfile::LOCAL_SCHEMA);

/** Emit a context-free diagnostic, swallowing errors of the diagnostic destination itself.
 * @param diagnostic Binary stderr destination.
 * @param code Nonzero contract exit code.
 * @param stage Static ASCII checkpoint name.
 */
void report(std::ostream &diagnostic, int code, std::string_view stage) noexcept;
} // namespace org::ttldtor::qdcxx::inspect
