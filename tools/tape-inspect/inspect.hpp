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
    protocol::TapeLimits tape{
        {1048576, 1048585},
        67108864,
        4096,
        {16, 8, {4096, 1024}, 8192},
        {{1024, 65536, 65536, 1024, 1048576}, 1024, {65536, 1024, 1048576}},
        16777216,
        {1048576, 1048576, 4194304, 1048576, 4194304, 4194304, {1048576, 4096, 4096}}}; ///< Explicit finite defaults.
};

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
 */
void report(std::ostream &output, int code, std::string_view stage, unsigned diagnosticsVersion = 1,
            protocol::TapeRejection reason = protocol::TapeRejection::NONE,
            inspect::OutputFormat format = inspect::OutputFormat::TSV) noexcept;
/** Read one bounded binary stream, retaining completed data prefix on errors.
 * @param input Binary source, borrowed for the call.
 * @param output Binary LF destination.
 * @param diagnostic Error destination.
 * @param schema Validated explicit local schema.
 * @param limits Finite IO/protocol budgets.
 * @param format TSV or safe quoted human output.
 * @param profile Explicit display grammar matching the schema; mismatch fails before input/output work.
 * @param diagnosticsVersion Explicit diagnostic grammar, defaulting to the legacy contract.
 * @return Native contract exit code; IO failure cannot be success.
 */
int inspect(std::istream &input, std::ostream &output, std::ostream &diagnostic, const core::LocalSchema &schema,
            Limits limits, inspect::OutputFormat format = inspect::OutputFormat::TSV,
            inspect::DisplayProfile profile = inspect::DisplayProfile::LOCAL_SCHEMA, unsigned diagnosticsVersion = 1);
} // namespace org::ttldtor::qdcxx::tape_inspect
