// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/indexed_event_model.hpp"
#include "qd/events/order.hpp"

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::api {

/** Owning, coherent bid and ask views for one exact symbol. */
struct OrderBookSnapshot {
    std::vector<events::Order> buyOrders;  ///< Bids in descending price order, with market-order tie breaks.
    std::vector<events::Order> sellOrders; ///< Asks in ascending price order, with market-order tie breaks.
};

/** Bounded pure-data order book built from ordinary Order events.
 * All operations must be serialized and nonreentrant. The model retains no Feed or caller-owned event.
 * Pending transactions are scoped by exact symbol and source. Returned snapshots own their data.
 */
class OrderBookModel final {
    IndexedEventModelLimits limits_;
    IndexedEventModel<events::Order> orders_;

    public:
    /** Create an empty order book.
     * @param limits Bounds on partitions, symbol units, input events, and pending and retained events.
     */
    explicit OrderBookModel(IndexedEventModelLimits limits = {});

    /** Apply a partition's ordered events, retaining only complete transactions in the visible book.
     * If any event has a different key or exceeds a bound, neither retained nor unfinished state changes.
     * A legacy index-zero terminator is accepted for an open snapshot, with the special-source exchange check.
     * @param symbol Exact logical symbol in the batch.
     * @param sourceId Signed source encoded in every order index.
     * @param input Borrowed order events in arrival order; only owning copies can survive this call.
     * @return Success or the typed refusal with prior state preserved.
     */
    [[nodiscard]] std::expected<void, IndexedEventModelError>
    processEvents(std::u16string_view symbol, std::int32_t sourceId, std::span<const events::Order> input);

    /** Copy both sides from one committed cut of all sources of this symbol.
     * Noncomposite zero-sized or NaN-sized orders are absent; composite orders retain their price indications.
     * NaN prices sort after numeric prices. Unspecified sides are placed with asks as in the reference model.
     * @param symbol Exact logical symbol; an absent symbol produces an empty book.
     * @return A detached owning snapshot, or a resource/allocation refusal.
     */
    [[nodiscard]] std::expected<OrderBookSnapshot, IndexedEventModelError>
    getSnapshot(std::u16string_view symbol) const;

    /** Drop retained and unfinished state. Previously returned snapshots remain valid. */
    void clear() noexcept;
};

} // namespace org::ttldtor::qdcxx::api
