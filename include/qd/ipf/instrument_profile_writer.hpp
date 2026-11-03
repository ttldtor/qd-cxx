// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <ostream>
#include <qd/ipf/instrument_profile.hpp>
#include <span>
#include <string>

namespace org::ttldtor::qdcxx::ipf {
/** Checked local writer failures; no partial owned document is returned. */
enum class IpfWriteError {
    INVALID_TYPE,          ///< The type's emitted format is forbidden by the reader.
    UNREPRESENTABLE_UTF16, ///< An unpaired surrogate would lose canonical text during UTF-8 encoding.
    REMOVED_FIELDS,        ///< Nonempty extra REMOVED fields would be discarded by the reference writer.
    PROFILE_LIMIT,         ///< Too many source profiles.
    TYPE_LIMIT,            ///< Too many distinct format types.
    FIELD_LIMIT,           ///< A format or source custom count exceeds the field bound.
    HEADER_LIMIT,          ///< Retained format names exceed their UTF-16 bound.
    INPUT_LIMIT,           ///< Actual source retained text exceeds the aggregate UTF-16 bound.
    OUTPUT_LIMIT,          ///< The complete prepared UTF-8 document exceeds its byte bound.
    WORK_LIMIT,            ///< Logical preparation work exceeds its finite bound.
    IO_ERROR,              ///< Stream open, write, flush or owned-file close failed.
    ALLOCATION_FAILURE     ///< An observable allocation failure prevented preparation/publication.
};

/** Fixed-size diagnostic, independent of source and stream lifetimes. */
struct IpfWriteDiagnostic {
    IpfWriteError error;          ///< First preparation/publication failure.
    std::size_t profileIndex = 0; ///< Zero-based source profile, or zero when no profile applies.
    std::size_t fieldIndex = 0;   ///< Standard enum index, then31 plus custom lexical index when applicable.
};

/** Configurable finite engineering bounds, independent of process settings. */
struct IpfWriteLimits {
    std::size_t profiles = 100000;      ///< Maximum borrowed source profile count.
    std::size_t types = 256;            ///< Maximum distinct retained formats.
    std::size_t fieldsPerType = 256;    ///< Maximum fields including TYPE and required SYMBOL.
    std::size_t headerUnits = 1048576;  ///< Retained type/field-name UTF-16 units.
    std::size_t inputUnits = 16777216;  ///< Aggregate actual source retained UTF-16 units.
    std::size_t outputBytes = 67108864; ///< Complete owning UTF-8 document bytes, including marker.
    std::size_t work = 33554432;        ///< Logical field/unit/output-byte preparation work.
};

/** Explicit per-call writer options; no ambient properties are read. */
struct IpfWriteOptions {
    IpfWriteLimits limits; ///< Input, format, work and complete-output bounds.
};

/** Publication outcome; an error never claims successful completion. */
struct IpfWriteResult {
    std::uint64_t confirmedBytes = 0; ///< Fully successful write-call bytes; a failing call may have side effects.
    bool complete = false;            ///< All writes/flushes and any owned-file close succeeded.
    std::optional<IpfWriteDiagnostic> error; ///< First preparation/publication failure, without source borrowing.
};

/** Prepare one complete uncompressed UTF-8 IPF document without I/O.
 * Canonical field text is preserved for representable input, with deterministic formats/CSV/CRLF.
 * Unpaired UTF-16, forbidden types and nonempty extra REMOVED fields are rejected without publication.
 * Raw numeric spelling, presence and signed-zero bits are not serialized. Inputs remain unchanged.
 * @param profiles Borrowed immutable values for this call; callers serialize mutation/observation.
 * @param options Explicit cumulative preparation/retention/output limits.
 * @return Entire owning document including the completion marker, or a fixed first diagnostic.
 */
[[nodiscard]] std::expected<std::string, IpfWriteDiagnostic>
composeInstrumentProfiles(std::span<const InstrumentProfile> profiles, IpfWriteOptions options = {});
/** Prepare and synchronously publish one local uncompressed IPF document.
 * Preparation completes before any I/O. The borrowed stream is never closed, sought, rolled back
 * or given a different exception mask. The body is written/flushed before the marker is attempted.
 * complete is true only after final flush. A late error may leave an externally visible marker;
 * confirmedBytes is a successful-write lower bound, not a physical cursor or atomicity promise.
 * Escaping bad_alloc maps to ALLOCATION_FAILURE; a swallowed stream failure maps to IO_ERROR.
 * Resource/work bounds are not blocking-I/O deadlines. No callback or input view is retained.
 * @param output Caller-owned stream whose lifetime covers the call.
 * @param profiles Borrowed immutable source values for the call.
 * @param options Explicit finite preparation options.
 * @return Owning success/failure outcome; input and caller exception mask remain unchanged.
 */
[[nodiscard]] IpfWriteResult writeInstrumentProfiles(std::ostream &output, std::span<const InstrumentProfile> profiles,
                                                     IpfWriteOptions options = {});
/** Prepare, then write one local file in binary/truncating mode.
 * Preparation failure leaves the destination unopened. Successful open may destroy prior contents;
 * publication is not a filesystem transaction. Only the owned ofstream is closed. complete becomes
 * true only after close succeeds. No URL, compression, temporary rename or network work occurs.
 * @param path Borrowed local path used for the owned binary file.
 * @param profiles Borrowed immutable source values for this call.
 * @param options Explicit finite preparation options.
 * @return Typed open/write/flush/close result; failed publication may leave a partial file.
 */
[[nodiscard]] IpfWriteResult writeInstrumentProfilesFile(const std::filesystem::path &path,
                                                         std::span<const InstrumentProfile> profiles,
                                                         IpfWriteOptions options = {});
} // namespace org::ttldtor::qdcxx::ipf
