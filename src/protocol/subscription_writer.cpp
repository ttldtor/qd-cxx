// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/subscription_writer.hpp"

#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {
namespace {
/** Record the actual allocator request used by shared output credits.
 * @tparam T Rebound allocation element selected by allocate_shared.
 */
template <typename T> struct CreditAllocator {
    using value_type = T;   ///< Standard allocator element type.
    std::size_t *requested; ///< Construction-local request counter; never read during deallocation.

    /** Bind the construction-local counter.
     * @param bytes Counter which outlives allocation, but need not outlive deallocation.
     */
    explicit CreditAllocator(std::size_t &bytes) noexcept : requested(&bytes) {
    }

    /** Preserve the counter across allocator rebinding.
     * @tparam U Source allocation element.
     * @param other Allocator rebound by allocate_shared.
     */
    template <typename U> CreditAllocator(const CreditAllocator<U> &other) noexcept : requested(other.requested) {
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
    template <typename U> bool operator==(const CreditAllocator<U> &other) const noexcept {
        static_cast<void>(other);

        return true;
    }
};

using Error = SubscriptionWriterError;
constexpr std::size_t COMPACT_BYTES = 9;
constexpr std::size_t MAX_WIRE_COUNT = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
constexpr std::byte RAW_SYMBOL{0xfd};
constexpr std::byte REPEAT_SYMBOL{0xff};
constexpr std::int32_t DESCRIBE_RECORDS = 2;
constexpr char16_t HIGH_FIRST = 0xd800;
constexpr char16_t HIGH_LAST = 0xdbff;
constexpr char16_t LOW_FIRST = 0xdc00;
constexpr char16_t LOW_LAST = 0xdfff;
constexpr std::uint32_t SUPPLEMENTARY = 0x10000;
constexpr unsigned SURROGATE_BITS = 10;

/** Debit a finite allowance before performing work.
 * @param amount Requested units.
 * @param remaining Caller-owned remaining credits.
 */
void debit(std::size_t amount, std::size_t &remaining) {
    if (amount > remaining) {
        throw Error::WORK_LIMIT;
    }

    remaining -= amount;
}

/** Add without wrapping or exceeding an explicit maximum.
 * @param amount Additional logical units.
 * @param total Running count.
 * @param limit Maximum admitted count.
 */
void add(std::size_t amount, std::size_t &total, std::size_t limit) {
    if (total > limit || amount > limit - total) {
        throw Error::RESOURCE_LIMIT;
    }

    total += amount;
}

/** Compare exact text after charging a conservative full comparison.
 * @param left First borrowed string.
 * @param right Second borrowed string.
 * @param work Remaining explicit turn work.
 * @return Whether exact units match.
 */
bool same(std::u16string_view left, std::u16string_view right, SubscriptionWriterWork &work) {
    debit(left.size(), work.textUnits);
    debit(right.size(), work.textUnits);

    return left == right;
}

/** Charge the bounded schema validation/copy before entering its existing factory.
 * @param schema Borrowed immutable source.
 * @param limits Required complete schema-copy bounds.
 * @param work Remaining explicit turn work.
 */
void checkSchema(const core::LocalSchema &schema, core::SchemaLimits limits, SubscriptionWriterWork &work) {
    if (schema.records().size() > limits.records) {
        throw Error::RESOURCE_LIMIT;
    }

    std::size_t fields = 0;
    std::size_t lookups = 0;
    std::size_t names = 0;

    for (const auto &record : schema.records()) {
        debit(1, work.visits);
        add(record.fields.size(), fields, limits.fields);
        add(record.lookups.size(), lookups, limits.lookups);

        if (record.name.size() > limits.nameUnits) {
            throw Error::RESOURCE_LIMIT;
        }

        add(record.name.size(), names, limits.totalNameUnits);
        debit(record.fields.size(), work.visits);
        debit(record.lookups.size(), work.visits);

        for (const auto &lookup : record.lookups) {
            debit(1, work.visits);

            if (lookup.name.size() > limits.nameUnits) {
                throw Error::RESOURCE_LIMIT;
            }

            add(lookup.name.size(), names, limits.totalNameUnits);
        }
    }

    // Existing boundedCopy rechecks bounds and copies the validated immutable declarations.
    for (unsigned pass = 0; pass < 3; ++pass) {
        debit(names, work.textUnits);
        debit(schema.records().size(), work.visits);
        debit(fields, work.visits);
        debit(lookups, work.visits);
    }
}
} // namespace

/** One fixed-size accounting owner shared by the composer and all of its outputs. */
struct PendingSubscriptionOutput::Budget {
    std::size_t retained = 0;        ///< Exact byte-array slots currently owned by outputs.
    std::size_t outputs = 0;         ///< Live output owners, including a reserved candidate.
    std::size_t allocationBytes = 0; ///< Actual shared credit allocator request including control metadata.
};

PendingSubscriptionOutput::PendingSubscriptionOutput(std::shared_ptr<Budget> budget, std::unique_ptr<std::byte[]> bytes,
                                                     std::size_t size) noexcept
    : budget_(std::move(budget)), bytes_(std::move(bytes)), size_(size) {
}

PendingSubscriptionOutput::PendingSubscriptionOutput(PendingSubscriptionOutput &&other) noexcept
    : budget_(std::move(other.budget_)), bytes_(std::move(other.bytes_)), size_(std::exchange(other.size_, 0)),
      consumed_(std::exchange(other.consumed_, 0)) {
}

PendingSubscriptionOutput &PendingSubscriptionOutput::operator=(PendingSubscriptionOutput &&other) noexcept {
    if (this != &other) {
        if (budget_) {
            budget_->retained -= size_;
            --budget_->outputs;
        }

        bytes_ = std::move(other.bytes_);
        budget_ = std::move(other.budget_);
        size_ = std::exchange(other.size_, 0);
        consumed_ = std::exchange(other.consumed_, 0);
    }

    return *this;
}

PendingSubscriptionOutput::~PendingSubscriptionOutput() {
    if (budget_) {
        budget_->retained -= size_;
        --budget_->outputs;
    }
}

std::span<const std::byte> PendingSubscriptionOutput::bytes() const noexcept {
    return {bytes_.get(), size_};
}

std::span<const std::byte> PendingSubscriptionOutput::pendingBytes() const noexcept {
    return bytes().subspan(consumed_);
}

std::expected<void, Error> PendingSubscriptionOutput::consume(std::size_t count) noexcept {
    if (count > size_ - consumed_) {
        return std::unexpected(Error::INVALID_CONSUMPTION);
    }

    consumed_ += count;

    return {};
}

SubscriptionWriter::SubscriptionWriter(core::LocalSchema schema, std::vector<OutgoingRecord> records,
                                       SubscriptionWriterLimits limits,
                                       std::shared_ptr<PendingSubscriptionOutput::Budget> budget)
    : schema_(std::move(schema)), records_(std::move(records)), limits_(limits), budget_(std::move(budget)) {
    storageBytes_ = measureStorageBytes();
}

std::expected<SubscriptionWriter, Error>
SubscriptionWriter::create(const core::LocalSchema &schema, std::span<const OutgoingRecord> records,
                           LayoutLimits layoutLimits, SubscriptionWriterLimits limits, SubscriptionWriterWork &work) {
    try {
        debit(1, work.operations);

        if (records.size() > layoutLimits.records) {
            throw Error::RESOURCE_LIMIT;
        }

        checkSchema(schema, layoutLimits.schema, work);
        std::size_t fields = 0;
        std::size_t names = 0;

        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto &record = records[i];

            debit(1, work.visits);

            if (record.senderId < 0 || record.senderId == std::numeric_limits<std::int32_t>::max() ||
                record.localRecordId < 0 || static_cast<std::size_t>(record.localRecordId) >= schema.records().size()) {
                throw Error::INVALID_LAYOUT;
            }

            for (std::size_t j = 0; j < i; ++j) {
                debit(1, work.visits);

                if (records[j].senderId == record.senderId || records[j].localRecordId == record.localRecordId) {
                    throw Error::INVALID_LAYOUT;
                }
            }

            const auto &local = schema.records()[static_cast<std::size_t>(record.localRecordId)];

            if (record.wireName.size() > layoutLimits.nameUnits) {
                throw Error::RESOURCE_LIMIT;
            }

            add(record.wireName.size(), names, layoutLimits.totalNameUnits);

            if (!same(record.wireName, local.name, work) || record.fields.size() != local.fields.size()) {
                throw Error::INVALID_LAYOUT;
            }

            if (record.fields.size() > MAX_WIRE_COUNT) {
                throw Error::RESOURCE_LIMIT;
            }

            add(record.fields.size(), fields, layoutLimits.fields);

            for (std::size_t j = 0; j < record.fields.size(); ++j) {
                const auto &field = record.fields[j];

                debit(1, work.visits);

                if (field.localFieldId < 0 || static_cast<std::size_t>(field.localFieldId) >= local.fields.size()) {
                    throw Error::INVALID_LAYOUT;
                }

                for (std::size_t k = 0; k < j; ++k) {
                    debit(1, work.visits);

                    if (record.fields[k].localFieldId == field.localFieldId) {
                        throw Error::INVALID_LAYOUT;
                    }
                }

                if (field.wireName.size() > layoutLimits.nameUnits) {
                    throw Error::RESOURCE_LIMIT;
                }

                add(field.wireName.size(), names, layoutLimits.totalNameUnits);
                bool found = false;

                for (const auto &lookup : local.lookups) {
                    debit(1, work.visits);

                    if (same(field.wireName, lookup.name, work)) {
                        found = lookup.fieldId == static_cast<std::size_t>(field.localFieldId);

                        break;
                    }
                }

                if (!found ||
                    field.serialTypeId != local.fields[static_cast<std::size_t>(field.localFieldId)].serialTypeId) {
                    throw Error::INVALID_LAYOUT;
                }
            }
        }

        debit(names, work.textUnits);
        debit(records.size(), work.visits);
        debit(fields, work.visits);
        auto ownedSchema = schema.boundedCopy(layoutLimits.schema);

        if (!ownedSchema) {
            throw ownedSchema.error() == core::SchemaError::ALLOCATION_FAILURE ? Error::ALLOCATION_FAILURE
                                                                               : Error::RESOURCE_LIMIT;
        }

        std::vector<OutgoingRecord> owned(records.begin(), records.end());
        std::size_t creditBytes = 0;
        auto budget = std::allocate_shared<PendingSubscriptionOutput::Budget>(
            CreditAllocator<PendingSubscriptionOutput::Budget>{creditBytes});
        budget->allocationBytes = creditBytes;

        // Cache actual adopted capacities in one additional bounded metadata pass.
        debit(records.size(), work.visits);
        debit(fields, work.visits);

        return SubscriptionWriter(std::move(*ownedSchema), std::move(owned), limits, std::move(budget));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

/** One call's borrowed inputs, sizing pass and candidate encoding; no persistent mutation. */
struct SubscriptionWriter::Operation {
    SubscriptionWriter &writer;                          ///< Composer configuration, borrowed for this call.
    SubscriptionWriterWork &work;                        ///< Caller credits, never refunded.
    std::optional<SubscriptionMessage> message;          ///< Absent for descriptions.
    std::span<const core::SubscriptionRequest> requests; ///< Borrowed exact ordered inputs.
    std::span<const std::int32_t> ids;                   ///< Borrowed ordered description sender IDs.
    std::span<std::byte> output{};                       ///< Candidate body, empty during sizing.
    bool encoding = false;                               ///< Whether output bytes are being filled.
    std::size_t size = 0;                                ///< Current body byte position.

    /** Append a bounded byte count before touching output.
     * @param count Encoded bytes to admit.
     */
    void admit(std::size_t count) {
        debit(count, work.outputBytes);
        add(count, size, std::min(writer.limits_.frameBytes, MAX_WIRE_COUNT));
    }

    /** Encode one compact scalar via the existing bounded primitive.
     * @param value Exact signed64 value; compact-long and int agree for signed32 values.
     */
    void number(std::int64_t value) {
        std::array<std::byte, COMPACT_BYTES> bytes{};
        const auto count = encoding::writeCompactLong(value, bytes).written;
        const auto offset = size;

        admit(count);

        if (encoding) {
            std::copy_n(bytes.begin(), count, output.begin() + static_cast<std::ptrdiff_t>(offset));
        }
    }

    /** Append one raw protocol marker.
     * @param value Exact symbol prefix.
     */
    void byte(std::byte value) {
        const auto offset = size;

        admit(1);

        if (encoding) {
            output[offset] = value;
        }
    }

    /** Size canonical UTFChar bytes using the existing primitive.
     * @param value Exact unit or supplementary scalar.
     * @return Encoded size.
     */
    std::size_t charSize(std::uint32_t value) {
        std::array<std::byte, COMPACT_BYTES> bytes{};

        return encoding::writeUtfChar(value, bytes).written;
    }

    /** Size or encode exact text through the existing atomic UTFString/CharArray writers.
     * @param value Borrowed non-null units.
     * @param symbol Whether to preserve individual UTF-16 units as a character array.
     */
    void text(std::u16string_view value, bool symbol) {
        debit(value.size(), work.textUnits);
        std::size_t body = 0;

        for (std::size_t i = 0; i < value.size(); ++i) {
            auto point = static_cast<std::uint32_t>(value[i]);

            if (!symbol && value[i] >= HIGH_FIRST && value[i] <= HIGH_LAST && i + 1 < value.size() &&
                value[i + 1] >= LOW_FIRST && value[i + 1] <= LOW_LAST) {
                point = SUPPLEMENTARY + ((point - HIGH_FIRST) << SURROGATE_BITS) + (value[++i] - LOW_FIRST);
            }

            add(charSize(point), body, MAX_WIRE_COUNT);
        }

        if (!symbol && (body > writer.limits_.text.bodyBytes || value.size() > writer.limits_.text.codeUnits)) {
            throw Error::RESOURCE_LIMIT;
        }

        std::array<std::byte, COMPACT_BYTES> prefix{};
        const auto count = symbol ? encoding::writeCompactInt(static_cast<std::int32_t>(value.size()), prefix).written
                                  : encoding::writeCompactLong(static_cast<std::int64_t>(body), prefix).written;
        std::size_t encoded = body;
        const auto offset = size;

        add(count, encoded, MAX_WIRE_COUNT);
        admit(encoded);

        if (encoding) {
            // Both atomic text writers traverse their input for sizing and encoding.
            debit(value.size(), work.textUnits);
            debit(value.size(), work.textUnits);
            const auto target = output.subspan(offset, encoded);
            const auto result = symbol ? encoding::writeCharArray(value, target, {encoded, writer.limits_.symbolUnits})
                                       : encoding::writeUtfString(value, target, writer.limits_.text);

            if (result.status != encoding::Status::OK || result.written != encoded) {
                throw Error::RESOURCE_LIMIT;
            }
        }
    }

    /** Resolve sparse sender identity with explicitly charged linear work.
     * @param id Exact declared sender identity.
     * @return Borrowed immutable outgoing layout.
     */
    const OutgoingRecord &byId(std::int32_t id) {
        for (const auto &record : writer.records_) {
            debit(1, work.visits);

            if (record.senderId == id) {
                return record;
            }
        }

        throw Error::INVALID_LAYOUT;
    }

    /** Resolve opaque provenance without extracting local IDs from caller contexts.
     * @param context Borrowed key provenance.
     * @return Borrowed immutable outgoing layout.
     */
    const OutgoingRecord &byContext(const core::RecordContext &context) {
        for (const auto &record : writer.records_) {
            debit(1, work.visits);

            if (context.matches(writer.schema_.recordContext(record.localRecordId))) {
                return record;
            }
        }

        throw Error::INVALID_CONTEXT;
    }

    /** Traverse one complete body with fresh message-local symbol state. */
    void walk() {
        number(message ? static_cast<std::int32_t>(*message) : DESCRIBE_RECORDS);

        if (!message) {
            for (const auto id : ids) {
                debit(1, work.visits);
                const auto &record = byId(id);

                number(record.senderId);
                text(record.wireName, false);
                number(static_cast<std::int32_t>(record.fields.size()));

                for (const auto &field : record.fields) {
                    debit(1, work.visits);
                    text(field.wireName, false);
                    number(field.serialTypeId);
                }
            }

            return;
        }

        std::optional<std::u16string_view> previous;
        const bool history =
            *message == SubscriptionMessage::HISTORY_ADD || *message == SubscriptionMessage::HISTORY_REMOVE;
        const bool time = *message == SubscriptionMessage::HISTORY_ADD;

        for (const auto &request : requests) {
            debit(1, work.visits);
            const auto &symbol = request.key.symbol;

            if (symbol.data() == nullptr) {
                throw Error::INVALID_SELECTOR;
            }

            if (symbol.size() > MAX_WIRE_COUNT || symbol.size() > writer.limits_.symbolUnits) {
                throw Error::RESOURCE_LIMIT;
            }

            if (!symbol.empty()) {
                debit(1, work.textUnits);

                if (symbol.front() == u'*') {
                    throw Error::INVALID_SELECTOR;
                }
            }

            const auto &record = byContext(request.key.context);
            const auto &local = writer.schema_.records()[static_cast<std::size_t>(record.localRecordId)];

            if (time != request.historyBound.has_value() || (history && (!local.hasTime || !local.indexedTimeLayout))) {
                throw Error::INVALID_HISTORY;
            }

            const bool repeated = previous && same(*previous, symbol, work);

            byte(repeated ? REPEAT_SYMBOL : RAW_SYMBOL);

            if (!repeated) {
                text(symbol, true);
            }

            number(record.senderId);

            if (time) {
                number(*request.historyBound);
            }

            previous = symbol;
        }
    }
};

std::expected<PendingSubscriptionOutput, Error>
SubscriptionWriter::execute(std::optional<SubscriptionMessage> message,
                            std::span<const core::SubscriptionRequest> requests, std::span<const std::int32_t> ids,
                            SubscriptionWriterWork &work) {
    try {
        debit(1, work.operations);

        if (!budget_) {
            throw Error::INVALID_STATE;
        }

        if (message) {
            switch (*message) {
            case SubscriptionMessage::TICKER_ADD:
            case SubscriptionMessage::TICKER_REMOVE:
            case SubscriptionMessage::STREAM_ADD:
            case SubscriptionMessage::STREAM_REMOVE:
            case SubscriptionMessage::HISTORY_ADD:
            case SubscriptionMessage::HISTORY_REMOVE:
                break;
            default:
                throw Error::INVALID_MESSAGE;
            }
        }

        if (requests.size() > limits_.operationEntries || ids.size() > limits_.operationEntries) {
            throw Error::RESOURCE_LIMIT;
        }

        Operation operation{*this, work, message, requests, ids};

        operation.walk();
        std::array<std::byte, COMPACT_BYTES> prefix{};
        const auto prefixSize = encoding::writeCompactInt(static_cast<std::int32_t>(operation.size), prefix).written;
        std::size_t total = operation.size;

        add(prefixSize, total, limits_.operationBytes);
        std::size_t resident = budget_->retained;

        add(total, resident, limits_.residentBytes);
        resident = budget_->retained;
        add(total, resident, limits_.peakBytes);
        std::size_t outputs = budget_->outputs;
        add(1, outputs, limits_.outputs);
        debit(prefixSize, work.outputBytes);
        debit(total, work.outputBytes);

        struct Reservation {
            PendingSubscriptionOutput::Budget &budget;
            std::size_t bytes;
            bool committed = false;

            ~Reservation() {
                if (!committed) {
                    budget.retained -= bytes;
                    --budget.outputs;
                }
            }
        };

        Reservation reservation{*budget_, total};
        budget_->retained = resident;
        budget_->outputs = outputs;
        auto candidate = std::make_unique<std::byte[]>(total);

        std::copy_n(prefix.begin(), prefixSize, candidate.get());
        debit(prefixSize, work.outputBytes);
        operation.output = {candidate.get() + prefixSize, operation.size};
        operation.encoding = true;
        operation.size = 0;

        operation.walk();
        reservation.committed = true;

        return PendingSubscriptionOutput(budget_, std::move(candidate), total);
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

std::expected<PendingSubscriptionOutput, Error>
SubscriptionWriter::compose(SubscriptionMessage message, std::span<const core::SubscriptionRequest> requests,
                            SubscriptionWriterWork &work) {
    return execute(message, requests, {}, work);
}

std::expected<PendingSubscriptionOutput, Error> SubscriptionWriter::describe(std::span<const std::int32_t> senderIds,
                                                                             SubscriptionWriterWork &work) {
    return execute(std::nullopt, {}, senderIds, work);
}

std::size_t SubscriptionWriter::retainedBytes() const noexcept {
    return budget_ ? budget_->retained : 0;
}

std::size_t SubscriptionWriter::measureStorageBytes() const noexcept {
    std::size_t bytes = sizeof(SubscriptionWriter);
    /** Saturate conservative aggregate ownership instead of wrapping a requested-capacity total.
     * @param amount Additional actual capacity or fixed metadata bytes.
     */
    const auto add = [&](std::size_t amount) {
        const auto maximum = std::numeric_limits<std::size_t>::max();
        bytes = amount > maximum - bytes ? maximum : bytes + amount;
    };
    add(schema_.storageBytes());
    add(records_.capacity() * sizeof(OutgoingRecord));
    add(budget_ ? budget_->allocationBytes : 0);

    for (const auto &record : records_) {
        add((record.wireName.capacity() + 1) * sizeof(char16_t));
        add(record.fields.capacity() * sizeof(OutgoingField));

        for (const auto &field : record.fields) {
            add((field.wireName.capacity() + 1) * sizeof(char16_t));
        }
    }

    return bytes;
}

std::size_t SubscriptionWriter::storageBytes() const noexcept {
    return budget_ ? storageBytes_ : 0;
}

bool SubscriptionWriter::matchesSchema(const core::LocalSchema &schema) const noexcept {
    return budget_ && schema_.sharesIdentityWith(schema);
}

std::size_t SubscriptionWriter::maximumOutputBytes() const noexcept {
    return budget_ ? limits_.operationBytes : 0;
}

} // namespace org::ttldtor::qdcxx::protocol
