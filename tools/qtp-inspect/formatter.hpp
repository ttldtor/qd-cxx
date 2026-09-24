// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/events/profile.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"
#include "qd/protocol/tape_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::inspect {
/** Tool-only stdout representation; decoding and diagnostics do not depend on it. */
enum class OutputFormat {
    TSV,  ///< Exact UTF-16 hexadecimal TSV, the default.
    HUMAN ///< Quoted UTF-8 with lossless unsafe-unit escapes.
};

/** Explicit display metadata selection, unrelated to schema lookup aliases. */
enum class DisplayProfile {
    LOCAL_SCHEMA,          ///< Independent schema with field[index] labels.
    ORACLE_SRE_V1,         ///< Built-in adapter with S.v and R.y/R.x/R.z labels.
    DEFAULT_QUOTE_V1,      ///< Explicit eight-field composite Quote with exact typed values.
    ORACLE_FIELD_TYPES_V1, ///< Generic exact fields with nullable UTF-16, using tape output version 4.
    DEFAULT_PROFILE_V1,    ///< Explicit default Profile with raw fields and event values, output version 5.
    EXTERNAL_SCHEMA_V1     ///< Caller-loaded ordinary exact fields, generic tape output version 6.
};

/** Logical row exceeds remaining output bytes; translated to the existing write resource error. */
struct OutputLimit {};

/** Bounded row presentation, with no wire parsing or output stream ownership. */
class Formatter {
    OutputFormat format_;    ///< Selected presentation.
    DisplayProfile profile_; ///< Caller-selected tool-local metadata.

    public:
    /** Select presentation and explicit metadata.
     * @param format Output representation.
     * @param profile Metadata matching the supplied schema; independent callers use LOCAL_SCHEMA.
     */
    Formatter(OutputFormat format, DisplayProfile profile) : format_(format), profile_(profile) {
    }

    /** Compose the separate tape format identity.
     * @param budget Remaining output bytes.
     * @return Complete bounded row or OutputLimit.
     */
    std::string tapeHeader(std::size_t budget) const;
    /** Compose the explicit timed tape format identity.
     * @param budget Remaining output bytes.
     * @return Complete version-3 row or OutputLimit.
     */
    std::string rawTapeHeader(std::size_t budget) const;
    /** Compose a lossless accepted RAW descriptor.
     * @param descriptor Accepted RAW/MESSAGE header.
     * @param budget Remaining encoded bytes.
     * @return Complete version-3 row or OutputLimit.
     */
    std::string rawTapeDescriptor(const protocol::TapeDescriptor &descriptor, std::size_t budget) const;
    /** Compose one explicit heartbeat, including empty payload and absent timestamp.
     * @param heartbeat Owned metadata borrowed for this call.
     * @param index Zero-based frame index.
     * @param budget Remaining output bytes.
     * @return Complete row or OutputLimit.
     */
    std::string tapeHeartbeat(const protocol::TapeHeartbeat &heartbeat, std::size_t index, std::size_t budget) const;
    /** Compose the timing shared by one RAW data frame's records.
     * @param timing Owned wire and delivered timing.
     * @param index Zero-based frame index.
     * @param budget Remaining output bytes.
     * @return Complete row or OutputLimit.
     */
    std::string tapeTiming(const protocol::TapeTiming &timing, std::size_t index, std::size_t budget) const;
    /** Compose validated tape diagnostic text, escaping untrusted version text.
     * @param descriptor Accepted owning descriptor.
     * @param budget Remaining encoded bytes.
     * @return Complete bounded row or OutputLimit.
     */
    std::string tapeDescriptor(const protocol::TapeDescriptor &descriptor, std::size_t budget) const;
    /** Compose a tape frame with the header type named separately.
     * @param frame Owning envelope borrowed for this call.
     * @param index Zero-based envelope index.
     * @param budget Remaining bytes.
     * @param mode Explicit tape policy; legacy presentation stays unchanged.
     * @return Complete bounded row or OutputLimit.
     */
    std::string tapeFrame(const protocol::Frame &frame, std::size_t index, std::size_t budget,
                          protocol::TapeMode mode = protocol::TapeMode::LEGACY) const;
    /** Describe marker presence without asserting an authenticated footer.
     * @param markers Number of accepted empty envelopes.
     * @param budget Remaining bytes.
     * @return Complete bounded row or OutputLimit.
     */
    std::string tapeMarkers(std::size_t markers, std::size_t budget) const;
    /** Compose the format/profile header.
     * @param budget Remaining stdout bytes.
     * @return Complete line; throws OutputLimit before expansion if it cannot fit.
     */
    std::string header(std::size_t budget) const;

    /** Compose envelope boundaries and type.
     * @param frame Borrowed decoded envelope.
     * @param index Zero-based frame index.
     * @param budget Remaining stdout bytes.
     * @return Complete line or OutputLimit.
     */
    std::string frame(const protocol::Frame &frame, std::size_t index, std::size_t budget) const;

    /** Compose a completed local record, never choosing labels from lookup aliases.
     * @param record Borrowed decoded local record.
     * @param name Exact local schema name.
     * @param frameIndex Owning frame index.
     * @param index Zero-based emitted index.
     * @param budget Remaining stdout bytes.
     * @param timing Optional delivered tape timing; null preserves untimed output.
     * @return Complete line or OutputLimit.
     */
    std::string record(const protocol::StreamDataRecord &record, std::u16string_view name, std::size_t frameIndex,
                       std::size_t index, std::size_t budget, const protocol::TapeTiming *timing = nullptr) const;

    /** Compose an atomic raw/event Profile pair after prepared mapping.
     * @param record Borrowed generic record retaining every local field.
     * @param name Exact local record name.
     * @param frameIndex Owning envelope index.
     * @param index Emitted record index.
     * @param event Owned projected event, borrowed for this call.
     * @param budget Remaining stdout bytes.
     * @return Both complete rows, or OutputLimit before either is published.
     */
    std::string profileRecord(const protocol::StreamDataRecord &record, std::u16string_view name,
                              std::size_t frameIndex, std::size_t index, const events::Profile &event,
                              std::size_t budget) const;

    /** Compose the success summary, whose physical delivery still requires a successful exit.
     * @param bytes Accepted input bytes.
     * @param frames Delivered frame count.
     * @param records Emitted record count.
     * @param chargedRecords Charged record attempts.
     * @param fields Charged wire fields.
     * @param budget Remaining stdout bytes.
     * @return Complete line or OutputLimit.
     */
    std::string complete(std::uint64_t bytes, std::size_t frames, std::size_t records, std::size_t chargedRecords,
                         std::size_t fields, std::size_t budget) const;
};
} // namespace org::ttldtor::qdcxx::inspect
