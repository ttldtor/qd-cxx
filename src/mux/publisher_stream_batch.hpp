// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../core/shared_record_payload.hpp"
#include "qd/mux/queue.hpp"

namespace org::ttldtor::qdcxx::mux::detail {

/** One immutable run entry. The owning run retains the complete client payload leases. */
struct PublisherStreamRow {
    const core::detail::SharedRecordPayload *payload;
    RawSourceMetadata source;
};

/** Borrowed selected portion of a homogeneous run. All references expire with the last batch owner. */
struct PublisherStreamRange {
    const MuxRecordView &common;
    std::span<const PublisherStreamRow> rows;
    core::DeliveryProjectionMask selection;
};

/** Detached private publisher callback owner. It contains no queue, peer or callback reference. */
class PublisherStreamBatch {
    friend class mux::MuxQueue;
    std::shared_ptr<const QueueBatch> owner_;
    std::size_t size_ = 0;

    /** Adopt the already charged batch containing exactly size selected rows. */
    PublisherStreamBatch(std::shared_ptr<const QueueBatch> owner, std::size_t size) noexcept
        : owner_(std::move(owner)), size_(size) {
    }

    public:
    PublisherStreamBatch() = default;

    /** @return Selected rows, or zero for an empty or moved-from owner. */
    [[nodiscard]] std::size_t size() const noexcept {
        return owner_ ? size_ : 0;
    }

    /** @return Nonempty range at a valid index in this batch, in FIFO order. */
    [[nodiscard]] PublisherStreamRange range(std::size_t index) const noexcept;
};

/** Publisher registration may receive ordinary batches whenever compact range routing is ineligible. */
struct PublisherStreamConsumer : MuxConsumer {
    /** Consume the selected ranges outside queue locks under the ordinary callback/PAUSE contract. */
    virtual BatchAction onStreamRanges(PublisherStreamBatch batch) = 0;
};

} // namespace org::ttldtor::qdcxx::mux::detail
