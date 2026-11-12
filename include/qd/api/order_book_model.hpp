// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/indexed_event_model.hpp"
#include "qd/api/model_source_epoch.hpp"
#include "qd/events/order.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::api {

/** Order scopes eligible for display before finer-scope suppression. */
enum class OrderBookModelFilter {
    UNFILTERED,                   ///< Keep all eligible scopes without suppression.
    COMPOSITE,                    ///< Composite quotes only.
    REGIONAL,                     ///< Regional quotes only.
    AGGREGATE,                    ///< Aggregate orders only.
    ORDER,                        ///< Individual orders only.
    COMPOSITE_REGIONAL,           ///< Composite and regional quotes.
    COMPOSITE_REGIONAL_AGGREGATE, ///< All scopes except individual orders.
    ALL                           ///< All scopes, with finer scopes hiding covered entries.
};

/** Options for one detached snapshot, without altering retained orders. */
struct OrderBookViewOptions {
    OrderBookModelFilter filter = OrderBookModelFilter::ALL; ///< Reference scope selection and fallback.
    std::optional<std::int32_t> sourceId;                    ///< Exact signed source, or all sources.
    std::optional<std::size_t> depth;                        ///< Maximum entries per side, or all eligible entries.
    std::int32_t lotSize = 1;                                ///< Positive multiplier for nonindividual sizes.
};

/** Where the model applies aggregate order correction. */
enum class OrderBookCorrectionPolicy {
    DISABLED,   ///< Preserve ordinary committed orders.
    SAME_SYMBOL ///< Compare qualifying aggregate orders across all sources of one exact symbol.
};

/** Explicit finite correction policy. Ages use business time, without wall-clock sampling. */
struct OrderBookCorrectionOptions {
    OrderBookCorrectionPolicy policy = OrderBookCorrectionPolicy::DISABLED; ///< Correction must be explicitly enabled.
    std::chrono::milliseconds keepAge = std::chrono::hours(24);             ///< Maximum age for an unrelated aggregate.
    std::chrono::milliseconds flipAge = std::chrono::minutes(1);            ///< Age threshold for crossing sides.
    std::size_t entries = 8192;                                             ///< Total live and suppressed stamps.
};

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
    struct CorrectionStamp {
        std::int64_t time;
        double price;
        events::Side side;
        bool suppressed;
    };

    using CorrectionEntries = std::map<std::int64_t, CorrectionStamp>;
    using Corrections = std::map<std::u16string, CorrectionEntries, std::less<>>;

    IndexedEventModelLimits limits_;
    IndexedEventModel<events::Order> orders_;
    OrderBookCorrectionOptions correctionOptions_;
    Corrections corrections_;
    detail::ModelSourceFence source_;

    /** Apply validated-lifecycle indexed input and its dependent correction state.
     * @param symbol Exact logical key.
     * @param sourceId Exact source encoded in each order index.
     * @param input Borrowed orders in arrival order.
     * @return Atomic success or the typed refusal.
     */
    [[nodiscard]] std::expected<void, IndexedEventModelError>
    applyEvents(std::u16string_view symbol, std::int32_t sourceId, std::span<const events::Order> input);

    /** Copy a view after the caller has selected its qualification policy.
     * @param symbol Exact logical key.
     * @param options Borrowed display options.
     * @return Detached view or a typed refusal.
     */
    [[nodiscard]] std::expected<OrderBookSnapshot, IndexedEventModelError>
    buildSnapshot(std::u16string_view symbol, const OrderBookViewOptions &options) const;

    /** Retire all unfinished values and optionally drop the committed cut.
     * @param retention The accepted historical retention policy.
     */
    void retireData(ModelRetention retention) noexcept;

    /** Stage correction stamps from the final completed changes of a single source.
     * @param entries Scratch stamps for the exact symbol.
     * @param sourceId The source of the prepared indexed change.
     * @param changes Completed values borrowed from the indexed-model prepare phase.
     * @param capacity Maximum stamps available for this symbol after charges for other symbols.
     * @return Success or a resource refusal; scratch mutations stay private until both models commit.
     */
    [[nodiscard]] std::expected<void, IndexedEventModelError>
    stageCorrections(CorrectionEntries &entries, std::int32_t sourceId,
                     std::span<const IndexedEventChange<events::Order>> changes, std::size_t capacity) const;

    /** Apply one qualifying aggregate and update the suppression stamps of displaced orders.
     * @param entries Scratch stamps for the exact symbol.
     * @param order Committed aggregate with positive size, numeric finite price and a known side.
     */
    void correct(CorrectionEntries &entries, const events::Order &order) const;

    public:
    /** Create an empty order book.
     * @param limits Bounds on partitions, symbol units, input events, and pending and retained events.
     */
    explicit OrderBookModel(IndexedEventModelLimits limits = {});

    /** Create an empty order book with an explicit aggregate correction policy.
     * @param limits Bounds on partitions, symbol units, input events, and pending and retained events.
     * @param corrections Correction scope, thresholds and live/suppressed entry budget.
     * @throws std::invalid_argument The policy is unrecognized or a threshold is negative.
     */
    OrderBookModel(IndexedEventModelLimits limits, OrderBookCorrectionOptions corrections);

    OrderBookModel(const OrderBookModel &) = delete;
    OrderBookModel &operator=(const OrderBookModel &) = delete;

    /** Transfer both indexed and correction state, leaving the source empty with its original options.
     * @param other Source model, unchanged if empty backing allocation fails.
     * @throws std::bad_alloc Empty backing allocation failed.
     */
    OrderBookModel(OrderBookModel &&other);

    /** Transfer both models together and leave the source empty with its original options.
     * @param other Source model, or this model for a no-op.
     * @return This model with the transferred state and options.
     */
    OrderBookModel &operator=(OrderBookModel &&other) noexcept;

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

    /** Apply source, scope, depth and lot options to a single committed cut.
     * Source selection precedes scope fallback. Depth applies independently to each sorted side.
     * Lot scaling changes only the returned nonindividual business size, leaving raw decimal bits intact.
     * @param symbol Exact logical symbol; an absent symbol produces an empty book.
     * @param options Borrowed view settings. A nonpositive lot size or unrecognized filter is INVALID_OPTIONS.
     * @return A detached owning snapshot or a typed refusal.
     */
    [[nodiscard]] std::expected<OrderBookSnapshot, IndexedEventModelError>
    getFilteredSnapshot(std::u16string_view symbol, const OrderBookViewOptions &options) const;

    /** @return Detached qualification of the model's current source cut. */
    [[nodiscard]] ModelSourceState getSourceState() const noexcept;

    /** Start a distinct source epoch and clear committed data and correction stamps.
     * @param ordinal Strictly increasing positive source ordinal known at the delivery boundary.
     * @return Token to capture with fresh deliveries, or a typed refusal.
     */
    [[nodiscard]] std::expected<ModelSourceEpoch, ModelSourceError> beginSourceEpoch(std::uint64_t ordinal) noexcept;

    /** Fence delivery for a matching gap and retire incomplete transactions.
     * @param epoch Token for the affected source epoch.
     * @param retention Whether the old complete cut stays available as historical data.
     * @return Success or a typed refusal.
     */
    [[nodiscard]] std::expected<void, ModelSourceError> markSourceGap(const ModelSourceEpoch &epoch,
                                                                      ModelRetention retention) noexcept;

    /** Qualify a fresh epoch once the caller has observed complete replay for its selected book.
     * @param epoch Token for the fresh receiving source.
     * @return Success when no indexed transaction is unfinished, or a typed refusal.
     */
    [[nodiscard]] std::expected<void, ModelSourceError> confirmSourceEpoch(const ModelSourceEpoch &epoch) noexcept;

    /** Terminally fence matching delivery and retire incomplete transactions.
     * @param epoch Token for the affected source epoch.
     * @param retention Whether the final complete cut stays available as historical data.
     * @return Success or a typed refusal. A closed model admits no later source epoch.
     */
    [[nodiscard]] std::expected<void, ModelSourceError> closeSource(const ModelSourceEpoch &epoch,
                                                                    ModelRetention retention) noexcept;

    /** Apply orders associated with their original source activation.
     * @param epoch Token captured with the originating batch.
     * @param symbol Exact logical key.
     * @param sourceId Exact signed order source, independent of the delivery epoch.
     * @param input Borrowed owning orders in arrival order.
     * @return Success or a typed refusal. A stale token changes neither data nor correction stamps.
     */
    [[nodiscard]] std::expected<void, IndexedEventModelError> processSourceEvents(const ModelSourceEpoch &epoch,
                                                                                  std::u16string_view symbol,
                                                                                  std::int32_t sourceId,
                                                                                  std::span<const events::Order> input);

    /** Copy a book cut together with its exact source qualification, including provisional and historical cuts.
     * @param symbol Exact logical key.
     * @param options Borrowed display options. By default the returned cut is unfiltered.
     * @return Owning snapshot and source qualification, or a typed refusal.
     */
    [[nodiscard]] std::expected<ModelSourceSnapshot<OrderBookSnapshot>, IndexedEventModelError>
    getSourceSnapshot(std::u16string_view symbol,
                      const OrderBookViewOptions &options = {.filter = OrderBookModelFilter::UNFILTERED}) const;

    /** Drop retained and unfinished state. This fences tracked input until a fresh source begins. */
    void clear() noexcept;
};

} // namespace org::ttldtor::qdcxx::api
