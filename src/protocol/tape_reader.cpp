// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/protocol/tape_reader.hpp"
#include "../core/schema_validation.hpp"
#include "field_time_binding.hpp"
#include "session_budget.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {
namespace {
constexpr std::int32_t DESCRIBE_PROTOCOL = 1; ///< Required initial header.
constexpr std::int32_t DESCRIBE_RECORDS = 2;  ///< Append-only description update.
constexpr std::int32_t STREAM_DATA = 15;      ///< Legacy data message.
constexpr std::int32_t RAW_DATA = 5;          ///< Opt-in raw wire records.
constexpr std::int32_t TICKER_DATA = 10;      ///< Generic ticker wire records, without collectors.
constexpr std::int32_t HISTORY_DATA = 20;     ///< Generic history wire records, without collectors.
constexpr std::int32_t DESCRIBE_RESERVED = 3; ///< Bounded stateless reserved message.
constexpr std::uint64_t MAX_DESCRIPTOR_UTF_BODY_BYTES = 8589934588ULL; ///< Java UTFString's four-byte unit bound.
constexpr std::int32_t HEARTBEAT = 0;            ///< Explicit timing message, distinct from an empty envelope.
constexpr std::int64_t MILLIS_PER_SECOND = 1000; ///< Signed wire timestamp resolution.
constexpr unsigned SECOND_SHIFT = 32;            ///< High signed seconds word in cursor timing.
constexpr unsigned MILLIS_SHIFT = 22;            ///< Milliseconds above the reserved sequence bits.
constexpr std::uint64_t MILLIS_MASK = 0x3FF;     ///< Ten-bit packed millisecond component.
/** Exact protocol-descriptor signature, preceding property and message maps. */
constexpr std::array<std::byte, 4> MAGIC{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};

/** Internal structured exit from descriptor examination. */
struct DescriptorFailure {
    TapeError error; ///< Category, separate from checkpoint.
};

/** Measure owning effective protocol metadata; fixed scalars share one header unit.
 * @param value Published or candidate descriptor.
 * @return Exact logical header/text units; descriptor bounds guarantee representability.
 */
std::size_t descriptorUnits(const TapeDescriptor &value) noexcept {
    std::size_t units = 1;

    for (const auto *text : {&value.version, &value.options, &value.time}) {
        if (*text) {
            units += 1 + (*text)->size();
        }
    }

    return units;
}

/** Recognize only the four generic record families. */
bool isDataFamily(std::int32_t type) noexcept {
    return type == RAW_DATA || type == TICKER_DATA || type == STREAM_DATA || type == HISTORY_DATA;
}

/** Numeric projection required by the descriptor field grammar. */
enum class NumberWidth {
    COMPACT_INT, ///< Signed low 32 bits, including full-width noncanonical encodings.
    COMPACT_LONG ///< Full signed value before property-count validation.
};

/** Bounded exact descriptor cursor; all outputs are owned. */
class DescriptorParser {
    std::span<const std::byte> payload_;
    TapeDescriptorLimits limits_;
    TapeMode mode_;
    FieldTimePolicy fieldTimePolicy_;
    TapeExternalTimePolicy externalTimePolicy_;
    TapeDiagnostic &position_;
    std::size_t cursor_ = 0;
    std::size_t units_ = 0;
    std::size_t properties_ = 0;
    std::size_t messages_ = 0;

    /** Test only the explicitly admitted finite header tokens.
     * @param value Borrowed exact time token.
     * @return Whether the immutable independent policies admit this token.
     */
    bool supportsCatalogTime(std::u16string_view value) const noexcept {
        if (value == u"NONE" || value == u"none" || value == u"MESSAGE" || value == u"message") {
            return true;
        }

        if (value == u"FIELD" || value == u"field") {
            return fieldTimePolicy_ == FieldTimePolicy::READ;
        }

        return externalTimePolicy_ == TapeExternalTimePolicy::SIDECAR &&
               (value == u"LONG" || value == u"long" || value == u"TEXT" || value == u"text");
    }

    /** Scoped text ownership within the descriptor's shared metadata ledger. */
    struct CatalogText {
        std::optional<std::u16string> value;
        detail::SessionBudget &budget;
        std::size_t units;

        CatalogText(std::optional<std::u16string> text, detail::SessionBudget &owner, std::size_t cost)
            : value(std::move(text)), budget(owner), units(cost) {
        }

        CatalogText(const CatalogText &) = delete;

        CatalogText(CatalogText &&other) noexcept
            : value(std::move(other.value)), budget(other.budget), units(std::exchange(other.units, 0)) {
        }

        ~CatalogText() {
            budget.release(units);
        }

        /** Transfer storage and its already admitted ownership to a candidate field. */
        std::optional<std::u16string> take() noexcept {
            units = 0;

            return std::move(value);
        }
    };

    /** Decode with a finite preallocation reservation; no admitted work is refunded. */
    CatalogText catalogText(detail::SessionBudget &budget) {
        const auto prefix = encoding::readCompactLong(payload_.subspan(cursor_));
        std::size_t reservation = 0;

        if (prefix.status == encoding::Status::OK && prefix.value > 0 &&
            static_cast<std::uint64_t>(prefix.value) <= MAX_DESCRIPTOR_UTF_BODY_BYTES) {
            reservation = static_cast<std::size_t>(std::min({static_cast<std::uint64_t>(prefix.value) + 1,
                                                             static_cast<std::uint64_t>(limits_.text.codeUnits),
                                                             static_cast<std::uint64_t>(limits_.textUnits - units_)}));
        }

        if (reservation == std::numeric_limits<std::size_t>::max() || !budget.admit(1 + reservation)) {
            reject(TapeError::RESOURCE_LIMIT);
        }

        CatalogText result{std::nullopt, budget, 1 + reservation};
        const auto saved = limits_.text.codeUnits;
        limits_.text.codeUnits = reservation;
        result.value = text();
        limits_.text.codeUnits = saved;
        const auto actual = result.value ? result.value->size() : 0;
        budget.release(reservation - actual);
        result.units = 1 + actual;

        return result;
    }

    /** Raise a validation error at the current checkpoint.
     * @param error Terminal classification.
     * @param reason Additional static policy reason, without changing numeric diagnostics.
     */
    [[noreturn]] void reject(TapeError error, TapeRejection reason = TapeRejection::NONE) {
        position_.consumed = cursor_;
        position_.errorOffset = cursor_;
        position_.rejection = reason;
        throw DescriptorFailure{error};
    }

    /** Read one numeric primitive preserving its actual width.
     * @param width Compact-long property count or projected compact-int count/ID.
     * @return Signed projected value or a structured truncation failure.
     */
    std::int64_t number(NumberWidth width) {
        position_.consumed = cursor_;
        position_.errorOffset = cursor_;
        /** Commit examined bytes while retaining the primitive's start error checkpoint.
         * @param result Result from the selected integer-width reader.
         * @return Signed value, or throw the descriptor's malformed-primitive failure.
         */
        const auto finishRead = [this](const auto &result) -> std::int64_t {
            cursor_ += result.consumed;
            position_.consumed = cursor_;

            if (result.status != encoding::Status::OK) {
                throw DescriptorFailure{TapeError::MALFORMED};
            }

            return result.value;
        };
        const auto input = payload_.subspan(cursor_);

        return width == NumberWidth::COMPACT_LONG ? finishRead(encoding::readCompactLong(input))
                                                  : finishRead(encoding::readCompactInt(input));
    }

    /** Read exact text while charging aggregate units.
     * @return Owned optional UTF-16; null remains distinguishable for pair validation.
     */
    std::optional<std::u16string> text() {
        position_.consumed = cursor_;
        position_.errorOffset = cursor_;
        auto result = encoding::readUtfString(
            payload_.subspan(cursor_),
            {limits_.text.bodyBytes, std::min(limits_.text.codeUnits, limits_.textUnits - units_)});
        cursor_ += result.consumed;
        position_.consumed = cursor_;

        if (result.status != encoding::Status::OK) {
            throw DescriptorFailure{result.status == encoding::Status::RESOURCE_LIMIT ? TapeError::RESOURCE_LIMIT
                                                                                      : TapeError::MALFORMED};
        }

        if (result.value) {
            units_ += result.value->size();
        }

        return std::move(result.value);
    }

    /** Read/validate a count before charging its cumulative allowance.
     * @param width Property counts use compact-long; message lists use compact-int.
     * @param used Previously charged entries.
     * @param maximum Finite aggregate bound.
     * @return Valid count, charged before any entries are allocated.
     */
    std::size_t count(NumberWidth width, std::size_t &used, std::size_t maximum) {
        auto value = number(width);

        if (value < 0 || value > std::numeric_limits<std::int32_t>::max()) {
            reject(TapeError::MALFORMED);
        }

        auto n = static_cast<std::size_t>(value);

        if (n > maximum - used) {
            reject(TapeError::RESOURCE_LIMIT);
        }

        used += n;

        return n;
    }

    public:
    /** Borrow one complete payload and a result position.
     * @param payload Complete descriptor bytes.
     * @param limits Finite text/collection bounds.
     * @param position Diagnostic checkpoints updated during examination.
     * @param mode Explicit container policy.
     * @param fieldTimePolicy Explicit finite FIELD recognition policy.
     * @param externalTimePolicy Explicit companion header admission policy, without source state.
     */
    DescriptorParser(std::span<const std::byte> payload, TapeDescriptorLimits limits, TapeDiagnostic &position,
                     TapeMode mode, FieldTimePolicy fieldTimePolicy, TapeExternalTimePolicy externalTimePolicy)
        : payload_(payload), limits_(limits), mode_(mode), fieldTimePolicy_(fieldTimePolicy),
          externalTimePolicy_(externalTimePolicy), position_(position) {
    }

    /** Parse the selected broad full/delta grammar with old/candidate/temporary accounting.
     * @param previous Prior effective descriptor, borrowed only during this call.
     * @param budget Shared metadata ledger, already charged for complete payload parse work.
     * @return Owned validated candidate; publication belongs to the reader.
     */
    TapeDescriptor parseCatalog(const std::optional<TapeDescriptor> &previous, detail::SessionBudget &budget) {
        position_.stage = "descriptor";

        for (auto byte : MAGIC) {
            if (cursor_ == payload_.size() || payload_[cursor_++] != byte) {
                reject(TapeError::MALFORMED);
            }
        }

        const auto candidateUnits = previous ? descriptorUnits(*previous) : 1;

        if (!budget.admit(candidateUnits)) {
            reject(TapeError::RESOURCE_LIMIT);
        }

        TapeDescriptor candidate = previous ? *previous : TapeDescriptor{};
        unsigned seen = 0;
        const auto pairs = count(NumberWidth::COMPACT_LONG, properties_, limits_.properties);

        for (std::size_t index = 0; index < pairs; ++index) {
            auto key = catalogText(budget);
            auto value = catalogText(budget);

            if (!key.value || key.value->empty() || !value.value) {
                reject(TapeError::MALFORMED);
            }

            const unsigned bit = *key.value == u"type"      ? 1U
                                 : *key.value == u"version" ? 2U
                                 : *key.value == u"opt"     ? 4U
                                 : *key.value == u"time"    ? 8U
                                                            : 0U;

            if (bit == 0 || (seen & bit) != 0) {
                reject(TapeError::UNSUPPORTED, TapeRejection::DESCRIPTOR);
            }

            seen |= bit;
            std::optional<std::u16string> *target = nullptr;

            if (bit == 1U) {
                if (*value.value != u"tape") {
                    reject(TapeError::UNSUPPORTED, TapeRejection::DESCRIPTOR);
                }
            } else if (bit == 2U) {
                target = &candidate.version;
            } else if (bit == 4U) {
                if (!value.value->empty() && *value.value != u"hs") {
                    reject(TapeError::UNSUPPORTED, TapeRejection::OPTIONS);
                }

                target = &candidate.options;
            } else {
                if (!supportsCatalogTime(*value.value)) {
                    reject(TapeError::UNSUPPORTED, TapeRejection::TIME);
                }

                candidate.explicitTimeNone = *value.value == u"NONE" || *value.value == u"none";
                target = &candidate.time;
            }

            if (target) {
                if (*target) {
                    budget.release(1 + (*target)->size());
                }

                *target = value.take();
            }
        }

        if (!previous && (seen & 1U) == 0) {
            reject(TapeError::UNSUPPORTED, TapeRejection::DESCRIPTOR);
        }

        unsigned sendSeen = 0;
        const auto sends = count(NumberWidth::COMPACT_INT, messages_, limits_.messages);

        for (std::size_t index = 0; index < sends; ++index) {
            const auto id = number(NumberWidth::COMPACT_INT);
            auto name = catalogText(budget);
            const auto props = count(NumberWidth::COMPACT_LONG, properties_, limits_.properties);
            unsigned bit = 0;
            bool *declared = nullptr;

            if (id == RAW_DATA && name.value == u"RAW_DATA") {
                bit = 1;
                declared = &candidate.explicitRawSend;
            }

            if (id == TICKER_DATA && name.value == u"TICKER_DATA") {
                bit = 2;
                declared = &candidate.explicitTickerSend;
            }

            if (id == STREAM_DATA && name.value == u"STREAM_DATA") {
                bit = 4;
                declared = &candidate.explicitStreamSend;
            }

            if (id == HISTORY_DATA && name.value == u"HISTORY_DATA") {
                bit = 8;
                declared = &candidate.explicitHistorySend;
            }

            if (!name.value || name.value->empty()) {
                reject(TapeError::MALFORMED);
            }

            if (bit == 0 || (sendSeen & bit) != 0 || props != 0) {
                reject(TapeError::UNSUPPORTED, TapeRejection::MESSAGE);
            }

            sendSeen |= bit;
            *declared = true;
        }

        if (count(NumberWidth::COMPACT_INT, messages_, limits_.messages) != 0) {
            reject(TapeError::UNSUPPORTED, TapeRejection::MESSAGE);
        }

        if (cursor_ < payload_.size()) {
            const auto length = number(NumberWidth::COMPACT_LONG);

            if (length < -1 || length > std::numeric_limits<std::int32_t>::max()) {
                reject(TapeError::MALFORMED);
            }

            if (length >= 0) {
                if (static_cast<std::uint64_t>(length) > payload_.size() - cursor_) {
                    reject(TapeError::MALFORMED);
                }

                cursor_ += static_cast<std::size_t>(length);
                auto endpointName = catalogText(budget);
                static_cast<void>(number(NumberWidth::COMPACT_LONG));
            }

            if (cursor_ != payload_.size()) {
                reject(TapeError::MALFORMED);
            }

            reject(TapeError::UNSUPPORTED, TapeRejection::DESCRIPTOR);
        }

        position_.consumed = cursor_;
        position_.errorOffset = cursor_;

        return candidate;
    }

    /** Examine the entire restricted header.
     * @return Owned diagnostic information; throws only internal failure/allocation exceptions.
     */
    TapeDescriptor parse() {
        position_.stage = "descriptor";

        for (auto b : MAGIC) {
            if (cursor_ == payload_.size()) {
                reject(TapeError::MALFORMED);
            }

            if (payload_[cursor_++] != b) {
                reject(TapeError::MALFORMED);
            }
        }

        TapeDescriptor result;
        std::vector<std::u16string> keys;
        bool type = false;
        bool opt = false;
        auto n = count(NumberWidth::COMPACT_LONG, properties_, limits_.properties);

        for (std::size_t i = 0; i < n; ++i) {
            auto key = text();
            auto value = text();

            if (!key || key->empty() || !value) {
                reject(TapeError::MALFORMED, key == u"time"  ? TapeRejection::TIME
                                             : key == u"opt" ? TapeRejection::OPTIONS
                                                             : TapeRejection::NONE);
            }

            if (std::find(keys.begin(), keys.end(), *key) != keys.end()) {
                reject(TapeError::MALFORMED);
            }

            keys.push_back(*key);

            if (*key == u"type") {
                if (*value != u"tape") {
                    reject(TapeError::UNSUPPORTED, TapeRejection::DESCRIPTOR);
                }

                type = true;
            } else if (*key == u"opt") {
                if (!value->empty() &&
                    !((mode_ == TapeMode::RAW_MESSAGE_V1 || mode_ == TapeMode::BINARY_CATALOG_V1) && *value == u"hs")) {
                    reject(TapeError::UNSUPPORTED, TapeRejection::OPTIONS);
                }

                opt = true;
                result.options = std::move(value);
            } else if (*key == u"time") {
                if (mode_ != TapeMode::RAW_MESSAGE_V1 ? *value != u"NONE"
                                                      : (*value != u"MESSAGE" && *value != u"message")) {
                    reject(TapeError::UNSUPPORTED, TapeRejection::TIME);
                }

                result.explicitTimeNone = *value == u"NONE";
                result.time = std::move(value);
            } else if (*key == u"version") {
                result.version = std::move(value);
            } else {
                reject(TapeError::UNSUPPORTED, TapeRejection::DESCRIPTOR);
            }
        }

        for (std::size_t direction = 0; direction < 2; ++direction) {
            auto entries = count(NumberWidth::COMPACT_INT, messages_, limits_.messages);

            for (std::size_t i = 0; i < entries; ++i) {
                auto id = number(NumberWidth::COMPACT_INT);
                auto name = text();

                if (id < 0 || !name || name->empty()) {
                    reject(TapeError::MALFORMED);
                }

                auto props = count(NumberWidth::COMPACT_LONG, properties_, limits_.properties);

                const bool acceptedMessage = mode_ != TapeMode::RAW_MESSAGE_V1
                                                 ? id == STREAM_DATA && *name == u"STREAM_DATA"
                                                 : id == RAW_DATA && *name == u"RAW_DATA";

                if (props != 0 || direction != 0 || entries != 1 || !acceptedMessage) {
                    reject(TapeError::UNSUPPORTED, TapeRejection::MESSAGE);
                }

                result.explicitStreamSend = mode_ != TapeMode::RAW_MESSAGE_V1;
                result.explicitRawSend = mode_ == TapeMode::RAW_MESSAGE_V1;
            }
        }

        if (cursor_ != payload_.size() || !type || (!opt && mode_ == TapeMode::LEGACY)) {
            reject(TapeError::UNSUPPORTED,
                   cursor_ == payload_.size() && type && !opt ? TapeRejection::OPTIONS : TapeRejection::DESCRIPTOR);
        }

        if (mode_ == TapeMode::RAW_MESSAGE_V1 && !result.time) {
            reject(TapeError::UNSUPPORTED, TapeRejection::TIME);
        }

        position_.consumed = cursor_;
        position_.errorOffset = cursor_;

        return result;
    }
};

/** Copy schema only after bounded validation.
 * @param schema Immutable local input.
 * @param limits Maximum owned logical elements.
 * @return Owned schema, or constructor exception before input is accepted.
 */
core::LocalSchema copySchema(const core::LocalSchema &schema, core::SchemaLimits limits) {
    auto result = schema.boundedCopy(limits);

    if (!result) {
        if (result.error() == core::SchemaError::ALLOCATION_FAILURE) {
            throw std::bad_alloc();
        }

        throw std::invalid_argument("tape schema exceeds configured bounds");
    }

    return std::move(*result);
}

/** Subtraction-based exact logical metadata count; never an allocator-capacity measurement. */
class UnitCount {
    std::size_t remaining_;
    std::size_t maximum_;
    bool fits_ = true;

    public:
    /** Select a finite bound before measuring borrowed declarations.
     * @param maximum Largest admissible logical count.
     */
    explicit UnitCount(std::size_t maximum) noexcept : remaining_(maximum), maximum_(maximum) {
    }

    /** Add only after overflow-safe admission.
     * @param amount One owning element/unit category.
     */
    void add(std::size_t amount) noexcept {
        if (amount > remaining_) {
            fits_ = false;
        } else if (fits_) {
            remaining_ -= amount;
        }
    }

    /** Obtain the complete count, or reject without a wrapped arithmetic result. */
    [[nodiscard]] std::optional<std::size_t> result() const noexcept {
        return fits_ ? std::optional{maximum_ - remaining_} : std::nullopt;
    }
};

/** Count owned local schema declarations and one immutable identity before copying.
 * @param records Validated borrowed declarations.
 * @param maximum Finite count ceiling.
 * @return S from INDEXED-SCHEMAS, or no value on overflow/excess.
 */
std::optional<std::size_t> schemaUnits(std::span<const core::LocalRecord> records, std::size_t maximum) noexcept {
    UnitCount count(maximum);
    count.add(1);

    for (const auto &record : records) {
        count.add(1);
        count.add(record.name.size());
        count.add(record.fields.size());
        count.add(record.lookups.size());
        count.add(record.indexedTimeLayout ? 1 : 0);

        for (const auto &key : record.lookups) {
            count.add(key.name.size());
        }
    }

    return count.result();
}

std::optional<std::size_t> schemaUnits(const core::LocalSchema &schema, std::size_t maximum) noexcept {
    return schemaUnits(schema.records(), maximum);
}

/** Count the D ownership of one active or staged raw declaration.
 * @param record Borrowed complete description.
 * @param count Overflow-safe aggregate counter.
 */
void descriptionUnits(const WireRecordDescription &record, UnitCount &count) noexcept {
    count.add(1);
    count.add(record.name.size());
    count.add(record.fields.size());

    for (const auto &field : record.fields) {
        count.add(field.name.size());
    }
}

/** Measure a raw table without allocating an ID-indexed structure.
 * @param descriptions Borrowed active or complete staging table.
 * @param maximum Finite count ceiling.
 * @return D, or no value on overflow/excess.
 */
std::optional<std::size_t> descriptionUnits(std::span<const WireRecordDescription> descriptions,
                                            std::size_t maximum) noexcept {
    UnitCount count(maximum);

    for (const auto &record : descriptions) {
        descriptionUnits(record, count);
    }

    return count.result();
}

/** Count complete plans, owned defaults and resolved indexed layout before construction.
 * @param schema Validated borrowed local declarations.
 * @param descriptions Borrowed final active table.
 * @param maximum Finite count ceiling.
 * @return C, or no value on overflow/excess.
 */
std::optional<std::size_t> contextUnits(const core::LocalSchema &schema,
                                        std::span<const WireRecordDescription> descriptions, std::size_t maximum,
                                        FieldTimePolicy fieldTimePolicy = FieldTimePolicy::DISABLED) noexcept {
    UnitCount count(maximum);

    for (const auto &record : schema.records()) {
        count.add(1);
        count.add(record.fields.size());
    }

    for (const auto &description : descriptions) {
        count.add(1);
        count.add(description.fields.size());
        count.add(fieldTimePolicy == FieldTimePolicy::READ ? 2 : 0);

        for (const auto &local : schema.records()) {
            if (local.name == description.name) {
                count.add(local.indexedTimeLayout ? 1 : 0);
                break;
            }
        }
    }

    return count.result();
}

/** Measure the complete initial new-mode state before either owning allocation.
 * @param schema Caller-owned schema, not part of the reader's retained units.
 * @param limits Finite work and peak bounds.
 * @return S plus empty-context C, or constructor invalid_argument.
 */
std::size_t initialSessionUnits(const core::LocalSchema &schema, const TapeLimits &limits) {
    const auto maximum = std::min(limits.snapshotUnits, limits.constructionUnits);
    const auto local = schemaUnits(schema, maximum);

    if (local) {
        const auto context = contextUnits(schema, {}, maximum - *local);

        if (context) {
            return *local + *context;
        }
    }

    throw std::invalid_argument("initial session metadata exceeds configured bounds");
}

/** Build the append-only logical name/type registry for explicit binary discovery.
 * Retired declarations remain in the validated schema and in every schema/metadata bound.
 */
std::expected<core::LocalSchema, TapeError>
discoverBinarySchema(const core::LocalSchema &previous, std::span<const WireRecordDescription> incoming,
                     const TapeLimits &limits, detail::SessionBudget &budget, FieldTimePolicy fieldTimePolicy) {
    const auto old = schemaUnits(previous, limits.snapshotUnits);

    if (!old || !budget.admit(*old - 1)) {
        return std::unexpected(TapeError::RESOURCE_LIMIT);
    }

    std::vector<core::LocalRecord> records(previous.records().begin(), previous.records().end());
    std::size_t staging = *old - 1;
    std::size_t fields = 0;
    std::size_t names = 0;

    for (const auto &local : records) {
        fields += local.fields.size();
        names += local.name.size();

        for (const auto &key : local.lookups) {
            names += key.name.size();
        }
    }

    const auto &bounds = limits.context.schema;

    for (const auto &description : incoming) {
        if (!core::detail::validSchemaKey(description.name)) {
            return std::unexpected(TapeError::MALFORMED);
        }

        auto local = std::find_if(records.begin(), records.end(), [&](const auto &value) {
            return value.name == description.name;
        });

        if (local == records.end()) {
            if (records.size() >= bounds.records ||
                records.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
                description.name.size() > bounds.nameUnits || description.name.size() > bounds.totalNameUnits - names ||
                !budget.admit(1 + description.name.size())) {
                return std::unexpected(TapeError::RESOURCE_LIMIT);
            }

            staging += 1 + description.name.size();
            names += description.name.size();
            records.push_back({static_cast<std::int32_t>(records.size()), description.name, false, {}, {}});
            local = records.end() - 1;
        }

        const auto metadataFields =
            fieldTimePolicy == FieldTimePolicy::READ ? detail::eventTimeFieldCount(description) : 0;

        for (const auto &field : std::span(description.fields).subspan(metadataFields)) {
            if (!core::supportsSerialType(field.serialTypeId, core::LocalProfile::BINARY_FIELDS_V1)) {
                return std::unexpected(TapeError::UNSUPPORTED);
            }

            if (!core::detail::validSchemaKey(field.name)) {
                return std::unexpected(TapeError::MALFORMED);
            }

            const auto key = std::find_if(local->lookups.begin(), local->lookups.end(), [&](const auto &value) {
                return value.name == field.name;
            });

            if (key != local->lookups.end()) {
                if (local->fields[key->fieldId].serialTypeId != field.serialTypeId) {
                    return std::unexpected(TapeError::UNSUPPORTED);
                }

                continue;
            }

            if (fields >= bounds.fields || fields >= bounds.lookups || field.name.size() > bounds.nameUnits ||
                field.name.size() > bounds.totalNameUnits - names || !budget.admit(2 + field.name.size())) {
                return std::unexpected(TapeError::RESOURCE_LIMIT);
            }

            staging += 2 + field.name.size();
            names += field.name.size();
            ++fields;
            local->lookups.push_back({field.name, local->fields.size()});
            local->fields.push_back({field.serialTypeId});
        }
    }

    const auto total = schemaUnits(records, limits.snapshotUnits);

    if (!total || !budget.admit(*total)) {
        return std::unexpected(TapeError::RESOURCE_LIMIT);
    }

    auto result = core::LocalSchema::create(records, bounds, core::LocalProfile::BINARY_FIELDS_V1);

    if (!result) {
        return std::unexpected(result.error() == core::SchemaError::ALLOCATION_FAILURE ? TapeError::ALLOCATION_FAILURE
                               : result.error() == core::SchemaError::RESOURCE_LIMIT   ? TapeError::RESOURCE_LIMIT
                                                                                       : TapeError::MALFORMED);
    }

    records.clear();
    budget.release(staging);

    return std::move(*result);
}

/** Validate all finite policy selections before any owning schema/framing allocation.
 * @param mode Explicit tape grammar.
 * @param fieldTimePolicy Independent FIELD-prefix policy.
 * @param externalTimePolicy Independent companion header admission policy.
 */
void validateTimePolicies(TapeMode mode, FieldTimePolicy fieldTimePolicy, TapeExternalTimePolicy externalTimePolicy) {
    if (mode != TapeMode::LEGACY && mode != TapeMode::RAW_MESSAGE_V1 && mode != TapeMode::BINARY_CATALOG_V1) {
        throw std::invalid_argument("invalid tape mode");
    }

    if ((fieldTimePolicy != FieldTimePolicy::DISABLED && fieldTimePolicy != FieldTimePolicy::READ) ||
        (fieldTimePolicy == FieldTimePolicy::READ && mode != TapeMode::BINARY_CATALOG_V1)) {
        throw std::invalid_argument("invalid field time policy");
    }

    if ((externalTimePolicy != TapeExternalTimePolicy::DISABLED &&
         externalTimePolicy != TapeExternalTimePolicy::SIDECAR) ||
        (externalTimePolicy == TapeExternalTimePolicy::SIDECAR && mode != TapeMode::BINARY_CATALOG_V1)) {
        throw std::invalid_argument("invalid external time policy");
    }
}

/** Preserve old copying semantics and opt into fresh indexed provenance explicitly.
 * @param schema Validated caller-owned local schema.
 * @param limits Finite constructor bounds.
 * @param mode Explicit protocol policy.
 * @param fieldTimePolicy Explicit prefix policy, validated before any owning allocation.
 * @param externalTimePolicy Explicit companion admission policy, validated before any owning allocation.
 * @return Bounded owning schema; constructor exceptions occur before any input.
 */
core::LocalSchema initializeSchema(const core::LocalSchema &schema, const TapeLimits &limits, TapeMode mode,
                                   FieldTimePolicy fieldTimePolicy, TapeExternalTimePolicy externalTimePolicy) {
    validateTimePolicies(mode, fieldTimePolicy, externalTimePolicy);

    if (mode != TapeMode::BINARY_CATALOG_V1) {
        for (const auto &record : schema.records()) {
            if (record.hasTime || record.indexedTimeLayout) {
                throw std::invalid_argument("indexed schemas require the catalog tape mode");
            }
        }

        return copySchema(schema, limits.context.schema);
    }

    if (schema.profile() != core::LocalProfile::BINARY_FIELDS_V1) {
        throw std::invalid_argument("catalog tape mode requires the binary field profile");
    }

    static_cast<void>(initialSessionUnits(schema, limits));
    auto result = core::LocalSchema::create(schema.records(), limits.context.schema, schema.profile());

    if (!result) {
        if (result.error() == core::SchemaError::ALLOCATION_FAILURE) {
            throw std::bad_alloc();
        }

        throw std::invalid_argument("invalid catalog schema bounds");
    }

    return std::move(*result);
}

/** Translate static construction failure without changing its detailed ordinal.
 * @param error Binding result.
 * @return Tape category.
 */
TapeError bindingError(BindingError error) {
    switch (error) {
    case BindingError::RESOURCE_LIMIT:
        return TapeError::RESOURCE_LIMIT;
    case BindingError::ALLOCATION_FAILURE:
        return TapeError::ALLOCATION_FAILURE;
    case BindingError::UNSUPPORTED_PROFILE:
        return TapeError::UNSUPPORTED;
    default:
        return TapeError::MALFORMED;
    }
}

/** Pack a signed wire timestamp without signed shifts or overflowing intermediate arithmetic.
 * @param timestamp Exact signed milliseconds.
 * @return Saturated Java cursor timing with reserved sequence bits zero.
 */
std::int64_t packTimestamp(std::int64_t timestamp) noexcept {
    auto seconds = timestamp / MILLIS_PER_SECOND;
    auto millis = timestamp % MILLIS_PER_SECOND;

    if (millis < 0) {
        --seconds;
        millis += MILLIS_PER_SECOND;
    }

    seconds = std::clamp(seconds, static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
                         static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()));
    const auto secondsBits = static_cast<std::uint32_t>(seconds);
    const auto packed = (static_cast<std::uint64_t>(secondsBits) << SECOND_SHIFT) |
                        (static_cast<std::uint64_t>(millis) << MILLIS_SHIFT);

    return std::bit_cast<std::int64_t>(packed);
}
} // namespace

std::int64_t TapeTiming::eventTimeMs() const noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(eventTimeSequence);
    const auto seconds = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> SECOND_SHIFT));

    return static_cast<std::int64_t>(seconds) * MILLIS_PER_SECOND +
           static_cast<std::int64_t>((bits >> MILLIS_SHIFT) & MILLIS_MASK);
}

TapeReader::TapeReader(const core::LocalSchema &schema, TapeLimits limits, TapeMode mode)
    : TapeReader(schema, limits, mode, FieldTimePolicy::DISABLED) {
}

TapeReader::TapeReader(const core::LocalSchema &schema, TapeLimits limits, TapeMode mode,
                       FieldTimePolicy fieldTimePolicy)
    : TapeReader(schema, limits, mode, fieldTimePolicy, TapeExternalTimePolicy::DISABLED) {
}

TapeReader::TapeReader(const core::LocalSchema &schema, TapeLimits limits, TapeMode mode,
                       FieldTimePolicy fieldTimePolicy, TapeExternalTimePolicy externalTimePolicy)
    : limits_(limits), mode_(mode),
      schema_(initializeSchema(schema, limits, mode, fieldTimePolicy, externalTimePolicy)), framer_(limits.frame) {
    fieldTimePolicy_ = fieldTimePolicy;
    externalTimePolicy_ = externalTimePolicy;

    if (mode_ == TapeMode::BINARY_CATALOG_V1) {
        construction_ = initialSessionUnits(schema_, limits_);
        snapshotUnits_ = construction_;
    }

    auto empty = StreamDataContext::createWithFieldTimePolicy(schema_, {}, limits_.context, fieldTimePolicy_);

    if (!empty) {
        if (empty.error() == BindingError::ALLOCATION_FAILURE) {
            throw std::bad_alloc();
        }

        throw std::invalid_argument("invalid empty tape context");
    }

    context_ = std::move(*empty);
}

TapeReader::TapeReader(TapeLimits limits, TapeMode mode)
    : TapeReader(
          [mode](core::SchemaLimits bounds) {
              validateTimePolicies(mode, FieldTimePolicy::DISABLED, TapeExternalTimePolicy::DISABLED);

              auto empty = core::LocalSchema::create({}, bounds, core::LocalProfile::EXTENDED_FIELDS_V1);

              if (!empty) {
                  if (empty.error() == core::SchemaError::ALLOCATION_FAILURE) {
                      throw std::bad_alloc();
                  }

                  throw std::invalid_argument("invalid empty wire schema");
              }

              return std::move(*empty);
          }(limits.context.schema),
          limits, mode) {
    discoverSchema_ = true;
}

TapeReader::TapeReader(TapeLimits limits, TapeDiscovery discovery)
    : TapeReader(limits, discovery, FieldTimePolicy::DISABLED) {
}

TapeReader::TapeReader(TapeLimits limits, TapeDiscovery discovery, FieldTimePolicy fieldTimePolicy)
    : TapeReader(limits, discovery, fieldTimePolicy, TapeExternalTimePolicy::DISABLED) {
}

TapeReader::TapeReader(TapeLimits limits, TapeDiscovery discovery, FieldTimePolicy fieldTimePolicy,
                       TapeExternalTimePolicy externalTimePolicy)
    : limits_(limits), mode_(TapeMode::BINARY_CATALOG_V1), schema_([&] {
          validateTimePolicies(TapeMode::BINARY_CATALOG_V1, fieldTimePolicy, externalTimePolicy);

          if (discovery != TapeDiscovery::BINARY_CATALOG_V1) {
              throw std::invalid_argument("invalid discovery policy");
          }

          if (limits.snapshotUnits == 0 || limits.constructionUnits == 0) {
              throw std::invalid_argument("initial discovery identity exceeds bounds");
          }

          auto empty = core::LocalSchema::create({}, limits.context.schema, core::LocalProfile::BINARY_FIELDS_V1);

          if (!empty) {
              if (empty.error() == core::SchemaError::ALLOCATION_FAILURE) {
                  throw std::bad_alloc();
              }

              throw std::invalid_argument("invalid empty binary schema");
          }

          return std::move(*empty);
      }()),
      framer_(limits.frame) {
    fieldTimePolicy_ = fieldTimePolicy;
    externalTimePolicy_ = externalTimePolicy;
    construction_ = 1;
    snapshotUnits_ = 1;
    auto empty = StreamDataContext::createWithFieldTimePolicy(schema_, {}, limits.context, fieldTimePolicy_);

    if (!empty) {
        if (empty.error() == BindingError::ALLOCATION_FAILURE) {
            throw std::bad_alloc();
        }

        throw std::invalid_argument("invalid empty discovery context");
    }

    context_ = std::move(*empty);
    discoverSchema_ = true;
}

void TapeReader::discoverDescriptions(std::vector<WireRecordDescription> &incoming) {
    // Preflight uses borrowed sources only. No owning local declaration or count-based reserve
    // is permitted until the complete union satisfies every final-schema bound.
    diagnostic_.stage = "wire-schema";

    if (descriptions_.size() > limits_.context.descriptions ||
        incoming.size() > limits_.context.descriptions - descriptions_.size()) {
        fail(TapeError::RESOURCE_LIMIT, "wire-schema");

        return;
    }

    const auto count = descriptions_.size() + incoming.size();
    const auto source = [&](std::size_t index) -> const WireRecordDescription & {
        return index < descriptions_.size() ? descriptions_[index] : incoming[index - descriptions_.size()];
    };
    auto bounds = limits_.context.schema;
    std::size_t shapeRemaining = limits_.constructionUnits;
    std::size_t contextRemaining = limits_.constructionUnits;
    const auto cost = [](std::size_t amount, std::size_t &remaining) {
        return core::detail::chargeBudget(amount, remaining);
    };
    std::size_t localRecords = 0;

    for (std::size_t index = 0; index < count; ++index) {
        const auto &record = source(index);
        bool newRecord = true;

        for (std::size_t previous = 0; previous < index; ++previous) {
            if (source(previous).senderId == record.senderId) {
                fail(TapeError::MALFORMED, "binding", static_cast<std::size_t>(BindingError::INVALID_DESCRIPTION));

                return;
            }

            newRecord = newRecord && source(previous).name != record.name;
        }

        if (!core::detail::validSchemaKey(record.name)) {
            fail(TapeError::MALFORMED, "wire-schema", static_cast<std::size_t>(core::SchemaError::INVALID_SCHEMA));

            return;
        }

        if (!cost(1, contextRemaining) || !cost(record.fields.size(), contextRemaining)) {
            fail(TapeError::RESOURCE_LIMIT, "construction");

            return;
        }

        if (newRecord) {
            if (localRecords > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
                !cost(1, bounds.records) || record.name.size() > bounds.nameUnits ||
                !cost(record.name.size(), bounds.totalNameUnits) || !cost(1, shapeRemaining) ||
                !cost(record.name.size(), shapeRemaining) || !cost(1, contextRemaining)) {
                fail(TapeError::RESOURCE_LIMIT, "wire-schema");

                return;
            }

            ++localRecords;
        }

        for (std::size_t fieldIndex = 0; fieldIndex < record.fields.size(); ++fieldIndex) {
            const auto &field = record.fields[fieldIndex];

            if (!core::supportsSerialType(field.serialTypeId, core::LocalProfile::EXTENDED_FIELDS_V1)) {
                diagnostic_.rejection = TapeRejection::TYPE;
                fail(TapeError::UNSUPPORTED, "wire-schema");

                return;
            }

            if (!core::detail::validSchemaKey(field.name)) {
                fail(TapeError::MALFORMED, "wire-schema", static_cast<std::size_t>(core::SchemaError::INVALID_SCHEMA));

                return;
            }

            for (std::size_t earlierField = 0; earlierField < fieldIndex; ++earlierField) {
                if (record.fields[earlierField].name == field.name) {
                    diagnostic_.rejection = TapeRejection::LOCAL_PROFILE;
                    fail(TapeError::UNSUPPORTED, "wire-schema");

                    return;
                }
            }

            bool newField = true;

            for (std::size_t previous = 0; previous < index; ++previous) {
                const auto &earlier = source(previous);

                if (earlier.name != record.name) {
                    continue;
                }

                for (const auto &known : earlier.fields) {
                    if (known.name == field.name) {
                        if (known.serialTypeId != field.serialTypeId) {
                            diagnostic_.rejection = TapeRejection::TYPE;
                            fail(TapeError::UNSUPPORTED, "wire-schema");

                            return;
                        }

                        newField = false;
                    }
                }
            }

            // One logical field plus its sole generated lookup are two declaration elements.
            constexpr std::size_t FIELD_AND_LOOKUP_ELEMENTS = 2;

            if (newField &&
                (!cost(1, bounds.fields) || !cost(1, bounds.lookups) || field.name.size() > bounds.nameUnits ||
                 !cost(field.name.size(), bounds.totalNameUnits) || !cost(FIELD_AND_LOOKUP_ELEMENTS, shapeRemaining) ||
                 !cost(field.name.size(), shapeRemaining) || !cost(1, contextRemaining))) {
                fail(TapeError::RESOURCE_LIMIT, "wire-schema");

                return;
            }
        }
    }

    const auto charge = [&](std::size_t amount) {
        if (amount > limits_.constructionUnits - construction_) {
            fail(TapeError::RESOURCE_LIMIT, "construction");

            return false;
        }

        construction_ += amount;

        return true;
    };
    // Charge only actual name copies. Fresh raw descriptions already own their names and move.
    std::size_t copyRemaining = limits_.constructionUnits;

    for (const auto &record : descriptions_) {
        if (!cost(1, copyRemaining) || !cost(record.name.size(), copyRemaining) ||
            !cost(record.fields.size(), copyRemaining)) {
            fail(TapeError::RESOURCE_LIMIT, "construction");

            return;
        }

        for (const auto &field : record.fields) {
            if (!cost(field.name.size(), copyRemaining)) {
                fail(TapeError::RESOURCE_LIMIT, "construction");

                return;
            }
        }
    }

    if (!charge(limits_.constructionUnits - copyRemaining)) {
        return;
    }

    auto candidate = descriptions_;

    for (auto &record : incoming) {
        candidate.push_back(std::move(record));
    }

    const auto shapeUnits = limits_.constructionUnits - shapeRemaining;

    if (!charge(shapeUnits)) {
        return;
    }

    std::vector<core::LocalRecord> records;

    for (const auto &record : candidate) {
        auto found = std::find_if(records.begin(), records.end(), [&](const auto &local) {
            return local.name == record.name;
        });

        if (found == records.end()) {
            records.push_back({static_cast<std::int32_t>(records.size()), record.name, false, {}, {}});
            found = records.end() - 1;
        }

        for (const auto &field : record.fields) {
            const auto key = std::find_if(found->lookups.begin(), found->lookups.end(), [&](const auto &lookup) {
                return lookup.name == field.name;
            });

            if (key == found->lookups.end()) {
                found->lookups.push_back({field.name, found->fields.size()});
                found->fields.push_back({field.serialTypeId});
            }
        }
    }

    if (!charge(shapeUnits)) {
        return;
    }

    auto nextSchema =
        core::LocalSchema::create(records, limits_.context.schema, core::LocalProfile::EXTENDED_FIELDS_V1);

    if (!nextSchema) {
        const auto error = nextSchema.error();
        fail(error == core::SchemaError::ALLOCATION_FAILURE ? TapeError::ALLOCATION_FAILURE
             : error == core::SchemaError::RESOURCE_LIMIT   ? TapeError::RESOURCE_LIMIT
                                                            : TapeError::MALFORMED,
             "wire-schema", static_cast<std::size_t>(error));

        return;
    }

    if (!charge(limits_.constructionUnits - contextRemaining)) {
        return;
    }

    auto nextContext = StreamDataContext::create(*nextSchema, candidate, limits_.context);

    if (!nextContext) {
        fail(bindingError(nextContext.error()), "binding", static_cast<std::size_t>(nextContext.error()));

        return;
    }

    // No throwing operation or external callback is allowed after publication starts.
    static_assert(noexcept(schema_ = std::move(*nextSchema)));
    static_assert(noexcept(context_ = std::move(*nextContext)));
    static_assert(noexcept(descriptions_.swap(candidate)));
    schema_ = std::move(*nextSchema);
    context_ = std::move(*nextContext);
    descriptions_.swap(candidate);
}

void TapeReader::replaceDescriptions(std::span<const std::byte> payload) {
    detail::SessionBudget budget(snapshotUnits_, limits_.snapshotUnits, construction_, limits_.constructionUnits);

    if (!budget.work(payload.size())) {
        fail(TapeError::RESOURCE_LIMIT, "construction");

        return;
    }

    diagnostic_.stage = "description";
    auto parsed =
        detail::parseSessionDescriptions(payload,
                                         {limits_.frame.maximumBodyBytes,
                                          limits_.context.descriptions,
                                          limits_.context.bindings.fields,
                                          limits_.context.bindings.fields,
                                          {limits_.frame.maximumBodyBytes, limits_.context.bindings.nameUnits},
                                          limits_.context.bindings.totalNameUnits},
                                         budget);
    diagnostic_.consumed = parsed.consumed;
    diagnostic_.errorOffset = parsed.errorOffset;

    if (parsed.status != DescriptionStatus::OK) {
        const auto error = parsed.status == DescriptionStatus::RESOURCE_LIMIT       ? TapeError::RESOURCE_LIMIT
                           : parsed.status == DescriptionStatus::ALLOCATION_FAILURE ? TapeError::ALLOCATION_FAILURE
                                                                                    : TapeError::MALFORMED;
        fail(error, "description", static_cast<std::size_t>(parsed.stage));

        return;
    }

    if (parsed.descriptions.empty()) {
        return;
    }

    std::optional<core::LocalSchema> nextSchema;

    if (discoverSchema_) {
        auto discovered = discoverBinarySchema(schema_, parsed.descriptions, limits_, budget, fieldTimePolicy_);

        if (!discovered) {
            if (discovered.error() == TapeError::UNSUPPORTED) {
                diagnostic_.rejection = TapeRejection::TYPE;
            }

            fail(discovered.error(), "wire-schema");

            return;
        }

        nextSchema.emplace(std::move(*discovered));
    }

    diagnostic_.stage = "construction";
    const auto ceiling = limits_.snapshotUnits;
    const auto rawUnits = descriptionUnits(parsed.descriptions, ceiling);
    const auto oldUnits = descriptionUnits(descriptions_, ceiling);

    if (!rawUnits || !oldUnits || !budget.admit(*oldUnits)) {
        fail(TapeError::RESOURCE_LIMIT, "construction");

        return;
    }

    auto candidate = descriptions_;
    auto candidateUnits = *oldUnits;

    for (const auto &incoming : parsed.descriptions) {
        UnitCount temporaryCount(ceiling);
        temporaryCount.add(1);
        temporaryCount.add(incoming.fields.size());
        temporaryCount.add(fieldTimePolicy_ == FieldTimePolicy::READ ? 2 : 0);
        const auto temporaryUnits = temporaryCount.result();

        if (!temporaryUnits || !budget.admit(*temporaryUnits)) {
            fail(TapeError::RESOURCE_LIMIT, "construction");

            return;
        }

        {
            // Validate every wire-order entry, including an intermediate entry later overwritten.
            auto validated = detail::bindRecordWithPrefix(
                incoming, nextSchema ? *nextSchema : schema_, limits_.context.bindings,
                fieldTimePolicy_ == FieldTimePolicy::READ ? detail::eventTimeFieldCount(incoming) : 0);

            if (!validated) {
                if (validated.error() == BindingError::UNSUPPORTED_PROFILE) {
                    diagnostic_.rejection = TapeRejection::TYPE;
                }

                fail(bindingError(validated.error()), "binding", static_cast<std::size_t>(validated.error()));

                return;
            }
        }

        budget.release(*temporaryUnits);
        const auto existing = std::find_if(candidate.begin(), candidate.end(), [&](const auto &record) {
            return record.senderId == incoming.senderId;
        });

        if (existing == candidate.end() && candidate.size() == limits_.context.descriptions) {
            fail(TapeError::RESOURCE_LIMIT, "construction");

            return;
        }

        const auto copiedUnits = descriptionUnits(std::span{&incoming, 1}, ceiling);

        if (!copiedUnits || !budget.admit(*copiedUnits)) {
            fail(TapeError::RESOURCE_LIMIT, "construction");

            return;
        }

        auto replacement = incoming;

        if (existing == candidate.end()) {
            candidate.push_back(std::move(replacement));
            candidateUnits += *copiedUnits; // Both owners have already passed the subtraction-based peak check.
        } else {
            const auto released = descriptionUnits(std::span{&*existing, 1}, ceiling);
            static_assert(noexcept(*existing = std::move(replacement)));
            *existing = std::move(replacement);
            budget.release(*released);
            candidateUnits = candidateUnits - *released + *copiedUnits;
        }
    }

    parsed.descriptions.clear();
    budget.release(*rawUnits);
    const auto localUnits = schemaUnits(nextSchema ? *nextSchema : schema_, ceiling);

    if (!localUnits || (!nextSchema && !budget.admit(*localUnits))) {
        fail(TapeError::RESOURCE_LIMIT, "construction");

        return;
    }

    // create issues a fresh identity; boundedCopy deliberately retains an old generation instead.
    if (!nextSchema) {
        auto copied = core::LocalSchema::create(schema_.records(), limits_.context.schema, schema_.profile());

        if (!copied) {
            fail(copied.error() == core::SchemaError::ALLOCATION_FAILURE ? TapeError::ALLOCATION_FAILURE
                                                                         : TapeError::RESOURCE_LIMIT,
                 "construction");

            return;
        }

        nextSchema.emplace(std::move(*copied));
    }

    const auto nextContextUnits = contextUnits(*nextSchema, candidate, ceiling, fieldTimePolicy_);

    if (!nextContextUnits || !budget.admit(*nextContextUnits)) {
        fail(TapeError::RESOURCE_LIMIT, "construction");

        return;
    }

    auto nextContext =
        StreamDataContext::createWithFieldTimePolicy(*nextSchema, candidate, limits_.context, fieldTimePolicy_);

    if (!nextContext) {
        if (nextContext.error() == BindingError::UNSUPPORTED_PROFILE) {
            diagnostic_.rejection = TapeRejection::TYPE;
        }

        fail(bindingError(nextContext.error()), "binding", static_cast<std::size_t>(nextContext.error()));

        return;
    }

    static_assert(noexcept(schema_ = std::move(*nextSchema)));
    static_assert(noexcept(context_ = std::move(*nextContext)));
    static_assert(noexcept(descriptions_.swap(candidate)));
    schema_ = std::move(*nextSchema);
    context_ = std::move(*nextContext);
    descriptions_.swap(candidate);
    snapshotUnits_ =
        *localUnits + candidateUnits + *nextContextUnits + (descriptor_ ? descriptorUnits(*descriptor_) : 0);
}

void TapeReader::fail(TapeError error, std::string_view stage, std::size_t detail) noexcept {
    if (status_ == TapeStatus::FAILED) {
        return;
    }

    diagnostic_.error = error;
    diagnostic_.stage = stage;
    diagnostic_.detail = detail;
    diagnostic_.accepted = framer_.totalAccepted();
    status_ = TapeStatus::FAILED;
}

void TapeReader::dispatch(TapeFeedResult &result) {
    const auto &frame = *result.frame;
    diagnostic_ = {};
    diagnostic_.frameStart = frame.startOffset;

    if (frame.kind == FrameKind::MESSAGE) {
        diagnostic_.payloadStart = frame.endOffset - frame.payload.size();
    }

    const bool catalog = mode_ == TapeMode::BINARY_CATALOG_V1;

    if (catalog && frame.kind == FrameKind::MESSAGE && frame.type == DESCRIBE_PROTOCOL) {
        detail::SessionBudget budget(snapshotUnits_, limits_.snapshotUnits, construction_, limits_.constructionUnits);

        if (!budget.work(frame.payload.size())) {
            fail(TapeError::RESOURCE_LIMIT, "construction");

            return;
        }

        DescriptorParser parser(frame.payload, limits_.descriptor, diagnostic_, mode_, fieldTimePolicy_,
                                externalTimePolicy_);
        try {
            auto candidate = parser.parseCatalog(descriptor_, budget);
            const auto old = descriptor_ ? descriptorUnits(*descriptor_) : 0;
            const auto next = descriptorUnits(candidate);
            static_assert(std::is_nothrow_move_assignable_v<TapeDescriptor>);
            descriptor_ = std::move(candidate);
            snapshotUnits_ = snapshotUnits_ - old + next;
        } catch (const DescriptorFailure &failure) {
            fail(failure.error, "descriptor");
        }

        return;
    }

    if (!catalog && !descriptor_) {
        if (frame.type != DESCRIBE_PROTOCOL) {
            diagnostic_.rejection = TapeRejection::SEQUENCE;
            fail(TapeError::UNSUPPORTED, "header");

            return;
        }

        DescriptorParser parser(frame.payload, limits_.descriptor, diagnostic_, mode_, fieldTimePolicy_,
                                externalTimePolicy_);
        try {
            descriptor_ = parser.parse();
        } catch (const DescriptorFailure &failure) {
            fail(failure.error, "descriptor");
        }

        return;
    }

    if (frame.kind == FrameKind::EMPTY_HEARTBEAT) {
        ++markers_;

        return;
    }

    if ((mode_ == TapeMode::RAW_MESSAGE_V1 && (frame.type == HEARTBEAT || frame.type == RAW_DATA)) ||
        (catalog && (frame.type == HEARTBEAT || isDataFamily(*frame.type)))) {
        diagnostic_.consumed = 0;
        diagnostic_.errorOffset = 0;

        if (timingEvents_ == limits_.timingEvents) {
            fail(TapeError::RESOURCE_LIMIT, "timing");

            return;
        }

        ++timingEvents_;
    }

    if ((mode_ == TapeMode::RAW_MESSAGE_V1 || catalog) && frame.type == HEARTBEAT) {
        diagnostic_.stage = "heartbeat";
        TapeHeartbeat heartbeat;
        std::size_t cursor = 0;

        if (!frame.payload.empty()) {
            const auto content = encoding::readCompactInt(frame.payload);
            cursor += content.consumed;
            diagnostic_.consumed = cursor;

            if (content.status != encoding::Status::OK) {
                fail(TapeError::MALFORMED, "heartbeat");

                return;
            }

            if (content.value != 0 && content.value != 1) {
                diagnostic_.rejection = TapeRejection::TIME;
                fail(TapeError::UNSUPPORTED, "heartbeat");

                return;
            }

            heartbeat.content = content.value;

            if (content.value == 1) {
                diagnostic_.errorOffset = cursor;
                const auto timestamp = encoding::readCompactLong(std::span(frame.payload).subspan(cursor));
                cursor += timestamp.consumed;
                diagnostic_.consumed = cursor;

                if (timestamp.status != encoding::Status::OK) {
                    fail(TapeError::MALFORMED, "heartbeat");

                    return;
                }

                heartbeat.timestampMs = timestamp.value;
            }
        }

        diagnostic_.errorOffset = cursor;

        if (cursor != frame.payload.size()) {
            diagnostic_.rejection = TapeRejection::TIME;
            fail(TapeError::UNSUPPORTED, "heartbeat");

            return;
        }

        if (heartbeat.timestampMs) {
            timing_ = {heartbeat.timestampMs, packTimestamp(*heartbeat.timestampMs)};
        }

        result.heartbeat = heartbeat;

        return;
    }

    if (frame.type == DESCRIBE_RECORDS) {
        if (mode_ == TapeMode::BINARY_CATALOG_V1) {
            replaceDescriptions(frame.payload);

            return;
        }

        if (discoverSchema_) {
            // Each materialized raw record/field needs a structural byte; decoded name units
            // never exceed their encoded bytes. This conservative charge includes failed parses.
            if (frame.payload.size() > limits_.constructionUnits - construction_) {
                fail(TapeError::RESOURCE_LIMIT, "construction");

                return;
            }

            construction_ += frame.payload.size();
        }

        diagnostic_.stage = "description";
        auto parsed =
            parseDescribeRecords(frame.payload, {limits_.frame.maximumBodyBytes,
                                                 limits_.context.descriptions - descriptions_.size(),
                                                 limits_.context.bindings.fields,
                                                 limits_.context.bindings.fields,
                                                 {limits_.frame.maximumBodyBytes, limits_.context.bindings.nameUnits},
                                                 limits_.context.bindings.totalNameUnits});
        diagnostic_.consumed = parsed.consumed;
        diagnostic_.errorOffset = parsed.errorOffset;

        if (parsed.status != DescriptionStatus::OK) {
            auto error = parsed.status == DescriptionStatus::RESOURCE_LIMIT       ? TapeError::RESOURCE_LIMIT
                         : parsed.status == DescriptionStatus::ALLOCATION_FAILURE ? TapeError::ALLOCATION_FAILURE
                                                                                  : TapeError::MALFORMED;
            fail(error, "description", static_cast<std::size_t>(parsed.stage));

            return;
        }

        if (discoverSchema_) {
            discoverDescriptions(parsed.descriptions);

            return;
        }

        diagnostic_.stage = "construction";
        std::size_t remaining = limits_.constructionUnits - construction_;
        const auto charge = [&](std::size_t amount) {
            if (amount > remaining) {
                return false;
            }

            remaining -= amount;

            return true;
        };
        bool fits = true;

        for (const auto &r : schema_.records()) {
            fits = fits && charge(1) && charge(r.name.size()) && charge(r.fields.size()) && charge(r.lookups.size());

            for (const auto &key : r.lookups) {
                fits = fits && charge(key.name.size());
            }
        }

        for (auto list : {std::span<const WireRecordDescription>(descriptions_),
                          std::span<const WireRecordDescription>(parsed.descriptions)}) {
            for (const auto &r : list) {
                fits = fits && charge(1) && charge(r.name.size()) && charge(r.fields.size());

                for (const auto &f : r.fields) {
                    fits = fits && charge(f.name.size());
                }
            }
        }

        if (!fits) {
            fail(TapeError::RESOURCE_LIMIT, "construction");

            return;
        }

        construction_ = limits_.constructionUnits - remaining;
        auto candidate = descriptions_;

        for (auto &r : parsed.descriptions) {
            candidate.push_back(std::move(r));
        }

        auto next = StreamDataContext::create(schema_, candidate, limits_.context);

        if (!next) {
            if (next.error() == BindingError::UNSUPPORTED_PROFILE) {
                diagnostic_.rejection = TapeRejection::TYPE;
            }

            fail(bindingError(next.error()), "binding", static_cast<std::size_t>(next.error()));

            return;
        }

        descriptions_.swap(candidate);
        context_ = std::move(*next);

        return;
    }

    if (catalog && frame.type == DESCRIBE_RESERVED) {
        return;
    }

    if (catalog ? !isDataFamily(*frame.type)
                : frame.type != (mode_ == TapeMode::RAW_MESSAGE_V1 ? RAW_DATA : STREAM_DATA)) {
        diagnostic_.rejection = frame.type == DESCRIBE_PROTOCOL ? TapeRejection::SEQUENCE : TapeRejection::MESSAGE;
        fail(TapeError::UNSUPPORTED, "type");

        return;
    }

    diagnostic_.stage = "data";

    if (mode_ == TapeMode::RAW_MESSAGE_V1 || catalog) {
        result.timing = timing_;
    }

    auto budget = limits_.data;
    budget.processedRecords -= records_;
    budget.wireFields -= fields_;
    budget.emittedRecords = std::min(budget.emittedRecords - emitted_, budget.processedRecords);
    budget.emittedValues -= values_;
    budget.ownedSymbolUnits -= symbols_;
    budget.fieldBytes -= fieldBytes_;
    budget.fieldUnits -= fieldUnits_;
    budget.ownedFieldUnits -= ownedFieldUnits_;
    budget.payloadEncodedBytes -= payloadEncodedBytes_;
    budget.payloadWorkBytes -= payloadWorkBytes_;
    budget.ownedPayloadBytes -= ownedPayloadBytes_;
    budget.ownedFieldTimeRecords -= ownedFieldTimeRecords_;

    if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
        budget.processedRecords = std::min(
            budget.processedRecords, static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max()) - records_);
    }

    auto decoded =
        mode_ == TapeMode::RAW_MESSAGE_V1 ? decodeRawData(frame.payload, *context_, budget)
        : mode_ == TapeMode::BINARY_CATALOG_V1
            ? decodeStreamDataWithEventPolicy(frame.payload, *context_, budget, DataEventPolicy::BINARY_CATALOG_V1)
            : decodeStreamData(frame.payload, *context_, budget);

    for (auto &record : decoded.records) {
        if (record.fieldTime) {
            record.fieldTime->sourceOrdinal += static_cast<std::uint64_t>(records_);
        }
    }

    records_ += decoded.processedRecords;
    fields_ += decoded.wireFields;
    fieldBytes_ += decoded.fieldBytes;
    fieldUnits_ += decoded.fieldUnits;
    ownedFieldUnits_ += decoded.ownedFieldUnits;
    payloadEncodedBytes_ += decoded.payloadEncodedBytes;
    payloadWorkBytes_ += decoded.payloadWorkBytes;
    ownedPayloadBytes_ += decoded.ownedPayloadBytes;
    ownedFieldTimeRecords_ += decoded.ownedFieldTimeRecords;

    for (const auto &r : decoded.records) {
        values_ += r.values.size() + r.typedValues.size();
        symbols_ += r.symbol.units.size();
    }

    emitted_ += decoded.records.size();
    result.records = std::move(decoded.records);
    diagnostic_.consumed = decoded.consumed;
    diagnostic_.errorOffset = decoded.errorOffset;

    if (decoded.status != StreamDataStatus::OK) {
        if (decoded.status == StreamDataStatus::UNSUPPORTED_PROFILE) {
            diagnostic_.rejection = TapeRejection::OPTIONS;
        }

        auto error = decoded.status == StreamDataStatus::RESOURCE_LIMIT        ? TapeError::RESOURCE_LIMIT
                     : decoded.status == StreamDataStatus::ALLOCATION_FAILURE  ? TapeError::ALLOCATION_FAILURE
                     : decoded.status == StreamDataStatus::UNSUPPORTED_PROFILE ? TapeError::UNSUPPORTED
                                                                               : TapeError::MALFORMED;
        fail(error, "data", static_cast<std::size_t>(decoded.stage));
    }
}

TapeFeedResult TapeReader::feed(std::span<const std::byte> input) {
    TapeFeedResult result;
    result.status = status_;

    if (status_ == TapeStatus::FAILED || status_ == TapeStatus::FINISHED) {
        return result;
    }

    status_ = TapeStatus::NEED_INPUT;

    if (input.empty()) {
        result.status = status_;

        return result;
    }

    diagnostic_ = {};
    diagnostic_.frameStart = framer_.totalAccepted() - framer_.pending().size();

    if (frames_ == limits_.frames) {
        fail(TapeError::RESOURCE_LIMIT, "frames");
        result.status = status_;

        return result;
    }

    auto remaining = limits_.inputBytes - static_cast<std::size_t>(framer_.totalAccepted());

    if (remaining == 0) {
        fail(TapeError::RESOURCE_LIMIT, "input");
        result.status = status_;

        return result;
    }

    auto framed = framer_.feed(input.first(std::min(input.size(), remaining)));
    result.accepted = framed.accepted;

    if (framed.status == FrameStatus::FAILED) {
        auto e = framer_.error();
        auto error = e == FrameError::BODY_LIMIT || e == FrameError::OFFSET_OVERFLOW ? TapeError::RESOURCE_LIMIT
                     : e == FrameError::ALLOCATION_FAILURE                           ? TapeError::ALLOCATION_FAILURE
                                                                                     : TapeError::MALFORMED;
        fail(error, "framing", static_cast<std::size_t>(e));
    } else if (framed.frame) {
        ++frames_;
        result.frame = std::move(framed.frame);
        status_ = TapeStatus::EVENT;
        try {
            dispatch(result);
        } catch (const std::bad_alloc &) {
            fail(TapeError::ALLOCATION_FAILURE, diagnostic_.stage);
        } catch (const std::length_error &) {
            fail(TapeError::ALLOCATION_FAILURE, diagnostic_.stage);
        }
    }

    result.status = status_;

    return result;
}

TapeStatus TapeReader::finish() noexcept {
    if (status_ == TapeStatus::FAILED || status_ == TapeStatus::FINISHED) {
        return status_;
    }

    diagnostic_ = {};
    diagnostic_.frameStart = framer_.totalAccepted() - framer_.pending().size();

    if (framer_.finish() == FrameStatus::FAILED) {
        fail(TapeError::MALFORMED, "framing", static_cast<std::size_t>(framer_.error()));
    } else if (!descriptor_ && !(mode_ == TapeMode::BINARY_CATALOG_V1 && !descriptions_.empty())) {
        fail(TapeError::MALFORMED, "header");
    } else {
        status_ = TapeStatus::FINISHED;
    }

    return status_;
}
} // namespace org::ttldtor::qdcxx::protocol
