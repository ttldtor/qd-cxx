// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/profile.hpp"

#include "decimal_projection.hpp"

#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
/// Required Description selection in the declared Profile property order.
constexpr std::size_t DESCRIPTION_INDEX = 14;
/** Measured property lookup names in default logical order. */
constexpr std::array<std::u16string_view, 16> PROPERTY_NAMES{
    u"Beta",        u"Eps",    u"DivFreq",     u"ExdDivAmount",   u"ExdDivDate",    u"HighPrice52",
    u"LowPrice52",  u"Shares", u"FreeFloat",   u"HighLimitPrice", u"LowLimitPrice", u"HaltStartTime",
    u"HaltEndTime", u"Flags",  u"Description", u"StatusReason"};
/** Measured local lookup names in default logical order. */
constexpr std::array<std::u16string_view, 16> LOCAL_NAMES{
    u"Beta",         u"Eps",    u"DivFreq",     u"ExdDiv.Amount",  u"ExdDiv.Date",   u"52High.Price",
    u"52Low.Price",  u"Shares", u"FreeFloat",   u"HighLimitPrice", u"LowLimitPrice", u"Halt.StartTime",
    u"Halt.EndTime", u"Flags",  u"Description", u"StatusReason"};
} // namespace

TradingStatus Profile::tradingStatus() const noexcept {
    constexpr std::uint32_t STATUS_MASK = 3;
    const auto code = static_cast<std::uint32_t>(flags) & STATUS_MASK;

    return code <= std::to_underlying(TradingStatus::ACTIVE) ? static_cast<TradingStatus>(code)
                                                             : TradingStatus::UNDEFINED;
}

ShortSaleRestriction Profile::shortSaleRestriction() const noexcept {
    constexpr unsigned RESTRICTION_SHIFT = 2;
    constexpr std::uint32_t RESTRICTION_MASK = 3;
    const auto code = (static_cast<std::uint32_t>(flags) >> RESTRICTION_SHIFT) & RESTRICTION_MASK;

    return code <= std::to_underlying(ShortSaleRestriction::INACTIVE) ? static_cast<ShortSaleRestriction>(code)
                                                                      : ShortSaleRestriction::UNDEFINED;
}

bool Profile::isTradingHalted() const noexcept {
    return tradingStatus() == TradingStatus::HALTED;
}

bool Profile::isShortSaleRestricted() const noexcept {
    return shortSaleRestriction() == ShortSaleRestriction::ACTIVE;
}

std::expected<core::LocalSchema, core::SchemaError> defaultProfileSchema(core::SchemaLimits limits) {
    try {
        core::LocalRecord record{0, u"Profile", false, {}, {}};

        for (std::size_t i = 0; i < PROFILE_FIELD_TYPES.size(); ++i) {
            record.fields.push_back({std::to_underlying(PROFILE_FIELD_TYPES[i])});
            record.lookups.push_back({std::u16string(LOCAL_NAMES[i]), i});
            record.lookups.push_back({u"Profile." + std::u16string(LOCAL_NAMES[i]), i});

            if (LOCAL_NAMES[i] != PROPERTY_NAMES[i]) {
                record.lookups.push_back({std::u16string(PROPERTY_NAMES[i]), i});
            }
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::EXTENDED_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedProfile::PreparedProfile(EventMapping mapping) : mapping_(std::move(mapping)) {
}

std::expected<PreparedProfile, MappingError> PreparedProfile::prepare(const core::LocalSchema &schema,
                                                                      std::int32_t localId, MappingLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array numericTypes{Type::WIDE_DECIMAL, Type::COMPACT_INT};
    constexpr std::array dateTypes{Type::DATE};
    constexpr std::array timeTypes{Type::TIME_SECONDS};
    constexpr std::array flagTypes{Type::COMPACT_INT};
    constexpr std::array stringTypes{Type::UTF_CHAR_ARRAY};
    std::array<std::array<std::u16string_view, 2>, PROFILE_FIELD_TYPES.size()> keys{};
    std::array<FieldRequest, PROFILE_FIELD_TYPES.size()> requests{};

    for (std::size_t i = 0; i < requests.size(); ++i) {
        keys[i] = {LOCAL_NAMES[i], PROPERTY_NAMES[i]};
        const auto keyCount = LOCAL_NAMES[i] == PROPERTY_NAMES[i] ? 1U : 2U;
        std::span<const Type> types;

        switch (PROFILE_FIELD_TYPES[i]) {
        case Type::WIDE_DECIMAL:
            types = numericTypes;
            break;
        case Type::DATE:
            types = dateTypes;
            break;
        case Type::TIME_SECONDS:
            types = timeTypes;
            break;
        case Type::COMPACT_INT:
            types = flagTypes;
            break;
        default:
            types = stringTypes;
            break;
        }

        requests[i] = {std::span(keys[i]).first(keyCount), types, i == DESCRIPTION_INDEX};
    }

    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedProfile(std::move(*mapping));
}

const core::RecordContext &PreparedProfile::context() const noexcept {
    return mapping_.context();
}

std::expected<Profile, MappingError> PreparedProfile::map(std::u16string_view symbol, RecordValueView values,
                                                          EventValueLimits limits, std::int64_t eventTime) const {
    const auto validation = mapping_.validate(values);

    if (!validation) {
        return std::unexpected(validation.error());
    }

    constexpr std::size_t STATUS_REASON_INDEX = 15;
    constexpr std::int64_t MILLISECONDS_PER_SECOND = 1000;
    const auto selections = mapping_.selections();
    const auto *description = &values.values[*selections[DESCRIPTION_INDEX]].units;
    const auto *statusReason =
        selections[STATUS_REASON_INDEX] ? &values.values[*selections[STATUS_REASON_INDEX]].units : nullptr;

    if (symbol.size() > limits.symbolUnits || symbol.size() > limits.totalUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    auto remainingUnits = limits.totalUnits - symbol.size();

    for (const auto *text : {description, statusReason}) {
        const auto count = text != nullptr && *text ? (*text)->size() : 0;

        if (count > limits.stringUnits || count > remainingUnits) {
            return std::unexpected(MappingError::RESOURCE_LIMIT);
        }

        remainingUnits -= count;
    }

    const auto raw = [&](std::size_t selection) -> std::int64_t {
        return selections[selection] ? values.values[*selections[selection]].raw : 0;
    };
    const auto decimal = [&](std::size_t selection) -> core::WideDecimal {
        if (!selections[selection]) {
            return {0};
        }

        const auto &field = values.values[*selections[selection]];

        return detail::projectPresentDecimal(field);
    };
    try {
        // Selection numbers follow the default declarations, independently of schema field positions.
        return Profile{std::u16string(symbol),
                       decimal(0),
                       decimal(1),
                       decimal(2),
                       decimal(3),
                       static_cast<std::int32_t>(raw(4)),
                       decimal(5),
                       decimal(6),
                       decimal(7),
                       decimal(8),
                       decimal(9),
                       decimal(10),
                       raw(11) * MILLISECONDS_PER_SECOND,
                       raw(12) * MILLISECONDS_PER_SECOND,
                       static_cast<std::int32_t>(raw(13)),
                       *description,
                       statusReason != nullptr ? *statusReason : std::nullopt,
                       eventTime};
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace org::ttldtor::qdcxx::events
