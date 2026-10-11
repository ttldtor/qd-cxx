// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/protocol/tape_writer.hpp"

namespace org::ttldtor::qdcxx::protocol {

/** Complete-input preflight failures; no failed prefix becomes an admitted recording. */
enum class AdmissionError {
    MALFORMED,         ///< Invalid descriptions, identities or truncated complete input.
    UNSUPPORTED,       ///< Information would be lost or the selected subset is unsupported.
    RESOURCE_LIMIT,    ///< Finite storage/work allowance or checked arithmetic exceeded.
    ALLOCATION_FAILURE ///< Owned storage allocation failed.
};

/** Stable strict refusal, independent of nested reader/writer diagnostics. */
enum class AdmissionCode {
    NATIVE,                    ///< A typed nested reader, parser, binding or writer failure.
    UNKNOWN_RECORD,            ///< Every description must resolve to a local record, including unused records.
    SKIPPED_FIELD,             ///< A wire field has no exact local target.
    REPEATED_TARGET,           ///< A local field occurs more than once in the wire description.
    MISSING_TARGET,            ///< A local field has no wire source.
    DUPLICATE_LAYOUT_IDENTITY, ///< Sender/local/name inventory identity occurs more than once.
    UNREPRESENTED_DESCRIPTOR   ///< Accepted metadata cannot be projected exactly.
};

/** Mandatory finite extra admission bounds, independently charged from reader and writer work. */
struct AdmissionLimits {
    std::size_t inputBytes;   ///< Complete retained source bytes.
    TapeLimits reader;        ///< Fresh RAW_MESSAGE_V1 reader bounds; also used by each independent second pass.
    LayoutLimits layouts;     ///< Complete retained schema and explicit outgoing layout bounds.
    std::size_t workUnits;    ///< Nonrefunding extra byte/element/unit visits and copies.
    std::size_t scratchBytes; ///< Peak simultaneous extra owning source and logical container/text bytes.
    std::size_t scratchUnits; ///< Peak simultaneous extra owning UTF-16 units before ownership transfer.
};

/** Actual logical preflight charges; allocator metadata and capacity are excluded. */
struct AdmissionCounters {
    std::size_t workUnits = 0;        ///< Charged visits/copies; failed attempts never refund prior work.
    std::size_t peakScratchBytes = 0; ///< Greatest simultaneous extra logical byte storage.
    std::size_t peakScratchUnits = 0; ///< Greatest simultaneous extra logical UTF-16 storage.
    std::size_t frames = 0;           ///< Successfully dispatched input envelopes, including markers.
};

/** Owning first-error checkpoints; names/input text are never borrowed or copied into diagnostics. */
struct AdmissionDiagnostic {
    AdmissionError error = AdmissionError::MALFORMED; ///< Terminal category.
    AdmissionCode code = AdmissionCode::NATIVE;       ///< Strict refusal or nested native cause.
    std::string_view stage = "none";                  ///< Static checkpoint label.
    std::uint64_t frameStart = 0;                     ///< Failing envelope start, or complete input EOF position.
    std::optional<std::size_t> record;                ///< Description position at the failing checkpoint.
    std::optional<std::size_t> field;                 ///< Field position at the failing checkpoint.
    std::optional<TapeDiagnostic> reader;             ///< Exact typed reader cause when applicable.
    std::optional<WriterDiagnostic> writer;           ///< Exact typed outgoing construction cause when applicable.
    std::optional<DescriptionStatus> description;     ///< Typed reparsing cause when applicable.
    std::optional<BindingError> binding;              ///< Typed exact binding cause when applicable.
    AdmissionCounters counters;                       ///< Actual charges at failure.
};

/** Immutable complete admitted source; only successful explicit EOF can construct this token.
 * Input/configuration accessors require valid(). A moved-from token is inactive and never proves EOF.
 */
class AdmittedTape {
    std::vector<std::byte> bytes_;
    OutgoingLayouts layouts_;
    OutgoingDescriptor descriptor_;
    AdmissionLimits limits_;
    AdmissionCounters counters_;
    bool active_ = true;

    friend std::expected<AdmittedTape, AdmissionDiagnostic> admitTape(const core::LocalSchema &,
                                                                      std::span<const std::byte>, AdmissionLimits);
    /** Adopt fully preflighted complete input.
     * @param bytes Owned exact complete bytes.
     * @param layouts Owned validated complete inventory and schema.
     * @param descriptor Exact represented header.
     * @param limits Bounds used for preflight and subsequent independent reading.
     * @param counters Actual preflight charges.
     */
    AdmittedTape(std::vector<std::byte> bytes, OutgoingLayouts layouts, OutgoingDescriptor descriptor,
                 AdmissionLimits limits, AdmissionCounters counters) noexcept;

    public:
    /** Transfer immutable ownership; borrowed source views expire.
     * @param other Complete source to transfer.
     */
    AdmittedTape(AdmittedTape &&other) noexcept;
    /** Copying is unavailable; a new admission must independently charge its owned copy.
     * @param other Source token.
     */
    AdmittedTape(const AdmittedTape &other) = delete;
    /** Assignment is unavailable; admitted state is immutable.
     * @param other Source token.
     * @return No result because assignment is deleted.
     */
    AdmittedTape &operator=(const AdmittedTape &other) = delete;
    /** Assignment is unavailable; admitted state is immutable.
     * @param other Source token.
     * @return No result because assignment is deleted.
     */
    AdmittedTape &operator=(AdmittedTape &&other) = delete;
    /** Inspect whether this token still owns its admitted source after moves.
     * @return True only for the original successful admission or its active move destination.
     */
    [[nodiscard]] bool valid() const noexcept;
    /** Inspect exact complete source bytes.
     * @return Borrowed view until destruction/move.
     */
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept;
    /** Inspect the independently owned validated local schema.
     * @return Borrowed schema until destruction/move.
     */
    [[nodiscard]] const core::LocalSchema &schema() const noexcept;
    /** Inspect final explicit inventory; source description placement stays in bytes().
     * @return Borrowed immutable declarations until destruction/move.
     */
    [[nodiscard]] const OutgoingLayouts &layouts() const noexcept;
    /** Inspect exact represented initial metadata.
     * @return Borrowed descriptor until destruction/move.
     */
    [[nodiscard]] const OutgoingDescriptor &descriptor() const noexcept;
    /** Inspect the finite configuration for a new independently charged second pass.
     * @return Value copy, containing no borrowed storage.
     */
    [[nodiscard]] TapeLimits readerLimits() const noexcept;
    /** Inspect executed extra preflight work and logical peak storage.
     * @return Value copy of actual charges.
     */
    [[nodiscard]] AdmissionCounters counters() const noexcept;
};

/** Admit an entire immutable byte sequence without I/O, ambient settings or retained decoded history.
 * The caller authenticates filesystem EOF/close separately; library EOF is exhaustion of input.
 * Every description is checked, including unused records. Failure publishes no token or inventory.
 * @param schema Validated caller schema, borrowed only during this call.
 * @param input Complete caller bytes, copied within finite bounds before publication.
 * @param limits Required finite copy, reader, layout and extra work/storage allowances.
 * @return Move-only owned complete source, or first typed failure with actual charged counters.
 */
[[nodiscard]] std::expected<AdmittedTape, AdmissionDiagnostic>
admitTape(const core::LocalSchema &schema, std::span<const std::byte> input, AdmissionLimits limits);

} // namespace org::ttldtor::qdcxx::protocol
