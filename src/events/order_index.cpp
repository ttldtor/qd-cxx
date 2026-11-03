// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/order_index.hpp"

#include <array>
#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr unsigned GENERIC_SOURCE_SHIFT = 32;            ///< Source above the unsigned wire Index.
constexpr unsigned SPECIAL_SOURCE_SHIFT = 48;            ///< Wrapped-event source above exchange and wire Index.
constexpr std::uint64_t WIRE_INDEX_MASK = 0xffffffffULL; ///< Unsigned low wire Index bits.
constexpr std::uint64_t SPECIAL_PAYLOAD_MASK = 0xffffffffffffULL; ///< Exchange plus wire Index bits.

/** Extract a Java signed high word without relying on signed right-shift conversion.
 * @param bits Complete unsigned index representation.
 * @return Signed high 16 bits, sign extended to int32.
 */
std::int32_t highSpecialWord(std::uint64_t bits) noexcept {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits >> SPECIAL_SOURCE_SHIFT));
}

/** Pack the production order delegate's exact generic record-source identity.
 * @param sourceId Resolved generic/default record source ID.
 * @param wireIndex Already validated signed32 local value.
 * @return Complete raw 64-bit event identity, without allocating.
 */
std::int64_t packRecordIndex(std::int32_t sourceId, std::int64_t wireIndex) noexcept {
    const auto sourceBits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(sourceId));
    const auto indexBits = static_cast<std::uint64_t>(wireIndex) & WIRE_INDEX_MASK;

    return std::bit_cast<std::int64_t>((sourceBits << GENERIC_SOURCE_SHIFT) | indexBits);
}
} // namespace

bool isSpecialOrderSourceId(std::int32_t sourceId) noexcept {
    constexpr std::int32_t FIRST_SPECIAL_SOURCE = 1;
    constexpr std::int32_t LAST_SPECIAL_SOURCE = 9;

    return sourceId >= FIRST_SPECIAL_SOURCE && sourceId <= LAST_SPECIAL_SOURCE;
}

std::int32_t orderIndexSourceId(std::int64_t index) noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(index);
    const auto specialId = highSpecialWord(bits);

    if (isSpecialOrderSourceId(specialId)) {
        return specialId;
    }

    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> GENERIC_SOURCE_SHIFT));
}

OrderSource orderIndexSource(std::int64_t index) {
    return OrderSource::valueOf(orderIndexSourceId(index));
}

std::int64_t changeOrderIndexSource(std::int64_t index, const OrderSource &source) noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(index);
    const auto shift = isSpecialOrderSourceId(source.id()) ? SPECIAL_SOURCE_SHIFT : GENERIC_SOURCE_SHIFT;
    const auto mask = isSpecialOrderSourceId(highSpecialWord(bits)) ? SPECIAL_PAYLOAD_MASK : WIRE_INDEX_MASK;
    const auto sourceBits = static_cast<std::uint64_t>(static_cast<std::int64_t>(source.id()));

    return std::bit_cast<std::int64_t>((sourceBits << shift) | (bits & mask));
}

PreparedOrderSource::PreparedOrderSource(EventMapping mapping, OrderSource source) noexcept
    : mapping_(std::move(mapping)), source_(std::move(source)) {
}

std::expected<PreparedOrderSource, MappingError>
PreparedOrderSource::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    if (!schema.recordContext(localId).valid()) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    const auto &name = schema.records()[static_cast<std::size_t>(localId)].name;

    if (name.size() > limits.nameUnits || name.size() > limits.totalNameUnits || name.size() > limits.comparisonWork) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    limits.totalNameUnits -= name.size();
    limits.comparisonWork -= name.size();

    try {
        const auto separator = name.rfind(u'#');
        auto source = OrderSource::DEFAULT;

        if (separator != std::u16string::npos) {
            const auto suffix = std::u16string_view(name).substr(separator + 1);

            if (suffix.empty() || suffix.size() > 4) {
                return std::unexpected(MappingError::INVALID_INPUT);
            }

            source = OrderSource::valueOf(suffix);
        }

        constexpr std::array<std::u16string_view, 1> KEYS{u"Index"};
        constexpr std::array<core::SerialFieldType, 1> TYPES{core::SerialFieldType::COMPACT_INT};
        const std::array requests{FieldRequest{KEYS, TYPES, true}};
        auto mapping = EventMapping::prepare(schema, localId, requests, limits);

        if (!mapping) {
            return std::unexpected(mapping.error());
        }

        return PreparedOrderSource(std::move(*mapping), std::move(source));
    } catch (const std::invalid_argument &) {
        return std::unexpected(MappingError::INVALID_INPUT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}

const core::RecordContext &PreparedOrderSource::context() const noexcept {
    return mapping_.context();
}

const OrderSource &PreparedOrderSource::source() const noexcept {
    return source_;
}

std::expected<std::int64_t, MappingError> PreparedOrderSource::mapIndex(RecordValueView values) const noexcept {
    if (const auto valid = mapping_.validate(values); !valid) {
        return std::unexpected(valid.error());
    }

    return packRecordIndex(source_.id(), values.values[*mapping_.selections()[0]].raw);
}

std::expected<std::int64_t, MappingError>
PreparedOrderSource::mapIndexView(BorrowedRecordValueView values) const noexcept {
    if (const auto valid = mapping_.validateView(values); !valid) {
        return std::unexpected(valid.error());
    }

    return packRecordIndex(source_.id(), values.values[*mapping_.selections()[0]].raw);
}
} // namespace org::ttldtor::qdcxx::events
