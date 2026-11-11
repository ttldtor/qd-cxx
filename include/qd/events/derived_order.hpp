// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/events/order.hpp"
#include <optional>

namespace org::ttldtor::qdcxx::events {
/** Explicit prepared physical field family, independent of the business Order source ID. */
enum class DerivedOrderKind {
    QUOTE,       ///< Composite or regional Quote fields; ordinary reception uses TICKER.
    MARKET_MAKER ///< Aggregate MarketMaker fields; ordinary reception uses HISTORY.
};
/** Actual factory delegate identity; unitary roles form one indivisible receiver group. */
enum class DerivedOrderRole {
    BID,         ///< Separate bid source.
    ASK,         ///< Separate ask source.
    UNITARY_BID, ///< Bid half, setting TX_PENDING and clearing END/SNIP.
    UNITARY_ASK  ///< Ask half, clearing BEGIN.
};

/** Independently owned exact-field plan for Quote/MarketMaker-derived Order delegates. */
class PreparedDerivedOrder {
    EventMapping mapping_;
    std::optional<core::IndexedTimeLayout> indexedTimeLayout_; ///< Actual owning source-time facts only.
    DerivedOrderKind kind_;
    char16_t recordExchange_;
    /** @param mapping Complete independently owned field plan.
     * @param kind Explicit validated physical family.
     * @param exchange Actual immutable Quote record suffix, zero for composite/MM.
     * @param layout Exact optional original physical indexed time layout, owned independently.
     */
    PreparedDerivedOrder(EventMapping mapping, DerivedOrderKind kind, char16_t exchange,
                         std::optional<core::IndexedTimeLayout> layout) noexcept;

    public:
    /** Prepare actual generated delegate fields without retaining caller declarations.
     * @param schema Explicit validated schema.
     * @param localId Exact physical local record identity.
     * @param kind Explicit supported field family; unknown values are refused.
     * @param limits Finite preparation allowances.
     * @return Complete immutable plan or atomic typed refusal.
     */
    [[nodiscard]] static std::expected<PreparedDerivedOrder, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, DerivedOrderKind kind, MappingLimits limits);
    /** @return Retained exact context until this plan is moved, assigned or destroyed. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** @return Explicit physical field family, without deriving transport permission from its name. */
    [[nodiscard]] DerivedOrderKind kind() const noexcept;
    /** Inspect the source selected by one real delegate.
     * @param role Known separate/unitary bid/ask identity.
     * @return Source ID1..9 or nullopt for an unknown role; no allocation occurs.
     */
    [[nodiscard]] std::optional<std::int32_t> sourceId(DerivedOrderRole role) const noexcept;
    /** Synchronously project one real delegate. Receivers separately enforce paired unitary selection.
     * @param symbol Exact physical QD symbol; no regional rewrite is applied.
     * @param values Complete context-tagged local values.
     * @param role Explicit supported delegate identity.
     * @param limits Finite owning symbol/maker-text bounds.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Actual collector-produced cursor flags.
     * @param indexedRemovalTime Exact raw REMOVE time; supported only for the retained MM indexed context.
     * @return Complete independent Order or atomic context/type/value/resource refusal.
     */
    [[nodiscard]] std::expected<Order, MappingError>
    map(std::u16string_view symbol, RecordValueView values, DerivedOrderRole role, EventValueLimits limits,
        std::int64_t eventTime = 0, std::int32_t eventFlags = 0,
        std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
    /** Project borrowed fields without constructing an intermediate owning record or event.
     * @param symbol Exact physical QD symbol, borrowed through return.
     * @param values Complete context-tagged field views, valid through return.
     * @param role Explicit supported delegate identity.
     * @param limits Finite owning symbol/maker-text bounds.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Actual collector-produced cursor flags.
     * @param indexedRemovalTime Exact raw REMOVE time; supported only for the retained MM indexed context.
     * @return Complete independent Order or atomic typed refusal; inputs are never retained.
     */
    [[nodiscard]] std::expected<Order, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, DerivedOrderRole role, EventValueLimits limits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0,
            std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
};
} // namespace org::ttldtor::qdcxx::events
