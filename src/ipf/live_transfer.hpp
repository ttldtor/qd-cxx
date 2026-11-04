// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "connection_sink.hpp"
#include "transfer_response.hpp"
#include <algorithm>

namespace org::ttldtor::qdcxx::ipf::detail {
/** Single-record adapter preserving the snapshot pipe/layout; at most one profile/control is staged. */
class StreamingProfiles {
    TransferProfiles inner_;
    IpfTransferResult &result_;
    connection_detail::StreamConsumer *consumer_ = nullptr;
    bool skipped_ = false;
    std::uint64_t flushes_ = 0;
    std::uint64_t completions_ = 0;

    public:
    /** Bind the ordinary bounded decoder/parser; its vector is drained after every single-record turn.
     * @param options Explicit parser limits.
     * @param result Stable attempt result.
     * @param work Persistent HTTP/copy work.
     * @param limits Stable remaining lifetime bounds.
     */
    StreamingProfiles(IpfReadOptions options, IpfTransferResult &result, TransferWork &work,
                      const IpfTransferLimits &limits)
        : inner_(options, result, work, limits), result_(result) {
    }

    /** Bind the stable outer sink before input.
     * @param consumer Worker-owned connection operation.
     */
    void bind(connection_detail::StreamConsumer &consumer) noexcept {
        consumer_ = &consumer;
    }

    /** Access the stable outer sink.
     * @return Bound consumer, alive through the attempt.
     */
    connection_detail::StreamConsumer &consumer() noexcept {
        return *consumer_;
    }

    /** Check one privately staged whole event.
     * @return True when an item must be delivered before more input.
     */
    bool skipped() const noexcept {
        return skipped_;
    }

    /** Mark equal-date body omission without claiming framing EOF. */
    void skip() noexcept {
        skipped_ = true;
    }

    /** @return True for one whole staged event awaiting outer publication. */
    bool staged() const noexcept {
        return !result_.ipf.profiles.empty() || result_.ipf.flushes != flushes_ ||
               result_.ipf.completions != completions_;
    }

    /** Deliver at most one staged event outside every parser/native invocation.
     * @return True after admission, or false without releasing the refused profile.
     */
    bool publish() {
        if (!result_.ipf.profiles.empty()) {
            if (!consumer_->profile(result_.ipf.profiles.front())) {
                return false;
            }

            result_.ipf.profiles.clear();
        } else if (result_.ipf.completions != completions_) {
            if (!consumer_->control(IpfItemKind::COMPLETE)) {
                return false;
            }

            completions_ = result_.ipf.completions;
        } else if (result_.ipf.flushes != flushes_) {
            if (!consumer_->control(IpfItemKind::FLUSH)) {
                return false;
            }

            flushes_ = result_.ipf.flushes;
        }

        return true;
    }

    /** Feed one record under finite original work.
     * @param input Borrowed encoded prefix.
     * @param work Mutable parser credits.
     * @return Exact consumed prefix; no input suffix borrow survives.
     */
    std::size_t feed(std::span<const std::byte> input, IpfReadWork &work) {
        if (staged()) {
            return 0;
        }

        work.records = (std::min)(work.records, std::size_t{1});

        return inner_.feed(input, work);
    }

    /** Drain cached decoded input only.
     * @param work Finite parser credits.
     * @return True only when no event/input remains.
     */
    bool drain(IpfReadWork &work) {
        if (staged()) {
            return false;
        }

        work.records = (std::min)(work.records, std::size_t{1});

        return inner_.drain(work) && !staged();
    }

    /** Finish one finite EOF turn.
     * @param work Remaining parser credits.
     * @return True for a terminal parser after all staged items are delivered.
     */
    bool finish(IpfReadWork &work) {
        if (staged()) {
            return false;
        }

        work.records = (std::min)(work.records, std::size_t{1});

        return inner_.finish(work) && !staged();
    }

    /** Inspect decoder/parser terminal failure.
     * @return Existing terminal failure.
     */
    bool failed() const noexcept {
        return inner_.failed();
    }

    /** Inspect retained decoded/item work.
     * @return True for the single event or already owned decoded prefix.
     */
    bool hasOwnedInput() const noexcept {
        return staged() || inner_.hasOwnedInput();
    }

    /** Select the same single gzip envelope.
     * @return Existing bounded decoder admission.
     */
    bool declareGzip() noexcept {
        return inner_.declareGzip();
    }
};
} // namespace org::ttldtor::qdcxx::ipf::detail
