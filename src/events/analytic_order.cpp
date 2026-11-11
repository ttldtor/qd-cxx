// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/analytic_order.hpp"
#include "order_variant_mapping.hpp"

namespace org::ttldtor::qdcxx::events {
double AnalyticOrder::getIcebergPeakSize() const noexcept {
    return detail::businessValue(icebergPeakSize, icebergNumericSnapshot, 0);
}

double AnalyticOrder::getIcebergHiddenSize() const noexcept {
    return detail::businessValue(icebergHiddenSize, icebergNumericSnapshot, 1);
}

double AnalyticOrder::getIcebergExecutedSize() const noexcept {
    return detail::businessValue(icebergExecutedSize, icebergNumericSnapshot, 2);
}

IcebergType AnalyticOrder::getIcebergType() const noexcept {
    constexpr std::uint32_t ICEBERG_TYPE_MASK = 3;
    const auto code = static_cast<std::uint32_t>(icebergFlags) & ICEBERG_TYPE_MASK;

    return code == ICEBERG_TYPE_MASK ? IcebergType::UNDEFINED : static_cast<IcebergType>(code);
}

std::expected<core::LocalSchema, core::SchemaError> defaultAnalyticOrderSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array names{u"Index", u"Time", u"Sequence", u"Price", u"Size", u"Count", u"Flags", u"MMID"};
    constexpr std::array types{Type::COMPACT_INT,  Type::TIME_SECONDS, Type::SEQUENCE,    Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::COMPACT_INT, Type::SHORT_STRING};
    try {
        core::LocalRecord record{0, u"AnalyticOrder", true, {}, {}, core::IndexedTimeLowInt32{0}};

        for (std::size_t i = 0; i < names.size(); ++i) {
            record.fields.push_back({std::to_underlying(types[i])});
            record.lookups.push_back({names[i], i});
        }

        record.lookups.push_back({u"MarketMaker", 7});
        const std::array records{std::move(record)};

        return core::LocalSchema::create(records, limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedAnalyticOrder::PreparedAnalyticOrder(EventMapping mapping, std::int32_t sourceId) noexcept
    : mapping_(std::move(mapping)), sourceId_(sourceId) {
}

std::expected<PreparedAnalyticOrder, MappingError>
PreparedAnalyticOrder::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    if (!schema.recordContext(localId).valid()) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    auto source = detail::prepareRecordOrderSource(schema.records()[static_cast<std::size_t>(localId)].name, limits);

    if (!source) {
        return std::unexpected(source.error());
    }

    auto mapping = detail::prepareVariant<false>(schema, localId, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedAnalyticOrder(std::move(*mapping), source->id());
}

const core::RecordContext &PreparedAnalyticOrder::context() const noexcept {
    return mapping_.context();
}

std::int32_t PreparedAnalyticOrder::sourceId() const noexcept {
    return sourceId_;
}

std::expected<AnalyticOrder, MappingError> PreparedAnalyticOrder::map(std::u16string_view symbol,
                                                                      RecordValueView values, EventValueLimits limits,
                                                                      std::int64_t eventTime,
                                                                      std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validate(values); !valid) {
        return std::unexpected(valid.error());
    }

    return detail::projectVariant<AnalyticOrder, false>(symbol, values.values, mapping_.selections(), sourceId_, limits,
                                                        eventTime, eventFlags);
}

std::expected<AnalyticOrder, MappingError>
PreparedAnalyticOrder::mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
                               std::int64_t eventTime, std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validateView(values); !valid) {
        return std::unexpected(valid.error());
    }

    return detail::projectVariant<AnalyticOrder, false>(symbol, values.values, mapping_.selections(), sourceId_, limits,
                                                        eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
