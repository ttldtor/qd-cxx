// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/client/subscription_receiver.hpp"
#include "../events/candle_receiving_limits.hpp"
#include "../events/derived_order_sources.hpp"
#include "../events/order_index_coordinate.hpp"
#include "qd/events/candle_routes.hpp"
#include "qd/events/event_flags.hpp"
#include "subscription_receiver_observation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::client {
namespace {
/** Add finite logical counts without wrapping.
 * @param left Existing count.
 * @param right Additional count.
 * @return Exact sum or local resource refusal.
 */
std::size_t checkedSum(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left)
        throw ReceiverError::RESOURCE_LIMIT;

    return left + right;
}

/** Multiply finite logical counts without wrapping.
 * @param count Element count.
 * @param size Element size.
 * @return Exact product or local resource refusal.
 */
std::size_t checkedProduct(std::size_t count, std::size_t size) {
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size)
        throw ReceiverError::RESOURCE_LIMIT;

    return count * size;
}

/** Debit explicit attempted work without replenishment or underflow.
 * @param remaining Caller-owned remaining credits.
 * @param count Conservative attempted work.
 */
void debit(std::size_t &remaining, std::size_t count) {
    if (count > remaining)
        throw ReceiverError::WORK_LIMIT;

    remaining -= count;
}

/** Conservatively prepay one existing mapper's independently bounded preparation.
 * @param limits Existing mapper's count, name-unit and comparison allowances.
 * @return Checked sum; it is work, not an allocator/RSS measurement.
 */
std::size_t preparationAllowance(events::MappingLimits limits) {
    std::size_t total = 0;

    for (const auto amount :
         std::array{limits.fields, limits.lookups, limits.selections, limits.keys, limits.typeAlternatives,
                    limits.nameUnits, limits.totalNameUnits, limits.comparisonWork})
        total = checkedSum(total, amount);

    return total;
}

/** Check supported explicit route settings without accepting unknown enum values.
 * @param binding Caller route under inspection.
 * @return Whether the mapper and timestamp policy are known.
 */
bool validBinding(const PreparedSubscriptionBinding &binding) noexcept {
    return (binding.kind == PreparedEventKind::QUOTE || binding.kind == PreparedEventKind::PROFILE ||
            binding.kind == PreparedEventKind::TRADE || binding.kind == PreparedEventKind::TRADE_ETH ||
            binding.kind == PreparedEventKind::SUMMARY || binding.kind == PreparedEventKind::TEXT_MESSAGE ||
            binding.kind == PreparedEventKind::TEXT_CONFIGURATION || binding.kind == PreparedEventKind::MESSAGE ||
            binding.kind == PreparedEventKind::CONFIGURATION || binding.kind == PreparedEventKind::ORDER ||
            binding.kind == PreparedEventKind::MARKET_MAKER || binding.kind == PreparedEventKind::SPREAD_ORDER ||
            binding.kind == PreparedEventKind::ANALYTIC_ORDER || binding.kind == PreparedEventKind::OTC_MARKETS_ORDER ||
            binding.kind == PreparedEventKind::ORDER_IMBALANCE || binding.kind == PreparedEventKind::SERIES ||
            binding.kind == PreparedEventKind::GREEKS || binding.kind == PreparedEventKind::UNDERLYING ||
            binding.kind == PreparedEventKind::THEO_PRICE || binding.kind == PreparedEventKind::CANDLE ||
            binding.kind == PreparedEventKind::TIME_AND_SALE || binding.kind == PreparedEventKind::OPTION_SALE ||
            (binding.kind == PreparedEventKind::NUAM_TRADE || binding.kind == PreparedEventKind::NUAM_TIME_AND_SALE)) &&
           (binding.eventTimePolicy == ReceiverEventTimePolicy::UNSPECIFIED ||
            binding.eventTimePolicy == ReceiverEventTimePolicy::FIELD_METADATA) &&
           (binding.projection == events::EventProjection::NATIVE ||
            (binding.kind == PreparedEventKind::ORDER &&
             (binding.projection == events::EventProjection::QUOTE_ORDERS ||
              binding.projection == events::EventProjection::MARKET_MAKER_ORDERS)));
}

std::variant<ReceiverError, events::MappingError> candleFailure(const events::CandleRouteFailure &failure) noexcept {
    if (failure.mappingError)
        return *failure.mappingError;

    switch (failure.error) {
    case events::CandleRouteError::RESOURCE_LIMIT:
        return ReceiverError::RESOURCE_LIMIT;
    case events::CandleRouteError::ALLOCATION_FAILURE:
        return ReceiverError::ALLOCATION_FAILURE;
    default:
        return ReceiverError::INVALID_INPUT;
    }
}
} // namespace

namespace detail {
struct CandleReceiverMapping {
    std::int32_t localId;
    events::PreparedCandle mapper;
    events::PreparedCandleRoutes routes;
};

/** One independently owned immutable mapper and its explicit projection bounds. */
struct ReceiverRoute {
    core::RecordContext context; ///< Retained exact origin/record identity, without declaration names.
    std::variant<events::PreparedQuote, events::PreparedProfile, events::PreparedTrade, events::PreparedTradeETH,
                 events::PreparedSummary, events::PreparedTextMessage, events::PreparedTextConfiguration,
                 events::PreparedMessage, events::PreparedConfiguration, events::PreparedOrder,
                 events::PreparedMarketMaker, events::PreparedSpreadOrder, events::PreparedAnalyticOrder,
                 events::PreparedOtcMarketsOrder, events::PreparedOrderImbalance, events::PreparedSeries,
                 events::PreparedGreeks, events::PreparedUnderlying, events::PreparedTheoPrice,
                 events::PreparedTimeAndSale, events::PreparedOptionSale, events::PreparedDerivedOrder,
                 CandleReceiverMapping, events::PreparedNuamTrade, events::PreparedNuamTimeAndSale>
        mapping;                          ///< Existing owned immutable mapper.
    events::EventValueLimits valueLimits; ///< Per-result owned text bounds.
    ReceiverEventTimePolicy timePolicy;   ///< Explicit optional timestamp source.
    std::size_t shapeFields;              ///< Complete shape inspection allowance, including unselected extra fields.
    std::size_t selectionVisits;         ///< Declared mapper selection allowance, including absent optional properties.
    std::size_t attachmentBytes = 65536; ///< Finite copied serialized-body allowance for attachment routes.
};

/** Finite immutable adapter backing; no desired map, handle or endpoint is retained. */
struct ReceiverPlan {
    std::vector<ReceiverRoute> routes; ///< Exact unique routes, bounded before allocation/preparation.
};

/** Delegate the existing whole-batch callback boundary without internally dispatching individual records. */
class SubscriptionReceiver final : public mux::MuxConsumer {
    ReceiverPlan plan_;                              ///< Immutable independently owned prepared routes.
    std::shared_ptr<SubscriptionConsumer> consumer_; ///< Executing application callable's strong lifetime guard.

    public:
    /** Adopt a complete candidate, without starting work.
     * @param plan Prepared bounded immutable routes.
     * @param consumer Application callable retained by the adapter.
     */
    SubscriptionReceiver(ReceiverPlan plan, std::shared_ptr<SubscriptionConsumer> consumer)
        : MuxConsumer(consumer->deliveryStatistics()), plan_(std::move(plan)), consumer_(std::move(consumer)) {
    }

    /** @return Adapter-owned plan, borrowed only while this exact queue registration is live. */
    const ReceiverPlan *observationPlan() const noexcept {
        return &plan_;
    }

    /** Preserve exactly one application invocation and the queue's existing exception boundary.
     * @param batch Underlying callback-only immutable backing.
     * @return Application's whole-batch action, without another user call after it returns.
     */
    mux::BatchAction onBatch(const mux::BorrowedMuxBatch &batch) override {
        const SubscriptionBatch view(batch, plan_);

        return consumer_->onBatch(view);
    }
};
} // namespace detail

SubscriptionBatch::SubscriptionBatch(const mux::BorrowedMuxBatch &batch, const detail::ReceiverPlan &plan) noexcept
    : batch_(&batch), plan_(&plan) {
}

std::span<const mux::MuxRecordView> SubscriptionBatch::records() const noexcept {
    return batch_->records();
}

std::optional<std::uint64_t> SubscriptionBatch::appliedDesiredRevision() const noexcept {
    return batch_->appliedDesiredRevision();
}

mux::OwnedMuxBatch SubscriptionBatch::retain() const noexcept {
    return batch_->retain();
}

namespace {
/** Conservative paid projection work for one actual owning result.
 * @param route Independently owned prepared route.
 * @return Exact checked declared allowances; actual copied text may be shorter.
 */
ReceiverProjectionWork projectionAllowance(const detail::ReceiverRoute &route) {
    if (std::holds_alternative<detail::CandleReceiverMapping>(route.mapping)) {
        const auto units = checkedProduct(route.valueLimits.symbolUnits, events::detail::CANDLE_SYMBOL_TEXT_PASSES);
        const auto bytes =
            checkedSum(checkedSum(sizeof(SubscriptionProjection), events::detail::CANDLE_SYMBOL_FIXED_BYTES),
                       checkedProduct(checkedSum(units, 1), sizeof(char16_t)));

        return {checkedSum(route.shapeFields, checkedSum(route.selectionVisits, 1)), units, bytes};
    }

    const bool profile = std::holds_alternative<events::PreparedProfile>(route.mapping);
    const bool timeAndSale = (std::holds_alternative<events::PreparedTimeAndSale>(route.mapping) ||
                              std::holds_alternative<events::PreparedNuamTimeAndSale>(route.mapping));
    const bool optionSale = std::holds_alternative<events::PreparedOptionSale>(route.mapping);
    const bool text = std::holds_alternative<events::PreparedTextMessage>(route.mapping) ||
                      std::holds_alternative<events::PreparedTextConfiguration>(route.mapping);
    const bool order = std::holds_alternative<events::PreparedOrder>(route.mapping) ||
                       std::holds_alternative<events::PreparedDerivedOrder>(route.mapping) ||
                       std::holds_alternative<events::PreparedSpreadOrder>(route.mapping) ||
                       std::holds_alternative<events::PreparedAnalyticOrder>(route.mapping) ||
                       std::holds_alternative<events::PreparedOtcMarketsOrder>(route.mapping);
    const bool attachment = std::holds_alternative<events::PreparedMessage>(route.mapping) ||
                            std::holds_alternative<events::PreparedConfiguration>(route.mapping);
    const bool symbolOnly = !profile && !text && !order && !timeAndSale && !optionSale;
    const auto units = symbolOnly ? std::min(route.valueLimits.symbolUnits, route.valueLimits.totalUnits)
                                  : route.valueLimits.totalUnits;
    const auto strings = symbolOnly    ? std::size_t{1}
                         : timeAndSale ? std::size_t{4}
                                       : ((profile || optionSale) ? std::size_t{3} : std::size_t{2});
    const auto bytes = checkedSum(
        attachment ? route.attachmentBytes : 0,
        checkedSum(sizeof(SubscriptionProjection), checkedProduct(checkedSum(units, strings), sizeof(char16_t))));

    return {checkedSum(route.shapeFields, checkedSum(route.selectionVisits, 1)), units, bytes};
}

/** Debit the complete attempted result/group before its first allocation.
 * @param work Remaining caller credits.
 * @param amount Complete checked logical work allowance.
 */
void prepayProjection(ReceiverProjectionWork &work, ReceiverProjectionWork amount) {
    debit(work.visits, amount.visits);

    if (amount.textUnits > work.textUnits || amount.bytes > work.bytes) {
        throw ReceiverError::WORK_LIMIT;
    }

    work.textUnits -= amount.textUnits;
    work.bytes -= amount.bytes;
}

/** Execute one already prepaid mapper without retaining the callback borrow.
 * @param record Actual immutable physical projection.
 * @param route Independently owned prepared mapper.
 * @param role Explicit derived delegate, absent for native routes.
 * @return One independent owning result or atomic typed refusal.
 */
std::expected<SubscriptionProjection, ReceiverProjectionFailure>
projectPrepared(const mux::MuxRecordView &record, const detail::ReceiverRoute &route,
                std::optional<events::DerivedOrderRole> role) {
    const bool textMessage = std::holds_alternative<events::PreparedTextMessage>(route.mapping);
    const bool textConfiguration = std::holds_alternative<events::PreparedTextConfiguration>(route.mapping);
    const bool order = std::holds_alternative<events::PreparedOrder>(route.mapping);
    const bool attachment = std::holds_alternative<events::PreparedMessage>(route.mapping) ||
                            std::holds_alternative<events::PreparedConfiguration>(route.mapping);
    const bool quote = std::holds_alternative<events::PreparedQuote>(route.mapping);
    const auto units = projectionAllowance(route).textUnits;
    const auto payloadBytes = attachment ? route.attachmentBytes : 0;
    std::optional<std::int64_t> eventTime;

    if (route.timePolicy == ReceiverEventTimePolicy::FIELD_METADATA && record.source && record.source->fieldTime)
        eventTime = record.versionDefault ? 0 : record.source->fieldTime->eventTimeMs();

    const events::BorrowedRecordValueView values{record.context, record.typedValues};
    const auto indexedRemovalTime =
        (record.eventFlags & events::indexed_event_flags::REMOVE_EVENT) ? record.rawTime : std::nullopt;

    if (std::holds_alternative<events::PreparedDerivedOrder>(route.mapping)) {
        if (!role) {
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});
        }

        auto mapped =
            std::get<events::PreparedDerivedOrder>(route.mapping)
                .mapView(record.symbol, values, *role, route.valueLimits, eventTime.value_or(0), record.eventFlags,
                         std::get<events::PreparedDerivedOrder>(route.mapping).kind() ==
                                 events::DerivedOrderKind::MARKET_MAKER
                             ? indexedRemovalTime
                             : std::nullopt);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (std::holds_alternative<events::PreparedMarketMaker>(route.mapping)) {
        auto mapped = std::get<events::PreparedMarketMaker>(route.mapping)
                          .mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0), record.eventFlags,
                                   indexedRemovalTime);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    const events::AttachmentEventLimits attachmentLimits{
        units, payloadBytes, checkedSum(checkedProduct(units, sizeof(char16_t)), payloadBytes)};

    if (std::holds_alternative<events::PreparedMessage>(route.mapping)) {
        auto mapped = std::get<events::PreparedMessage>(route.mapping)
                          .mapView(record.symbol, values, attachmentLimits, eventTime.value_or(0));

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (std::holds_alternative<events::PreparedConfiguration>(route.mapping)) {
        auto mapped = std::get<events::PreparedConfiguration>(route.mapping)
                          .mapView(record.symbol, values, attachmentLimits, eventTime.value_or(0));

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (textMessage) {
        auto mapped = std::get<events::PreparedTextMessage>(route.mapping)
                          .mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0));

        if (!mapped)
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (textConfiguration) {
        auto mapped = std::get<events::PreparedTextConfiguration>(route.mapping)
                          .mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0));

        if (!mapped)
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (std::holds_alternative<events::PreparedTrade>(route.mapping)) {
        auto mapped =
            std::get<events::PreparedTrade>(route.mapping).mapView(record.symbol, values, units, eventTime.value_or(0));

        if (!mapped)
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedNuamTrade>(&route.mapping)) {
        auto mapped = prepared->mapView(record.symbol, values, units, eventTime.value_or(0));

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (std::holds_alternative<events::PreparedTradeETH>(route.mapping)) {
        auto mapped = std::get<events::PreparedTradeETH>(route.mapping)
                          .mapView(record.symbol, values, units, eventTime.value_or(0));

        if (!mapped)
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedSpreadOrder>(&route.mapping)) {
        auto mapped =
            prepared->mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0), record.eventFlags);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedAnalyticOrder>(&route.mapping)) {
        auto mapped =
            prepared->mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0), record.eventFlags);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedOtcMarketsOrder>(&route.mapping)) {
        auto mapped =
            prepared->mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0), record.eventFlags);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedSeries>(&route.mapping)) {
        auto mapped = prepared->mapView(record.symbol, values, units, eventTime.value_or(0), record.eventFlags);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedOptionSale>(&route.mapping)) {
        auto mapped = prepared->mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0),
                                        record.eventFlags, indexedRemovalTime);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedGreeks>(&route.mapping)) {
        auto mapped = prepared->mapView(record.symbol, values, units, eventTime.value_or(0), record.eventFlags,
                                        indexedRemovalTime);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedUnderlying>(&route.mapping)) {
        auto mapped = prepared->mapView(record.symbol, values, units, eventTime.value_or(0), record.eventFlags,
                                        indexedRemovalTime);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedTheoPrice>(&route.mapping)) {
        auto mapped = prepared->mapView(record.symbol, values, units, eventTime.value_or(0), record.eventFlags,
                                        indexedRemovalTime);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedNuamTimeAndSale>(&route.mapping)) {
        auto mapped = prepared->mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0),
                                        record.eventFlags, indexedRemovalTime);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedTimeAndSale>(&route.mapping)) {
        auto mapped = prepared->mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0),
                                        record.eventFlags, indexedRemovalTime);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<detail::CandleReceiverMapping>(&route.mapping)) {
        const auto symbol = prepared->routes.restore(prepared->localId, record.symbol);

        if (!symbol)
            return std::unexpected(candleFailure(symbol.error()));

        auto mapped =
            indexedRemovalTime
                ? prepared->mapper.mapIndexedRemovalView(*symbol, values, route.valueLimits.symbolUnits,
                                                         eventTime.value_or(0), record.eventFlags, *indexedRemovalTime)
                : prepared->mapper.mapView(*symbol, values, route.valueLimits.symbolUnits, eventTime.value_or(0),
                                           record.eventFlags);

        if (!mapped)
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (const auto *prepared = std::get_if<events::PreparedOrderImbalance>(&route.mapping)) {
        auto mapped = prepared->mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0));

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (order) {
        auto mapped = std::get<events::PreparedOrder>(route.mapping)
                          .mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0), record.eventFlags);

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (std::holds_alternative<events::PreparedSummary>(route.mapping)) {
        auto mapped = std::get<events::PreparedSummary>(route.mapping)
                          .mapView(record.symbol, values, units, eventTime.value_or(0));

        if (!mapped) {
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});
        }

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    if (quote) {
        auto mapped =
            std::get<events::PreparedQuote>(route.mapping).mapView(record.symbol, values, units, eventTime.value_or(0));

        if (!mapped)
            return std::unexpected(ReceiverProjectionFailure{mapped.error()});

        return SubscriptionProjection{std::move(*mapped), eventTime};
    }

    auto mapped = std::get<events::PreparedProfile>(route.mapping)
                      .mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0));

    if (!mapped)
        return std::unexpected(ReceiverProjectionFailure{mapped.error()});

    return SubscriptionProjection{std::move(*mapped), eventTime};
}
} // namespace

std::expected<SubscriptionProjection, ReceiverProjectionFailure>
detail::projectTickerLatest(const ReceiverPlan &plan, const collectors::TickerValue &value,
                            ReceiverProjectionWork &work) {
    try {
        if (!value.sourceRecord || !value.current)
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});

        const auto &source = *value.sourceRecord;
        const ReceiverRoute *selected = nullptr;

        for (const auto &route : plan.routes) {
            debit(work.visits, 1);

            if (route.context.matches(source.context) &&
                !std::holds_alternative<events::PreparedDerivedOrder>(route.mapping)) {
                selected = &route;
                break;
            }
        }

        if (!selected || source.typedValues.size() != selected->shapeFields)
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});

        auto allowance = projectionAllowance(*selected);
        allowance.visits = checkedSum(allowance.visits, source.typedValues.size());
        allowance.bytes =
            checkedSum(allowance.bytes, checkedProduct(source.typedValues.size(), sizeof(core::FieldValueView)));
        prepayProjection(work, allowance);

        std::vector<core::FieldValueView> fields;
        fields.reserve(source.typedValues.size());

        if (fields.capacity() != source.typedValues.size())
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::RESOURCE_LIMIT});

        for (const auto &field : source.typedValues)
            fields.push_back({field.type, field.raw, field.units, field.bytes});

        const auto &raw = value.rawSourceRecord ? *value.rawSourceRecord : source;
        mux::MuxRecordView record{};
        record.family = core::SubscriptionFamily::TICKER;
        record.context = source.context;
        record.symbol = source.symbol;
        record.typedValues = fields;
        record.source = mux::RawSourceMetadata{raw.senderId,    raw.cipher,    raw.eventFlags,
                                               raw.removalTime, raw.fieldTime, value.bindingGeneration};
        record.versionDefault = value.versionDefault;

        return projectPrepared(record, *selected, {});
    } catch (ReceiverError error) {
        return std::unexpected(ReceiverProjectionFailure{error});
    } catch (const std::bad_alloc &) {
        return std::unexpected(ReceiverProjectionFailure{ReceiverError::ALLOCATION_FAILURE});
    } catch (const std::length_error &) {
        return std::unexpected(ReceiverProjectionFailure{ReceiverError::RESOURCE_LIMIT});
    }
}

std::size_t SubscriptionBatch::eventCount() const noexcept {
    std::size_t count = 0;

    for (const auto &record : records()) {
        count += record.projections.count();
    }

    return count;
}

std::expected<SubscriptionProjectionGroup, ReceiverProjectionFailure>
SubscriptionBatch::projectAll(std::size_t index, ReceiverProjectionWork &work) const {
    try {
        debit(work.visits, 1);
        const auto input = records();

        if (index >= input.size()) {
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});
        }

        const auto &record = input[index];
        const auto mask = record.projections.mask;
        constexpr auto UNITARY_MASK = events::detail::DERIVED_UNITARY_PAIR;

        if (!core::validDeliveryProjectionMask(mask) ||
            ((mask & UNITARY_MASK) != 0 && (mask & UNITARY_MASK) != UNITARY_MASK)) {
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});
        }

        const detail::ReceiverRoute *native = nullptr, *derived = nullptr;

        for (const auto &route : plan_->routes) {
            debit(work.visits, 1);

            if (!route.context.matches(record.context)) {
                continue;
            }

            if (std::holds_alternative<events::PreparedDerivedOrder>(route.mapping)) {
                derived = &route;
            } else {
                native = &route;
            }
        }

        if ((mask & ~core::NATIVE_DELIVERY_PROJECTION) && !derived) {
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});
        }

        ReceiverProjectionWork paid{0, 0, sizeof(SubscriptionProjectionGroup)};
        const auto pay = [&](const detail::ReceiverRoute *route) {
            if (!route) {
                return;
            }

            const auto one = projectionAllowance(*route);
            paid.visits = checkedSum(paid.visits, one.visits);
            paid.textUnits = checkedSum(paid.textUnits, one.textUnits);
            paid.bytes = checkedSum(paid.bytes, one.bytes - sizeof(SubscriptionProjection));
        };

        if (mask & core::NATIVE_DELIVERY_PROJECTION) {
            pay(native);
        }

        for (std::size_t i = 1; i < core::MAX_DELIVERY_PROJECTIONS; ++i) {
            if (mask & (1U << i)) {
                pay(derived);
            }
        }

        prepayProjection(work, paid);
        SubscriptionProjectionGroup result;
        const auto addNative = [&]() -> std::expected<void, ReceiverProjectionFailure> {
            if (!(mask & core::NATIVE_DELIVERY_PROJECTION)) {
                return {};
            }

            if (!native) {
                ++result.size;

                return {};
            }

            auto projected = projectPrepared(record, *native, {});

            if (!projected) {
                return std::unexpected(projected.error());
            }

            result.values[result.size++] = std::move(*projected);

            return {};
        };
        const bool maker = native && std::holds_alternative<events::PreparedMarketMaker>(native->mapping);

        if (!maker) {
            if (auto added = addNative(); !added) {
                return std::unexpected(added.error());
            }
        }

        for (std::size_t i = 1; i < core::MAX_DELIVERY_PROJECTIONS; ++i) {
            if (!(mask & (1U << i))) {
                continue;
            }

            auto projected = projectPrepared(record, *derived, static_cast<events::DerivedOrderRole>(i - 1));

            if (!projected) {
                return std::unexpected(projected.error());
            }

            result.values[result.size++] = std::move(*projected);
        }

        if (maker) {
            if (auto added = addNative(); !added) {
                return std::unexpected(added.error());
            }
        }

        return result;
    } catch (ReceiverError error) {
        return std::unexpected(ReceiverProjectionFailure{error});
    }
}

std::expected<SubscriptionProjection, ReceiverProjectionFailure>
SubscriptionBatch::project(std::size_t index, ReceiverProjectionWork &work) const {
    try {
        debit(work.visits, 1);
        const auto input = records();

        if (index >= input.size() || input[index].projections.count() != 1) {
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});
        }

        const auto &record = input[index];
        const auto mask = record.projections.mask;

        if (!core::validDeliveryProjectionMask(mask) || (mask & events::detail::DERIVED_UNITARY_PAIR)) {
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});
        }

        const bool native = (mask & core::NATIVE_DELIVERY_PROJECTION) != 0;

        for (const auto &route : plan_->routes) {
            debit(work.visits, 1);

            if (!route.context.matches(record.context) ||
                native == std::holds_alternative<events::PreparedDerivedOrder>(route.mapping)) {
                continue;
            }

            prepayProjection(work, projectionAllowance(route));
            const auto role = native ? std::optional<events::DerivedOrderRole>{}
                                     : std::optional{static_cast<events::DerivedOrderRole>(std::countr_zero(mask) - 1)};

            return projectPrepared(record, route, role);
        }

        if (!native) {
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});
        }

        return SubscriptionProjection{};
    } catch (ReceiverError error) {
        return std::unexpected(ReceiverProjectionFailure{error});
    }
}

std::expected<mux::MuxHandle, ReceiverFailure> detail::openObservedSubscriptionReceiver(
    mux::MuxQueue &queue, const core::LocalSchema &schema, core::SubscriptionFamily family,
    core::SubscriptionSelectorPolicy policy, std::span<const PreparedSubscriptionBinding> bindings,
    const std::shared_ptr<SubscriptionConsumer> &consumer, ReceiverLimits receiverLimits,
    mux::ClientLimits clientLimits, ReceiverWork &receiverWork, mux::QueueWork &queueWork,
    const ReceiverPlan **observationPlan) {
    if (observationPlan)
        *observationPlan = nullptr;

    std::optional<std::size_t> bindingIndex;

    try {
        debit(receiverWork.visits, 1);

        if (!consumer)
            return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, {}});

        if (bindings.size() > receiverLimits.bindings)
            return std::unexpected(ReceiverFailure{ReceiverError::RESOURCE_LIMIT, {}});

        const auto compatible = queue.validateReceiverBinding(schema, family, policy, queueWork);

        if (!compatible)
            return std::unexpected(ReceiverFailure{compatible.error(), {}});

        for (std::size_t i = 0; i < bindings.size(); ++i) {
            bindingIndex = i;
            debit(receiverWork.visits, 1);
            const auto &binding = bindings[i];

            if (!validBinding(binding) || binding.localId < 0 ||
                static_cast<std::size_t>(binding.localId) >= schema.records().size())
                return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});

            if ((binding.projection == events::EventProjection::QUOTE_ORDERS &&
                 family != core::SubscriptionFamily::TICKER) ||
                ((binding.projection == events::EventProjection::MARKET_MAKER_ORDERS ||
                  binding.kind == PreparedEventKind::MARKET_MAKER || binding.kind == PreparedEventKind::SPREAD_ORDER ||
                  binding.kind == PreparedEventKind::ANALYTIC_ORDER ||
                  binding.kind == PreparedEventKind::OTC_MARKETS_ORDER || binding.kind == PreparedEventKind::SERIES ||
                  binding.kind == PreparedEventKind::OPTION_SALE) &&
                 family != core::SubscriptionFamily::HISTORY) ||
                (binding.kind == PreparedEventKind::ORDER_IMBALANCE && family != core::SubscriptionFamily::TICKER)) {
                return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});
            }

            const auto &record = schema.records()[static_cast<std::size_t>(binding.localId)];

            if ((binding.kind == PreparedEventKind::GREEKS || binding.kind == PreparedEventKind::UNDERLYING ||
                 binding.kind == PreparedEventKind::THEO_PRICE || binding.kind == PreparedEventKind::CANDLE) &&
                (family == core::SubscriptionFamily::STREAM ||
                 (family == core::SubscriptionFamily::HISTORY && !record.hasTime))) {
                return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});
            }

            if ((binding.kind == PreparedEventKind::TIME_AND_SALE ||
                 binding.kind == PreparedEventKind::NUAM_TIME_AND_SALE) &&
                (family == core::SubscriptionFamily::TICKER ||
                 (family == core::SubscriptionFamily::HISTORY && !record.hasTime))) {
                return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});
            }

            if (binding.kind == PreparedEventKind::SPREAD_ORDER || binding.kind == PreparedEventKind::ANALYTIC_ORDER ||
                binding.kind == PreparedEventKind::OTC_MARKETS_ORDER || binding.kind == PreparedEventKind::SERIES ||
                binding.kind == PreparedEventKind::OPTION_SALE) {
                debit(receiverWork.visits, record.lookups.size());

                if (!events::detail::hasOrderIndexCoordinate(record)) {
                    return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});
                }
            }

            if (binding.kind == PreparedEventKind::ORDER_IMBALANCE && record.hasTime) {
                return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});
            }

            for (std::size_t previous = 0; previous < i; ++previous) {
                debit(receiverWork.visits, 1);

                if (bindings[previous].localId == binding.localId) {
                    const auto &other = bindings[previous];
                    const bool native = binding.projection == events::EventProjection::NATIVE;
                    const bool previousNative = other.projection == events::EventProjection::NATIVE;

                    if (native == previousNative) {
                        return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});
                    }

                    const auto nativeKind = native ? binding.kind : other.kind;
                    const auto derivedKind = native ? other.projection : binding.projection;

                    if ((derivedKind == events::EventProjection::QUOTE_ORDERS &&
                         nativeKind != PreparedEventKind::QUOTE) ||
                        (derivedKind == events::EventProjection::MARKET_MAKER_ORDERS &&
                         nativeKind != PreparedEventKind::MARKET_MAKER)) {
                        return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});
                    }
                }
            }

            const auto fields = schema.records()[static_cast<std::size_t>(binding.localId)].fields.size();

            if (fields > receiverLimits.shapeFields || binding.mappingLimits.selections > receiverLimits.selections)
                return std::unexpected(ReceiverFailure{ReceiverError::RESOURCE_LIMIT, i});

            receiverLimits.shapeFields -= fields;
            receiverLimits.selections -= binding.mappingLimits.selections;
        }

        detail::ReceiverPlan plan;
        plan.routes.reserve(bindings.size());

        for (std::size_t i = 0; i < bindings.size(); ++i) {
            bindingIndex = i;
            debit(receiverWork.visits, 1);
            const auto &binding = bindings[i];
            debit(receiverWork.preparationWork, preparationAllowance(binding.mappingLimits));
            const auto fields = schema.records()[static_cast<std::size_t>(binding.localId)].fields.size();

            if (binding.projection != events::EventProjection::NATIVE) {
                auto prepared =
                    events::PreparedDerivedOrder::prepare(schema, binding.localId,
                                                          binding.projection == events::EventProjection::QUOTE_ORDERS
                                                              ? events::DerivedOrderKind::QUOTE
                                                              : events::DerivedOrderKind::MARKET_MAKER,
                                                          binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::MARKET_MAKER) {
                auto prepared = events::PreparedMarketMaker::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::QUOTE) {
                auto prepared = events::PreparedQuote::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared)
                    return std::unexpected(ReceiverFailure{prepared.error(), i});

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::TRADE) {
                auto prepared = events::PreparedTrade::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared)
                    return std::unexpected(ReceiverFailure{prepared.error(), i});

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::TRADE_ETH) {
                auto prepared = events::PreparedTradeETH::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::ORDER) {
                auto prepared = events::PreparedOrder::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::SPREAD_ORDER) {
                auto prepared = events::PreparedSpreadOrder::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::ANALYTIC_ORDER) {
                auto prepared = events::PreparedAnalyticOrder::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::OTC_MARKETS_ORDER) {
                auto prepared =
                    events::PreparedOtcMarketsOrder::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::SERIES) {
                auto prepared = events::PreparedSeries::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::NUAM_TRADE) {
                auto prepared = events::PreparedNuamTrade::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::OPTION_SALE) {
                auto prepared = events::PreparedOptionSale::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::GREEKS) {
                auto prepared = events::PreparedGreeks::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::UNDERLYING) {
                auto prepared = events::PreparedUnderlying::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::THEO_PRICE) {
                auto prepared = events::PreparedTheoPrice::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::NUAM_TIME_AND_SALE) {
                auto prepared =
                    events::PreparedNuamTimeAndSale::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::TIME_AND_SALE) {
                auto prepared = events::PreparedTimeAndSale::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::CANDLE) {
                const auto &record = schema.records()[static_cast<std::size_t>(binding.localId)];
                debit(receiverWork.preparationWork, preparationAllowance(binding.mappingLimits));
                debit(receiverWork.visits, record.lookups.size());
                auto routes = events::PreparedCandleRoutes::prepare(
                    schema, std::span(&binding.localId, 1),
                    {1, binding.mappingLimits.nameUnits, binding.valueLimits.symbolUnits, binding.mappingLimits});

                if (!routes) {
                    return std::visit(
                        [&](auto cause) {
                            return std::unexpected(ReceiverFailure{cause, i});
                        },
                        candleFailure(routes.error()));
                }

                const auto kind = record.name == u"TradeHistory" ? events::CandleRecordKind::TRADE_HISTORY
                                                                 : events::CandleRecordKind::CANDLE;
                auto mapper = events::PreparedCandle::prepare(schema, binding.localId, kind, binding.mappingLimits);

                if (!mapper)
                    return std::unexpected(ReceiverFailure{mapper.error(), i});

                detail::CandleReceiverMapping prepared{binding.localId, std::move(*mapper), std::move(*routes)};
                plan.routes.push_back({schema.recordContext(binding.localId), std::move(prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::ORDER_IMBALANCE) {
                auto prepared = events::PreparedOrderImbalance::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::SUMMARY) {
                auto prepared = events::PreparedSummary::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared)
                    return std::unexpected(ReceiverFailure{prepared.error(), i});

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::MESSAGE) {
                auto prepared = events::PreparedMessage::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections,
                                       binding.attachmentBytes});
            } else if (binding.kind == PreparedEventKind::CONFIGURATION) {
                auto prepared = events::PreparedConfiguration::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared) {
                    return std::unexpected(ReceiverFailure{prepared.error(), i});
                }

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections,
                                       binding.attachmentBytes});
            } else if (binding.kind == PreparedEventKind::TEXT_MESSAGE) {
                auto prepared = events::PreparedTextMessage::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared)
                    return std::unexpected(ReceiverFailure{prepared.error(), i});

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::TEXT_CONFIGURATION) {
                auto prepared =
                    events::PreparedTextConfiguration::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared)
                    return std::unexpected(ReceiverFailure{prepared.error(), i});

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else {
                auto prepared = events::PreparedProfile::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared)
                    return std::unexpected(ReceiverFailure{prepared.error(), i});

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            }
        }

        bindingIndex.reset();
        auto adapter = std::make_shared<detail::SubscriptionReceiver>(std::move(plan), consumer);
        auto handle = queue.open(clientLimits, adapter, queueWork);

        if (!handle)
            return std::unexpected(ReceiverFailure{handle.error(), {}});

        if (observationPlan)
            *observationPlan = adapter->observationPlan();

        return std::move(*handle);
    } catch (ReceiverError error) {
        return std::unexpected(ReceiverFailure{error, bindingIndex});
    } catch (const std::bad_alloc &) {
        return std::unexpected(ReceiverFailure{ReceiverError::ALLOCATION_FAILURE, bindingIndex});
    } catch (const std::length_error &) {
        return std::unexpected(ReceiverFailure{ReceiverError::ALLOCATION_FAILURE, bindingIndex});
    }
}

std::expected<mux::MuxHandle, ReceiverFailure>
openSubscriptionReceiver(mux::MuxQueue &queue, const core::LocalSchema &schema, core::SubscriptionFamily family,
                         core::SubscriptionSelectorPolicy policy, std::span<const PreparedSubscriptionBinding> bindings,
                         const std::shared_ptr<SubscriptionConsumer> &consumer, ReceiverLimits receiverLimits,
                         mux::ClientLimits clientLimits, ReceiverWork &receiverWork, mux::QueueWork &queueWork) {
    return detail::openObservedSubscriptionReceiver(queue, schema, family, policy, bindings, consumer, receiverLimits,
                                                    clientLimits, receiverWork, queueWork, nullptr);
}
} // namespace org::ttldtor::qdcxx::client
