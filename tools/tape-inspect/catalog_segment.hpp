// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "inspect.hpp"
#include "source_format.hpp"
#include "source_time.hpp"
#include <optional>
#include <ostream>
#include <stop_token>

namespace org::ttldtor::qdcxx::tape_inspect::detail {
/** Fixed internal stop/failure transfer; no allocation or retained input text. */
struct SequenceAbort {
    int code;                ///< Existing process category, or7 for first observed cancellation.
    std::string_view stage;  ///< Static operation literal.
    std::string_view reason; ///< Static bounded reason literal, never exception text.
};

/** Concrete synchronous output owner shared by the legacy catalog and finite source operation. */
class CatalogOutput {
    std::ostream &output_;
    std::ostream &diagnostic_;
    std::stop_token stop_;
    std::size_t remaining_;
    std::size_t terminalRemaining_;
    std::optional<std::size_t> sourceRemaining_;
    std::size_t emittedBytes_ = 0;

    public:
    /** Borrow destinations synchronously and divide one allowance without allocation.
     * @param output Data destination, alive until this owner is destroyed.
     * @param diagnostic Terminal diagnostic destination, with the same lifetime.
     * @param outputBytes One combined encoded write allowance.
     * @param terminalBytes Protected subset unavailable to normal publication.
     * @param stop Explicit cancellation state; no callback is registered.
     */
    CatalogOutput(std::ostream &output, std::ostream &diagnostic, std::size_t outputBytes, std::size_t terminalBytes,
                  std::stop_token stop = {}) noexcept;
    /** Observe cancellation without IO; throws the fixed STOPPED transfer on a request. */
    void checkpoint() const;
    /** Inspect remaining writable capacity without work.
     * @param terminal Select protected terminal capacity instead of normal/source capacity.
     * @return Remaining encoded bytes for the selected role.
     */
    [[nodiscard]] std::size_t available(bool terminal = false) const noexcept;
    /** Observe cancellation before constructing a normal row.
     * @return Remaining normal/source bytes, or throws STOPPED before allocation.
     */
    [[nodiscard]] std::size_t rowBudget() const;
    /** Activate one already-admitted source's independent output cap.
     * @param outputBytes Exact declared encoded source capacity; zero remains literal.
     */
    void beginSource(std::size_t outputBytes) noexcept;
    /** Release the current source role without refunding any global encoded charges. */
    void endSource() noexcept;
    /** Charge and submit one already bounded row, preserving finite failed-write charges.
     * @param row Owning complete encoded row, borrowed only until this call returns.
     * @param terminal Use protected capacity; permitted for bounded cleanup after cancellation.
     * @param diagnostic Select the diagnostic destination instead of ordinary output.
     */
    void write(const std::string &row, bool terminal = false, bool diagnostic = false);
    /** Flush without source reads; failure cannot restore completion.
     * @param diagnostic Select the diagnostic destination.
     */
    void flush(bool diagnostic = false);
    /** Inspect encoded write-attempt charges, separate from OS delivery.
     * @return Scalar charge count, including failed write attempts.
     */
    [[nodiscard]] std::size_t emittedBytes() const noexcept;
};

/** One bounded feed result after all committed metadata/record prefix was rendered synchronously. */
struct CatalogFeedCheckpoint {
    std::size_t accepted = 0; ///< Consumed prefix of this call only; caller resubmits the suffix.
    protocol::TapeStatus status = protocol::TapeStatus::NEED_INPUT; ///< Reader result after synchronous rendering.
};

/** Concrete catalog parser/render operation; source opening, reads, closure and COMPLETE stay outside. */
class CatalogSegment {
    protocol::TapeReader reader_;
    inspect::Formatter formatter_;
    CatalogOutput &output_;
    std::optional<SourceRecordTiming> source_;
    std::size_t &aggregateEmitted_;
    std::size_t emitted_ = 0;
    std::size_t generation_ = 0;
    bool descriptorSeen_ = false;
    bool timestampAssigned_ = false;
    SourceDiagnostic *firstCause_ = nullptr; ///< Concrete borrowed owner cause; never a callback or retained input.

    void checkDescriptor();
    void preserveParserFailure() noexcept;
    void render(const protocol::TapeFeedResult &result, const core::RecordContext &before,
                const std::optional<protocol::TapeTiming> &sidecarTiming);

    public:
    /** Create a fresh concrete catalog reader with no source IO or retained input span.
     * @param limits Exact resolved and admitted per-source capacities; never enlarged here.
     * @param format Existing safe record presentation.
     * @param output Borrowed synchronous output owner, alive longer than this object.
     * @param aggregateEmitted Borrowed checked aggregate emitted ordinal, with the same lifetime.
     * @param source Fixed source presentation; absence preserves the historical output8 path.
     * @param firstCause Fixed owning cause destination for source mode, alive longer than this object.
     */
    CatalogSegment(Limits limits, inspect::OutputFormat format, CatalogOutput &output, std::size_t &aggregateEmitted,
                   std::optional<SourceRecordTiming> source = {}, SourceDiagnostic *firstCause = nullptr);
    /** Feed at most one existing parser event and render its completed prefix before return.
     * @param input Borrowed bytes, never retained; unaccepted suffix must be resubmitted.
     * @param sidecarTiming Optional currently applied owning mark; packed zero remains an assignment.
     * @return Accepted prefix/status; schema views and records do not escape.
     */
    [[nodiscard]] CatalogFeedCheckpoint feed(std::span<const std::byte> input,
                                             const std::optional<protocol::TapeTiming> &sidecarTiming = {});
    /** Finalize parser syntax only; performs no source IO, physical close, COMPLETE or flush.
     * @return Existing reader status, or a fixed source-profile refusal.
     */
    [[nodiscard]] protocol::TapeStatus finish();
    /** Inspect reader-owned scalar/schema state synchronously.
     * @return Borrowed reader, valid only while this concrete operation lives.
     */
    [[nodiscard]] const protocol::TapeReader &reader() const noexcept;
    /** Inspect successfully rendered record count for this segment.
     * @return Segment-local emitted ordinal, independent from parser attempts and FIELD ordinal.
     */
    [[nodiscard]] std::size_t emittedRecords() const noexcept;
};
} // namespace org::ttldtor::qdcxx::tape_inspect::detail
