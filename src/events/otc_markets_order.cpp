// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/otc_markets_order.hpp"
#include "order_fields.hpp"
#include "sourced_market_record.hpp"
#include <array>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr std::uint32_t OPEN = 1U;
constexpr std::uint32_t UNSOLICITED = 1U << 1;
constexpr unsigned PRICE_TYPE_SHIFT = 2;
constexpr std::uint32_t PRICE_TYPE_MASK = 3U;
constexpr std::uint32_t SATURATED = 1U << 4;
constexpr std::uint32_t AUTO_EXECUTION = 1U << 5;
constexpr std::uint32_t NMS_CONDITIONAL = 1U << 6;
constexpr std::size_t PAYMENT_SELECTION = detail::order_fields::SELECTION_COUNT;
constexpr std::size_t OTC_FLAGS_SELECTION = PAYMENT_SELECTION + 1;

/** Extend one validated common projection with the two required OTC fields.
 * @tparam Field Owning or borrowed canonical field representation.
 * @param symbol Exact event symbol borrowed through return.
 * @param fields Complete validated local-order fields.
 * @param selections Exact common and OTC selections.
 * @param sourceId Validated named-record source.
 * @param limits Output symbol and market maker limits.
 * @param eventTime Independent FIELD milliseconds.
 * @param eventFlags Indexed delivery flags.
 * @return Complete owning OTC event or original refusal.
 */
template <class Field>
std::expected<OtcMarketsOrder, MappingError> project(std::u16string_view symbol, std::span<const Field> fields,
                                                     std::span<const std::optional<std::size_t>> selections,
                                                     std::int32_t sourceId, EventValueLimits limits,
                                                     std::int64_t eventTime, std::int32_t eventFlags) {
    auto order =
        detail::order_fields::projectOrder(symbol, fields, selections, sourceId, limits, eventTime, eventFlags);

    if (!order) {
        return std::unexpected(order.error());
    }

    const auto &payment = fields[*selections[PAYMENT_SELECTION]];
    const auto &flags = fields[*selections[OTC_FLAGS_SELECTION]];

    return OtcMarketsOrder{std::move(*order), detail::projectPhysicalInt(payment.type, payment.raw),
                           detail::projectPhysicalInt(flags.type, flags.raw)};
}
} // namespace

std::int64_t OtcMarketsOrder::getIndex() const noexcept {
    return order.getIndex();
}

std::int32_t OtcMarketsOrder::getSourceId() const noexcept {
    return order.getSourceId();
}

OrderSource OtcMarketsOrder::getSource() const {
    return order.getSource();
}

std::int32_t OtcMarketsOrder::getQuoteAccessPayment() const noexcept {
    return quoteAccessPayment;
}

bool OtcMarketsOrder::isOpen() const noexcept {
    return (static_cast<std::uint32_t>(otcMarketsFlags) & OPEN) != 0;
}

bool OtcMarketsOrder::isUnsolicited() const noexcept {
    return (static_cast<std::uint32_t>(otcMarketsFlags) & UNSOLICITED) != 0;
}

OtcMarketsPriceType OtcMarketsOrder::getOtcMarketsPriceType() const noexcept {
    const auto code = (static_cast<std::uint32_t>(otcMarketsFlags) >> PRICE_TYPE_SHIFT) & PRICE_TYPE_MASK;

    return marketEnumValue(marketEnumFromCode<OtcMarketsPriceType>(code)).value_or(OtcMarketsPriceType::UNPRICED);
}

bool OtcMarketsOrder::isSaturated() const noexcept {
    return (static_cast<std::uint32_t>(otcMarketsFlags) & SATURATED) != 0;
}

bool OtcMarketsOrder::isAutoExecution() const noexcept {
    return (static_cast<std::uint32_t>(otcMarketsFlags) & AUTO_EXECUTION) != 0;
}

bool OtcMarketsOrder::isNmsConditional() const noexcept {
    return (static_cast<std::uint32_t>(otcMarketsFlags) & NMS_CONDITIONAL) != 0;
}

std::expected<core::LocalSchema, core::SchemaError> defaultOtcMarketsOrderSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array names{u"Index", u"Time", u"Sequence",           u"Price",          u"Size", u"Count",
                               u"Flags", u"MMID", u"QuoteAccessPayment", u"OtcMarketsFlags"};
    constexpr std::array types{Type::COMPACT_INT,  Type::TIME_SECONDS, Type::SEQUENCE,    Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::COMPACT_INT, Type::SHORT_STRING,
                               Type::COMPACT_INT,  Type::COMPACT_INT};
    constexpr std::size_t COUNT_POSITION = 5;
    constexpr std::size_t MAKER_POSITION = 7;

    try {
        std::array records{core::LocalRecord{0, u"OtcMarketsOrder", true, {}, {}, core::IndexedTimeLowInt32{0}},
                           core::LocalRecord{1, u"OtcMarketsOrder#pink", true, {}, {}, core::IndexedTimeLowInt32{0}}};

        for (auto &record : records) {
            for (std::size_t i = 0; i < names.size(); ++i) {
                if (i == COUNT_POSITION && record.id != 0) {
                    continue;
                }

                const auto field = record.fields.size();
                record.fields.push_back({std::to_underlying(types[i])});
                record.lookups.push_back({names[i], field});

                if (i == MAKER_POSITION) {
                    record.lookups.push_back({u"MarketMaker", field});
                }
            }
        }

        return core::LocalSchema::create(records, limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedOtcMarketsOrder::PreparedOtcMarketsOrder(EventMapping mapping, std::int32_t sourceId) noexcept
    : mapping_(std::move(mapping)), sourceId_(sourceId) {
}

std::expected<PreparedOtcMarketsOrder, MappingError>
PreparedOtcMarketsOrder::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    const auto source = detail::prepareSourcedMarketRecord(schema, localId, u"OtcMarketsOrder", limits);

    if (!source) {
        return std::unexpected(source.error());
    }

    constexpr std::array<std::u16string_view, 1> payment{u"QuoteAccessPayment"};
    constexpr std::array<std::u16string_view, 1> flags{u"OtcMarketsFlags"};
    const std::array extra{FieldRequest{payment, detail::RAW_INT_TYPES, true},
                           FieldRequest{flags, detail::RAW_INT_TYPES, true}};
    auto mapping = detail::order_fields::prepare(schema, localId, limits, extra);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedOtcMarketsOrder(std::move(*mapping), *source);
}

const core::RecordContext &PreparedOtcMarketsOrder::context() const noexcept {
    return mapping_.context();
}

std::int32_t PreparedOtcMarketsOrder::sourceId() const noexcept {
    return sourceId_;
}

std::expected<OtcMarketsOrder, MappingError>
PreparedOtcMarketsOrder::map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits,
                             std::int64_t eventTime, std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validate(values); !valid) {
        return std::unexpected(valid.error());
    }

    return project(symbol, values.values, mapping_.selections(), sourceId_, limits, eventTime, eventFlags);
}

std::expected<OtcMarketsOrder, MappingError>
PreparedOtcMarketsOrder::mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
                                 std::int64_t eventTime, std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validateView(values); !valid) {
        return std::unexpected(valid.error());
    }

    return project(symbol, values.values, mapping_.selections(), sourceId_, limits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
