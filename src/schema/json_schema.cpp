// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/schema/json_schema.hpp"

#include <simdjson.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::schema {
namespace {

/** Fixed-size decoded member collection; no untrusted member count controls allocation.
 * @tparam N Number of permitted grammar members.
 */
template <std::size_t N> struct ObjectMembers {
    std::array<simdjson::dom::element, N> values{}; ///< Borrowed DOM values by grammar position.
    std::size_t seen = 0;                           ///< Bitset of present known members.
    bool unknown = false;                           ///< Whether any unknown member occurred.

    /** Inspect whether the complete required member set was supplied.
     * @return True for every required member and no unknown members.
     */
    [[nodiscard]] bool complete() const noexcept {
        return seen == (std::size_t{1} << N) - 1 && !unknown;
    }
};

/** Collect known members and reject duplicate decoded keys, using a fixed bitset.
 * @tparam N Number of permitted grammar members.
 * @param value Object candidate in the live DOM.
 * @param keys Required keys in the returned positional order.
 * @param deferShape Allow root identity checks before missing/unknown-member rejection.
 * @return Borrowed members, or an invalid object shape.
 */
template <std::size_t N>
std::expected<ObjectMembers<N>, JsonSchemaError>
readObject(simdjson::dom::element value, const std::array<std::string_view, N> &keys, bool deferShape = false) {
    simdjson::dom::object object;

    if (value.get_object().get(object)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    ObjectMembers<N> members;

    for (const auto member : object) {
        const auto position = std::find(keys.begin(), keys.end(), member.key);

        if (position == keys.end()) {
            members.unknown = true;
            continue;
        }

        const auto index = static_cast<std::size_t>(position - keys.begin());
        const auto bit = std::size_t{1} << index;

        if ((members.seen & bit) != 0) {
            return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
        }

        members.seen |= bit;
        members.values[index] = member.value;
    }

    if (!deferShape && !members.complete()) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    return members;
}

/** Require an integer JSON kind without float rounding or coercion.
 * @param value Candidate numeric member.
 * @return Nonnegative signed-32-bit member, or invalid kind/range.
 */
std::expected<std::int32_t, JsonSchemaError> readInteger(simdjson::dom::element value) {
    std::int64_t number = 0;

    if (value.get_int64().get(number) || number < 0 || number > std::numeric_limits<std::int32_t>::max()) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    return static_cast<std::int32_t>(number);
}

/** Charge a finite remaining budget before a proportional allocation.
 * @param amount Requested count.
 * @param remaining Remaining budget, reduced only on success.
 * @return True if the entire count fits.
 */
bool charge(std::size_t amount, std::size_t &remaining) noexcept {
    if (amount > remaining) {
        return false;
    }

    remaining -= amount;

    return true;
}

/** Convert validated scalar UTF-8 to owned UTF-16 after charging exact units.
 * @param value Candidate string in the validated DOM.
 * @param limits Remaining logical budgets; only totalNameUnits is reduced.
 * @return Exact owned units, or invalid kind/budget exhaustion.
 */
std::expected<std::u16string, JsonSchemaError> readName(simdjson::dom::element value, core::SchemaLimits &limits) {
    std::string_view text;

    if (value.get_string().get(text)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    constexpr unsigned char CONTINUATION_MASK = 0xc0;
    constexpr unsigned char CONTINUATION_TAG = 0x80;
    constexpr unsigned char CONTINUATION_PAYLOAD_MASK = 0x3f;
    constexpr unsigned CONTINUATION_PAYLOAD_BITS = 6;
    constexpr unsigned char TWO_BYTE_LEAD = 0xc0;
    constexpr unsigned char THREE_BYTE_LEAD = 0xe0;
    constexpr unsigned char FOUR_BYTE_LEAD = 0xf0;
    std::size_t units = 0;

    for (const unsigned char byte : text) {
        if ((byte & CONTINUATION_MASK) != CONTINUATION_TAG) {
            units += byte >= FOUR_BYTE_LEAD ? 2 : 1;
        }
    }

    if (units > limits.nameUnits || !charge(units, limits.totalNameUnits)) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    std::u16string result;
    result.reserve(units);

    for (std::size_t offset = 0; offset < text.size();) {
        const auto lead = static_cast<unsigned char>(text[offset++]);
        std::uint32_t scalar = lead;
        std::size_t continuationCount = 0;

        if (lead >= FOUR_BYTE_LEAD) {
            scalar = lead & 0x07; // Four-byte UTF-8 carries three leading payload bits.
            continuationCount = 3;
        } else if (lead >= THREE_BYTE_LEAD) {
            scalar = lead & 0x0f; // Three-byte UTF-8 carries four leading payload bits.
            continuationCount = 2;
        } else if (lead >= TWO_BYTE_LEAD) {
            scalar = lead & 0x1f; // Two-byte UTF-8 carries five leading payload bits.
            continuationCount = 1;
        }

        for (std::size_t i = 0; i < continuationCount; ++i) {
            scalar = (scalar << CONTINUATION_PAYLOAD_BITS) |
                     (static_cast<unsigned char>(text[offset++]) & CONTINUATION_PAYLOAD_MASK);
        }

        constexpr std::uint32_t SUPPLEMENTARY_START = 0x10000;
        constexpr std::uint32_t HIGH_SURROGATE_START = 0xd800;
        constexpr std::uint32_t LOW_SURROGATE_START = 0xdc00;
        constexpr std::uint32_t SURROGATE_PAYLOAD_MASK = 0x3ff;
        constexpr unsigned SURROGATE_PAYLOAD_BITS = 10;

        if (scalar < SUPPLEMENTARY_START) {
            result.push_back(static_cast<char16_t>(scalar));
        } else {
            scalar -= SUPPLEMENTARY_START;
            result.push_back(static_cast<char16_t>(HIGH_SURROGATE_START + (scalar >> SURROGATE_PAYLOAD_BITS)));
            result.push_back(static_cast<char16_t>(LOW_SURROGATE_START + (scalar & SURROGATE_PAYLOAD_MASK)));
        }
    }

    return result;
}

/** Read one native record while bounding all owned arrays and names.
 * @param value Candidate record object.
 * @param limits Remaining cumulative schema budgets.
 * @return Owned declaration for authoritative core validation, or typed loading error.
 */
std::expected<core::LocalRecord, JsonSchemaError> readRecord(simdjson::dom::element value, core::SchemaLimits &limits) {
    constexpr std::array<std::string_view, 5> RECORD_KEYS{"id", "name", "hasTime", "fields", "lookups"};
    const auto members = readObject(value, RECORD_KEYS);

    if (!members) {
        return std::unexpected(members.error());
    }

    const auto id = readInteger(members->values[0]);

    if (!id) {
        return std::unexpected(id.error());
    }

    bool hasTime = false;

    if (members->values[2].get_bool().get(hasTime)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    if (hasTime) {
        return std::unexpected(JsonSchemaError::UNSUPPORTED_INDEXED);
    }

    simdjson::dom::array fields;
    simdjson::dom::array lookups;

    if (members->values[3].get_array().get(fields) || members->values[4].get_array().get(lookups)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    if (!charge(fields.size(), limits.fields) || !charge(lookups.size(), limits.lookups)) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    auto name = readName(members->values[1], limits);

    if (!name) {
        return std::unexpected(name.error());
    }

    core::LocalRecord record{*id, std::move(*name), false, {}, {}};
    record.fields.reserve(fields.size());
    record.lookups.reserve(lookups.size());
    constexpr std::array<std::string_view, 1> FIELD_KEYS{"serialTypeId"};

    for (const auto field : fields) {
        const auto fieldMembers = readObject(field, FIELD_KEYS);

        if (!fieldMembers) {
            return std::unexpected(fieldMembers.error());
        }

        const auto type = readInteger(fieldMembers->values[0]);

        if (!type) {
            return std::unexpected(type.error());
        }

        if (!core::supportsSerialType(*type, core::LocalProfile::EXTENDED_FIELDS_V1)) {
            return std::unexpected(JsonSchemaError::UNSUPPORTED_TYPE);
        }

        record.fields.push_back({*type});
    }

    constexpr std::array<std::string_view, 2> LOOKUP_KEYS{"name", "fieldId"};

    for (const auto lookup : lookups) {
        const auto lookupMembers = readObject(lookup, LOOKUP_KEYS);

        if (!lookupMembers) {
            return std::unexpected(lookupMembers.error());
        }

        const auto fieldId = readInteger(lookupMembers->values[1]);

        if (!fieldId) {
            return std::unexpected(fieldId.error());
        }

        auto key = readName(lookupMembers->values[0], limits);

        if (!key) {
            return std::unexpected(key.error());
        }

        record.lookups.push_back({std::move(*key), static_cast<std::size_t>(*fieldId)});
    }

    return record;
}

/** Resolve identity before interpreting the native record grammar.
 * @param root Fully parsed document root.
 * @param limits Original logical budgets for loading and authoritative core validation.
 * @return Complete owned schema or typed error.
 */
std::expected<core::LocalSchema, JsonSchemaError> readSchema(simdjson::dom::element root, core::SchemaLimits limits) {
    constexpr std::array<std::string_view, 3> ROOT_KEYS{"formatVersion", "profile", "records"};
    const auto members = readObject(root, ROOT_KEYS, true);
    constexpr std::size_t IDENTITY_MEMBERS = 0b011;

    if (!members || (members->seen & IDENTITY_MEMBERS) != IDENTITY_MEMBERS) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    const auto formatVersion = readInteger(members->values[0]);

    if (!formatVersion) {
        return std::unexpected(formatVersion.error());
    }

    constexpr std::int32_t FORMAT_VERSION = 1;

    if (*formatVersion != FORMAT_VERSION) {
        return std::unexpected(JsonSchemaError::UNSUPPORTED_VERSION);
    }

    constexpr std::array<std::string_view, 2> PROFILE_KEYS{"id", "version"};
    const auto profile = readObject(members->values[1], PROFILE_KEYS);

    if (!profile) {
        return std::unexpected(profile.error());
    }

    std::string_view profileId;

    if (profile->values[0].get_string().get(profileId)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    const auto profileVersion = readInteger(profile->values[1]);

    if (!profileVersion) {
        return std::unexpected(profileVersion.error());
    }

    if (profileId != "qdcxx-native") {
        return std::unexpected(JsonSchemaError::UNSUPPORTED_PROFILE);
    }

    constexpr std::int32_t PROFILE_VERSION = 1;

    if (*profileVersion != PROFILE_VERSION) {
        return std::unexpected(JsonSchemaError::UNSUPPORTED_VERSION);
    }

    if (!members->complete()) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    simdjson::dom::array records;

    if (members->values[2].get_array().get(records)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    if (records.size() > limits.records) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    std::vector<core::LocalRecord> owned;
    owned.reserve(records.size());
    auto remaining = limits;

    for (const auto record : records) {
        auto loaded = readRecord(record, remaining);

        if (!loaded) {
            return std::unexpected(loaded.error());
        }

        owned.push_back(std::move(*loaded));
    }

    auto result = core::LocalSchema::create(owned, limits, core::LocalProfile::EXTENDED_FIELDS_V1);

    if (result) {
        return std::move(*result);
    }

    switch (result.error()) {
    case core::SchemaError::RESOURCE_LIMIT:
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    case core::SchemaError::ALLOCATION_FAILURE:
        return std::unexpected(JsonSchemaError::ALLOCATION_FAILURE);
    default:
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }
}

/** Bound container nesting, including empty containers omitted from DOM depth bookkeeping.
 * This is a resource preflight only; simdjson remains responsible for all JSON syntax validation.
 * @param input Bounded borrowed JSON bytes.
 * @param limit Maximum simultaneous object/array count.
 * @return False as soon as a container would exceed the budget.
 */
bool depthFits(std::string_view input, std::size_t limit) noexcept {
    std::size_t depth = 0;
    bool inString = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto byte = input[i];

        if (inString) {
            if (byte == '\\' && i + 1 < input.size()) {
                ++i;
            } else if (byte == '"') {
                inString = false;
            }
        } else if (byte == '"') {
            inString = true;
        } else if (byte == '{' || byte == '[') {
            if (depth == limit) {
                return false;
            }

            ++depth;
        } else if ((byte == '}' || byte == ']') && depth != 0) {
            --depth;
        }
    }

    return true;
}

/** Translate parser failures without retaining or exposing parser diagnostics.
 * @param error Non-success parser status.
 * @return Stable resource, allocation or malformed-document category.
 */
JsonSchemaError parserError(simdjson::error_code error) noexcept {
    switch (error) {
    case simdjson::CAPACITY:
    case simdjson::DEPTH_ERROR:
    case simdjson::OUT_OF_CAPACITY:
        return JsonSchemaError::RESOURCE_LIMIT;
    case simdjson::MEMALLOC:
        return JsonSchemaError::ALLOCATION_FAILURE;
    default:
        return JsonSchemaError::MALFORMED_JSON;
    }
}

/** Checked accumulator used before any proportional binary-loader action. */
struct BudgetModel {
    std::size_t value = 0; ///< Accumulated logical count or bytes.

    /** Add a product without overflowing an intermediate.
     * @param count Number of units.
     * @param width Charge per unit.
     * @return False without wrapping when the product or total is unrepresentable.
     */
    bool add(std::size_t count, std::size_t width = 1) noexcept {
        if (width != 0 && count > (std::numeric_limits<std::size_t>::max() - value) / width) {
            return false;
        }

        value += count * width;

        return true;
    }
};

/** Compute an exact triangular comparison count without an unnecessary doubled intermediate.
 * @param count Number of entries.
 * @param total Destination accumulator.
 * @return False on unrepresentable total.
 */
bool addPairs(std::size_t count, BudgetModel &total) noexcept {
    return count == 0 || (count % 2 == 0 ? total.add(count / 2, count - 1) : total.add(count, (count - 1) / 2));
}

/** Charge the pinned fresh-parser allocation requests before entering the SDK.
 * All nine pinned backends have an eight-byte open_container and the same five-word
 * derived state. One extra word and maximum alignment conservatively cover derived padding.
 * Allocator metadata and the caller's borrowed source are outside this request envelope.
 * @param bytes Actual document length.
 * @param depth Actual parser stack bound, including its bookkeeping level.
 * @return Conservative request bytes, or an arithmetic resource failure.
 */
std::expected<std::size_t, JsonSchemaError> parserRequestBytes(std::size_t bytes, std::size_t depth) noexcept {
    constexpr std::size_t ALIGNMENT = 64;
    constexpr std::size_t FIXED_IMPLEMENTATION =
        sizeof(simdjson::internal::dom_parser_implementation) + 6 * sizeof(void *) + alignof(std::max_align_t);
    const auto capacity = std::max(bytes, simdjson::dom::MINIMAL_DOCUMENT_CAPACITY);
    BudgetModel safe;

    if (!safe.add(capacity, 5) || !safe.add(simdjson::SIMDJSON_PADDING + ALIGNMENT)) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    /** Round a preflighted SDK allocation length.
     * @param size Length whose addition/rounding headroom was checked above.
     * @return Smallest aligned length at least size.
     */
    const auto round = [](std::size_t size) {
        return (size + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
    };
    BudgetModel model;

    if (!model.add(bytes) || !model.add(simdjson::SIMDJSON_PADDING) ||
        !model.add(round(capacity) + 9, sizeof(std::uint32_t)) ||
        !model.add(round(capacity + 3), sizeof(std::uint64_t)) ||
        !model.add(round(5 * capacity / 3 + simdjson::SIMDJSON_PADDING)) ||
        !model.add(depth, 2 * sizeof(std::uint32_t) + sizeof(bool)) || !model.add(FIXED_IMPLEMENTATION)) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    return model.value;
}

/** Read a name's exact UTF-16 length without allocating or retaining a DOM view.
 * @param value DOM string candidate.
 * @return Scalar-text UTF-16 units, or wrong-kind failure.
 */
std::expected<std::size_t, JsonSchemaError> nameLength(simdjson::dom::element value) noexcept {
    std::string_view text;

    if (value.get_string().get(text)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    std::size_t units = 0;

    for (const unsigned char byte : text) {
        if ((byte & 0xc0) != 0x80) {
            units += byte >= 0xf0 ? 2 : 1;
        }
    }

    return units;
}

/** Select only the new explicit identity before interpreting any record grammar.
 * @param root Parsed root, borrowed while the parser lives.
 * @return Borrowed record array, or stable identity/shape error.
 */
std::expected<simdjson::dom::array, JsonSchemaError> binaryRecords(simdjson::dom::element root) {
    constexpr std::array<std::string_view, 3> ROOT_KEYS{"formatVersion", "profile", "records"};
    constexpr std::array<std::string_view, 2> PROFILE_KEYS{"id", "version"};
    const auto members = readObject(root, ROOT_KEYS, true);

    if (!members || (members->seen & 3) != 3) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    const auto version = readInteger(members->values[0]);

    if (!version) {
        return std::unexpected(version.error());
    }

    if (*version != 1) {
        return std::unexpected(JsonSchemaError::UNSUPPORTED_VERSION);
    }

    const auto profile = readObject(members->values[1], PROFILE_KEYS);

    if (!profile) {
        return std::unexpected(profile.error());
    }

    std::string_view id;
    const auto profileVersion = readInteger(profile->values[1]);

    if (profile->values[0].get_string().get(id) || !profileVersion) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    if (id != "qdcxx-native-binary") {
        return std::unexpected(JsonSchemaError::UNSUPPORTED_PROFILE);
    }

    if (*profileVersion != 1) {
        return std::unexpected(JsonSchemaError::UNSUPPORTED_VERSION);
    }

    simdjson::dom::array records;

    if (!members->complete() || members->values[2].get_array().get(records)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    return records;
}

constexpr std::array<std::string_view, 6> BINARY_RECORD_KEYS{
    "id", "name", "hasTime", "fields", "lookups", "indexedTimeLayout"}; ///< Exact binary record grammar.

/** Preflight the entire declaration/model/core-validation envelope without owned proportional storage.
 * The byte passes are charged before parsing. This pass only counts, never duplicates core identity validation.
 * @param records Borrowed complete DOM records.
 * @param limits Original independent loading limits.
 * @param byteWork Already charged whole-document byte-pass credits.
 * @return Success before staging, or an invalid shape/resource failure.
 */
std::expected<void, JsonSchemaError> preflightBinary(simdjson::dom::array records, const BinaryJsonSchemaLimits &limits,
                                                     std::size_t byteWork) {
    auto remaining = limits.loading.schema;
    BudgetModel fields, lookups, names, pairs, coverage;
    std::size_t longest = 0;

    if (records.size() > remaining.records || !addPairs(records.size(), pairs)) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    /** Count one DOM name without allocating or retaining its source.
     * @param value Candidate string in the current parser.
     * @return Success after exact cumulative charging, or wrong-kind/resource failure.
     */
    const auto countName = [&](simdjson::dom::element value) -> std::expected<void, JsonSchemaError> {
        const auto units = nameLength(value);

        if (!units) {
            return std::unexpected(units.error());
        }

        if (*units > remaining.nameUnits || !charge(*units, remaining.totalNameUnits) || !names.add(*units)) {
            return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
        }

        longest = std::max(longest, *units);

        return {};
    };

    for (const auto record : records) {
        const auto members = readObject(record, BINARY_RECORD_KEYS);
        simdjson::dom::array fieldArray, lookupArray;

        if (!members || members->values[3].get_array().get(fieldArray) ||
            members->values[4].get_array().get(lookupArray)) {
            return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
        }

        if (!charge(fieldArray.size(), remaining.fields) || !charge(lookupArray.size(), remaining.lookups) ||
            !fields.add(fieldArray.size()) || !lookups.add(lookupArray.size()) ||
            !addPairs(lookupArray.size(), pairs) || !coverage.add(fieldArray.size(), lookupArray.size())) {
            return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
        }

        if (const auto result = countName(members->values[1]); !result) {
            return std::unexpected(result.error());
        }

        constexpr std::array<std::string_view, 2> LOOKUP_KEYS{"name", "fieldId"};

        for (const auto lookup : lookupArray) {
            const auto key = readObject(lookup, LOOKUP_KEYS);

            if (!key) {
                return std::unexpected(key.error());
            }

            if (const auto result = countName(key->values[0]); !result) {
                return std::unexpected(result.error());
            }
        }
    }

    BudgetModel linear;
    BudgetModel work;

    if (!linear.add(records.size()) || !linear.add(fields.value) || !linear.add(lookups.value) ||
        !linear.add(names.value) || !linear.add(1) || !work.add(byteWork) || !work.add(linear.value, 8) ||
        !work.add(pairs.value, longest + 1) || !work.add(coverage.value) || work.value > limits.workUnits) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    // Logical payloads count both staging and the core's copied result. Fixed controls count conservatively;
    // capacity growth/SSO and allocator bookkeeping are intentionally not represented by this model.
    constexpr std::size_t PROVENANCE_MODEL_BYTES = 32 * sizeof(void *);
    constexpr std::size_t FIXED_MODEL = 2 * sizeof(std::vector<core::LocalRecord>) + sizeof(core::LocalSchema) +
                                        2 * sizeof(core::LocalRecord) + 2 * sizeof(std::u16string) +
                                        PROVENANCE_MODEL_BYTES;
    BudgetModel oneCopy, model;

    if (!oneCopy.add(records.size(), sizeof(core::LocalRecord)) ||
        !oneCopy.add(fields.value, sizeof(core::LocalField)) ||
        !oneCopy.add(lookups.value, sizeof(core::FieldLookup)) || !oneCopy.add(names.value, sizeof(char16_t)) ||
        !oneCopy.add(records.size(), sizeof(char16_t)) || !oneCopy.add(lookups.value, sizeof(char16_t)) ||
        !model.add(FIXED_MODEL) || !model.add(oneCopy.value, 2) || model.value > limits.schemaModelBytes) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    return {};
}

/** Read explicit logical contributor IDs without inferring field names or Java slots.
 * @param value Layout object or the ordinary-record null marker.
 * @return Owned fixed layout selection, or a shape/range/unsupported-kind failure.
 */
std::expected<std::optional<core::IndexedTimeLayout>, JsonSchemaError> readLayout(simdjson::dom::element value) {
    if (value.is_null()) {
        return std::optional<core::IndexedTimeLayout>{};
    }

    constexpr std::array<std::string_view, 4> KEYS{"kind", "highFieldId", "lowFieldId", "fieldId"};
    const auto members = readObject(value, KEYS, true);
    std::string_view kind;

    if (!members || (members->seen & 1) == 0 || members->unknown || members->values[0].get_string().get(kind)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    if (kind == "pair-int32") {
        if (members->seen != 7) {
            return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
        }

        const auto high = readInteger(members->values[1]);
        const auto low = readInteger(members->values[2]);

        if (!high || !low) {
            return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
        }

        return core::IndexedTimeLayout{
            core::IndexedTimePairInt32{static_cast<std::size_t>(*high), static_cast<std::size_t>(*low)}};
    }

    if (kind == "single-int64") {
        if (members->seen != 9) {
            return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
        }

        const auto field = readInteger(members->values[3]);

        if (!field) {
            return std::unexpected(field.error());
        }

        return core::IndexedTimeLayout{core::IndexedTimeSingleInt64{static_cast<std::size_t>(*field)}};
    }

    return std::unexpected(JsonSchemaError::UNSUPPORTED_INDEXED);
}

/** Stage one preflighted binary declaration for authoritative core validation.
 * @param value Record DOM whose counts/name units have already been admitted.
 * @param remaining Remaining original logical budgets for name allocation checks.
 * @return Owned staging record; all DOM views expire before publication.
 */
std::expected<core::LocalRecord, JsonSchemaError> readBinaryRecord(simdjson::dom::element value,
                                                                   core::SchemaLimits &remaining) {
    const auto members = readObject(value, BINARY_RECORD_KEYS);

    if (!members) {
        return std::unexpected(members.error());
    }

    const auto id = readInteger(members->values[0]);
    bool hasTime = false;

    if (!id || members->values[2].get_bool().get(hasTime)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    auto layout = readLayout(members->values[5]);

    if (!layout) {
        return std::unexpected(layout.error());
    }

    auto name = readName(members->values[1], remaining);

    if (!name) {
        return std::unexpected(name.error());
    }

    core::LocalRecord record{*id, std::move(*name), hasTime, {}, {}, std::move(*layout)};
    simdjson::dom::array fields, lookups;

    if (members->values[3].get_array().get(fields) || members->values[4].get_array().get(lookups)) {
        return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
    }

    record.fields.reserve(fields.size());
    record.lookups.reserve(lookups.size());
    constexpr std::array<std::string_view, 1> FIELD_KEYS{"serialTypeId"};

    for (const auto field : fields) {
        const auto entry = readObject(field, FIELD_KEYS);

        if (!entry) {
            return std::unexpected(entry.error());
        }

        const auto type = readInteger(entry->values[0]);

        if (!type) {
            return std::unexpected(type.error());
        }

        if (!core::supportsSerialType(*type, core::LocalProfile::BINARY_FIELDS_V1)) {
            return std::unexpected(JsonSchemaError::UNSUPPORTED_TYPE);
        }

        record.fields.push_back({*type});
    }

    constexpr std::array<std::string_view, 2> LOOKUP_KEYS{"name", "fieldId"};

    for (const auto lookup : lookups) {
        const auto entry = readObject(lookup, LOOKUP_KEYS);

        if (!entry) {
            return std::unexpected(entry.error());
        }

        const auto fieldId = readInteger(entry->values[1]);
        auto key = readName(entry->values[0], remaining);

        if (!fieldId || !key) {
            return std::unexpected(key ? JsonSchemaError::INVALID_SCHEMA : key.error());
        }

        record.lookups.push_back({std::move(*key), static_cast<std::size_t>(*fieldId)});
    }

    return std::move(record);
}

} // namespace

std::expected<core::LocalSchema, JsonSchemaError> loadJsonSchema(std::string_view input, JsonSchemaLimits limits) {
    // Reserve conservative arithmetic headroom for DOM tape, structural indexes, padding and depth storage.
    constexpr std::size_t PARSER_ALLOCATION_FACTOR = 64;
    constexpr std::size_t MAX_SAFE_INPUT = std::numeric_limits<std::size_t>::max() / PARSER_ALLOCATION_FACTOR - 1;

    if (input.size() > limits.documentBytes || input.size() > MAX_SAFE_INPUT ||
        input.size() > simdjson::SIMDJSON_MAXSIZE_BYTES || !depthFits(input, limits.depth)) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    if (input.empty() || input.starts_with("\xef\xbb\xbf")) {
        return std::unexpected(JsonSchemaError::MALFORMED_JSON);
    }

    try {
        // The parser requires this fixed minimum even for tiny malformed documents.
        const auto capacity = std::max(input.size(), simdjson::dom::MINIMAL_DOCUMENT_CAPACITY);
        simdjson::dom::parser parser(capacity);
        // One bookkeeping level above the caller's containers; a document cannot nest beyond its bytes.
        const auto parserDepth = std::min(limits.depth, input.size()) + 1;

        if (const auto error = parser.allocate(capacity, parserDepth)) {
            return std::unexpected(parserError(error));
        }

        simdjson::dom::element root;

        if (const auto error = parser.parse(input.data(), input.size()).get(root)) {
            return std::unexpected(parserError(error));
        }

        return readSchema(root, limits.schema);
    } catch (const std::bad_alloc &) {
        return std::unexpected(JsonSchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(JsonSchemaError::ALLOCATION_FAILURE);
    }
}

std::expected<core::LocalSchema, JsonSchemaError> loadBinaryJsonSchema(std::string_view input,
                                                                       BinaryJsonSchemaLimits limits) {
    // Thirty-two source-byte credits cover depth, parsing, the DOM counting/staging passes,
    // decoded fixed-key comparisons and UTF-8 counting/conversion, including rejected members.
    constexpr std::size_t BYTE_PASS_CREDITS = 32;
    BudgetModel byteWork;

    if (input.size() > limits.loading.documentBytes || input.size() > simdjson::SIMDJSON_MAXSIZE_BYTES ||
        !byteWork.add(input.size(), BYTE_PASS_CREDITS) || byteWork.value > limits.workUnits) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    if (!depthFits(input, limits.loading.depth)) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    if (input.empty() || input.starts_with("\xef\xbb\xbf")) {
        return std::unexpected(JsonSchemaError::MALFORMED_JSON);
    }

    const auto parserDepth = std::min(limits.loading.depth, input.size()) + 1;
    const auto requests = parserRequestBytes(input.size(), parserDepth);

    if (!requests || *requests > limits.parserBytes) {
        return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
    }

    try {
        const auto capacity = std::max(input.size(), simdjson::dom::MINIMAL_DOCUMENT_CAPACITY);
        simdjson::dom::parser parser(capacity);

        if (const auto error = parser.allocate(capacity, parserDepth)) {
            return std::unexpected(parserError(error));
        }

        simdjson::dom::element root;

        if (const auto error = parser.parse(input.data(), input.size()).get(root)) {
            return std::unexpected(parserError(error));
        }

        const auto records = binaryRecords(root);

        if (!records) {
            return std::unexpected(records.error());
        }

        if (const auto admitted = preflightBinary(*records, limits, byteWork.value); !admitted) {
            return std::unexpected(admitted.error());
        }

        std::vector<core::LocalRecord> owned;
        owned.reserve(records->size());
        auto remaining = limits.loading.schema;

        for (const auto record : *records) {
            auto loaded = readBinaryRecord(record, remaining);

            if (!loaded) {
                return std::unexpected(loaded.error());
            }

            owned.push_back(std::move(*loaded));
        }

        auto result = core::LocalSchema::create(owned, limits.loading.schema, core::LocalProfile::BINARY_FIELDS_V1);

        if (result) {
            return std::move(*result);
        }

        switch (result.error()) {
        case core::SchemaError::RESOURCE_LIMIT:
            return std::unexpected(JsonSchemaError::RESOURCE_LIMIT);
        case core::SchemaError::ALLOCATION_FAILURE:
            return std::unexpected(JsonSchemaError::ALLOCATION_FAILURE);
        case core::SchemaError::UNSUPPORTED_PROFILE:
            return std::unexpected(JsonSchemaError::UNSUPPORTED_INDEXED);
        default:
            return std::unexpected(JsonSchemaError::INVALID_SCHEMA);
        }
    } catch (const std::bad_alloc &) {
        return std::unexpected(JsonSchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(JsonSchemaError::ALLOCATION_FAILURE);
    }
}

std::string_view jsonSchemaErrorName(JsonSchemaError error) noexcept {
    switch (error) {
    case JsonSchemaError::MALFORMED_JSON:
        return "MALFORMED_JSON";
    case JsonSchemaError::INVALID_SCHEMA:
        return "INVALID_SCHEMA";
    case JsonSchemaError::UNSUPPORTED_PROFILE:
        return "UNSUPPORTED_PROFILE";
    case JsonSchemaError::UNSUPPORTED_VERSION:
        return "UNSUPPORTED_VERSION";
    case JsonSchemaError::UNSUPPORTED_TYPE:
        return "UNSUPPORTED_TYPE";
    case JsonSchemaError::UNSUPPORTED_INDEXED:
        return "UNSUPPORTED_INDEXED";
    case JsonSchemaError::RESOURCE_LIMIT:
        return "RESOURCE_LIMIT";
    case JsonSchemaError::ALLOCATION_FAILURE:
        return "ALLOCATION_FAILURE";
    }

    return "UNKNOWN";
}

} // namespace org::ttldtor::qdcxx::schema
