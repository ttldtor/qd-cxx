#include "qd/core/local_schema.hpp"

#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace qd::core {
namespace {

/** Check the local subset's nonempty, well-formed UTF-16 requirement.
 * @param name Borrowed record or lookup name.
 * @return Whether every surrogate belongs to a complete pair.
 */
bool validName(std::u16string_view name) {
    constexpr char16_t HIGH_START = 0xd800;
    constexpr char16_t LOW_START = 0xdc00;
    constexpr char16_t SURROGATE_END = 0xdfff;

    if (name.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < name.size(); ++i) {
        if (name[i] >= HIGH_START && name[i] < LOW_START) {
            if (++i == name.size() || name[i] < LOW_START || name[i] > SURROGATE_END) {
                return false;
            }
        } else if (name[i] >= LOW_START && name[i] <= SURROGATE_END) {
            return false;
        }
    }

    return true;
}

/** Charge a logical count without overflowing arithmetic.
 * @param count Requested logical elements.
 * @param remaining Remaining caller budget, decreased only on success.
 * @return Whether the request fits.
 */
bool charge(std::size_t count, std::size_t &remaining) {
    if (count > remaining) {
        return false;
    }

    remaining -= count;

    return true;
}

} // namespace

LocalSchema::LocalSchema(std::span<const LocalRecord> records) : records_(records.begin(), records.end()) {
}

std::span<const LocalRecord> LocalSchema::records() const noexcept {
    return records_;
}

std::expected<LocalSchema, SchemaError> LocalSchema::create(std::span<const LocalRecord> records, SchemaLimits limits) {
    if (records.size() > limits.records) {
        return std::unexpected(SchemaError::RESOURCE_LIMIT);
    }

    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto &record = records[i];

        if (i > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
            record.id != static_cast<std::int32_t>(i) || !validName(record.name)) {
            return std::unexpected(SchemaError::INVALID_SCHEMA);
        }

        if (record.name.size() > limits.nameUnits || !charge(record.name.size(), limits.totalNameUnits)) {
            return std::unexpected(SchemaError::RESOURCE_LIMIT);
        }

        for (std::size_t j = 0; j < i; ++j) {
            if (records[j].name == record.name) {
                return std::unexpected(SchemaError::INVALID_SCHEMA);
            }
        }

        if (record.hasTime) {
            return std::unexpected(SchemaError::UNSUPPORTED_PROFILE);
        }

        if (!charge(record.fields.size(), limits.fields) || !charge(record.lookups.size(), limits.lookups)) {
            return std::unexpected(SchemaError::RESOURCE_LIMIT);
        }

        for (const auto &field : record.fields) {
            if (field.serialTypeId != std::to_underlying(SerialFieldType::COMPACT_INT)) {
                return std::unexpected(SchemaError::UNSUPPORTED_PROFILE);
            }
        }

        for (std::size_t j = 0; j < record.lookups.size(); ++j) {
            const auto &key = record.lookups[j];

            if (!validName(key.name) || key.fieldId >= record.fields.size()) {
                return std::unexpected(SchemaError::INVALID_SCHEMA);
            }

            if (key.name.size() > limits.nameUnits || !charge(key.name.size(), limits.totalNameUnits)) {
                return std::unexpected(SchemaError::RESOURCE_LIMIT);
            }

            for (std::size_t k = 0; k < j; ++k) {
                if (record.lookups[k].name == key.name) {
                    return std::unexpected(SchemaError::INVALID_SCHEMA);
                }
            }
        }

        for (std::size_t j = 0; j < record.fields.size(); ++j) {
            bool found = false;

            for (const auto &key : record.lookups) {
                found = found || key.fieldId == j;
            }

            if (!found) {
                return std::unexpected(SchemaError::INVALID_SCHEMA);
            }
        }
    }

    try {
        return LocalSchema(records);
    } catch (const std::bad_alloc &) {
        return std::unexpected(SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(SchemaError::ALLOCATION_FAILURE);
    }
}

} // namespace qd::core
