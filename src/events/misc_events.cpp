// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "decimal_projection.hpp"
#include "qd/events/configuration.hpp"
#include <array>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr std::array VERSION_TYPES{
    core::SerialFieldType::VOID_FIELD,   core::SerialFieldType::BYTE,         core::SerialFieldType::UTF_CHAR,
    core::SerialFieldType::SHORT,        core::SerialFieldType::INT,          core::SerialFieldType::COMPACT_INT,
    core::SerialFieldType::DECIMAL,      core::SerialFieldType::SHORT_STRING, core::SerialFieldType::TIME_SECONDS,
    core::SerialFieldType::SEQUENCE,     core::SerialFieldType::DATE,         core::SerialFieldType::LONG,
    core::SerialFieldType::WIDE_DECIMAL, core::SerialFieldType::TIME_MILLIS};

template <class Event, class Field>
std::expected<Event, MappingError> project(std::u16string_view symbol, std::span<const Field> fields,
                                           std::span<const std::optional<std::size_t>> selections,
                                           AttachmentEventLimits limits, std::int64_t time) {
    const auto &payload = fields[*selections[0]];
    const auto bodyBytes = payload.bytes ? payload.bytes->size() : 0;

    if (symbol.size() > limits.symbolUnits || bodyBytes > limits.payloadBytes || bodyBytes > limits.totalBytes ||
        symbol.size() > (limits.totalBytes - bodyBytes) / sizeof(char16_t)) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    std::optional<std::span<const std::byte>> view;

    if (payload.bytes) {
        view = std::span<const std::byte>(*payload.bytes);
    }

    auto attachment = SerializedAttachment::create(payload.type, view, limits.payloadBytes);

    if (!attachment) {
        return std::unexpected(attachment.error() == AttachmentError::RESOURCE_LIMIT
                                   ? MappingError::RESOURCE_LIMIT
                                   : MappingError::ALLOCATION_FAILURE);
    }

    try {
        Event result;
        result.symbol = symbol;
        result.eventTime = time;
        result.attachment = std::move(*attachment);

        if constexpr (std::is_same_v<Event, Configuration>) {
            if (selections[1]) {
                const auto &version = fields[*selections[1]];
                result.version = detail::projectPhysicalInt(version.type, version.raw);
            }
        }

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}

std::expected<EventMapping, MappingError> prepare(const core::LocalSchema &schema, std::int32_t id,
                                                  MappingLimits limits, bool configuration) {
    constexpr std::array<std::u16string_view, 1> message{u"Message"};
    constexpr std::array<std::u16string_view, 1> config{u"Configuration"};
    constexpr std::array<std::u16string_view, 1> version{u"Version"};
    constexpr std::array payloadType{core::SerialFieldType::SERIAL_OBJECT};
    const std::array requests{FieldRequest{configuration ? config : message, payloadType, true},
                              FieldRequest{version, VERSION_TYPES, false}};

    return EventMapping::prepare(schema, id, std::span(requests).first(configuration ? 2 : 1), limits);
}

std::expected<core::LocalSchema, core::SchemaError> defaultSchema(core::SchemaLimits limits, bool configuration) {
    try {
        core::LocalRecord record{0, configuration ? u"Configuration" : u"Message", false, {}, {}};

        if (configuration) {
            record.fields.push_back({8});
            record.lookups.push_back({u"Version", 0});
        }

        record.lookups.push_back({configuration ? u"Configuration" : u"Message", record.fields.size()});
        record.fields.push_back({249});

        return core::LocalSchema::create(std::array{std::move(record)}, limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}
} // namespace

const SerializedAttachment &Message::getAttachment() const noexcept {
    return attachment;
}

std::int64_t Message::getEventTime() const noexcept {
    return eventTime;
}

const SerializedAttachment &Configuration::getAttachment() const noexcept {
    return attachment;
}

std::int32_t Configuration::getVersion() const noexcept {
    return version;
}

std::int64_t Configuration::getEventTime() const noexcept {
    return eventTime;
}

std::expected<core::LocalSchema, core::SchemaError> defaultMessageSchema(core::SchemaLimits limits) {
    return defaultSchema(limits, false);
}

std::expected<core::LocalSchema, core::SchemaError> defaultConfigurationSchema(core::SchemaLimits limits) {
    return defaultSchema(limits, true);
}

PreparedMessage::PreparedMessage(EventMapping mapping) : mapping_(std::move(mapping)) {
}

PreparedConfiguration::PreparedConfiguration(EventMapping mapping) : mapping_(std::move(mapping)) {
}

std::expected<PreparedMessage, MappingError> PreparedMessage::prepare(const core::LocalSchema &schema,
                                                                      std::int32_t localId, MappingLimits limits) {
    auto mapping = events::prepare(schema, localId, limits, false);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedMessage{std::move(*mapping)};
}

std::expected<PreparedConfiguration, MappingError>
PreparedConfiguration::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    auto mapping = events::prepare(schema, localId, limits, true);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedConfiguration{std::move(*mapping)};
}

const core::RecordContext &PreparedMessage::context() const noexcept {
    return mapping_.context();
}

const core::RecordContext &PreparedConfiguration::context() const noexcept {
    return mapping_.context();
}

std::optional<std::size_t> PreparedConfiguration::versionField() const noexcept {
    return mapping_.selections()[1];
}

std::expected<Message, MappingError> PreparedMessage::map(std::u16string_view symbol, RecordValueView values,
                                                          AttachmentEventLimits limits, std::int64_t eventTime) const {
    const auto valid = mapping_.validate(values);

    if (!valid) {
        return std::unexpected(valid.error());
    }

    return project<Message>(symbol, values.values, mapping_.selections(), limits, eventTime);
}

std::expected<Message, MappingError> PreparedMessage::mapView(std::u16string_view symbol,
                                                              BorrowedRecordValueView values,
                                                              AttachmentEventLimits limits,
                                                              std::int64_t eventTime) const {
    const auto valid = mapping_.validateView(values);

    if (!valid) {
        return std::unexpected(valid.error());
    }

    return project<Message>(symbol, values.values, mapping_.selections(), limits, eventTime);
}

std::expected<Configuration, MappingError> PreparedConfiguration::map(std::u16string_view symbol,
                                                                      RecordValueView values,
                                                                      AttachmentEventLimits limits,
                                                                      std::int64_t eventTime) const {
    const auto valid = mapping_.validate(values);

    if (!valid) {
        return std::unexpected(valid.error());
    }

    return project<Configuration>(symbol, values.values, mapping_.selections(), limits, eventTime);
}

std::expected<Configuration, MappingError> PreparedConfiguration::mapView(std::u16string_view symbol,
                                                                          BorrowedRecordValueView values,
                                                                          AttachmentEventLimits limits,
                                                                          std::int64_t eventTime) const {
    const auto valid = mapping_.validateView(values);

    if (!valid) {
        return std::unexpected(valid.error());
    }

    return project<Configuration>(symbol, values.values, mapping_.selections(), limits, eventTime);
}
} // namespace org::ttldtor::qdcxx::events
