// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/mux/queue.hpp"

#include <array>
#include <cstdlib>
#include <doctest/doctest.h>
#include <expected>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace m = org::ttldtor::qdcxx::mux;
constexpr c::SchemaLimits SCHEMA_LIMITS{4, 64, 128, 128, 8192}; ///< Finite schema construction bounds.
constexpr e::MappingLimits MAPPING_LIMITS{64, 128, 32, 64, 128, 128, 8192, 100000}; ///< Finite preparation bounds.
constexpr std::int32_t UNKNOWN_TYPE_BIT = 0x100; ///< Outside the supported full serial identity catalog.
/** A compact-int primitive with an unsupported high representation bit must not become a known type. */
constexpr auto UNKNOWN_FIELD_TYPE =
    static_cast<c::SerialFieldType>(std::to_underlying(c::SerialFieldType::COMPACT_INT) | UNKNOWN_TYPE_BIT);
bool failAllocations = false;       ///< Injection applies only around ordinary production allocation requests.
std::size_t allocationAttempts = 0; ///< Observed ordinary allocation attempts inside the injected scope.

static_assert(std::is_same_v<m::MuxFieldValueView, c::FieldValueView>);
static_assert(std::is_member_function_pointer_v<decltype(&e::EventMapping::validate)>);
static_assert(std::is_member_function_pointer_v<decltype(&e::PreparedQuote::map)>);
static_assert(std::is_member_function_pointer_v<decltype(&e::PreparedProfile::map)>);
static_assert(c::validFieldValueView({c::SerialFieldType::COMPACT_INT, 42, {}, {}}));
static_assert(!c::validFieldValueView({c::SerialFieldType::UTF_CHAR_ARRAY, 1, {}, {}}));

/** Disable ordinary allocation only during production API calls, never test assertions. */
struct AllocationFault {
    /** Begin one isolated no-allocation scope. */
    AllocationFault() {
        allocationAttempts = 0;
        failAllocations = true;
    }

    /** Restore allocation before result inspection. */
    ~AllocationFault() {
        failAllocations = false;
    }
};

/** Borrow one field without copying text/byte contents or changing nullable empty values.
 * @param field Owning source, kept stable until its returned view is no longer used.
 * @return Exact descriptor borrowing the same payload addresses.
 */
c::FieldValueView borrow(const c::FieldValue &field) {
    return {field.type, field.raw, field.units ? std::optional<std::u16string_view>(*field.units) : std::nullopt,
            field.bytes ? std::optional<std::span<const std::byte>>(*field.bytes) : std::nullopt};
}

/** Borrow all source fields into test-owned descriptor storage before measuring production calls.
 * @param fields Complete source fields whose payload storage remains stable.
 * @return Borrowed descriptors, not owning payload copies.
 */
std::vector<c::FieldValueView> borrowAll(std::span<const c::FieldValue> fields) {
    std::vector<c::FieldValueView> result;
    result.reserve(fields.size());

    for (const auto &field : fields) {
        result.push_back(borrow(field));
    }

    return result;
}

/** Construct a real prepared-event schema with extra full binary fields.
 * @param profile Select the unchanged default Profile rather than Quote business fields.
 * @return Independent schema retaining every default lookup and additional generic fields.
 */
c::LocalSchema schema(bool profile = false) {
    auto base = profile ? e::defaultProfileSchema(SCHEMA_LIMITS) : e::defaultQuoteSchema(SCHEMA_LIMITS);
    REQUIRE(base);

    auto record = base->records()[0];
    const std::array extraTypes{c::SerialFieldType::UTF_CHAR_ARRAY, c::SerialFieldType::STRING,
                                c::SerialFieldType::CUSTOM_OBJECT,  c::SerialFieldType::SERIAL_OBJECT,
                                c::SerialFieldType::BYTE_ARRAY,     c::SerialFieldType::LONG};
    const std::array<std::u16string_view, 6> extraKeys{u"ExtraText",   u"ExtraString", u"ExtraCustom",
                                                       u"ExtraSerial", u"ExtraBytes",  u"ExtraLong"};

    for (std::size_t i = 0; i < extraTypes.size(); ++i) {
        record.lookups.push_back({std::u16string(extraKeys[i]), record.fields.size()});
        record.fields.push_back({std::to_underlying(extraTypes[i])});
    }

    auto result = c::LocalSchema::create(std::span(&record, 1), SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Supply complete canonical zero/null fields for an arbitrary selected event schema.
 * @param local Immutable validated schema containing record zero.
 * @return Independently owned values with all exact full tags, including extras.
 */
std::vector<c::FieldValue> values(const c::LocalSchema &local) {
    std::vector<c::FieldValue> result;

    for (const auto &field : local.records()[0].fields) {
        result.push_back({static_cast<c::SerialFieldType>(field.serialTypeId), 0});
    }

    return result;
}

/** Compare every public Quote member without floating-point approximations.
 * @param actual Borrowed-input projection.
 * @param expected Owning-input projection of the same raw values.
 */
void checkQuote(const e::Quote &actual, const e::Quote &expected) {
    CHECK(actual.symbol == expected.symbol);
    CHECK(actual.bidTimeMs == expected.bidTimeMs);
    CHECK(actual.bidExchangeCode == expected.bidExchangeCode);
    CHECK(actual.bidPrice.raw == expected.bidPrice.raw);
    CHECK(actual.bidSize.raw == expected.bidSize.raw);
    CHECK(actual.askTimeMs == expected.askTimeMs);
    CHECK(actual.askExchangeCode == expected.askExchangeCode);
    CHECK(actual.askPrice.raw == expected.askPrice.raw);
    CHECK(actual.askSize.raw == expected.askSize.raw);
    CHECK(actual.timeMs == expected.timeMs);
    CHECK(actual.eventTime == expected.eventTime);
    CHECK(actual.sequence == expected.sequence);
    CHECK(actual.timeNanoPart == expected.timeNanoPart);
}

/** Compare every public Profile member with exact decimal bits and nullable strings.
 * @param actual Borrowed-input projection.
 * @param expected Owning-input projection of the same raw values.
 */
void checkProfile(const e::Profile &actual, const e::Profile &expected) {
    CHECK(actual.symbol == expected.symbol);
    CHECK(actual.beta.raw == expected.beta.raw);
    CHECK(actual.earningsPerShare.raw == expected.earningsPerShare.raw);
    CHECK(actual.dividendFrequency.raw == expected.dividendFrequency.raw);
    CHECK(actual.exDividendAmount.raw == expected.exDividendAmount.raw);
    CHECK(actual.exDividendDayId == expected.exDividendDayId);
    CHECK(actual.high52WeekPrice.raw == expected.high52WeekPrice.raw);
    CHECK(actual.low52WeekPrice.raw == expected.low52WeekPrice.raw);
    CHECK(actual.shares.raw == expected.shares.raw);
    CHECK(actual.freeFloat.raw == expected.freeFloat.raw);
    CHECK(actual.highLimitPrice.raw == expected.highLimitPrice.raw);
    CHECK(actual.lowLimitPrice.raw == expected.lowLimitPrice.raw);
    CHECK(actual.haltStartTimeMs == expected.haltStartTimeMs);
    CHECK(actual.haltEndTimeMs == expected.haltEndTimeMs);
    CHECK(actual.flags == expected.flags);
    CHECK(actual.description == expected.description);
    CHECK(actual.statusReason == expected.statusReason);
    CHECK(actual.eventTime == expected.eventTime);
}
} // namespace

/** Allocate ordinary test-process storage with one scoped no-allocation check.
 * @param size Requested bytes; zero still obtains deletable storage.
 * @return Storage, or bad_alloc for any allocation in the injected scope.
 */
void *operator new(std::size_t size) {
    if (failAllocations) {
        ++allocationAttempts;

        throw std::bad_alloc();
    }

    if (auto *storage = std::malloc(size == 0 ? 1 : size)) {
        return storage;
    }

    throw std::bad_alloc();
}

/** Allocate array storage through the same ordinary injection route.
 * @param size Requested bytes.
 * @return Storage or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return ::operator new(size);
}

/** Release ordinary test storage.
 * @param storage Matching allocation pointer or null.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release test array storage.
 * @param storage Matching allocation pointer or null.
 */
void operator delete[](void *storage) noexcept {
    std::free(storage);
}

/** Release ordinary sized storage using the matching allocator.
 * @param storage Matching allocation pointer or null.
 * @param size Unused implementation-provided allocation size.
 */
void operator delete(void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

/** Release sized array storage using the matching allocator.
 * @param storage Matching allocation pointer or null.
 * @param size Unused implementation-provided allocation size.
 */
void operator delete[](void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

TEST_CASE("borrowed canonical validation preserves exact ranges and nullable payloads") {
    const std::array canonical{
        c::FieldValue{c::SerialFieldType::COMPACT_INT, std::numeric_limits<std::int32_t>::min()},
        c::FieldValue{c::SerialFieldType::DECIMAL, std::numeric_limits<std::int32_t>::max()},
        c::FieldValue{c::SerialFieldType::LONG, std::numeric_limits<std::int64_t>::min()},
        c::FieldValue{c::SerialFieldType::WIDE_DECIMAL, std::numeric_limits<std::int64_t>::max()},
        c::FieldValue{c::SerialFieldType::UTF_CHAR_ARRAY, 0},
        c::FieldValue{c::SerialFieldType::UTF_CHAR_ARRAY, 0, u""},
        c::FieldValue{c::SerialFieldType::STRING, 0, std::u16string{u'X', u'\0', char16_t{0xd800}}},
        c::FieldValue{c::SerialFieldType::CUSTOM_OBJECT, 0, {}, std::vector<std::byte>{}},
        c::FieldValue{c::SerialFieldType::SERIAL_OBJECT, 0, {}, std::vector{std::byte{0}, std::byte{0xff}}},
        c::FieldValue{c::SerialFieldType::BYTE_ARRAY, 0}};

    for (const auto &field : canonical) {
        auto view = borrow(field);
        CHECK(c::validFieldValue(field));
        CHECK(c::validFieldValueView(view));
        CHECK(view.units.has_value() == field.units.has_value());
        CHECK(view.bytes.has_value() == field.bytes.has_value());

        if (field.units) {
            CHECK(view.units->data() == field.units->data());
            CHECK(view.units->size() == field.units->size());
        }

        if (field.bytes) {
            CHECK(view.bytes->data() == field.bytes->data());
            CHECK(view.bytes->size() == field.bytes->size());
        }
    }

    const std::array malformed{c::FieldValue{c::SerialFieldType::COMPACT_INT, std::int64_t{1} << 32},
                               c::FieldValue{c::SerialFieldType::BYTE, 128},
                               c::FieldValue{c::SerialFieldType::UTF_CHAR_ARRAY, 1},
                               c::FieldValue{c::SerialFieldType::COMPACT_INT, 0, u""},
                               c::FieldValue{c::SerialFieldType::STRING, 0, u"", std::vector<std::byte>{}},
                               c::FieldValue{c::SerialFieldType::CUSTOM_OBJECT, 0, u""},
                               c::FieldValue{c::SerialFieldType::SERIAL_OBJECT, 1},
                               c::FieldValue{UNKNOWN_FIELD_TYPE, 0}};

    for (const auto &field : malformed) {
        CHECK_FALSE(c::validFieldValue(field));
        CHECK_FALSE(c::validFieldValueView(borrow(field)));
    }
}

TEST_CASE("borrowed complete-shape validation is allocation free and old empty-brace calls remain valid") {
    auto local = schema();
    auto fields = values(local);
    fields[8].units = std::u16string(128, u'X');
    fields[10].bytes = std::vector<std::byte>(128, std::byte{0xff});
    auto views = borrowAll(fields);
    const std::array<std::u16string_view, 1> keys{u"BidPrice"};
    const std::array types{c::SerialFieldType::WIDE_DECIMAL};
    const std::array requests{e::FieldRequest{keys, types, true}};
    auto mapping = e::EventMapping::prepare(local, 0, requests, MAPPING_LIMITS);
    REQUIRE(mapping);

    std::expected<void, e::MappingError> owning;
    std::expected<void, e::MappingError> borrowed;

    {
        AllocationFault fault;
        owning = mapping->validate({local.recordContext(0), fields});
        borrowed = mapping->validateView({local.recordContext(0), views});
    }

    CHECK(owning);
    CHECK(borrowed);
    CHECK(allocationAttempts == 0);
    CHECK(views[8].units->data() == fields[8].units->data());
    CHECK(views[10].bytes->data() == fields[10].bytes->data());
    CHECK(mapping->validate({local.recordContext(0), {}}).error() == e::MappingError::INVALID_VALUES);

    auto quote = e::PreparedQuote::prepare(local, 0, MAPPING_LIMITS);
    REQUIRE(quote);
    CHECK(quote->map(u"", {local.recordContext(0), {}}, 0).error() == e::MappingError::INVALID_VALUES);

    fields.back().type = c::SerialFieldType::TIME_MILLIS;
    views.back().type = fields.back().type;
    CHECK(mapping->validate({local.recordContext(0), fields}).error() == e::MappingError::INVALID_VALUES);
    CHECK(mapping->validateView({local.recordContext(0), views}).error() == e::MappingError::INVALID_VALUES);
}

TEST_CASE("borrowed mappings reject independent provenance before malformed fields and ownership limits") {
    auto local = schema();
    auto foreign = schema();
    auto prepared = e::PreparedQuote::prepare(local, 0, MAPPING_LIMITS);
    REQUIRE(prepared);
    auto fields = values(local);
    auto views = borrowAll(fields);
    views[0].type = UNKNOWN_FIELD_TYPE;

    CHECK(prepared->mapView(u"SYMBOL", {foreign.recordContext(0), views}, 0).error() == e::MappingError::WRONG_CONTEXT);
    CHECK(prepared->mapView(u"SYMBOL", {local.recordContext(0), views}, 0).error() == e::MappingError::INVALID_VALUES);
    views = borrowAll(fields);
    CHECK(prepared->mapView(u"SYMBOL", {local.recordContext(0), views}, 0).error() == e::MappingError::RESOURCE_LIMIT);
}

TEST_CASE("borrowed Quote projects mixed reordered fields and packed metadata exactly like owning values") {
    c::LocalRecord record{0,
                          u"ConfiguredQuote",
                          false,
                          {{8}, {120}, {8}, {120}, {56}, {2}, {72}, {8}, {10}},
                          {{u"AskSize", 0},
                           {u"BidPrice", 1},
                           {u"BidSize", 2},
                           {u"AskPrice", 3},
                           {u"BidTime", 4},
                           {u"BidExchangeCode", 5},
                           {u"Sequence", 6},
                           {u"TimeNanoPart", 7},
                           {u"Extra", 8}}};
    auto local = c::LocalSchema::create(std::span(&record, 1), SCHEMA_LIMITS, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(local);
    auto prepared = e::PreparedQuote::prepare(*local, 0, MAPPING_LIMITS);
    REQUIRE(prepared);
    auto fields = values(*local);
    fields[0].raw = std::numeric_limits<std::int32_t>::max();
    fields[1].raw = std::numeric_limits<std::int64_t>::min();
    fields[2].raw = std::numeric_limits<std::int32_t>::min();
    fields[3].raw = 32002;
    fields[4].raw = -17;
    fields[5].raw = 0x1f600;
    fields[6].raw = -1;
    fields[7].raw = std::numeric_limits<std::int32_t>::min();
    fields[8].units = u"extra";
    auto views = borrowAll(fields);
    auto owning = prepared->map(u"Q", {local->recordContext(0), fields}, 1, -123);
    auto borrowed = prepared->mapView(u"Q", {local->recordContext(0), views}, 1, -123);
    REQUIRE(owning);
    REQUIRE(borrowed);
    checkQuote(*borrowed, *owning);
    fields.clear();
    views.clear();
    CHECK(borrowed->symbol == u"Q");
    CHECK(borrowed->bidPrice.raw == std::numeric_limits<std::int64_t>::min());
    CHECK(borrowed->eventTime == -123);
}

TEST_CASE("borrowed Profile preserves all owned fields and nullable text after source destruction") {
    std::optional<e::Profile> result;

    {
        auto local = schema(true);
        auto prepared = e::PreparedProfile::prepare(local, 0, MAPPING_LIMITS);
        REQUIRE(prepared);
        auto fields = values(local);

        for (std::size_t i = 0; i < 14; ++i) {
            fields[i].raw = static_cast<std::int64_t>(i) - 7;
        }

        fields[14].units = std::u16string{u'D', u'\0', char16_t{0xd800}, char16_t{0xdfff}};
        fields[15].units = u"";
        fields[16].units = u"extra";
        fields[18].bytes = std::vector{std::byte{0}, std::byte{0xfe}};
        auto views = borrowAll(fields);
        auto owning = prepared->map(u"P", {local.recordContext(0), fields}, {1, 4, 5}, 123);
        auto borrowed = prepared->mapView(u"P", {local.recordContext(0), views}, {1, 4, 5}, 123);
        REQUIRE(owning);
        REQUIRE(borrowed);
        checkProfile(*borrowed, *owning);
        result = std::move(*borrowed);
        CHECK(prepared->mapView(u"P", {local.recordContext(0), views}, {1, 3, 5}).error() ==
              e::MappingError::RESOURCE_LIMIT);
        CHECK(prepared->mapView(u"P", {local.recordContext(0), views}, {1, 4, 4}).error() ==
              e::MappingError::RESOURCE_LIMIT);

        fields[14].units.reset();
        fields[15].units.reset();
        views = borrowAll(fields);
        auto nulls = prepared->mapView(u"", {local.recordContext(0), views}, {0, 0, 0});
        REQUIRE(nulls);
        CHECK_FALSE(nulls->description);
        CHECK_FALSE(nulls->statusReason);

        fields[14].units = u"";
        views = borrowAll(fields);
        auto empty = prepared->mapView(u"", {local.recordContext(0), views}, {0, 0, 0});
        REQUIRE(empty);
        REQUIRE(empty->description);
        CHECK(empty->description->empty());
        CHECK_FALSE(empty->statusReason);
        CHECK(prepared->map(u"", {local.recordContext(0), {}}, {0, 0, 0}).error() == e::MappingError::INVALID_VALUES);
    }

    REQUIRE(result);
    CHECK(result->symbol == u"P");
    REQUIRE(result->description);
    CHECK(result->description->size() == 4);
    CHECK((*result->description)[1] == u'\0');
    CHECK((*result->description)[2] == char16_t{0xd800});
    REQUIRE(result->statusReason);
    CHECK(result->statusReason->empty());
    CHECK(result->eventTime == 123);
}

TEST_CASE("borrowed prepared projections reject noncanonical extra fields before publishing events") {
    auto local = schema(true);
    auto prepared = e::PreparedProfile::prepare(local, 0, MAPPING_LIMITS);
    REQUIRE(prepared);
    auto fields = values(local);
    auto views = borrowAll(fields);

    for (const std::size_t extraIndex :
         {std::size_t{16}, std::size_t{17}, std::size_t{18}, std::size_t{19}, std::size_t{20}}) {
        fields[extraIndex].raw = 1;
        views[extraIndex].raw = 1;
        CHECK(prepared->map(u"", {local.recordContext(0), fields}, {0, 0, 0}).error() ==
              e::MappingError::INVALID_VALUES);
        CHECK(prepared->mapView(u"", {local.recordContext(0), views}, {0, 0, 0}).error() ==
              e::MappingError::INVALID_VALUES);
        fields[extraIndex].raw = 0;
        views[extraIndex].raw = 0;
    }
}

TEST_CASE("borrowed event allocation failures are reported without changing source payloads") {
    auto local = schema(true);
    auto prepared = e::PreparedProfile::prepare(local, 0, MAPPING_LIMITS);
    REQUIRE(prepared);
    auto fields = values(local);
    fields[14].units = std::u16string(128, u'X');
    auto views = borrowAll(fields);
    std::expected<e::Profile, e::MappingError> result = std::unexpected(e::MappingError::INVALID_INPUT);

    {
        AllocationFault fault;
        result = prepared->mapView(u"", {local.recordContext(0), views}, {0, 128, 128});
    }

    REQUIRE_FALSE(result);
    CHECK(result.error() == e::MappingError::ALLOCATION_FAILURE);
    CHECK(allocationAttempts > 0);
    REQUIRE(fields[14].units);
    CHECK(fields[14].units->size() == 128);
    CHECK(views[14].units->data() == fields[14].units->data());
}
