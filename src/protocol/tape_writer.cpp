// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/tape_writer.hpp"

#include "qd/encoding/utf_char.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {
namespace {
constexpr std::int32_t DESCRIBE_PROTOCOL = 1; ///< Sole descriptor message identity.
constexpr std::int32_t DESCRIBE_RECORDS = 2;  ///< Atomic outgoing description group.
constexpr std::int32_t RAW_DATA = 5;          ///< Admitted outgoing data message identity.
constexpr std::int32_t HEARTBEAT = 0;         ///< Explicit heartbeat, distinct from an empty envelope.
constexpr std::size_t COMPACT_BYTES = 9;      ///< Maximum canonical compact-long primitive width.
constexpr std::int32_t ARRAY_TYPE = 10;       ///< Exact character-array full identity.
constexpr std::int32_t LONG_TYPE = 120;       ///< Exact signed64 wide-decimal full identity.
constexpr std::int32_t CHAR_TYPE = 2;         ///< One UTFChar scalar field.
/** Java UTFString body domain: four encoded bytes per representable unit count. */
constexpr std::uint64_t MAX_TEXT_BODY = static_cast<std::uint64_t>(INT32_MAX) * 4;
constexpr char16_t HIGH_FIRST = 0xd800;          ///< First high UTF-16 surrogate.
constexpr char16_t HIGH_LAST = 0xdbff;           ///< Last high UTF-16 surrogate.
constexpr char16_t LOW_FIRST = 0xdc00;           ///< First low UTF-16 surrogate.
constexpr char16_t LOW_LAST = 0xdfff;            ///< Last low UTF-16 surrogate.
constexpr std::uint32_t SUPPLEMENTARY = 0x10000; ///< Supplementary code-point base.
constexpr unsigned SURROGATE_BITS = 10;          ///< Payload bits in each surrogate unit.
constexpr std::byte RAW_SYMBOL{0xfd};            ///< Nonnull raw character-array symbol prefix.
constexpr std::byte REPEAT_SYMBOL{0xff};         ///< Repeat previous exact symbol within one message.
/** Exact descriptor signature preceding its property and message maps. */
constexpr std::array<std::byte, 4> MAGIC{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};

/** Internal exit preserves one owning diagnostic through every allocation boundary. */
struct Failure {
    WriterDiagnostic diagnostic; ///< Static and numeric checkpoint only.
};

/** Translate schema-copy outcomes without broadening profile semantics.
 * @param error Existing bounded schema failure.
 * @return Corresponding outgoing construction category.
 */
WriterError schemaError(core::SchemaError error) noexcept {
    switch (error) {
    case core::SchemaError::INVALID_SCHEMA:
        return WriterError::INVALID_LAYOUT;
    case core::SchemaError::UNSUPPORTED_PROFILE:
        return WriterError::UNSUPPORTED;
    case core::SchemaError::RESOURCE_LIMIT:
        return WriterError::RESOURCE_LIMIT;
    case core::SchemaError::ALLOCATION_FAILURE:
        return WriterError::ALLOCATION_FAILURE;
    }

    return WriterError::INVALID_LAYOUT;
}

/** Form a static construction checkpoint.
 * @param error Failure category.
 * @param stage Static stage label.
 * @param record Optional supplied record position.
 * @param field Optional supplied field position.
 * @return Owning layout diagnostic.
 */
WriterDiagnostic layoutFailure(WriterError error, std::string_view stage, std::optional<std::size_t> record = {},
                               std::optional<std::size_t> field = {}) {
    return {error, WriterOperation::LAYOUT, stage, record, field};
}

/** Spend a finite remaining allowance using subtraction before addition.
 * @param amount Logical amount to admit.
 * @param used Previously admitted amount, modified only on success.
 * @param maximum Required finite maximum.
 * @return Whether the complete amount fits.
 */
bool charge(std::size_t amount, std::size_t &used, std::size_t maximum) noexcept {
    if (used > maximum || amount > maximum - used) {
        return false;
    }

    used += amount;

    return true;
}

/** Validate exact nonempty schema-key units without normalization.
 * @param name Borrowed name within a previously checked unit bound.
 * @return Whether surrogate units form complete pairs.
 */
bool validName(std::u16string_view name) noexcept {
    if (name.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < name.size(); ++i) {
        const auto unit = name[i];

        if (unit >= HIGH_FIRST && unit <= HIGH_LAST) {
            if (++i == name.size() || name[i] < LOW_FIRST || name[i] > LOW_LAST) {
                return false;
            }
        } else if (unit >= LOW_FIRST && unit <= LOW_LAST) {
            return false;
        }
    }

    return true;
}

/** Check schema counts before any complete owned copy.
 * @param schema Immutable validated declarations.
 * @param limits Required finite copy bounds.
 * @return Whether every declaration and name fits.
 */
bool schemaFits(const core::LocalSchema &schema, core::SchemaLimits limits) noexcept {
    if (schema.records().size() > limits.records) {
        return false;
    }

    std::size_t fields = 0;
    std::size_t lookups = 0;
    std::size_t units = 0;

    for (const auto &record : schema.records()) {
        if (!charge(record.fields.size(), fields, limits.fields) ||
            !charge(record.lookups.size(), lookups, limits.lookups) || record.name.size() > limits.nameUnits ||
            !charge(record.name.size(), units, limits.totalNameUnits)) {
            return false;
        }

        for (const auto &lookup : record.lookups) {
            if (lookup.name.size() > limits.nameUnits || !charge(lookup.name.size(), units, limits.totalNameUnits)) {
                return false;
            }
        }
    }

    return true;
}

/** Size a canonical compact integer using its existing atomic primitive.
 * @param value Exact signed32 value.
 * @return Canonical byte count.
 */
std::size_t intSize(std::int32_t value) noexcept {
    std::array<std::byte, COMPACT_BYTES> bytes{};

    return encoding::writeCompactInt(value, bytes).written;
}

/** Size a canonical compact long using its existing atomic primitive.
 * @param value Exact signed64 value.
 * @return Canonical byte count.
 */
std::size_t longSize(std::int64_t value) noexcept {
    std::array<std::byte, COMPACT_BYTES> bytes{};

    return encoding::writeCompactLong(value, bytes).written;
}

/** Size one canonical UTFChar through the existing primitive.
 * @param value Admitted scalar or exact UTF-16 unit.
 * @return Canonical byte count.
 */
std::size_t charSize(std::int64_t value) noexcept {
    std::array<std::byte, COMPACT_BYTES> bytes{};

    return encoding::writeUtfChar(value, bytes).written;
}
} // namespace

OutgoingLayouts::OutgoingLayouts(core::LocalSchema schema, std::vector<OutgoingRecord> records,
                                 LayoutLimits limits) noexcept
    : schema_(std::move(schema)), records_(std::move(records)), limits_(limits) {
}

std::expected<OutgoingLayouts, WriterDiagnostic>
OutgoingLayouts::create(const core::LocalSchema &schema, std::span<const OutgoingRecord> records, LayoutLimits limits) {
    if (records.size() > limits.records || !schemaFits(schema, limits.schema)) {
        return std::unexpected(layoutFailure(WriterError::RESOURCE_LIMIT, "layout-count"));
    }

    std::size_t fields = 0;
    std::size_t names = 0;

    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto &record = records[i];

        if (record.senderId < 0 || record.senderId == std::numeric_limits<std::int32_t>::max() ||
            record.localRecordId < 0 || static_cast<std::size_t>(record.localRecordId) >= schema.records().size()) {
            return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "record-id", i));
        }

        for (std::size_t j = 0; j < i; ++j) {
            if (records[j].senderId == record.senderId || records[j].localRecordId == record.localRecordId) {
                return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "duplicate-record-id", i));
            }
        }

        if (record.wireName.size() > limits.nameUnits ||
            !charge(record.wireName.size(), names, limits.totalNameUnits)) {
            return std::unexpected(layoutFailure(WriterError::RESOURCE_LIMIT, "record-name", i));
        }

        const auto &local = schema.records()[static_cast<std::size_t>(record.localRecordId)];

        if (!validName(record.wireName) || record.wireName != local.name) {
            return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "record-name", i));
        }

        for (std::size_t j = 0; j < i; ++j) {
            if (records[j].wireName == record.wireName) {
                return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "duplicate-record-name", i));
            }
        }

        if (record.fields.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
            return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "field-count", i));
        }

        if (!charge(record.fields.size(), fields, limits.fields)) {
            return std::unexpected(layoutFailure(WriterError::RESOURCE_LIMIT, "field-count", i));
        }

        for (std::size_t j = 0; j < record.fields.size(); ++j) {
            const auto &field = record.fields[j];

            if (field.localFieldId < 0 || static_cast<std::size_t>(field.localFieldId) >= local.fields.size()) {
                return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "field-id", i, j));
            }

            for (std::size_t k = 0; k < j; ++k) {
                if (record.fields[k].localFieldId == field.localFieldId) {
                    return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "duplicate-field-id", i, j));
                }
            }

            if (field.wireName.size() > limits.nameUnits ||
                !charge(field.wireName.size(), names, limits.totalNameUnits)) {
                return std::unexpected(layoutFailure(WriterError::RESOURCE_LIMIT, "field-name", i, j));
            }

            if (!validName(field.wireName)) {
                return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "field-name", i, j));
            }

            for (std::size_t k = 0; k < j; ++k) {
                if (record.fields[k].wireName == field.wireName) {
                    return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "duplicate-field-name", i, j));
                }
            }

            const auto lookup = std::find_if(local.lookups.begin(), local.lookups.end(), [&](const auto &key) {
                return key.name == field.wireName;
            });

            if (lookup == local.lookups.end() || lookup->fieldId != static_cast<std::size_t>(field.localFieldId)) {
                return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "field-lookup", i, j));
            }

            if (!core::supportsSerialType(field.serialTypeId, schema.profile()) ||
                field.serialTypeId != local.fields[static_cast<std::size_t>(field.localFieldId)].serialTypeId) {
                return std::unexpected(layoutFailure(WriterError::UNSUPPORTED, "field-type", i, j));
            }
        }

        if (record.fields.size() != local.fields.size()) {
            return std::unexpected(layoutFailure(WriterError::INVALID_LAYOUT, "field-coverage", i));
        }
    }

    try {
        auto owned = schema.boundedCopy(limits.schema);

        if (!owned) {
            return std::unexpected(layoutFailure(schemaError(owned.error()), "schema-copy"));
        }

        return OutgoingLayouts(std::move(*owned), {records.begin(), records.end()}, limits);
    } catch (const std::bad_alloc &) {
        return std::unexpected(layoutFailure(WriterError::ALLOCATION_FAILURE, "layout-copy"));
    } catch (const std::length_error &) {
        return std::unexpected(layoutFailure(WriterError::ALLOCATION_FAILURE, "layout-copy"));
    }
}

const core::LocalSchema &OutgoingLayouts::schema() const noexcept {
    return schema_;
}

std::span<const OutgoingRecord> OutgoingLayouts::records() const noexcept {
    return records_;
}

const OutgoingRecord *OutgoingLayouts::find(std::int32_t senderId) const noexcept {
    for (const auto &record : records_) {
        if (record.senderId == senderId) {
            return &record;
        }
    }

    return nullptr;
}

/** One operation's borrowed inputs and bounded size/reservation state; no callbacks or retained inputs. */
struct TapeWriter::Operation {
    std::span<const std::int32_t> ids;           ///< Borrowed description group, valid only for this operation.
    std::span<const OutgoingRecordView> records; ///< Borrowed data batch, valid only for this operation.
    const TapeHeartbeat *heartbeat;     ///< Borrowed fixed-size metadata, or null outside heartbeat operations.
    TapeWriter &writer;                 ///< Mutable owning lifecycle and accounting.
    WriterDiagnostic checkpoint;        ///< Current static/numeric error position.
    std::size_t bodyBytes = 0;          ///< Complete frame body, before its compact length prefix.
    std::size_t encodingWork = 0;       ///< Work reserved before the allocation/encoding phase.
    std::size_t encodingText = 0;       ///< UTFString visits reserved for two primitive passes.
    std::size_t encodingFieldBytes = 0; ///< Type-10 bytes reserved for two primitive passes.
    std::size_t encodingFieldUnits = 0; ///< Type-10 units reserved for two primitive passes.
    std::size_t encodingSymbols = 0;    ///< Symbol equality/copy/primitive visits reserved before allocation.
    std::size_t symbolPeak = 0;         ///< Largest message-local owned previous symbol.

    /** Borrow one owning writer for one bounded operation.
     * @param writer Writer whose state/accounting are updated.
     * @param operation Numeric action checkpoint.
     * @param ids Borrowed ordered description group.
     * @param records Borrowed local-order data batch.
     * @param heartbeat Borrowed fixed-size metadata, or null for other operations.
     */
    Operation(TapeWriter &writer, WriterOperation operation, std::span<const std::int32_t> ids,
              std::span<const OutgoingRecordView> records, const TapeHeartbeat *heartbeat)
        : ids(ids), records(records), heartbeat(heartbeat), writer(writer), checkpoint{WriterError::NONE, operation} {
    }

    /** Raise an internal failure without allocating or borrowing caller text.
     * @param error Terminal category.
     * @param stage Static failure checkpoint.
     */
    [[noreturn]] void reject(WriterError error, std::string_view stage) {
        checkpoint.error = error;
        checkpoint.stage = stage;
        throw Failure{checkpoint};
    }

    /** Charge one finite cumulative allowance before traversal.
     * @param amount Logical work/storage amount.
     * @param used Cumulative counter.
     * @param maximum Required finite maximum.
     * @param stage Static resource checkpoint.
     */
    void spend(std::size_t amount, std::size_t &used, std::size_t maximum, std::string_view stage) {
        if (!charge(amount, used, maximum)) {
            reject(WriterError::RESOURCE_LIMIT, stage);
        }
    }

    /** Spend cumulative work before an actual examination/copy/primitive traversal.
     * @param amount Element/unit visits to reserve.
     */
    void work(std::size_t amount) {
        spend(amount, writer.counters_.workUnits, writer.limits_.workUnits, "work");
    }

    /** Add checked canonical bytes before frame allocation.
     * @param amount Canonical byte count.
     */
    void bytes(std::size_t amount) {
        spend(amount, bodyBytes, std::numeric_limits<std::size_t>::max(), "size-arithmetic");
    }

    /** Reserve later work while checking representability before addition.
     * @param amount Complete amount for later passes.
     * @param used Reservation accumulator.
     */
    void reserve(std::size_t amount, std::size_t &used) {
        spend(amount, used, std::numeric_limits<std::size_t>::max(), "size-arithmetic");
    }

    /** Inspect a finite sender table without sender-indexed allocation.
     * @param senderId Exact wire identity.
     * @return Array position, or the table size when unknown.
     */
    std::size_t find(std::int32_t senderId) {
        for (std::size_t i = 0; i < writer.layouts_.records_.size(); ++i) {
            work(1);

            if (writer.layouts_.records_[i].senderId == senderId) {
                reserve(i + 1, encodingWork);

                return i;
            }
        }

        return writer.layouts_.records_.size();
    }

    /** Size one UTFString using existing UTFChar/compact writers, preserving lone surrogates.
     * @param value Borrowed exact units, including empty.
     * @param emit Whether later encoding should be reserved.
     * @return Canonical prefix/body byte count.
     */
    std::size_t text(std::u16string_view value, bool emit = true) {
        if (value.size() > MAX_TEXT_BODY) {
            reject(WriterError::INVALID_VALUE, "text-count");
        }

        if (value.size() > writer.limits_.text.codeUnits) {
            reject(WriterError::RESOURCE_LIMIT, "text-units");
        }

        spend(value.size(), writer.counters_.textUnits, writer.limits_.textUnits, "text-total");
        work(value.size());
        std::size_t body = 0;

        for (std::size_t i = 0; i < value.size(); ++i) {
            auto point = static_cast<std::uint32_t>(value[i]);

            if (value[i] >= HIGH_FIRST && value[i] <= HIGH_LAST && i + 1 < value.size() && value[i + 1] >= LOW_FIRST &&
                value[i + 1] <= LOW_LAST) {
                point = SUPPLEMENTARY + ((point - HIGH_FIRST) << SURROGATE_BITS) + (value[++i] - LOW_FIRST);
            }

            const auto encoded = charSize(point);

            if (encoded > MAX_TEXT_BODY - body) {
                reject(WriterError::INVALID_VALUE, "text-count");
            }

            spend(encoded, body, writer.limits_.text.bodyBytes, "text-bytes");
        }

        if (body > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            reject(WriterError::RESOURCE_LIMIT, "text-size");
        }

        std::size_t size = longSize(static_cast<std::int64_t>(body));
        reserve(body, size);

        if (emit) {
            for (unsigned pass = 0; pass < 2; ++pass) {
                reserve(value.size(), encodingText);
                reserve(value.size(), encodingWork);
            }
        }

        return size;
    }

    /** Size a character array, checking the wire count before finite unit bounds.
     * @param value Nullable exact units borrowed during this call.
     * @param field Whether field-string limits/charges apply rather than symbol limits.
     * @param emit Whether later primitive passes should be reserved.
     * @return Canonical prefix/body size.
     */
    std::size_t array(std::optional<std::u16string_view> value, bool field, bool emit = true) {
        if (value && value->size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
            reject(WriterError::INVALID_VALUE, field ? "field-count" : "symbol-count");
        }

        const auto units = value ? value->size() : 0;
        const auto limits =
            field ? writer.limits_.fieldString
                  : encoding::CharArrayLimits{std::numeric_limits<std::size_t>::max(), writer.limits_.symbolUnits};

        if (units > limits.codeUnits) {
            reject(WriterError::RESOURCE_LIMIT, field ? "field-string-units" : "symbol-units");
        }

        if (field) {
            spend(units, writer.counters_.fieldUnits, writer.limits_.fieldUnits, "field-units");
        } else {
            spend(units, writer.counters_.symbolUnits, writer.limits_.totalSymbolUnits, "symbol-total");
        }

        work(units);
        std::size_t size = intSize(value ? static_cast<std::int32_t>(units) : -1);

        if (size > limits.encodedBytes) {
            reject(WriterError::RESOURCE_LIMIT, field ? "field-string-bytes" : "symbol-bytes");
        }

        if (value) {
            for (const auto unit : *value) {
                spend(charSize(unit), size, limits.encodedBytes, field ? "field-string-bytes" : "symbol-bytes");
            }
        }

        if (field) {
            spend(size, writer.counters_.fieldBytes, writer.limits_.fieldBytes, "field-bytes");

            if (emit) {
                for (unsigned pass = 0; pass < 2; ++pass) {
                    reserve(size, encodingFieldBytes);
                    reserve(units, encodingFieldUnits);
                    reserve(units, encodingWork);
                }
            }
        } else {
            // Two primitive traversals and one owned previous-symbol copy occur only for a new symbol.
            for (unsigned pass = 0; pass < 3; ++pass) {
                reserve(units, encodingSymbols);
                reserve(units, encodingWork);
            }
        }

        return size;
    }

    /** Examine exact symbol equality with separate charges for both input unit visits.
     * @param previous Previous borrowed symbol in this size pass.
     * @param current Current borrowed symbol.
     * @return True only for complete exact unit equality.
     */
    bool sameSymbol(std::u16string_view previous, std::u16string_view current) {
        if (previous.size() != current.size()) {
            return false;
        }

        for (std::size_t i = 0; i < current.size(); ++i) {
            spend(2, writer.counters_.symbolUnits, writer.limits_.totalSymbolUnits, "symbol-total");
            work(2);
            reserve(2, encodingSymbols);
            reserve(2, encodingWork);

            if (previous[i] != current[i]) {
                return false;
            }
        }

        return true;
    }

    /** Reserve every encoding traversal and all private storage before allocation.
     * @return Complete compact-length plus body byte count.
     */
    std::size_t preflight() {
        spend(encodingText, writer.counters_.textUnits, writer.limits_.textUnits, "text-total");
        spend(encodingFieldBytes, writer.counters_.fieldBytes, writer.limits_.fieldBytes, "field-bytes");
        spend(encodingFieldUnits, writer.counters_.fieldUnits, writer.limits_.fieldUnits, "field-units");
        spend(encodingSymbols, writer.counters_.symbolUnits, writer.limits_.totalSymbolUnits, "symbol-total");
        work(encodingWork);

        if (bodyBytes > writer.limits_.frameBytes) {
            reject(WriterError::RESOURCE_LIMIT, "frame-bytes");
        }

        if (bodyBytes > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            reject(WriterError::RESOURCE_LIMIT, "frame-size");
        }

        std::size_t size = longSize(static_cast<std::int64_t>(bodyBytes));
        reserve(bodyBytes, size);

        if (size > writer.limits_.scratchBytes) {
            reject(WriterError::RESOURCE_LIMIT, "scratch-bytes");
        }

        if (symbolPeak > writer.limits_.scratchUnits) {
            reject(WriterError::RESOURCE_LIMIT, "scratch-units");
        }

        if (size > writer.limits_.outputBytes - writer.counters_.outputBytes) {
            reject(WriterError::RESOURCE_LIMIT, "output-bytes");
        }

        return size;
    }

    /** Obtain a sender already checked during the size pass; its traversal is reserved.
     * @param senderId Preflighted exact identity.
     * @return Finite layout position; called only after successful validation.
     */
    std::size_t indexOf(std::int32_t senderId) const noexcept {
        std::size_t index = 0;

        while (writer.layouts_.records_[index].senderId != senderId) {
            ++index;
        }

        return index;
    }

    /** Validate and size borrowed operation inputs before any frame allocation. */
    void sizeInput() {
        if (checkpoint.operation == WriterOperation::BEGIN) {
            bytes(intSize(DESCRIBE_PROTOCOL));
            bytes(MAGIC.size());
            bytes(longSize(2 + writer.descriptor_.version.has_value() + writer.descriptor_.options.has_value()));
            bytes(text(u"type"));
            bytes(text(u"tape"));

            if (writer.descriptor_.version) {
                bytes(text(u"version"));
                bytes(text(*writer.descriptor_.version));
            }

            if (writer.descriptor_.options) {
                bytes(text(u"opt"));
                bytes(text(*writer.descriptor_.options));
            }

            bytes(text(u"time"));
            bytes(text(writer.descriptor_.time));
            bytes(intSize(writer.descriptor_.explicitRawSend ? 1 : 0));

            if (writer.descriptor_.explicitRawSend) {
                bytes(intSize(RAW_DATA));
                bytes(text(u"RAW_DATA"));
                bytes(longSize(0));
            }

            bytes(intSize(0));
        } else if (checkpoint.operation == WriterOperation::DESCRIBE) {
            if (ids.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
                reject(WriterError::INVALID_VALUE, "description-count");
            }

            spend(ids.size(), writer.counters_.records, writer.limits_.records, "records");
            bytes(intSize(DESCRIBE_RECORDS));

            for (std::size_t i = 0; i < ids.size(); ++i) {
                checkpoint.record = i;
                work(1);
                reserve(1, encodingWork);
                const auto index = find(ids[i]);

                if (index == writer.layouts_.records_.size()) {
                    reject(WriterError::INVALID_VALUE, "unknown-id");
                }

                for (std::size_t j = 0; j < i; ++j) {
                    work(1);

                    if (ids[j] == ids[i]) {
                        reject(WriterError::INVALID_VALUE, "repeated-description-id");
                    }
                }

                if (writer.announced_[index]) {
                    reject(WriterError::INVALID_VALUE, "already-announced-id");
                }

                const auto &record = writer.layouts_.records_[index];
                reserve(index + 2, encodingWork);
                bytes(intSize(record.senderId));
                bytes(text(record.wireName));
                bytes(intSize(static_cast<std::int32_t>(record.fields.size())));

                for (std::size_t j = 0; j < record.fields.size(); ++j) {
                    checkpoint.field = j;
                    spend(1, writer.counters_.fields, writer.limits_.fields, "fields");
                    work(1);
                    reserve(1, encodingWork);
                    bytes(text(record.fields[j].wireName));
                    bytes(intSize(record.fields[j].serialTypeId));
                }

                checkpoint.field.reset();
            }
        } else if (checkpoint.operation == WriterOperation::HEARTBEAT) {
            bytes(intSize(HEARTBEAT));

            if ((!heartbeat->content && heartbeat->timestampMs) ||
                (heartbeat->content == 0 && heartbeat->timestampMs) ||
                (heartbeat->content == 1 && !heartbeat->timestampMs) ||
                (heartbeat->content && *heartbeat->content != 0 && *heartbeat->content != 1)) {
                reject(WriterError::INVALID_VALUE, "heartbeat");
            }

            if (heartbeat->content) {
                bytes(intSize(*heartbeat->content));
            }

            if (heartbeat->timestampMs) {
                bytes(longSize(*heartbeat->timestampMs));
            }
        } else if (checkpoint.operation == WriterOperation::DATA) {
            bytes(intSize(RAW_DATA));
            std::optional<std::u16string_view> previous;

            for (std::size_t i = 0; i < records.size(); ++i) {
                checkpoint.record = i;
                spend(1, writer.counters_.records, writer.limits_.records, "records");
                work(1);
                reserve(1, encodingWork);
                const auto &record = records[i];

                if (record.eventFlags != 0) {
                    reject(WriterError::UNSUPPORTED, "flags");
                }

                if (record.symbol.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
                    reject(WriterError::INVALID_VALUE, "symbol-count");
                }

                if (record.symbol.size() > writer.limits_.symbolUnits) {
                    reject(WriterError::RESOURCE_LIMIT, "symbol-units");
                }

                const bool repeated = previous && sameSymbol(*previous, record.symbol);
                bytes(1);

                if (!repeated) {
                    bytes(array(record.symbol, false));
                    symbolPeak = std::max(symbolPeak, record.symbol.size());
                }

                previous = record.symbol;
                const auto index = find(record.senderId);

                if (index == writer.layouts_.records_.size()) {
                    reject(WriterError::INVALID_VALUE, "unknown-id");
                }

                if (!writer.announced_[index]) {
                    reject(WriterError::INVALID_VALUE, "unannounced-id");
                }

                const auto &layout = writer.layouts_.records_[index];
                const auto &local = writer.layouts_.schema_.records()[static_cast<std::size_t>(layout.localRecordId)];

                if (record.values.size() != local.fields.size()) {
                    reject(WriterError::INVALID_VALUE, "value-count");
                }

                for (std::size_t j = 0; j < record.values.size(); ++j) {
                    checkpoint.field = j;
                    spend(1, writer.counters_.fields, writer.limits_.fields, "fields");
                    work(1);
                    const auto &value = record.values[j];
                    const auto type = static_cast<std::int32_t>(value.type);

                    if (!core::supportsSerialType(type, writer.layouts_.schema_.profile()) ||
                        type != local.fields[j].serialTypeId) {
                        reject(WriterError::UNSUPPORTED, "value-type");
                    }

                    if (!core::validFieldValue(value)) {
                        reject(WriterError::INVALID_VALUE, "value");
                    }

                    if (type == ARRAY_TYPE) {
                        const auto units =
                            value.units ? std::optional<std::u16string_view>(*value.units) : std::nullopt;
                        static_cast<void>(array(units, true, false));
                    }
                }

                bytes(intSize(record.senderId));

                for (std::size_t j = 0; j < layout.fields.size(); ++j) {
                    checkpoint.field = j;
                    work(1);
                    reserve(1, encodingWork);
                    const auto &field = layout.fields[j];
                    const auto &value = record.values[static_cast<std::size_t>(field.localFieldId)];

                    if (field.serialTypeId == ARRAY_TYPE) {
                        const auto units =
                            value.units ? std::optional<std::u16string_view>(*value.units) : std::nullopt;
                        bytes(array(units, true));
                    } else if (field.serialTypeId == CHAR_TYPE) {
                        bytes(charSize(value.raw));
                    } else if (field.serialTypeId == LONG_TYPE) {
                        bytes(longSize(value.raw));
                    } else {
                        bytes(intSize(static_cast<std::int32_t>(value.raw)));
                    }
                }

                checkpoint.field.reset();
            }
        }
    }

    /** Encode already validated inputs with all traversals reserved before allocation.
     * @param chunk Sole privately owned whole-envelope buffer.
     * @return Exact canonical bytes written, checked before publication.
     */
    std::size_t encode(WriterChunk &chunk) {
        std::size_t cursor = 0;
        std::u16string previous;
        bool hasPrevious = false;
        /** Commit one already-preflighted existing primitive to the private envelope.
         * @param result Existing writer outcome at the current output suffix.
         */
        const auto written = [&](encoding::WriteResult result) {
            if (result.status != encoding::Status::OK) {
                reject(WriterError::INVALID_VALUE, "encoding");
            }

            cursor += result.written;
        };
        /** Obtain the remaining private envelope capacity.
         * @return Writable borrowed suffix, retained only for the primitive call.
         */
        const auto output = [&]() {
            return std::span(chunk.bytes).subspan(cursor);
        };
        /** Emit exact signed32 through the existing canonical primitive.
         * @param value Preflighted integer.
         */
        const auto number = [&](std::int32_t value) {
            written(encoding::writeCompactInt(value, output()));
        };
        /** Emit exact signed64 through the existing canonical primitive.
         * @param value Preflighted integer.
         */
        const auto wide = [&](std::int64_t value) {
            written(encoding::writeCompactLong(value, output()));
        };
        /** Emit exact UTFString through the existing atomic primitive.
         * @param value Previously sized exact units.
         */
        const auto text = [&](std::u16string_view value) {
            written(encoding::writeUtfString(value, output(), writer.limits_.text));
        };
        wide(static_cast<std::int64_t>(bodyBytes));

        if (checkpoint.operation == WriterOperation::BEGIN) {
            number(DESCRIBE_PROTOCOL);

            for (const auto byte : MAGIC) {
                chunk.bytes[cursor++] = byte;
            }

            wide(2 + writer.descriptor_.version.has_value() + writer.descriptor_.options.has_value());
            text(u"type");
            text(u"tape");

            if (writer.descriptor_.version) {
                text(u"version");
                text(*writer.descriptor_.version);
            }

            if (writer.descriptor_.options) {
                text(u"opt");
                text(*writer.descriptor_.options);
            }

            text(u"time");
            text(writer.descriptor_.time);
            number(writer.descriptor_.explicitRawSend ? 1 : 0);

            if (writer.descriptor_.explicitRawSend) {
                number(RAW_DATA);
                text(u"RAW_DATA");
                wide(0);
            }

            number(0);
        } else if (checkpoint.operation == WriterOperation::DESCRIBE) {
            number(DESCRIBE_RECORDS);

            for (const auto id : ids) {
                const auto &record = writer.layouts_.records_[indexOf(id)];
                number(record.senderId);
                text(record.wireName);
                number(static_cast<std::int32_t>(record.fields.size()));

                for (const auto &field : record.fields) {
                    text(field.wireName);
                    number(field.serialTypeId);
                }
            }
        } else if (checkpoint.operation == WriterOperation::HEARTBEAT) {
            number(HEARTBEAT);

            if (heartbeat->content) {
                number(*heartbeat->content);
            }

            if (heartbeat->timestampMs) {
                wide(*heartbeat->timestampMs);
            }
        } else if (checkpoint.operation == WriterOperation::DATA) {
            number(RAW_DATA);

            for (const auto &record : records) {
                const auto &layout = writer.layouts_.records_[indexOf(record.senderId)];
                const bool repeated = hasPrevious && previous == record.symbol;
                chunk.bytes[cursor++] = repeated ? REPEAT_SYMBOL : RAW_SYMBOL;

                if (!repeated) {
                    written(encoding::writeCharArray(record.symbol, output(),
                                                     {writer.limits_.frameBytes, writer.limits_.symbolUnits}));
                    previous.clear();
                    previous.assign(record.symbol);
                    hasPrevious = true;
                    writer.counters_.peakScratchUnits = std::max(writer.counters_.peakScratchUnits, previous.size());
                }

                number(record.senderId);

                for (const auto &field : layout.fields) {
                    const auto &value = record.values[static_cast<std::size_t>(field.localFieldId)];

                    if (field.serialTypeId == ARRAY_TYPE) {
                        const auto units =
                            value.units ? std::optional<std::u16string_view>(*value.units) : std::nullopt;
                        written(encoding::writeCharArray(units, output(), writer.limits_.fieldString));
                    } else if (field.serialTypeId == CHAR_TYPE) {
                        written(encoding::writeUtfChar(value.raw, output()));
                    } else if (field.serialTypeId == LONG_TYPE) {
                        wide(value.raw);
                    } else {
                        number(static_cast<std::int32_t>(value.raw));
                    }
                }
            }
        }

        return cursor;
    }
};

TapeWriter::TapeWriter(OutgoingLayouts layouts, OutgoingDescriptor descriptor, WriterLimits limits,
                       WriterCounters counters)
    : layouts_(std::move(layouts)), descriptor_(std::move(descriptor)), limits_(limits),
      announced_(layouts_.records_.size(), false), counters_(counters) {
}

TapeWriter::TapeWriter(TapeWriter &&other) noexcept
    : layouts_(std::move(other.layouts_)), descriptor_(std::move(other.descriptor_)), limits_(other.limits_),
      announced_(std::move(other.announced_)), state_(other.state_), diagnostic_(other.diagnostic_),
      counters_(other.counters_) {
    other.state_ = WriterState::FAILED;
    other.diagnostic_ = {WriterError::INVALID_STATE, WriterOperation::CREATE, "moved-from"};
}

std::expected<TapeWriter, WriterDiagnostic>
TapeWriter::create(const OutgoingLayouts &layouts, const OutgoingDescriptor &descriptor, WriterLimits limits) {
    WriterCounters counters;
    WriterDiagnostic diagnostic{WriterError::RESOURCE_LIMIT, WriterOperation::CREATE, "construction-work"};
    /** Charge every planned schema/layout visit and copy before entering the traversal.
     * @param amount Exact logical element/unit count for one pass.
     * @return Whether both validation and copy passes fit.
     */
    const auto copied = [&](std::size_t amount) {
        return charge(amount, counters.workUnits, limits.workUnits) &&
               charge(amount, counters.workUnits, limits.workUnits);
    };

    if (!copied(layouts.schema_.records().size()) || !copied(layouts.records_.size())) {
        return std::unexpected(diagnostic);
    }

    if (!charge(layouts.records_.size(), counters.workUnits, limits.workUnits)) {
        return std::unexpected(diagnostic);
    }

    for (const auto &record : layouts.schema_.records()) {
        if (!copied(record.fields.size()) || !copied(record.lookups.size()) || !copied(record.name.size())) {
            return std::unexpected(diagnostic);
        }

        for (const auto &key : record.lookups) {
            if (!copied(key.name.size())) {
                return std::unexpected(diagnostic);
            }
        }
    }

    for (const auto &record : layouts.records_) {
        if (!copied(record.fields.size()) || !copied(record.wireName.size())) {
            return std::unexpected(diagnostic);
        }

        for (const auto &field : record.fields) {
            if (!copied(field.wireName.size())) {
                return std::unexpected(diagnostic);
            }
        }
    }

    const std::array<std::u16string_view, 3> texts{descriptor.version ? std::u16string_view(*descriptor.version) : u"",
                                                   descriptor.options ? std::u16string_view(*descriptor.options) : u"",
                                                   descriptor.time};

    for (const auto text : texts) {
        if (text.size() > MAX_TEXT_BODY) {
            return std::unexpected(WriterDiagnostic{WriterError::INVALID_VALUE, WriterOperation::CREATE, "text-count"});
        }

        if (text.size() > limits.text.codeUnits || !copied(text.size()) ||
            !charge(text.size(), counters.textUnits, limits.textUnits) ||
            !charge(text.size(), counters.textUnits, limits.textUnits)) {
            diagnostic.stage = "descriptor-text";

            return std::unexpected(diagnostic);
        }

        std::size_t body = 0;

        for (std::size_t i = 0; i < text.size(); ++i) {
            auto point = static_cast<std::uint32_t>(text[i]);

            if (text[i] >= HIGH_FIRST && text[i] <= HIGH_LAST && i + 1 < text.size() && text[i + 1] >= LOW_FIRST &&
                text[i + 1] <= LOW_LAST) {
                point = SUPPLEMENTARY + ((point - HIGH_FIRST) << SURROGATE_BITS) + (text[++i] - LOW_FIRST);
            }

            const auto encoded = charSize(point);

            if (encoded > MAX_TEXT_BODY - body) {
                return std::unexpected(
                    WriterDiagnostic{WriterError::INVALID_VALUE, WriterOperation::CREATE, "text-count"});
            }

            if (!charge(encoded, body, limits.text.bodyBytes)) {
                diagnostic.stage = "descriptor-text";

                return std::unexpected(diagnostic);
            }
        }
    }

    /** Compare policy text after charging the caller unit and both work-unit reads.
     * @param input Borrowed exact descriptor value.
     * @param required Static supported spelling.
     * @return Exact equality, or a resource failure before the unadmitted comparison.
     */
    const auto equalText = [&](std::u16string_view input, std::u16string_view required) {
        if (input.size() != required.size()) {
            return false;
        }

        for (std::size_t i = 0; i < input.size(); ++i) {
            if (!charge(1, counters.textUnits, limits.textUnits)) {
                throw Failure{{WriterError::RESOURCE_LIMIT, WriterOperation::CREATE, "descriptor-text-comparison"}};
            }

            if (!charge(2, counters.workUnits, limits.workUnits)) {
                throw Failure{diagnostic};
            }

            if (input[i] != required[i]) {
                return false;
            }
        }

        return true;
    };

    try {
        if (descriptor.options && !descriptor.options->empty() && !equalText(*descriptor.options, u"hs")) {
            return std::unexpected(WriterDiagnostic{WriterError::UNSUPPORTED, WriterOperation::CREATE, "options"});
        }

        if (!equalText(descriptor.time, u"MESSAGE") && !equalText(descriptor.time, u"message")) {
            return std::unexpected(WriterDiagnostic{WriterError::UNSUPPORTED, WriterOperation::CREATE, "time"});
        }

        // Immutable declarations already passed their retained finite copy bounds.
        OutgoingLayouts copy(layouts.schema_, layouts.records_, layouts.limits_);

        return TapeWriter(std::move(copy), descriptor, limits, counters);
    } catch (const Failure &failure) {
        return std::unexpected(failure.diagnostic);
    } catch (const std::bad_alloc &) {
        return std::unexpected(
            WriterDiagnostic{WriterError::ALLOCATION_FAILURE, WriterOperation::CREATE, "configuration-copy"});
    } catch (const std::length_error &) {
        return std::unexpected(
            WriterDiagnostic{WriterError::ALLOCATION_FAILURE, WriterOperation::CREATE, "configuration-copy"});
    }
}

std::expected<WriterChunk, WriterDiagnostic> TapeWriter::execute(WriterOperation action,
                                                                 std::span<const std::int32_t> ids,
                                                                 std::span<const OutgoingRecordView> records,
                                                                 const TapeHeartbeat *heartbeat) {
    if (state_ == WriterState::FAILED) {
        return std::unexpected(diagnostic_);
    }

    if (action == WriterOperation::FINISH && state_ == WriterState::FINISHED) {
        return WriterChunk{};
    }

    Operation operation(*this, action, ids, records, heartbeat);

    try {
        if ((action == WriterOperation::BEGIN && state_ != WriterState::READY) ||
            (action != WriterOperation::BEGIN && state_ != WriterState::ACTIVE)) {
            operation.reject(WriterError::INVALID_STATE, "state");
        }

        if (action == WriterOperation::FINISH) {
            state_ = WriterState::FINISHED;

            return WriterChunk{};
        }

        if (counters_.frames == limits_.frames) {
            operation.reject(WriterError::RESOURCE_LIMIT, "frames");
        }

        operation.sizeInput();

        const auto size = operation.preflight();
        WriterChunk chunk;
        chunk.bytes.resize(size);
        counters_.peakScratchBytes = std::max(counters_.peakScratchBytes, size);
        const auto cursor = operation.encode(chunk);

        if (cursor != size) {
            operation.reject(WriterError::INVALID_VALUE, "encoded-size");
        }

        if (action == WriterOperation::DESCRIBE) {
            // Announcement commit has no allocation and follows complete envelope success.
            for (const auto id : ids) {
                announced_[operation.indexOf(id)] = true;
            }
        }

        counters_.outputBytes += size;
        ++counters_.frames;

        if (action == WriterOperation::BEGIN) {
            state_ = WriterState::ACTIVE;
        }

        return chunk;
    } catch (const Failure &failure) {
        diagnostic_ = failure.diagnostic;
    } catch (const std::bad_alloc &) {
        operation.checkpoint.error = WriterError::ALLOCATION_FAILURE;
        operation.checkpoint.stage = "allocation";
        diagnostic_ = operation.checkpoint;
    } catch (const std::length_error &) {
        operation.checkpoint.error = WriterError::ALLOCATION_FAILURE;
        operation.checkpoint.stage = "allocation";
        diagnostic_ = operation.checkpoint;
    }

    state_ = WriterState::FAILED;

    return std::unexpected(diagnostic_);
}

std::expected<WriterChunk, WriterDiagnostic> TapeWriter::begin() {
    return execute(WriterOperation::BEGIN, {}, {}, nullptr);
}

std::expected<WriterChunk, WriterDiagnostic> TapeWriter::describe(std::span<const std::int32_t> senderIds) {
    return execute(WriterOperation::DESCRIBE, senderIds, {}, nullptr);
}

std::expected<WriterChunk, WriterDiagnostic> TapeWriter::heartbeat(const TapeHeartbeat &value) {
    return execute(WriterOperation::HEARTBEAT, {}, {}, &value);
}

std::expected<WriterChunk, WriterDiagnostic> TapeWriter::data(std::span<const OutgoingRecordView> records) {
    return execute(WriterOperation::DATA, {}, records, nullptr);
}

std::expected<WriterChunk, WriterDiagnostic> TapeWriter::marker() {
    return execute(WriterOperation::MARKER, {}, {}, nullptr);
}

std::expected<WriterChunk, WriterDiagnostic> TapeWriter::finish() {
    return execute(WriterOperation::FINISH, {}, {}, nullptr);
}

WriterState TapeWriter::state() const noexcept {
    return state_;
}

const WriterDiagnostic &TapeWriter::diagnostic() const noexcept {
    return diagnostic_;
}

const WriterCounters &TapeWriter::counters() const noexcept {
    return counters_;
}

} // namespace org::ttldtor::qdcxx::protocol
