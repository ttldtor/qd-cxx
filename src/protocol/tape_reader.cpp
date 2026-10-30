// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/protocol/tape_reader.hpp"
#include "../core/schema_validation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {
namespace {
constexpr std::int32_t DESCRIBE_PROTOCOL = 1;    ///< Required initial header.
constexpr std::int32_t DESCRIBE_RECORDS = 2;     ///< Append-only description update.
constexpr std::int32_t STREAM_DATA = 15;         ///< Legacy data message.
constexpr std::int32_t RAW_DATA = 5;             ///< Opt-in raw wire records.
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
    TapeDiagnostic &position_;
    std::size_t cursor_ = 0;
    std::size_t units_ = 0;
    std::size_t properties_ = 0;
    std::size_t messages_ = 0;

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
     */
    DescriptorParser(std::span<const std::byte> payload, TapeDescriptorLimits limits, TapeDiagnostic &position,
                     TapeMode mode)
        : payload_(payload), limits_(limits), mode_(mode), position_(position) {
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
                if (!value->empty() && !(mode_ == TapeMode::RAW_MESSAGE_V1 && *value == u"hs")) {
                    reject(TapeError::UNSUPPORTED, TapeRejection::OPTIONS);
                }

                opt = true;
                result.options = std::move(value);
            } else if (*key == u"time") {
                if (mode_ == TapeMode::LEGACY ? *value != u"NONE" : (*value != u"MESSAGE" && *value != u"message")) {
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

                const bool acceptedMessage = mode_ == TapeMode::LEGACY ? id == STREAM_DATA && *name == u"STREAM_DATA"
                                                                       : id == RAW_DATA && *name == u"RAW_DATA";

                if (props != 0 || direction != 0 || entries != 1 || !acceptedMessage) {
                    reject(TapeError::UNSUPPORTED, TapeRejection::MESSAGE);
                }

                result.explicitStreamSend = mode_ == TapeMode::LEGACY;
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
    : limits_(limits), mode_(mode), schema_(copySchema(schema, limits.context.schema)), framer_(limits.frame) {
    if (mode_ != TapeMode::LEGACY && mode_ != TapeMode::RAW_MESSAGE_V1) {
        throw std::invalid_argument("invalid tape mode");
    }

    auto empty = StreamDataContext::create(schema_, {}, limits_.context);

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
          [](core::SchemaLimits bounds) {
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

    if (!descriptor_) {
        if (frame.type != DESCRIBE_PROTOCOL) {
            diagnostic_.rejection = TapeRejection::SEQUENCE;
            fail(TapeError::UNSUPPORTED, "header");

            return;
        }

        DescriptorParser parser(frame.payload, limits_.descriptor, diagnostic_, mode_);
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

    if (mode_ == TapeMode::RAW_MESSAGE_V1 && (frame.type == HEARTBEAT || frame.type == RAW_DATA)) {
        diagnostic_.consumed = 0;
        diagnostic_.errorOffset = 0;

        if (timingEvents_ == limits_.timingEvents) {
            fail(TapeError::RESOURCE_LIMIT, "timing");

            return;
        }

        ++timingEvents_;
    }

    if (mode_ == TapeMode::RAW_MESSAGE_V1 && frame.type == HEARTBEAT) {
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

    if (frame.type != (mode_ == TapeMode::LEGACY ? STREAM_DATA : RAW_DATA)) {
        diagnostic_.rejection = frame.type == DESCRIBE_PROTOCOL ? TapeRejection::SEQUENCE : TapeRejection::MESSAGE;
        fail(TapeError::UNSUPPORTED, "type");

        return;
    }

    diagnostic_.stage = "data";

    if (mode_ == TapeMode::RAW_MESSAGE_V1) {
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
    auto decoded = mode_ == TapeMode::LEGACY ? decodeStreamData(frame.payload, *context_, budget)
                                             : decodeRawData(frame.payload, *context_, budget);
    records_ += decoded.processedRecords;
    fields_ += decoded.wireFields;
    fieldBytes_ += decoded.fieldBytes;
    fieldUnits_ += decoded.fieldUnits;
    ownedFieldUnits_ += decoded.ownedFieldUnits;

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
    } else if (!descriptor_) {
        fail(TapeError::MALFORMED, "header");
    } else {
        status_ = TapeStatus::FINISHED;
    }

    return status_;
}
} // namespace org::ttldtor::qdcxx::protocol
