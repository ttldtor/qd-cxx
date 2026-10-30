// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "compressed_input.hpp"
#include "source_time.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::tape_inspect {
/** Successful segment counters; identity and all coordinates remain segment local. */
struct SourceEnd {
    std::size_t sourceOrdinal = 0;      ///< Zero-based source occurrence.
    std::uint64_t accepted = 0;         ///< Logical parser-accepted data bytes.
    std::uint64_t frames = 0;           ///< Accepted envelopes.
    std::uint64_t emitted = 0;          ///< Emitted records.
    std::uint64_t processed = 0;        ///< Processed record attempts, including skips.
    std::uint64_t wireFields = 0;       ///< Examined wire-field attempts.
    std::uint64_t physicalFetched = 0;  ///< Actual data-source bytes fetched.
    std::uint64_t physicalConsumed = 0; ///< Actual data-container bytes consumed.
};

/** Checked aggregate summary; its encoded marker is provisional until final flush and success. */
struct SequenceSummary {
    std::uint64_t sources = 0;    ///< Successfully closed occurrences.
    std::uint64_t accepted = 0;   ///< Aggregate logical parser-accepted data bytes.
    std::uint64_t frames = 0;     ///< Aggregate accepted envelopes.
    std::uint64_t emitted = 0;    ///< Aggregate emitted records.
    std::uint64_t processed = 0;  ///< Aggregate processed record attempts.
    std::uint64_t wireFields = 0; ///< Aggregate wire-field attempts.
};

/** Borrowed diagnostic presentation input; strings must be static safe tokens, never input text.
 * All optional coordinates are segment local; absence is emitted as '-', never an invented zero.
 * This object is consumed synchronously and no member is retained by the formatter.
 */
struct SourceDiagnostic {
    int exitCode = 0;                              ///< Nonzero terminal process code.
    std::string_view stage;                        ///< Static stage token, at most 64 lowercase ASCII token bytes.
    std::string_view staticReason;                 ///< Static reason token, with the same token bound.
    std::optional<std::size_t> sourceOrdinal;      ///< Source occurrence, absent for list-level failure.
    std::string_view layer;                        ///< One of data/time/parser/sequence/sink.
    std::optional<std::uint64_t> physicalFetched;  ///< Actual bytes fetched by the selected source.
    std::optional<std::uint64_t> physicalConsumed; ///< Actual selected-container bytes consumed.
    std::optional<std::uint64_t> logicalProduced;  ///< Logical bytes produced by the selected container.
    std::optional<std::uint64_t> parserAccepted;   ///< Logical bytes accepted by the parser.
    std::optional<std::uint64_t> frameStart;       ///< Logical segment-local frame start.
    std::optional<std::uint64_t> payloadStart;     ///< Logical segment-local payload start.
    std::optional<std::uint64_t> sidecarLine;      ///< Segment-local companion line ordinal.
};

/** Compose the exact version-9 TSV header.
 * @param budget Remaining encoded bytes.
 * @return Complete row; throws inspect::OutputLimit before allocation on excess.
 */
std::string sourceHeader(std::size_t budget);
/** Compose explicit homogeneous policy and envelope selections immediately after each source-begin.
 * @param policy Explicit timing declaration.
 * @param data Data envelope.
 * @param time Companion envelope, absent without a companion.
 * @param budget Remaining encoded bytes.
 * @return Complete row; invalid_argument for invalid enums, OutputLimit on excess.
 */
std::string sourcePolicy(SourceTimePolicy policy, ContainerKind data, std::optional<ContainerKind> time,
                         std::size_t budget);
/** Compose native path identity without transcoding, retaining no caller data.
 * @param ordinal Zero-based occurrence.
 * @param path Native data path borrowed for this call.
 * @param companion Optional native companion path borrowed for this call.
 * @param budget Remaining encoded bytes.
 * @return Complete five-cell row; OutputLimit before allocation on excess.
 */
std::string sourceIdentity(std::size_t ordinal, const std::filesystem::path &path,
                           const std::filesystem::path *companion, std::size_t budget);
/** Count exact path identity bytes without allocation, copies or IO for owner admission.
 * @param ordinal Zero-based occurrence.
 * @param path Native data path borrowed only for this call.
 * @param companion Optional native companion path borrowed only for this call.
 * @return Exact encoded row length, or nullopt on checked arithmetic overflow.
 */
std::optional<std::size_t> sourceIdentityBytes(std::size_t ordinal, const std::filesystem::path &path,
                                               const std::filesystem::path *companion) noexcept;
/** Compose an occurrence boundary.
 * @param ordinal Zero-based occurrence.
 * @param budget Remaining encoded bytes.
 * @return Complete row or OutputLimit.
 */
std::string sourceBegin(std::size_t ordinal, std::size_t budget);
/** Compose a successfully closed segment summary.
 * @param counters Borrowed segment-local counters.
 * @param budget Remaining encoded bytes.
 * @return Complete nine-cell row or OutputLimit.
 */
std::string sourceEnd(const SourceEnd &counters, std::size_t budget);
/** Compose the provisional aggregate marker, never an assertion of sink flush success.
 * @param counters Borrowed checked aggregate counters.
 * @param budget Remaining encoded bytes.
 * @return Complete seven-cell row or OutputLimit.
 */
std::string sourceComplete(const SequenceSummary &counters, std::size_t budget);
/** Compose one member of the owner's fixed resource inventory, retaining no strings.
 * @param scope Static sequence/data/time/scanner/parser token.
 * @param ordinal Source occurrence, absent for sequence scope.
 * @param name Static counter token, at most 64 ASCII alphanumeric/hyphen bytes.
 * @param reserved Admitted capacity, never a measurement.
 * @param observed Actual counter, absent when unavailable.
 * @param budget Remaining encoded bytes.
 * @return Complete six-cell row; invalid_argument for invalid tokens, OutputLimit on excess.
 */
std::string sourceResource(std::string_view scope, std::optional<std::size_t> ordinal, std::string_view name,
                           std::uint64_t reserved, std::optional<std::uint64_t> observed, std::size_t budget);
/** Compose a fixed diagnostic with no raw path/input text or compressed inverse mapping.
 * @param diagnostic Static tokens and optional known coordinates, borrowed only during this call.
 * @param cleanup Select cleanup-v3 instead of incomplete-v3.
 * @param budget Remaining encoded bytes.
 * @return Complete thirteen-cell row; invalid_argument for invalid tokens/code, OutputLimit on excess.
 */
std::string sourceDiagnostic(const SourceDiagnostic &diagnostic, bool cleanup, std::size_t budget);
/** Calculate a conservative encoded terminal reserve using exact maximum-width row builders.
 * Paths belong to earlier identity rows, so they do not enter this terminal grammar.
 * @param resourceRows Number of terminal resource rows in the owner's fixed inventory.
 * @param diagnosticRows Maximum primary and cleanup diagnostic rows.
 * @return Complete marker plus row capacity; nullopt on checked arithmetic overflow.
 */
std::optional<std::size_t> terminalReserveWorstCase(std::size_t resourceRows, std::size_t diagnosticRows) noexcept;
} // namespace org::ttldtor::qdcxx::tape_inspect
