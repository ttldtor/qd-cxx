// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"
#include <string_view>

namespace org::ttldtor::qdcxx::protocol {
/** Required finite descriptor text and collection bounds. */
struct TapeDescriptorLimits {
    std::size_t properties;         ///< Global and message property pairs combined.
    std::size_t messages;           ///< Send and receive entries combined.
    encoding::UtfStringLimits text; ///< One encoded/decoded string.
    std::size_t textUnits;          ///< All successfully decoded strings combined.
};

/** Required finite tape storage and cumulative work bounds. */
struct TapeLimits {
    FrameLimits frame;               ///< FrameReader storage configuration.
    std::size_t inputBytes;          ///< Total accepted bytes.
    std::size_t frames;              ///< All envelopes, including empty markers.
    TapeDescriptorLimits descriptor; ///< Initial descriptor bounds.
    ContextLimits context;           ///< Schema and retained descriptions/actions/names.
    std::size_t constructionUnits;   ///< Cumulative full candidate and local schema elements.
    StreamDataLimits data;           ///< Aggregate file work/output bounds and per-symbol bounds.
};
/** Terminal categories, independent of Java exception classes. */
enum class TapeError {
    NONE,              ///< No failure.
    MALFORMED,         ///< Invalid/truncated wire input, duplicate or undescribed ID.
    UNSUPPORTED,       ///< Explicit profile or sequencing policy.
    RESOURCE_LIMIT,    ///< Finite logical bound exceeded.
    ALLOCATION_FAILURE ///< Owned storage allocation failed.
};
/** Incremental result state. */
enum class TapeStatus {
    NEED_INPUT, ///< No complete event yet; empty feed is not EOF.
    EVENT,      ///< One complete envelope was dispatched.
    FAILED,     ///< First terminal error; no further input is accepted.
    FINISHED    ///< Clean explicit EOF with an accepted header.
};

/** Owning diagnostic header text; missing version is distinct from empty. */
struct TapeDescriptor {
    std::optional<std::u16string> version; ///< Opaque text, never a support test.
    bool explicitTimeNone = false;         ///< Whether time=NONE was present.
    bool explicitStreamSend = false;       ///< Whether the send set explicitly contained STREAM_DATA.
};

/** First error and exact separate framing/payload positions. */
struct TapeDiagnostic {
    TapeError error = TapeError::NONE;         ///< Terminal category.
    std::string_view stage = "none";           ///< Static checkpoint label, no borrowed input.
    std::size_t detail = 0;                    ///< Nested parser enum ordinal when applicable.
    std::uint64_t frameStart = 0;              ///< Actual start of failing envelope, or EOF position.
    std::uint64_t accepted = 0;                ///< Total accepted file bytes, not payload examination.
    std::optional<std::uint64_t> payloadStart; ///< Actual payload start for complete messages.
    std::optional<std::size_t> consumed;       ///< Payload-relative committed examination.
    std::optional<std::size_t> errorOffset;    ///< Payload-relative primitive/checkpoint position.
};

/** At most one owning event and its completed data prefix. */
struct TapeFeedResult {
    std::size_t accepted = 0;                   ///< Bytes copied from this feed only.
    TapeStatus status = TapeStatus::NEED_INPUT; ///< Reader state after this call.
    std::optional<Frame> frame;                 ///< Owning envelope, also present on payload failure.
    std::vector<StreamDataRecord> records;      ///< Complete records, never a partial current record.
};

/** Bounded single-threaded tape reader; no reset, replacement or recovery API. */
class TapeReader {
    TapeLimits limits_;
    core::LocalSchema schema_;
    FrameReader framer_;
    std::vector<WireRecordDescription> descriptions_;
    std::optional<StreamDataContext> context_;
    std::optional<TapeDescriptor> descriptor_;
    TapeStatus status_ = TapeStatus::NEED_INPUT;
    TapeDiagnostic diagnostic_;
    std::size_t frames_ = 0;
    std::size_t markers_ = 0;
    std::size_t construction_ = 0;
    std::size_t records_ = 0;
    std::size_t fields_ = 0;
    std::size_t emitted_ = 0;
    std::size_t values_ = 0;
    std::size_t symbols_ = 0;
    /** Set the first failure.
     * @param error Category.
     * @param stage Static checkpoint.
     * @param detail Nested enum ordinal.
     */
    void fail(TapeError error, std::string_view stage, std::size_t detail = 0) noexcept;
    /** Dispatch one complete envelope.
     * @param result Owns the frame and receives completed data.
     */
    void dispatch(TapeFeedResult &result);

    public:
    /** Copy a validated local schema within bounds and allocate framing storage.
     * @param schema Borrowed validated schema, copied before returning.
     * @param limits Mandatory finite bounds; invalid configuration throws invalid_argument.
     * Allocation failure before input may throw bad_alloc.
     */
    TapeReader(const core::LocalSchema &schema, TapeLimits limits);
    /** Accept up to one envelope; caller resubmits any unaccepted suffix.
     * @param input Borrowed bytes, not retained by reference.
     * @return Owned event/prefix and exact accepted count; terminal failures accept zero later.
     */
    [[nodiscard]] TapeFeedResult feed(std::span<const std::byte> input);
    /** Declare actual EOF; idempotent, no recovery.
     * @return FINISHED only for clean EOF after the header; otherwise FAILED.
     */
    TapeStatus finish() noexcept;

    /** Inspect first failure.
     * @return Borrowed diagnostic valid until the reader is destroyed.
     */
    [[nodiscard]] const TapeDiagnostic &diagnostic() const noexcept {
        return diagnostic_;
    }

    /** Inspect accepted descriptor.
     * @return Immutable owning header storage or absent before acceptance.
     */
    [[nodiscard]] const std::optional<TapeDescriptor> &descriptor() const noexcept {
        return descriptor_;
    }

    /** Inspect the current immutable snapshot.
     * @return Borrowed context until next successful description update or destruction.
     */
    [[nodiscard]] const StreamDataContext *context() const noexcept {
        return context_ ? &*context_ : nullptr;
    }

    /** Inspect accepted bytes.
     * @return Exact cumulative file position.
     */
    [[nodiscard]] std::uint64_t acceptedBytes() const noexcept {
        return framer_.totalAccepted();
    }

    /** Inspect envelope charges.
     * @return Frames including markers and complete failing frames.
     */
    [[nodiscard]] std::size_t frameCount() const noexcept {
        return frames_;
    }

    /** Inspect marker presence independently of parsing completion.
     * @return Empty envelopes successfully accepted after the header.
     */
    [[nodiscard]] std::size_t markerCount() const noexcept {
        return markers_;
    }

    /** Inspect record work.
     * @return Charged attempts including skips and later failures.
     */
    [[nodiscard]] std::size_t processedRecords() const noexcept {
        return records_;
    }

    /** Inspect field work.
     * @return Charged attempts including skips and repeated targets.
     */
    [[nodiscard]] std::size_t wireFields() const noexcept {
        return fields_;
    }

    /** Inspect reconstruction work.
     * @return Charged complete-candidate logical elements.
     */
    [[nodiscard]] std::size_t constructionUnits() const noexcept {
        return construction_;
    }
};
} // namespace org::ttldtor::qdcxx::protocol
