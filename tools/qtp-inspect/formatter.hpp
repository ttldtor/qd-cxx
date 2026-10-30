// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../tape-inspect/source_time.hpp"
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
    EXTERNAL_SCHEMA_V1,    ///< Caller-loaded ordinary exact fields, generic tape output version 6.
    WIRE_SCHEMA_V1,        ///< Explicit ordinary wire discovery with source metadata, output version 7.
    BINARY_CATALOG_V1      ///< Explicit broader discovery with lossless fields and metadata, output version 8.
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
    /** Compose effective broad descriptor metadata after one committed full/delta event.
     * @param descriptor Validated owning effective header.
     * @param index Source frame ordinal.
     * @param budget Remaining encoded output bytes.
     * @return Fixed version-8 descriptor row or OutputLimit.
     */
    std::string catalogDescriptor(const protocol::TapeDescriptor &descriptor, std::size_t index,
                                  std::size_t budget) const;
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
     * @param generation Current accepted description generation, used only by WIRE_SCHEMA_V1.
     * @return Complete line or OutputLimit; invalid_argument for noncanonical values or payload
     * identities9/137/233/249 unsupported by the existing output grammars, including null values.
     * Nonzero raw symbol flags or engaged removalTime also cause invalid_argument before buffering.
     * Explicit zero-flags prefixes retain the old output regardless of eventFlagsBytes.
     */
    std::string record(const protocol::StreamDataRecord &record, std::u16string_view name, std::size_t frameIndex,
                       std::size_t index, std::size_t budget, const protocol::TapeTiming *timing = nullptr,
                       std::size_t generation = 0) const;
    /** Compose a broad typed row after validating its authoritative schema and provenance.
     * @param record Owning decoded values, exact local/source identity and metadata.
     * @param schema Expected validated binary schema; supplies the record name and exact field shape.
     * @param frameIndex Source envelope ordinal.
     * @param index Emitted record ordinal.
     * @param budget Remaining encoded bytes, inspected only after complete record/schema validation.
     * @param timing Explicit owned frame timing, or null for absent timestamp/zero sequence.
     * @param generation Current committed nonempty description publication.
     * @return Version-8 row or OutputLimit; invalid_argument for any schema/context/shape/profile mismatch.
     */
    std::string catalogRecord(const protocol::StreamDataRecord &record, const core::LocalSchema &schema,
                              std::size_t frameIndex, std::size_t index, std::size_t budget,
                              const protocol::TapeTiming *timing, std::size_t generation) const;

    /** Compose lossless output9 with independent raw FIELD and effective time.
     * All inputs are borrowed only during this call; payloads and schemas are never copied or retained.
     * Validation precedes output counting/allocation. This does not enforce source declaration sequencing.
     * @param record Owned record borrowed unchanged, including exact raw FIELD metadata.
     * @param schema Authoritative validated schema, consumed before any generation transition.
     * @param frameIndex Segment-local envelope ordinal.
     * @param segmentRecordIndex Segment-local emitted record ordinal.
     * @param aggregateRecordIndex Checked cross-source emitted record ordinal.
     * @param generation Segment-local committed nonempty description generation.
     * @param budget Remaining encoded output bytes.
     * @param timing Fixed owning explicit policy/assignment metadata borrowed only for this call.
     * @return Complete 27-cell TSV row or safe HUMAN row; invalid_argument for invalid schema,
     * values, policy, assignment or FIELD shape, OutputLimit before allocation on encoded excess.
     */
    std::string sourceCatalogRecord(const protocol::StreamDataRecord &record, const core::LocalSchema &schema,
                                    std::size_t frameIndex, std::size_t segmentRecordIndex,
                                    std::size_t aggregateRecordIndex, std::size_t generation, std::size_t budget,
                                    const tape_inspect::SourceRecordTiming &timing) const;
    /** Begin a bounded version-7 description snapshot.
     * @param frameIndex Successful description envelope index.
     * @param generation One-based successful description event count.
     * @param records Complete local record count.
     * @param descriptions Complete source description count.
     * @param budget Remaining encoded output bytes.
     * @return Complete snapshot header row or OutputLimit.
     */
    std::string wireSchema(std::size_t frameIndex, std::size_t generation, std::size_t records,
                           std::size_t descriptions, std::size_t budget) const;
    /** Compose one local declaration and its exact first-seen field order.
     * @param generation Current snapshot generation.
     * @param record Borrowed validated ordinary record; generated lookups contain exactly one name per field.
     * @param budget Remaining encoded output bytes, checked before allocation.
     * @return Complete record/field row group or OutputLimit.
     */
    std::string wireLocalRecord(std::size_t generation, const core::LocalRecord &record, std::size_t budget) const;
    /** Compose one original source declaration with authoritative binding targets.
     * @param generation Current snapshot generation.
     * @param index Original append position in the retained description list.
     * @param record Borrowed exact source declaration.
     * @param plan Borrowed current context plan for that sender; mismatch throws invalid_argument.
     * @param budget Remaining encoded output bytes, checked before allocation.
     * @return Complete source record/field row group or OutputLimit.
     */
    std::string wireDescription(std::size_t generation, std::size_t index,
                                const protocol::WireRecordDescription &record, const protocol::StreamRecordPlan &plan,
                                std::size_t budget) const;
    /** Compose output9 source declarations, retaining recognized virtual FIELD prefix actions.
     * Exactly the leading EventTime/full56 and optional immediate EventSequence/full72 may have
     * absent targets, and only when the authoritative plan recognizes that complete prefix.
     * Every ordinary action still requires an exact type and a valid local target. All validation
     * precedes output counting/allocation. Inputs are borrowed synchronously and never retained.
     * @param generation Nonzero segment-local committed description generation.
     * @param index Segment-local append position in the retained source descriptions.
     * @param record Complete physical source declaration, including virtual metadata fields.
     * @param plan Validated current sender plan; its context must remain valid through this call.
     * @param budget Remaining encoded output bytes.
     * @return Complete wire-record/wire-field group, with '-' targets only for virtual FIELD actions;
     * invalid_argument on a binding/prefix/profile mismatch, or OutputLimit before allocation on excess.
     */
    std::string sourceWireDescription(std::size_t generation, std::size_t index,
                                      const protocol::WireRecordDescription &record,
                                      const protocol::StreamRecordPlan &plan, std::size_t budget) const;
    /** Mark the end of a fully emitted snapshot.
     * @param generation Current snapshot generation.
     * @param budget Remaining encoded output bytes.
     * @return Complete closing row or OutputLimit.
     */
    std::string wireSchemaEnd(std::size_t generation, std::size_t budget) const;

    /** Compose an atomic raw/event Profile pair after prepared mapping.
     * @param record Borrowed generic record retaining every local field.
     * @param name Exact local record name.
     * @param frameIndex Owning envelope index.
     * @param index Emitted record index.
     * @param event Owned projected event, borrowed for this call.
     * @param budget Remaining stdout bytes.
     * @return Both complete rows, or OutputLimit before either is published; invalid_argument
     * before buffering for nonzero raw symbol flags or engaged removalTime, including zero.
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
