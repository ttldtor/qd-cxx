// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "compressed_input.hpp"
#include "inspect.hpp"
#include "sidecar_time.hpp"
#include "source_format.hpp"
#include <filesystem>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>

namespace org::ttldtor::qdcxx::tape_inspect {
/** Explicit owning companion selection; no inferred filename or filesystem discovery. */
struct TapeCompanionSpec {
    std::filesystem::path path;                    ///< Native path owned by the caller, copied only after admission.
    ContainerKind container = ContainerKind::NONE; ///< Independently selected envelope.
    ContainerLimits containerLimits;               ///< Literal per-instance capacities, including decoder storage.
    SidecarLimits scannerLimits;                   ///< Literal per-instance scanner capacities.
};

/** One replay occurrence; repeated paths remain separate occurrences in caller order. */
struct TapeSegmentSpec {
    std::filesystem::path path;                    ///< Native data path owned by the caller.
    ContainerKind container = ContainerKind::NONE; ///< Independently selected data envelope.
    ContainerLimits containerLimits;               ///< Literal per-instance data capacities.
    Limits limits;                              ///< Fully resolved parser/IO/output limits; never implicitly enlarged.
    std::optional<TapeCompanionSpec> companion; ///< Required exactly for LONG and TEXT.
};

/** Conservative sums of full declared capacities; every initial zero is literal.
 * Nested field meanings/units are those of the existing limit types. Per-value and peak capacities
 * are summed as well as cumulative work, so fresh instances cannot refill aggregate reservations.
 * These are admitted capacities, never actual byte/work/RSS measurements.
 */
struct SequenceReservations {
    protocol::TapeLimits tape{}; ///< Every nested parser, schema, descriptor, payload and FIELD capacity.
    ContainerLimits dataContainer{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};      ///< Data envelope capacities.
    ContainerLimits companionContainer{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; ///< Companion envelope capacities.
    SidecarLimits scanner{0, 0, 0, 0, 0, 0}; ///< Selected companion scanner capacities only.
    std::size_t chunkBytes = 0;              ///< Sum of per-occurrence parser feed-buffer capacities.
    std::size_t outputBytes = 0;             ///< Sum of per-occurrence encoded output allowances.
    std::size_t eventUnits = 0;              ///< Sum of per-occurrence event-copy allowances.
    std::uint64_t physicalProbes = 0;        ///< One physical diagnostic excess byte per selected container, even NONE.
    std::uint64_t inflatedProbes = 0;        ///< One discarded logical excess byte per selected container, even NONE.
};

/** Finite owning-orchestration configuration; aggregate defaults authorize no source work. */
struct TapeSequenceOptions {
    SourceTimePolicy time = SourceTimePolicy::NONE; ///< Explicit common self-contained timing profile.
    SequenceReservations aggregate;     ///< Explicit full-capacity reservations, with no automatic multiplication.
    std::size_t occurrences = 256;      ///< Maximum caller-selected occurrences, including repeated paths.
    std::size_t pathNativeUnits = 4096; ///< Maximum one path length in native code units, excluding terminator.
    std::size_t totalPathNativeUnits = 1048576; ///< Sum of path lengths, including repeated data/companion names.
    std::size_t pathEncodedBytes = 2097152;     ///< Sum of safe native-byte hex diagnostic encodings.
    std::size_t pathStorageBytes = 1048576;     ///< Copied spec objects plus native path characters and terminators.
    std::size_t diagnosticBytes = 1024;         ///< Fixed owning primary and secondary diagnostic object storage.
    std::size_t sourceHandles = 2;      ///< Maximum simultaneously owned physical files; only one pair is active.
    std::size_t outputBytes = 67108864; ///< One sequence-wide allowance including boundaries and terminal summary.
    std::size_t terminalBytes = 65536;  ///< Reserved within outputBytes before any segment publication.
    inspect::OutputFormat format = inspect::OutputFormat::TSV; ///< Safe TSV or HUMAN record presentation.
};

/** Admission failures occur before allocation, copying, source opening or stream work. */
enum class SequenceError {
    NONE,                       ///< Configuration and every reservation fit.
    EMPTY_SEQUENCE,             ///< No occurrence was selected.
    INVALID_ARGUMENT,           ///< Invalid enum, path or incompatible per-source configuration.
    UNSUPPORTED_SOURCE_PROFILE, ///< Companion selection disagrees with the explicit homogeneous timing policy.
    UNSUPPORTED_CONTAINER,      ///< Known envelope backend unavailable in this build.
    RESOURCE_LIMIT,             ///< A finite aggregate/name/storage/handle/output allowance is too small.
    UNREPRESENTABLE             ///< A required sum, product or IO/storage representation would overflow.
};

/** Fixed owning diagnostic; no input path, schema, span, callback or dynamic text is retained. */
struct SequenceDiagnostic {
    SequenceError error = SequenceError::NONE; ///< First admission cause.
    std::size_t sourceOrdinal = 0;             ///< Zero-based occurrence checkpoint; zero for list-level validation.
    std::string_view component = "none";       ///< Static component literal, never a caller-owned view.
};

/** Allocation-free admission result; a failed prefix is never an authorization to open sources. */
struct SequenceAdmission {
    SequenceDiagnostic diagnostic;    ///< First fixed cause, or NONE after complete list validation.
    SequenceReservations reserved;    ///< Full checked sums on success; only the checked prefix on failure.
    std::size_t pathNativeUnits = 0;  ///< Sum of selected native name units, excluding terminators.
    std::size_t pathEncodedBytes = 0; ///< Safe native-byte hex encoding capacity, excluding grammar delimiters.
    std::size_t pathStorageBytes = 0; ///< Copied specs and native name bytes including terminators.
};

/** Admit all names and every nested full-capacity reservation without allocation or IO.
 * Caller inputs are borrowed synchronously, unchanged and never retained. The owner must copy only
 * after success, use these exact per-source limits, and never refund or borrow reservations.
 * This checks configuration only; it does not establish source existence, content or completion.
 * @param segments Explicit finite occurrence list; caller order and repeats are preserved.
 * @param options Homogeneous timing, storage and aggregate bounds; all zeros are literal.
 * @return Fixed owning result; only NONE authorizes later copies/opens.
 */
[[nodiscard]] SequenceAdmission admitSequence(std::span<const TapeSegmentSpec> segments,
                                              const TapeSequenceOptions &options) noexcept;

/** Owning terminal state; a printed marker is provisional until this result is COMPLETE. */
enum class SequenceStatus {
    COMPLETE, ///< Every selected layer and physical close plus both final flushes succeeded.
    FAILED,   ///< First causal configuration, source, parse, output or cleanup failure.
    STOPPED   ///< Cancellation was the first observed cause; cleanup performed no further reads.
};

/** Fixed owning result; no input path/span/schema pointer or callback survives the call. */
struct TapeSequenceResult {
    SequenceStatus status = SequenceStatus::FAILED; ///< Owning terminal state, never inferred from printed bytes.
    SourceDiagnostic primary; ///< First fixed cause; static token views never borrow input or exception text.
    std::optional<SourceDiagnostic> dataClose; ///< At most one secondary data-close failure with source identity.
    std::optional<SourceDiagnostic> timeClose; ///< At most one secondary companion-close failure.
    std::optional<SourceDiagnostic> flush;     ///< At most one secondary output/diagnostic flush or reporting failure.
    SequenceSummary counters;      ///< Actual processed prefix; sources counts only successfully closed occurrences.
    SequenceReservations reserved; ///< Full admitted capacities, separate from observed counters.
    std::size_t maximumOpenHandles = 0; ///< Actual maximum simultaneous physical files owned by this call.
    std::size_t outputChargedBytes = 0; ///< Encoded write-attempt charges; not an OS-delivery or durable-byte claim.
};

/** Inspect a fully admitted explicit finite list, owning one physical pair at a time.
 * @param segments Caller-owned specifications, borrowed only during the synchronous call.
 * @param output Borrowed output destination, valid through both fallible final flushes.
 * @param diagnostic Borrowed bounded terminal diagnostic destination.
 * @param options Explicit fully resolved per-source and aggregate configuration.
 * @param stop Optional synchronous cancellation state; no callback or detached work is registered.
 * @return Fixed owning status; COMPLETE requires every layer, physical close and final flush.
 */
[[nodiscard]] TapeSequenceResult inspectSequence(std::span<const TapeSegmentSpec> segments, std::ostream &output,
                                                 std::ostream &diagnostic, const TapeSequenceOptions &options,
                                                 std::stop_token stop = {});
} // namespace org::ttldtor::qdcxx::tape_inspect
