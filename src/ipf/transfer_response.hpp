// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "source_envelope.hpp"
#include "transfer_source.hpp"

#include <boost/beast/http/basic_parser.hpp>

namespace org::ttldtor::qdcxx::ipf::detail {

/** Whole-load monotonic HTTP work/counts; resetting a message parser never resets this state. */
struct TransferWork {
    std::size_t calls = 0;          ///< Charged parser calls, including zero progress/failure.
    std::size_t presented = 0;      ///< Charged parser/copy byte visits.
    std::size_t body = 0;           ///< Admitted payload bytes.
    std::size_t framing = 0;        ///< Accepted syntax bytes.
    std::size_t extensions = 0;     ///< Admitted chunk extension bytes.
    std::size_t informationals = 0; ///< Completed nonfinal messages.
};

/** Admit checked monotonic work without refund.
 * @param used Already admitted amount.
 * @param amount Whole next charge.
 * @param maximum Finite ceiling.
 * @return False without mutation when charge cannot fit.
 */
[[nodiscard]] bool charge(std::size_t &used, std::size_t amount, std::size_t maximum) noexcept;

/** Existing incremental IPF parser plus detached whole-profile collection. */
class TransferProfiles {
    InstrumentProfileParser parser_;  ///< Existing independent UTF-8/IPF parser.
    IpfTransferResult &result_;       ///< Stable enclosing operation result.
    TransferWork &work_;              ///< Whole-load copy-work accounting, including profile-vector growth.
    const IpfTransferLimits &limits_; ///< Stable copy-work ceilings.
    std::size_t profileLimit_;        ///< Existing logical profile limit, never increased by capacity growth.
    bool parserPending_ = false;      ///< Accepted bytes may still have a whole row/item pending before NEED_INPUT.
    SourceEnvelope envelope_;         ///< Owned single-layer finite decoder, never a caller-input borrow.
    bool failed_ = false;             ///< IPF parser failure independent of an envelope/native cause.
    /** Commit one existing parser step and detached progress.
     * @param step Owned parser result.
     */
    void observe(IpfReadStep step);

    public:
    /** Prepare existing bounded parser and stable result borrow.
     * @param options Existing explicit parser limits/policy.
     * @param result Stable owning operation result.
     * @param work Whole-load conservative array/text copy charges.
     * @param limits Stable finite work limits.
     */
    TransferProfiles(IpfReadOptions options, IpfTransferResult &result, TransferWork &work,
                     const IpfTransferLimits &limits);
    /** Feed at most the accepted prefix under one finite IPF work vector.
     * @param input Borrowed body bytes.
     * @param work Remaining positive parser-turn credits.
     * @return Exact accepted input prefix; no suffix borrow is retained.
     */
    [[nodiscard]] std::size_t feed(std::span<const std::byte> input, IpfReadWork &work);

    /** Observe collector rejection independently of an already retained native cause.
     * @return True only after a parser/signature failure.
     */
    [[nodiscard]] bool failed() const noexcept {
        return failed_ || envelope_.failed();
    }

    /** Select the same single gzip envelope from HTTP metadata before any payload.
     * @return False for conflicting/late selection.
     */
    bool declareGzip() noexcept {
        return envelope_.declareGzip();
    }

    /** Parse only already owned decoded bytes; never invokes decoder/native/provider progress.
     * @param work Finite existing IPF credits.
     * @return True when no owned decoded prefix remains or the IPF parser failed.
     */
    bool drain(IpfReadWork &work);

    /** Check whether failure retirement still has decoded prefix to publish.
     * @return True for nonempty decoded cache and an eligible parser.
     */
    bool hasOwnedInput() const noexcept {
        return !failed_ && (!envelope_.output().empty() || parserPending_);
    }

    /** Perform one finite EOF turn after a valid HTTP/local end.
     * @param work Mutable parser work.
     * @return True when parser EOF is terminal, including an error.
     */
    [[nodiscard]] bool finish(IpfReadWork &work);
};

/** Allocation-free derived HTTP parser; no dynamic fields/body container or application callback. */
class TransferResponse final : public boost::beast::http::basic_parser<false> {
    const IpfTransferLimits &limits_;              ///< Stable immutable operation settings.
    TransferWork &work_;                           ///< Whole-load counters shared by every response.
    TransferProfiles &profiles_;                   ///< Existing owned IPF pipeline.
    IpfTransferResult &result_;                    ///< Stable result; preserves first cause.
    std::size_t fields_ = 0;                       ///< Current message field count.
    std::size_t trailers_ = 0;                     ///< Current message trailer count.
    bool locationSeen_ = false;                    ///< Duplicate Location is rejected.
    bool modifiedSeen_ = false;                    ///< Duplicate Last-Modified is rejected.
    bool codingSeen_ = false;                      ///< Duplicate/stacked Content-Encoding is refused.
    bool liveSeen_ = false;                        ///< Duplicate X-Live is rejected.
    std::array<char, SOURCE_CAPACITY> location_{}; ///< Bounded derived redirect value.
    std::size_t locationSize_ = 0;                 ///< Initialized Location bytes.
    /** Record a first static HTTP/limit failure.
     * @param error Detached category.
     * @param stage Static checkpoint.
     * @param ec Beast callback rejection destination.
     */
    void fail(IpfTransferError error, std::string_view stage, boost::system::error_code &ec);
    /** Deliver bounded nonchunked/chunked body bytes to the same collector.
     * @param body Borrowed payload.
     * @param ec Callback rejection.
     * @return Exact body prefix accepted.
     */
    std::size_t body(boost::beast::string_view body, boost::system::error_code &ec);

    /** Reject the request start line in a response-only parser.
     * @param method Ignored request verb.
     * @param verbText Ignored raw request verb.
     * @param target Ignored request target.
     * @param version Ignored HTTP version.
     * @param ec Receives the response-only failure.
     */
    void on_request_impl(boost::beast::http::verb method, boost::beast::string_view verbText,
                         boost::beast::string_view target, int version, boost::system::error_code &ec) override;
    /** Observe a bounded response start line.
     * @param code Numeric status.
     * @param reason Borrowed reason phrase, never retained.
     * @param version HTTP version accepted by Beast.
     * @param ec Receives unsupported-upgrade rejection.
     */
    void on_response_impl(int code, boost::beast::string_view reason, int version,
                          boost::system::error_code &ec) override;
    /** Validate one bounded initial field and copy only selected metadata.
     * @param field Known field identifier, or unknown.
     * @param name Borrowed exact field name.
     * @param value Borrowed field value.
     * @param ec Receives duplicate, syntax or capacity rejection.
     */
    void on_field_impl(boost::beast::http::field field, boost::beast::string_view name, boost::beast::string_view value,
                       boost::system::error_code &ec) override;
    /** Count trailers while refusing fields that could alter already selected semantics.
     * @param field Known field identifier, or unknown.
     * @param name Borrowed exact trailer name.
     * @param value Borrowed trailer value, never retained.
     * @param ec Receives forbidden-field or capacity rejection.
     */
    void on_trailer_field_impl(boost::beast::http::field field, boost::beast::string_view name,
                               boost::beast::string_view value, boost::system::error_code &ec) override;
    /** Start the independently bounded trailer phase after initial headers.
     * @param ec Callback rejection destination.
     */
    void on_header_impl(boost::system::error_code &ec) override;
    /** Check declared payload against the remaining cumulative load bound.
     * @param length Declared payload length, absent for streaming framing.
     * @param ec Receives whole-message capacity rejection.
     */
    void on_body_init_impl(boost::optional<std::uint64_t> const &length, boost::system::error_code &ec) override;
    /** Deliver a nonchunked payload prefix.
     * @param value Borrowed body bytes.
     * @param ec Receives collector/capacity rejection.
     * @return Exact accepted body prefix.
     */
    std::size_t on_body_impl(boost::beast::string_view value, boost::system::error_code &ec) override;
    /** Bound one chunk's declared size and metadata before payload delivery.
     * @param size Declared chunk payload bytes.
     * @param extensions Borrowed raw chunk extension bytes.
     * @param ec Receives chunk or cumulative metadata rejection.
     */
    void on_chunk_header_impl(std::uint64_t size, boost::beast::string_view extensions,
                              boost::system::error_code &ec) override;
    /** Deliver a chunked payload prefix under the same collector limits.
     * @param remain Bytes still declared in this chunk.
     * @param value Borrowed currently available payload.
     * @param ec Receives collector/capacity rejection.
     * @return Exact accepted body prefix.
     */
    std::size_t on_chunk_body_impl(std::uint64_t remain, boost::beast::string_view value,
                                   boost::system::error_code &ec) override;
    /** Observe framing completion without inventing IPF EOF or native/TLS retirement.
     * @param ec Callback rejection destination.
     */
    void on_finish_impl(boost::system::error_code &ec) override;

    public:
    IpfReadWork ipfWork{}; ///< Renewed once per caller HTTP turn; not retained outside synchronous parser entry.
    unsigned status = 0;   ///< Current parsed numeric response status.
    /** Prepare one message without resetting cumulative work.
     * @param limits Immutable bounded header/body/field settings.
     * @param work Whole-load counters.
     * @param profiles Existing bounded collector.
     * @param result Stable detached output.
     */
    TransferResponse(const IpfTransferLimits &limits, TransferWork &work, TransferProfiles &profiles,
                     IpfTransferResult &result);
    /** Borrow the bounded current Location value.
     * @return View valid through this parser's retirement.
     */
    [[nodiscard]] std::string_view location() const noexcept;
};

} // namespace org::ttldtor::qdcxx::ipf::detail
