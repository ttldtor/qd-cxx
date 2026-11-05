// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/core/local_schema.hpp"

#include "schema_validation.hpp"

#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::core {
namespace {
/** Record the actual allocator request used by shared immutable provenance.
 * @tparam T Rebound allocation element selected by allocate_shared.
 */
template <typename T> struct IdentityAllocator {
    using value_type = T;   ///< Standard allocator element type.
    std::size_t *requested; ///< Construction-local request counter; never read during deallocation.

    /** Bind the construction-local counter.
     * @param bytes Counter which outlives allocation, but need not outlive deallocation.
     */
    explicit IdentityAllocator(std::size_t &bytes) noexcept : requested(&bytes) {
    }

    /** Preserve the counter across allocator rebinding.
     * @tparam U Source allocation element.
     * @param other Allocator rebound by allocate_shared.
     */
    template <typename U> IdentityAllocator(const IdentityAllocator<U> &other) noexcept : requested(other.requested) {
    }

    /** Allocate exact requested element backing and record successful requests.
     * @param count Requested elements.
     * @return Allocated storage; failure propagates as bad_alloc.
     */
    T *allocate(std::size_t count) {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_alloc{};
        }

        const auto bytes = count * sizeof(T);

        if (bytes > std::numeric_limits<std::size_t>::max() - *requested) {
            throw std::bad_alloc{};
        }

        auto *result = static_cast<T *>(::operator new(bytes));
        *requested += bytes;

        return result;
    }

    /** Release backing without touching the construction-local counter.
     * @param pointer Allocated storage.
     * @param count Original element request.
     */
    void deallocate(T *pointer, std::size_t count) noexcept {
        static_cast<void>(count);
        ::operator delete(pointer);
    }

    /** Compare interchangeable allocation ownership.
     * @tparam U Rebound allocator element.
     * @param other Allocator to compare.
     * @return True; all instances use ordinary operator new/delete.
     */
    template <typename U> bool operator==(const IdentityAllocator<U> &other) const noexcept {
        static_cast<void>(other);

        return true;
    }
};

/** Validate explicit logical contributors without importing source slots or protocol state.
 * @param record Borrowed structurally identified local record.
 * @param profile Exact selected field policy.
 * @return First layout error, or no error.
 */
std::optional<SchemaError> validateIndexedLayout(const LocalRecord &record, LocalProfile profile) noexcept {
    if (profile != LocalProfile::BINARY_FIELDS_V1) {
        return record.hasTime || record.indexedTimeLayout ? std::optional{SchemaError::UNSUPPORTED_PROFILE}
                                                          : std::nullopt;
    }

    if (record.hasTime != record.indexedTimeLayout.has_value()) {
        return SchemaError::INVALID_SCHEMA;
    }

    if (!record.indexedTimeLayout) {
        return std::nullopt;
    }

    if (const auto *pair = std::get_if<IndexedTimePairInt32>(&*record.indexedTimeLayout)) {
        if (pair->firstField >= record.fields.size() || pair->secondField >= record.fields.size() ||
            pair->firstField == pair->secondField) {
            return SchemaError::INVALID_SCHEMA;
        }

        for (const auto fieldId : {pair->firstField, pair->secondField}) {
            const auto type = record.fields[fieldId].serialTypeId;

            if (type != std::to_underlying(SerialFieldType::INT) &&
                type != std::to_underlying(SerialFieldType::COMPACT_INT) &&
                type != std::to_underlying(SerialFieldType::DECIMAL) &&
                type != std::to_underlying(SerialFieldType::SHORT_STRING) &&
                type != std::to_underlying(SerialFieldType::TIME_SECONDS) &&
                type != std::to_underlying(SerialFieldType::SEQUENCE) &&
                type != std::to_underlying(SerialFieldType::DATE)) {
                return SchemaError::UNSUPPORTED_PROFILE;
            }
        }
    } else if (const auto *low = std::get_if<IndexedTimeLowInt32>(&*record.indexedTimeLayout)) {
        if (low->field >= record.fields.size()) {
            return SchemaError::INVALID_SCHEMA;
        }

        if (record.fields[low->field].serialTypeId != std::to_underlying(SerialFieldType::COMPACT_INT)) {
            return SchemaError::UNSUPPORTED_PROFILE;
        }
    } else {
        const auto fieldId = std::get<IndexedTimeSingleInt64>(*record.indexedTimeLayout).field;

        if (fieldId >= record.fields.size()) {
            return SchemaError::INVALID_SCHEMA;
        }

        const auto type = record.fields[fieldId].serialTypeId;

        if (type != std::to_underlying(SerialFieldType::LONG) &&
            type != std::to_underlying(SerialFieldType::WIDE_DECIMAL) &&
            type != std::to_underlying(SerialFieldType::TIME_MILLIS)) {
            return SchemaError::UNSUPPORTED_PROFILE;
        }
    }

    return std::nullopt;
}
} // namespace

/** Fixed-size lifetime token; never retains source declarations or protocol state. */
struct RecordContext::Identity {
    std::size_t allocationBytes = 0; ///< Actual shared provenance allocator request, including its control metadata.
};

RecordContext::RecordContext(std::shared_ptr<const Identity> identity, std::int32_t localId) noexcept
    : identity_(std::move(identity)), localId_(localId) {
}

bool RecordContext::valid() const noexcept {
    return identity_ != nullptr && localId_ >= 0;
}

bool RecordContext::matches(const RecordContext &other) const noexcept {
    return valid() && other.valid() && identity_ == other.identity_ && localId_ == other.localId_;
}

SchemaIdentity::SchemaIdentity(std::shared_ptr<const RecordContext::Identity> identity) noexcept
    : identity_(std::move(identity)) {
}

bool SchemaIdentity::valid() const noexcept {
    return identity_ != nullptr;
}

bool SchemaIdentity::matches(const SchemaIdentity &other) const noexcept {
    return identity_ && identity_ == other.identity_;
}

LocalSchema::LocalSchema(std::span<const LocalRecord> records, LocalProfile profile)
    : records_(records.begin(), records.end()), profile_(profile) {
    std::size_t bytes = 0;
    auto identity = std::allocate_shared<RecordContext::Identity>(IdentityAllocator<RecordContext::Identity>{bytes});
    identity->allocationBytes = bytes;
    identity_ = std::move(identity);
    storageBytes_ = measureStorageBytes();
}

LocalSchema::LocalSchema(const LocalSchema &other)
    : records_(other.records_), profile_(other.profile_), identity_(other.identity_) {
    storageBytes_ = measureStorageBytes();
}

LocalSchema::LocalSchema(LocalSchema &&other) noexcept
    : records_(std::move(other.records_)), profile_(other.profile_), identity_(std::move(other.identity_)),
      storageBytes_(std::exchange(other.storageBytes_, sizeof(LocalSchema))) {
}

LocalSchema &LocalSchema::operator=(const LocalSchema &other) {
    if (this != &other) {
        LocalSchema candidate(other);
        records_.swap(candidate.records_);
        std::swap(profile_, candidate.profile_);
        identity_.swap(candidate.identity_);
        std::swap(storageBytes_, candidate.storageBytes_);
    }

    return *this;
}

LocalSchema &LocalSchema::operator=(LocalSchema &&other) noexcept {
    if (this != &other) {
        records_ = std::move(other.records_);
        profile_ = other.profile_;
        identity_ = std::move(other.identity_);
        storageBytes_ = std::exchange(other.storageBytes_, sizeof(LocalSchema));
    }

    return *this;
}

std::size_t LocalSchema::measureStorageBytes() const noexcept {
    std::size_t bytes = sizeof(LocalSchema);
    /** Saturate conservative aggregate ownership rather than understating a wrapped total.
     * @param amount Additional actual capacity or fixed metadata bytes.
     */
    const auto add = [&](std::size_t amount) {
        const auto maximum = std::numeric_limits<std::size_t>::max();
        bytes = amount > maximum - bytes ? maximum : bytes + amount;
    };
    add(records_.capacity() * sizeof(LocalRecord));
    add(identity_ ? identity_->allocationBytes : 0);

    for (const auto &record : records_) {
        add((record.name.capacity() + 1) * sizeof(char16_t));
        add(record.fields.capacity() * sizeof(LocalField));
        add(record.lookups.capacity() * sizeof(FieldLookup));

        for (const auto &lookup : record.lookups) {
            add((lookup.name.capacity() + 1) * sizeof(char16_t));
        }
    }

    return bytes;
}

std::size_t LocalSchema::storageBytes() const noexcept {
    return storageBytes_;
}

bool LocalSchema::sharesIdentityWith(const LocalSchema &other) const noexcept {
    return identity_ && identity_ == other.identity_;
}

SchemaIdentity LocalSchema::identity() const noexcept {
    return SchemaIdentity(identity_);
}

RecordContext LocalSchema::recordContext(std::int32_t localId) const noexcept {
    if (!identity_ || localId < 0 || static_cast<std::size_t>(localId) >= records_.size()) {
        return {};
    }

    return RecordContext(identity_, localId);
}

std::expected<LocalSchema, SchemaError> LocalSchema::boundedCopy(SchemaLimits limits) const {
    if (records_.size() > limits.records) {
        return std::unexpected(SchemaError::RESOURCE_LIMIT);
    }

    for (const auto &record : records_) {
        if (!detail::chargeBudget(record.fields.size(), limits.fields) ||
            !detail::chargeBudget(record.lookups.size(), limits.lookups) || record.name.size() > limits.nameUnits ||
            !detail::chargeBudget(record.name.size(), limits.totalNameUnits)) {
            return std::unexpected(SchemaError::RESOURCE_LIMIT);
        }

        for (const auto &lookup : record.lookups) {
            if (lookup.name.size() > limits.nameUnits ||
                !detail::chargeBudget(lookup.name.size(), limits.totalNameUnits)) {
                return std::unexpected(SchemaError::RESOURCE_LIMIT);
            }
        }
    }

    try {
        return *this;
    } catch (const std::bad_alloc &) {
        return std::unexpected(SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(SchemaError::ALLOCATION_FAILURE);
    }
}

std::span<const LocalRecord> LocalSchema::records() const noexcept {
    return records_;
}

std::expected<LocalSchema, SchemaError> LocalSchema::create(std::span<const LocalRecord> records, SchemaLimits limits,
                                                            LocalProfile profile) {
    if (profile != LocalProfile::COMPACT_INT_V1 && profile != LocalProfile::QUOTE_FIELDS_V1 &&
        profile != LocalProfile::EXTENDED_FIELDS_V1 && profile != LocalProfile::BINARY_FIELDS_V1) {
        return std::unexpected(SchemaError::UNSUPPORTED_PROFILE);
    }

    if (records.size() > limits.records) {
        return std::unexpected(SchemaError::RESOURCE_LIMIT);
    }

    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto &record = records[i];

        if (i > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
            record.id != static_cast<std::int32_t>(i) || !detail::validSchemaKey(record.name)) {
            return std::unexpected(SchemaError::INVALID_SCHEMA);
        }

        if (record.name.size() > limits.nameUnits || !detail::chargeBudget(record.name.size(), limits.totalNameUnits)) {
            return std::unexpected(SchemaError::RESOURCE_LIMIT);
        }

        for (std::size_t j = 0; j < i; ++j) {
            if (records[j].name == record.name) {
                return std::unexpected(SchemaError::INVALID_SCHEMA);
            }
        }

        if (const auto error = validateIndexedLayout(record, profile)) {
            return std::unexpected(*error);
        }

        if (!detail::chargeBudget(record.fields.size(), limits.fields) ||
            !detail::chargeBudget(record.lookups.size(), limits.lookups)) {
            return std::unexpected(SchemaError::RESOURCE_LIMIT);
        }

        for (const auto &field : record.fields) {
            if (!supportsSerialType(field.serialTypeId, profile)) {
                return std::unexpected(SchemaError::UNSUPPORTED_PROFILE);
            }
        }

        for (std::size_t j = 0; j < record.lookups.size(); ++j) {
            const auto &key = record.lookups[j];

            if (!detail::validSchemaKey(key.name) || key.fieldId >= record.fields.size()) {
                return std::unexpected(SchemaError::INVALID_SCHEMA);
            }

            if (key.name.size() > limits.nameUnits || !detail::chargeBudget(key.name.size(), limits.totalNameUnits)) {
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
        return LocalSchema(records, profile);
    } catch (const std::bad_alloc &) {
        return std::unexpected(SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(SchemaError::ALLOCATION_FAILURE);
    }
}

} // namespace org::ttldtor::qdcxx::core
