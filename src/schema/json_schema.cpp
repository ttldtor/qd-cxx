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
