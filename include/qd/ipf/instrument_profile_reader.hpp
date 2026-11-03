// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/ipf/instrument_profile.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace org::ttldtor::qdcxx::ipf {
/** Explicit policy for a clean EOF without the IPF completion marker. */
enum class IpfCompletionPolicy {
    ALLOW,  ///< Return a clean incomplete snapshot without ambient logging or properties.
    REQUIRE ///< Report MISSING_COMPLETE while retaining the accepted prefix.
};
/** First terminal reader/parser failure; no partial profile is emitted. */
enum class IpfReadError {
    UNQUOTED_QUOTE,     ///< A quote occurred after unquoted field text.
    UNPAIRED_QUOTE,     ///< Text followed a closing quote before a separator/end.
    UNTERMINATED_QUOTE, ///< Explicit EOF occurred inside a quoted field.
    UNDEFINED_FORMAT,   ///< A nonempty row has no preceding matching format.
    FORBIDDEN_FORMAT,   ///< A format attempts to redefine reserved metadata/control names.
    FIELD_COUNT,        ///< Data row has a different field count than its current format.
    PROFILE_CONVERSION, ///< A nonempty standard/custom field violates its profile conversion/limits.
    INPUT_LIMIT,        ///< Cumulative accepted input bytes would exceed the configured bound.
    FIELD_LIMIT,        ///< A decoded field would exceed its UTF-16 value bound.
    RECORD_LIMIT,       ///< A row's decoded syntax/text, field count or cumulative row count exceeds its bound.
    FORMAT_LIMIT,       ///< Retained format count would exceed its bound.
    HEADER_LIMIT,       ///< Retained format-name/field-name units would exceed their aggregate bound.
    OUTPUT_LIMIT,       ///< Emitted profile count/UTF-16 units would exceed their aggregate bound.
    MISSING_COMPLETE,   ///< Clean EOF did not contain a completion marker under REQUIRE policy.
    POSITION_OVERFLOW,  ///< A logical source counter cannot be incremented without overflow.
    INVALID_STATE,      ///< Nonempty input was submitted after explicit EOF began.
    INVALID_OPTIONS,    ///< A synchronous driver's work quotas cannot make progress.
    IO_ERROR,           ///< A local stream/file operation failed; the accepted profile prefix remains owned.
    ALLOCATION_FAILURE  ///< Allocation failed; the accepted prefix and first error remain available.
};

/** Logical source position, independent of underlying stream buffering/read-ahead. */
struct IpfPosition {
    std::uint64_t byteOffset = 0; ///< Zero-based position in accepted UTF-8 bytes.
    std::uint64_t line = 1;       ///< One-based physical line; CRLF counts once.
    std::uint64_t record = 1;     ///< One-based CSV row, including blank/comment/header/control rows.
    std::size_t field = 0;        ///< Zero-based CSV field within the row, including the type field.
};

/** Fixed-size owning diagnostic; producing it requires no string allocation. */
struct IpfDiagnostic {
    IpfReadError error;       ///< First terminal failure category.
    IpfPosition position;     ///< Logical failure cursor; no tellg/physical rollback is implied.
    std::uint64_t recordLine; ///< Physical start line of the rejected row.
    std::uint64_t fieldLine;  ///< Physical start line of the rejected field; useful for multiline CSV errors.
    std::optional<ProfileError> profileError; ///< Nested profile error only for PROFILE_CONVERSION.
};

/** Finite per-instance logical storage/output/input limits; zero is meaningful. */
struct IpfReadLimits {
    std::size_t inputBytes = 67108864; ///< Maximum cumulative accepted UTF-8 bytes.
    std::size_t fieldUnits = 65536;    ///< Maximum unescaped UTF-16 units in one field.
    std::size_t recordUnits =
        1048576; ///< Maximum decoded units in one row, including quotes/commas, excluding its terminator.
    std::size_t fieldsPerRecord = 256; ///< Maximum fields in a row, including its first/type field.
    std::size_t records = 1000000;     ///< Maximum admitted rows, including blank/comment/header/control rows.
    std::size_t formats = 256;         ///< Maximum retained distinct type formats.
    std::size_t headerUnits =
        1048576; ///< Maximum retained type/field-name UTF-16 units; a staged replacement is bounded by recordUnits.
    std::size_t profiles = 100000;       ///< Maximum emitted profiles.
    std::size_t profileUnits = 16777216; ///< Maximum cumulative emitted standard values plus custom names/values.
    ProfileLimits profile{};             ///< Independent limits for each owned profile value.
};

/** Mutable work allowance consumed by a single parser call; exhausted work is never refunded. */
struct IpfReadWork {
    std::size_t inputBytes = 4096;   ///< Bytes accepted this call, including malformed UTF-8 bytes.
    std::size_t decodedUnits = 4096; ///< UTF-16 units processed this call, including replacements and syntax.
    std::size_t fields = 256;        ///< Completed field conversion/metadata operations this call.
    std::size_t records = 256;       ///< Complete row admission operations this call.
};

/** Explicit local read options; no properties, environment variables or URLs are resolved. */
struct IpfReadOptions {
    IpfReadLimits limits{};                                      ///< Independent input/storage/output bounds.
    IpfCompletionPolicy completion = IpfCompletionPolicy::ALLOW; ///< Explicit EOF completion policy.
    IpfReadWork turnWork{}; ///< Positive per-turn quotas used only by synchronous convenience readers.
};
/** Owning parser output kind; control records do not terminate parsing. */
enum class IpfItemKind {
    PROFILE, ///< A complete owned instrument profile.
    FLUSH,   ///< Exact ## control record, after validation of its CSV remainder.
    COMPLETE ///< Exact `##COMPLETE` control record, after validation of its CSV remainder.
};

/** One owning output; no input/profile/header view is retained in the result. */
struct IpfItem {
    IpfItemKind kind;                         ///< Whole profile or control event.
    std::optional<InstrumentProfile> profile; ///< Engaged exactly for PROFILE.
    IpfPosition position;                     ///< Start of the admitted row.
};
/** Parser progress/status; empty feed is never EOF. */
enum class IpfReadStatus {
    NEED_INPUT,     ///< No output; supply more input or explicit finish.
    ITEM_READY,     ///< Exactly one owned item; resubmit the unaccepted byte suffix.
    WORK_EXHAUSTED, ///< No more permitted work; replenish before retrying the remaining suffix/finish.
    FINISHED,       ///< Explicit EOF was accepted cleanly under the selected completion policy.
    FAILED          ///< First terminal error; no further bytes are accepted.
};

/** One bounded parser operation. */
struct IpfReadStep {
    std::size_t accepted = 0; ///< Bytes consumed from this call's span, excluding its remaining suffix.
    IpfReadStatus status = IpfReadStatus::NEED_INPUT; ///< Input/output/work/terminal outcome.
    std::optional<IpfItem> item;                      ///< Independent item exactly on ITEM_READY.
};

/** Snapshot of committed parser progress, independent of returned item lifetimes. */
struct IpfReadProgress {
    std::uint64_t inputBytes = 0;  ///< Accepted input, including the rejected row's examined prefix.
    std::uint64_t records = 0;     ///< Fully admitted rows; a rejected row is excluded.
    std::uint64_t profiles = 0;    ///< Whole profiles emitted by this parser.
    std::uint64_t flushes = 0;     ///< Admitted flush controls.
    std::uint64_t completions = 0; ///< Admitted completion controls.
    std::size_t formats = 0;       ///< Retained distinct formats.
    std::size_t headerUnits = 0;   ///< Retained format type/field-name units.
    std::size_t profileUnits = 0;  ///< Cumulative units charged for emitted profiles.
    bool completionSeen = false;   ///< A completion control was admitted, even if later parsing fails.
};
struct IpfReadResult;

/** Single-threaded owning incremental UTF-8/CSV/IPF parser, without I/O, callbacks or output queues.
 * Input spans are borrowed only for a call. Returned profiles survive further input/destruction.
 * UTF-8 decoding uses measured JDK replacement behavior; BOM is preserved as ordinary text.
 * Complete headers replace formats atomically. Current row/field staging remains bounded separately
 * from retained formats; syntax/conversion/operational failures discard only the current profile.
 * Construction can throw std::bad_alloc. Parsing maps allocation failures to a fixed diagnostic.
 * Copying/moving is disabled; create a fresh parser for a new stream/lifecycle.
 */
class InstrumentProfileParser {
    public:
    /** Construct an independent parser.
     * @param options Per-instance finite bounds and explicit completion policy; turnWork is driver-only.
     */
    explicit InstrumentProfileParser(IpfReadOptions options = {});
    /** Destroy all owned parser/header/current-row state; returned values remain independent. */
    ~InstrumentProfileParser();
    InstrumentProfileParser(const InstrumentProfileParser &) = delete;
    InstrumentProfileParser &operator=(const InstrumentProfileParser &) = delete;
    /** Accept a bounded prefix and emit at most one item.
     * @param input UTF-8 bytes borrowed only until return; no unaccepted suffix is retained.
     * @param work Remaining explicit allowances, debited before their operations; any zero allowance pauses its work.
     * @return Exact accepted count plus one item, required input/work, or a terminal status.
     */
    [[nodiscard]] IpfReadStep feed(std::span<const std::byte> input, IpfReadWork &work);
    /** Declare source EOF and drain remaining UTF-8/field/row state.
     * @param work Remaining explicit allowances; exhausted work can be replenished on a later finish call.
     * @return One remaining item, WORK_EXHAUSTED, FINISHED or FAILED; EOF never implies `##COMPLETE`.
     */
    [[nodiscard]] IpfReadStep finish(IpfReadWork &work);
    /** Observe committed counters without exposing internal storage.
     * @return Independent counter snapshot; counters are not an OS allocation/RSS measurement.
     */
    [[nodiscard]] IpfReadProgress progress() const noexcept;
    /** Observe the first terminal failure.
     * @return Fixed owning diagnostic, or null before failure.
     */
    [[nodiscard]] std::optional<IpfDiagnostic> diagnostic() const noexcept;

    private:
    friend IpfReadResult readInstrumentProfiles(std::istream &input, IpfReadOptions options);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/** Complete local-read outcome, including its exact accepted profile prefix on failure. */
struct IpfReadResult {
    std::vector<InstrumentProfile> profiles; ///< Owned whole profile prefix in source order.
    std::uint64_t acceptedBytes = 0;         ///< Logical parser consumption; underlying istream may have read ahead.
    std::uint64_t flushes = 0; ///< Admitted flush count; individual control order is available through the parser API.
    std::uint64_t completions = 0;      ///< Admitted completion count.
    bool completionSeen = false;        ///< A marker was admitted before EOF/failure.
    bool finished = false;              ///< True only for clean EOF under the requested policy.
    bool complete = false;              ///< True only when finished and completionSeen are both true.
    std::optional<IpfDiagnostic> error; ///< First parser/driver failure, with the retained prefix.
};

/** Read one uncompressed local UTF-8 IPF stream synchronously.
 * The stream is borrowed for this call and never closed, sought or rolled back. Its streambuf may
 * prefetch physical bytes; acceptedBytes reports logical parser consumption only. I/O may block;
 * finite work/storage limits do not establish a wall-clock deadline. The caller's exception mask
 * is preserved. A bad_alloc escaping the stream maps to ALLOCATION_FAILURE; if the stream suppresses
 * it and sets badbit, only IO_ERROR is observable. Parser/output allocation failures also map to
 * ALLOCATION_FAILURE. Typed failures retain complete earlier profiles. Positive turnWork is validated before I/O.
 * @param input Caller-owned stream whose lifetime covers this call; current position is the read start.
 * @param options Explicit per-instance limits/completion/work; no ambient properties are read.
 * @return Owning accepted prefix, independent completion/EOF evidence and a typed first failure.
 */
[[nodiscard]] IpfReadResult readInstrumentProfiles(std::istream &input, IpfReadOptions options = {});
/** Read one local filesystem path in binary mode, preserving quoted CR/LF bytes.
 * Opens/closes only its own ifstream; URLs, decompression and network acquisition are unsupported.
 * @param path Borrowed local path, used only while opening the owned file stream.
 * @param options Explicit input/storage/output/completion/work options.
 * @return Owning complete/prefix outcome; open/read failure maps to IO_ERROR.
 */
[[nodiscard]] IpfReadResult readInstrumentProfilesFile(const std::filesystem::path &path, IpfReadOptions options = {});
} // namespace org::ttldtor::qdcxx::ipf
