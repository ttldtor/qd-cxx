// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../core/shared_record_block.hpp"

namespace org::ttldtor::qdcxx::api::detail {

/** Prepared immutable publication and independent admission credits; contains no runtime owner. */
struct Publication {
    core::detail::RecordByteReservation input; ///< Complete input credit through final collector consumption/discard.
    core::detail::RecordByteReservation rows;  ///< Admission rows, including a producer still preparing.
    core::detail::RecordByteReservation
        committedRows; ///< Visible accepted-row statistics through complete consumption.
    std::shared_ptr<const core::detail::BoundRecordBatch>
        block;                         ///< Single immutable packed payload and schema binding.
    std::shared_ptr<Publication> next; ///< Commit-ordered owning link, detached and reclaimed iteratively.
    const std::size_t family;          ///< Existing collector family shared by every row in this call.

    /** Adopt already reserved complete storage without allocating.
     * @param admission Whole publication input reservation.
     * @param count Complete row admission credit.
     * @param payload Complete immutable payload; aliases retain its independent physical credit.
     * @param lane Collector family index.
     */
    Publication(core::detail::RecordByteReservation admission, core::detail::RecordByteReservation count,
                std::shared_ptr<const core::detail::BoundRecordBatch> payload, std::size_t lane)
        : input(std::move(admission)), rows(std::move(count)), block(std::move(payload)), family(lane) {
    }
};

/** Iteratively discard detached ownership without recursively destroying a publication chain.
 * @param chain Detached head; no shared queue mutex may be held by the caller.
 */
inline void discardPublications(std::shared_ptr<Publication> chain) noexcept {
    while (chain) {
        auto next = std::move(chain->next);
        chain.reset();
        chain = std::move(next);
    }
}

/** Worker-owned selection cursor. A partial prefix retains the complete credit of its publication. */
struct PublicationCursor {
    std::shared_ptr<Publication> publication;  ///< Current publication and remaining detached FIFO.
    std::size_t row = 0;                       ///< First unconsumed row in the current immutable publication.
    std::size_t selectedRows = 0;              ///< Consecutive rows staged across ready same-family publications.
    std::optional<std::size_t> applyingFamily; ///< Selection awaiting complete collector consumption.

    /** Reclaim any abandoned detached chain without recursion. */
    ~PublicationCursor() {
        discardPublications(std::move(publication));
    }

    /** Mark a prepared selection; input credits remain owned through collector completion.
     * @param count Number of rows now staged for the current collector.
     */
    void staged(std::size_t count) noexcept {
        selectedRows = count;
        applyingFamily = publication->family;
    }

    /** Finish a consumed collector selection, retaining the full credit of any partially consumed publication. */
    void collected() noexcept {
        applyingFamily.reset();
        auto remaining = std::exchange(selectedRows, 0);

        while (publication && remaining) {
            const auto available = publication->block->count - row;

            if (remaining < available) {
                row += remaining;
                break;
            }

            remaining -= available;
            auto next = std::move(publication->next);
            publication.reset();
            publication = std::move(next);
            row = 0;
        }
    }
};

/** Visit consecutive ready rows of one family within the existing source quantum.
 * @tparam Select Callable accepting a publication, first row, remaining quantum and selected offset; returns selected
 * rows.
 * @tparam Append Callable accepting an exhausted detached tail and appending only already-ready publication ownership.
 * @param cursor Worker-owned first unconsumed publication; not advanced until collector acknowledgement.
 * @param maximum Existing source row quantum.
 * @param select Select a prefix from (publication, first, remaining quantum, selected offset); return its row count.
 * @param append Append already-ready ownership to an exhausted detached tail, without waiting for another producer.
 * @return Rows selected across complete publications and at most one final partial publication.
 */
template <class Select, class Append>
std::size_t selectReadyPublicationRows(PublicationCursor &cursor, std::size_t maximum, Select &&select,
                                       Append &&append) {
    const auto family = cursor.publication->family;
    auto *publication = cursor.publication.get();
    auto first = cursor.row;
    std::size_t count = 0;

    while (publication && publication->family == family && count < maximum) {
        const auto selected = select(*publication, first, maximum - count, count);
        count += selected;

        if (selected == 0 || count == maximum || first + selected != publication->block->count) {
            break;
        }

        if (!publication->next) {
            append(*publication);
        }

        publication = publication->next.get();
        first = 0;
    }

    return count;
}

/** Intrusive owning FIFO. Every operation is fenced by the owning port mutex. */
class PublicationQueue {
    std::shared_ptr<Publication> head_;
    Publication *tail_ = nullptr;

    public:
    /** Construct an empty fence-owned FIFO. */
    PublicationQueue() = default;
    PublicationQueue(const PublicationQueue &) = delete;
    PublicationQueue &operator=(const PublicationQueue &) = delete;

    /** Iteratively reclaim any remaining ownership after the containing port becomes unreachable. */
    ~PublicationQueue() {
        discardPublications(std::move(head_));
    }

    /** Link a fully prepared publication without allocation or payload access.
     * @param publication Exclusive unlinked node; caller owns the queue fence.
     */
    void commit(std::shared_ptr<Publication> publication) noexcept {
        auto *tail = publication.get();

        if (tail_) {
            tail_->next = std::move(publication);
        } else {
            head_ = std::move(publication);
        }

        tail_ = tail;
    }

    /** Transfer the complete ready chain in constant time.
     * @return Detached owning chain, possibly empty.
     */
    std::shared_ptr<Publication> detach() noexcept {
        tail_ = nullptr;

        return std::move(head_);
    }

    /** Inspect the first committed family under the owning fence.
     * @return Family index, or no value for an empty queue.
     */
    std::optional<std::size_t> firstFamily() const noexcept {
        return head_ ? std::optional{head_->family} : std::nullopt;
    }

    /** Inspect readiness while holding the queue fence.
     * @return Whether a committed node is available.
     */
    bool ready() const noexcept {
        return static_cast<bool>(head_);
    }
};

/** Check the actual FIFO head without bypassing a worker-owned publication.
 * @tparam Ready Predicate accepting a collector family index.
 * @param cursor Current detached head, including a partially consumed publication.
 * @param mailbox Later committed publications, inspected under their owning fence.
 * @param ready Whether the selected family can make local progress now.
 * @return Whether the actual next publication can advance; later mailbox work never bypasses a detached head.
 */
template <class Ready>
bool runnablePublicationHead(const PublicationCursor &cursor, const PublicationQueue &mailbox, Ready &&ready) {
    const auto family = cursor.publication ? std::optional{cursor.publication->family} : mailbox.firstFamily();

    return family && ready(*family);
}

/** Keep an already selected whole block, or create genuinely smaller independent retained storage.
 * @param block Complete immutable source publication, borrowed for this synchronous call.
 * @param first First selected row.
 * @param count Number of selected rows.
 * @param physical Finite physical domain for an independent partial selection.
 * @return Original owner without allocation for the entire block, otherwise an independent packed range or error.
 */
inline std::expected<std::shared_ptr<const core::detail::BoundRecordBatch>, core::detail::RecordBlockError>
selectPublicationRange(const std::shared_ptr<const core::detail::BoundRecordBatch> &block, std::size_t first,
                       std::size_t count, const core::detail::RecordByteBudget &physical) {
    if (!block) {
        return std::unexpected(core::detail::RecordBlockError::INVALID_RANGE);
    }

    if (first == 0 && count == block->count) {
        return block;
    }

    return core::detail::SharedRecordBlock::copyRange(*block, first, count, physical);
}
} // namespace org::ttldtor::qdcxx::api::detail
