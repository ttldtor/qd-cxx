// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/nuam_order.hpp"
#include "order_fields.hpp"
#include "order_record_source.hpp"
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr std::size_t BASE_FIELDS = detail::order_fields::SELECTION_COUNT;
enum class Extra : std::size_t {
    ACTOR,
    PARTICIPANT,
    SUBMITTER,
    ON_BEHALF,
    CLIENT_ID,
    ACCOUNT,
    CUSTOMER_INFO,
    EXCHANGE_INFO,
    TIME_IN_FORCE_DATA,
    TRIGGER_BOOK,
    TRIGGER_PRICE,
    TRIGGER_SESSION,
    ORDER_QUANTITY,
    DISPLAY_QUANTITY,
    REFRESH_QUANTITY,
    LEAVES_QUANTITY,
    MATCHED_QUANTITY,
    FLAGS
};

constexpr std::size_t index(Extra field) noexcept {
    return BASE_FIELDS + std::to_underlying(field);
}

constexpr std::array<std::u16string_view, 18> KEYS{
    u"ActorId",         u"ParticipantId",      u"SubmitterId",   u"OnBehalfOfSubmitterId", u"ClientOrderId",
    u"CustomerAccount", u"CustomerInfo",       u"ExchangeInfo",  u"TimeInForceData",       u"TriggerOrderBookId",
    u"TriggerPrice",    u"TriggerSessionType", u"OrderQuantity", u"DisplayQuantity",       u"RefreshQuantity",
    u"LeavesQuantity",  u"MatchedQuantity",    u"NuamFlags"};
constexpr std::array STRING_TYPES{core::SerialFieldType::UTF_CHAR_ARRAY, core::SerialFieldType::STRING};
constexpr std::array NUMERIC{Extra::TRIGGER_PRICE,    Extra::ORDER_QUANTITY,  Extra::DISPLAY_QUANTITY,
                             Extra::REFRESH_QUANTITY, Extra::LEAVES_QUANTITY, Extra::MATCHED_QUANTITY};

template <class Field>
std::expected<NuamOrder, MappingError>
project(std::u16string_view symbol, std::span<const Field> fields, std::span<const std::optional<std::size_t>> selected,
        std::int32_t sourceId, EventValueLimits limits, std::int64_t eventTime, std::int32_t eventFlags) {
    if (symbol.size() > limits.symbolUnits || symbol.size() > limits.totalUnits)
        return std::unexpected(MappingError::RESOURCE_LIMIT);

    auto remaining = limits.totalUnits - symbol.size();
    constexpr std::array textFields{Extra::CLIENT_ID, Extra::ACCOUNT, Extra::CUSTOMER_INFO, Extra::EXCHANGE_INFO};

    for (const auto field : textFields) {
        const auto &value = fields[*selected[index(field)]];
        const auto length = value.units ? value.units->size() : 0;

        if (length > limits.stringUnits || length > remaining)
            return std::unexpected(MappingError::RESOURCE_LIMIT);

        remaining -= length;
    }

    // NuamOrder's Java delegate never reads a market-maker field, even when an unrelated extra exists.
    std::array<std::optional<std::size_t>, BASE_FIELDS> baseSelections{};
    std::ranges::copy(selected.first(BASE_FIELDS), baseSelections.begin());
    baseSelections[detail::order_fields::fieldIndex(detail::order_fields::OrderField::MARKET_MAKER)].reset();
    auto base =
        detail::order_fields::projectOrder(symbol, fields, baseSelections, sourceId, limits, eventTime, eventFlags);

    if (!base)
        return std::unexpected(base.error());

    try {
        NuamOrder result;
        static_cast<Order &>(result) = std::move(*base);
        const auto raw = [&](Extra field) {
            const auto &value = fields[*selected[index(field)]];

            return detail::projectPhysicalInt(value.type, value.raw);
        };
        result.actorId = raw(Extra::ACTOR);
        result.participantId = raw(Extra::PARTICIPANT);
        result.submitterId = raw(Extra::SUBMITTER);
        result.onBehalfOfSubmitterId = raw(Extra::ON_BEHALF);
        result.timeInForceData = raw(Extra::TIME_IN_FORCE_DATA);
        result.triggerOrderBookId = raw(Extra::TRIGGER_BOOK);
        result.triggerSessionType = raw(Extra::TRIGGER_SESSION);
        result.nuamFlags = raw(Extra::FLAGS);
        const std::array textTargets{&result.clientOrderId, &result.customerAccount, &result.customerInfo,
                                     &result.exchangeInfo};

        for (std::size_t i = 0; i < textFields.size(); ++i) {
            const auto &value = fields[*selected[index(textFields[i])]];

            if (value.units)
                textTargets[i]->emplace(*value.units);
        }

        const std::array numericTargets{&result.triggerPrice,    &result.orderQuantity,  &result.displayQuantity,
                                        &result.refreshQuantity, &result.leavesQuantity, &result.matchedQuantity};
        std::array<detail::BusinessNumericSnapshot, NUMERIC.size()> snapshots{};

        for (std::size_t i = 0; i < NUMERIC.size(); ++i) {
            const auto &value = fields[*selected[index(NUMERIC[i])]];
            *numericTargets[i] = detail::projectPresentDecimal(value.type, value.raw);
            snapshots[i] = {numericTargets[i]->raw, detail::projectPresentBusinessDouble(value.type, value.raw)};
        }

        result.nuamNumericSnapshot = snapshots;

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace

NuamOrderType NuamOrder::getOrderType() const noexcept {
    const auto code = static_cast<std::uint32_t>(nuamFlags) & 0xf;

    return code <= std::to_underlying(NuamOrderType::IMBALANCE) ? static_cast<NuamOrderType>(code)
                                                                : NuamOrderType::UNDEFINED;
}

NuamTimeInForceType NuamOrder::getTimeInForce() const noexcept {
    const auto code = (static_cast<std::uint32_t>(nuamFlags) >> 4) & 0xf;

    return code <= std::to_underlying(NuamTimeInForceType::NUMBER_OF_DAYS) ? static_cast<NuamTimeInForceType>(code)
                                                                           : NuamTimeInForceType::UNDEFINED;
}

double NuamOrder::getTriggerPrice() const noexcept {
    return detail::businessValue(triggerPrice, nuamNumericSnapshot, 0);
}

double NuamOrder::getOrderQuantity() const noexcept {
    return detail::businessValue(orderQuantity, nuamNumericSnapshot, 1);
}

double NuamOrder::getDisplayQuantity() const noexcept {
    return detail::businessValue(displayQuantity, nuamNumericSnapshot, 2);
}

double NuamOrder::getRefreshQuantity() const noexcept {
    return detail::businessValue(refreshQuantity, nuamNumericSnapshot, 3);
}

double NuamOrder::getLeavesQuantity() const noexcept {
    return detail::businessValue(leavesQuantity, nuamNumericSnapshot, 4);
}

double NuamOrder::getMatchedQuantity() const noexcept {
    return detail::businessValue(matchedQuantity, nuamNumericSnapshot, 5);
}

std::expected<core::LocalSchema, core::SchemaError> defaultNuamOrderSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array baseNames{u"Index", u"Time", u"Sequence", u"Price", u"Size", u"Flags"};
    constexpr std::array baseTypes{Type::COMPACT_INT,  Type::TIME_SECONDS, Type::SEQUENCE,
                                   Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::COMPACT_INT};
    try {
        core::LocalRecord record{0, u"NuamOrder", true, {}, {}, core::IndexedTimeLowInt32{0}};

        for (std::size_t i = 0; i < baseNames.size(); ++i) {
            record.fields.push_back({std::to_underlying(baseTypes[i])});
            record.lookups.push_back({baseNames[i], i});
        }

        for (std::size_t i = 0; i < KEYS.size(); ++i) {
            const bool text =
                i >= std::to_underlying(Extra::CLIENT_ID) && i <= std::to_underlying(Extra::EXCHANGE_INFO);
            const bool number = std::ranges::find(NUMERIC, static_cast<Extra>(i)) != NUMERIC.end();
            const auto type = text ? Type::UTF_CHAR_ARRAY : number ? Type::WIDE_DECIMAL : Type::COMPACT_INT;
            record.lookups.push_back({std::u16string(KEYS[i]), record.fields.size()});
            record.fields.push_back({std::to_underlying(type)});
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedNuamOrder::PreparedNuamOrder(EventMapping mapping, std::int32_t sourceId) noexcept
    : mapping_(std::move(mapping)), sourceId_(sourceId) {
}

std::expected<PreparedNuamOrder, MappingError> PreparedNuamOrder::prepare(const core::LocalSchema &schema,
                                                                          std::int32_t localId, MappingLimits limits) {
    if (!schema.recordContext(localId).valid())
        return std::unexpected(MappingError::INVALID_INPUT);

    auto source = detail::prepareRecordOrderSource(schema.records()[static_cast<std::size_t>(localId)].name, limits);

    if (!source)
        return std::unexpected(source.error());

    std::array<FieldRequest, KEYS.size()> requests{};

    for (std::size_t i = 0; i < KEYS.size(); ++i) {
        const bool text = i >= std::to_underlying(Extra::CLIENT_ID) && i <= std::to_underlying(Extra::EXCHANGE_INFO);
        const bool number = std::ranges::find(NUMERIC, static_cast<Extra>(i)) != NUMERIC.end();
        std::span<const core::SerialFieldType> types = detail::RAW_INT_TYPES;

        if (text)
            types = STRING_TYPES;
        else if (number)
            types = detail::NUMERIC_TYPES;

        requests[i] = {std::span(KEYS).subspan(i, 1), types, true};
    }

    auto mapping = detail::order_fields::prepare(schema, localId, limits, requests, false);

    if (!mapping)
        return std::unexpected(mapping.error());

    return PreparedNuamOrder(std::move(*mapping), source->id());
}

const core::RecordContext &PreparedNuamOrder::context() const noexcept {
    return mapping_.context();
}

std::int32_t PreparedNuamOrder::sourceId() const noexcept {
    return sourceId_;
}

std::expected<NuamOrder, MappingError> PreparedNuamOrder::map(std::u16string_view symbol, RecordValueView values,
                                                              EventValueLimits limits, std::int64_t eventTime,
                                                              std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validate(values); !valid)
        return std::unexpected(valid.error());

    return project(symbol, values.values, mapping_.selections(), sourceId_, limits, eventTime, eventFlags);
}

std::expected<NuamOrder, MappingError> PreparedNuamOrder::mapView(std::u16string_view symbol,
                                                                  BorrowedRecordValueView values,
                                                                  EventValueLimits limits, std::int64_t eventTime,
                                                                  std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validateView(values); !valid)
        return std::unexpected(valid.error());

    return project(symbol, values.values, mapping_.selections(), sourceId_, limits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
