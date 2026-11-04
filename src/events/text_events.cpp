// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/text_events.hpp"

#include "decimal_projection.hpp"

#include <array>
#include <bit>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr std::uint32_t SEQUENCE_MASK = (1U << 22) - 1;
constexpr unsigned MILLIS_SHIFT = 22;
constexpr std::uint32_t MILLIS_MASK = 0x3ff;
constexpr unsigned WORD_BITS = 32;
constexpr std::int64_t MILLIS_PER_SECOND = 1000;
constexpr std::array<std::u16string_view, 4> NAMES{u"Time", u"Sequence", u"Version", u"Text"};
constexpr std::array TEXT_TYPES{core::SerialFieldType::UTF_CHAR_ARRAY, core::SerialFieldType::STRING};

std::int64_t getTime(std::uint64_t packed) noexcept {
    const auto seconds = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(packed >> WORD_BITS));
    const auto millis = static_cast<std::uint32_t>(packed >> MILLIS_SHIFT) & MILLIS_MASK;

    return static_cast<std::int64_t>(seconds) * MILLIS_PER_SECOND + millis;
}

std::int32_t getSequence(std::uint64_t packed) noexcept {
    return static_cast<std::int32_t>(packed & SEQUENCE_MASK);
}

std::expected<EventMapping, MappingError> prepareMapping(const core::LocalSchema &schema, std::int32_t localId,
                                                         MappingLimits limits, bool configuration) {
    std::array<FieldRequest, 4> requests{};

    for (std::size_t i = 0; i < requests.size(); ++i) {
        requests[i] = {std::span(&NAMES[i], 1),
                       i == 3 ? std::span<const core::SerialFieldType>(TEXT_TYPES)
                              : std::span<const core::SerialFieldType>(detail::RAW_INT_TYPES),
                       i == 3 || (!configuration && i < 2)};
    }

    if (!configuration) {
        requests[2] = requests[3];

        return EventMapping::prepare(schema, localId, std::span(requests).first(3), limits);
    }

    return EventMapping::prepare(schema, localId, requests, limits);
}

template <typename Event, typename View>
std::expected<Event, MappingError> project(const EventMapping &mapping, std::u16string_view symbol, View values,
                                           EventValueLimits limits, std::int64_t eventTime) {
    const auto valid = [&] {
        if constexpr (std::is_same_v<View, RecordValueView>)
            return mapping.validate(values);
        else
            return mapping.validateView(values);
    }();

    if (!valid) {
        return std::unexpected(valid.error());
    }

    const auto selections = mapping.selections();
    const auto &text = values.values[*selections[std::is_same_v<Event, TextConfiguration> ? 3 : 2]].units;
    const auto textUnits = text ? text->size() : 0;

    if (symbol.size() > limits.symbolUnits || textUnits > limits.stringUnits || symbol.size() > limits.totalUnits ||
        textUnits > limits.totalUnits - symbol.size()) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    const auto raw = [&](std::size_t index) -> std::int32_t {
        if (!selections[index]) {
            return 0;
        }

        const auto &field = values.values[*selections[index]];

        return detail::projectPhysicalInt(field.type, field.raw);
    };
    const auto seconds = static_cast<std::uint32_t>(raw(0));
    const auto sequence = static_cast<std::uint32_t>(raw(1));
    try {
        Event result;
        result.symbol = std::u16string(symbol);
        result.eventTime = eventTime;
        result.timeSequence = (static_cast<std::uint64_t>(seconds) << WORD_BITS) | sequence;

        if constexpr (std::is_same_v<Event, TextConfiguration>) {
            result.version = raw(2);
        }

        if (text) {
            result.text.emplace(*text);
        }

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace

namespace {
std::expected<core::LocalSchema, core::SchemaError> defaultSchema(core::SchemaLimits limits, bool configuration) {
    try {
        const std::u16string name = configuration ? u"TextConfiguration" : u"TextMessage";
        core::LocalRecord record{0, name, false, {}, {}};
        const std::array types{core::SerialFieldType::TIME_SECONDS, core::SerialFieldType::SEQUENCE,
                               core::SerialFieldType::COMPACT_INT, core::SerialFieldType::UTF_CHAR_ARRAY};

        for (std::size_t i = 0; i < NAMES.size(); ++i) {
            if (!configuration && i == 2) {
                continue;
            }

            const auto index = record.fields.size();
            record.fields.push_back({std::to_underlying(types[i])});
            record.lookups.push_back({std::u16string(NAMES[i]), index});
            record.lookups.push_back({name + u"." + std::u16string(NAMES[i]), index});
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}
} // namespace

std::expected<core::LocalSchema, core::SchemaError> defaultTextMessageSchema(core::SchemaLimits limits) {
    return defaultSchema(limits, false);
}

std::expected<core::LocalSchema, core::SchemaError> defaultTextConfigurationSchema(core::SchemaLimits limits) {
    return defaultSchema(limits, true);
}

std::int64_t TextMessage::getTime() const noexcept {
    return events::getTime(timeSequence);
}

std::int32_t TextMessage::getSequence() const noexcept {
    return events::getSequence(timeSequence);
}

std::int64_t TextConfiguration::getTime() const noexcept {
    return events::getTime(timeSequence);
}

std::int32_t TextConfiguration::getSequence() const noexcept {
    return events::getSequence(timeSequence);
}

PreparedTextMessage::PreparedTextMessage(EventMapping mapping) : mapping_(std::move(mapping)) {
}

std::expected<PreparedTextMessage, MappingError>
PreparedTextMessage::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    auto mapping = prepareMapping(schema, localId, limits, false);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedTextMessage(std::move(*mapping));
}

const core::RecordContext &PreparedTextMessage::context() const noexcept {
    return mapping_.context();
}

std::expected<TextMessage, MappingError> PreparedTextMessage::map(std::u16string_view symbol, RecordValueView values,
                                                                  EventValueLimits limits,
                                                                  std::int64_t eventTime) const {
    return project<TextMessage>(mapping_, symbol, values, limits, eventTime);
}

std::expected<TextMessage, MappingError> PreparedTextMessage::mapView(std::u16string_view symbol,
                                                                      BorrowedRecordValueView values,
                                                                      EventValueLimits limits,
                                                                      std::int64_t eventTime) const {
    return project<TextMessage>(mapping_, symbol, values, limits, eventTime);
}

PreparedTextConfiguration::PreparedTextConfiguration(EventMapping mapping) : mapping_(std::move(mapping)) {
}

std::expected<PreparedTextConfiguration, MappingError>
PreparedTextConfiguration::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    auto mapping = prepareMapping(schema, localId, limits, true);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedTextConfiguration(std::move(*mapping));
}

const core::RecordContext &PreparedTextConfiguration::context() const noexcept {
    return mapping_.context();
}

std::optional<std::size_t> PreparedTextConfiguration::versionField() const noexcept {
    return mapping_.selections()[2];
}

std::expected<TextConfiguration, MappingError> PreparedTextConfiguration::map(std::u16string_view symbol,
                                                                              RecordValueView values,
                                                                              EventValueLimits limits,
                                                                              std::int64_t eventTime) const {
    return project<TextConfiguration>(mapping_, symbol, values, limits, eventTime);
}

std::expected<TextConfiguration, MappingError> PreparedTextConfiguration::mapView(std::u16string_view symbol,
                                                                                  BorrowedRecordValueView values,
                                                                                  EventValueLimits limits,
                                                                                  std::int64_t eventTime) const {
    return project<TextConfiguration>(mapping_, symbol, values, limits, eventTime);
}
} // namespace org::ttldtor::qdcxx::events
