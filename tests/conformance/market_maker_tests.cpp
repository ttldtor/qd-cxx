// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/derived_order.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/market_maker.hpp"
#include "qd/events/quote.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>
#include <limits>

namespace {
namespace q = org::ttldtor::qdcxx;
constexpr q::core::SchemaLimits SCHEMA{4, 128, 256, 128, 16384};
constexpr q::events::MappingLimits MAPPING{128, 256, 32, 64, 256, 128, 32768, 1000000};
constexpr q::events::EventValueLimits VALUES{128, 8, 136};

/** @param schema Exact source declarations.
 * @return Canonical zero values with the original complete local shape.
 */
std::vector<q::core::FieldValue> values(const q::core::LocalSchema &schema) {
    std::vector<q::core::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        result.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
    }

    return result;
}

/** @param schema Exact source declarations.
 * @param fields Complete local values to modify.
 * @param key Explicit lookup key.
 * @param raw Canonical scalar representation.
 */
void set(const q::core::LocalSchema &schema, std::vector<q::core::FieldValue> &fields, std::u16string_view key,
         std::int64_t raw) {
    for (const auto &lookup : schema.records()[0].lookups) {
        if (lookup.name == key) {
            fields[lookup.fieldId].raw = raw;

            return;
        }
    }

    FAIL("Missing required test lookup");
}
} // namespace

TEST_CASE("MarketMaker preserves physical codepoint while public index and derived strings follow Java") {
    auto schema = q::events::defaultMarketMakerSchema(SCHEMA);
    REQUIRE(schema);
    REQUIRE(schema->records()[0].hasTime);
    CHECK(std::holds_alternative<q::core::IndexedTimePairInt32>(*schema->records()[0].indexedTimeLayout));
    auto maker = q::events::PreparedMarketMaker::prepare(*schema, 0, MAPPING);
    REQUIRE(maker);
    auto order =
        q::events::PreparedDerivedOrder::prepare(*schema, 0, q::events::DerivedOrderKind::MARKET_MAKER, MAPPING);
    REQUIRE(order);
    auto fields = values(*schema);
    set(*schema, fields, u"MMExchange", 0x10041);
    set(*schema, fields, u"MMID", -1);
    set(*schema, fields, u"MMBid.Time", -3);
    set(*schema, fields, u"MMAsk.Time", 4);
    set(*schema, fields, u"MMBid.Price", 0x180);
    set(*schema, fields, u"MMBid.Size", 0x180);
    auto raw = maker->map(u"AAPL&Q", {schema->recordContext(0), fields}, VALUES, 42, 31);
    REQUIRE(raw);
    CHECK(fields[0].raw == 0x10041);
    CHECK(raw->getSourceId() == 0);
    CHECK(raw->getSource() == q::events::IndexedEventSource::DEFAULT);
    CHECK(raw->getIndex() == (std::int64_t{65} << 32 | 0xffffffffLL));
    CHECK(raw->getExchangeCode() == u'A');
    CHECK(raw->getTime() == 4000);
    CHECK(raw->getBidTime() == -3000);
    CHECK(raw->getAskTime() == 4000);
    CHECK(raw->getMarketMaker() == std::optional<std::u16string>{std::u16string(4, u'\u00ff')});
    CHECK(raw->getBidPrice() == 1);
    CHECK(raw->eventFlags == 31);
    CHECK(raw->eventTime == 42);
    const auto bid =
        order->map(u"AAPL&Q", {schema->recordContext(0), fields}, q::events::DerivedOrderRole::BID, VALUES, 42, 31);
    REQUIRE(bid);
    CHECK(bid->marketMaker == std::optional<std::u16string>{std::u16string(8, u'\u00ff')});
    CHECK(bid->symbol == u"AAPL&Q");
    CHECK(bid->getSourceId() == 5);
    CHECK(bid->eventFlags == 31);
    CHECK(bid->getTime() == -3000);
    CHECK(bid->getSequence() == 0);
    CHECK(bid->getScope() == q::events::Scope::AGGREGATE);
    CHECK(bid->getOrderSide() == q::events::Side::BUY);
    CHECK(bid->getPrice() == 1);
    CHECK(std::isnan(bid->getExecutedSize()));
    CHECK(std::isnan(bid->getTradePrice()));
    CHECK(std::isnan(bid->getTradeSize()));
    auto copy = *raw;
    fields.clear();
    schema = std::unexpected(q::core::SchemaError::INVALID_SCHEMA);
    maker = std::unexpected(q::events::MappingError::INVALID_INPUT);
    order = std::unexpected(q::events::MappingError::INVALID_INPUT);
    CHECK(copy.symbol == u"AAPL&Q");
    CHECK(copy.getMarketMaker()->size() == 4);
    CHECK(copy.getBidPrice() == 1);
}

TEST_CASE("unitary aggregate sides preserve original flags and distinct index identities") {
    auto schema = q::events::defaultMarketMakerSchema(SCHEMA);
    REQUIRE(schema);
    auto mapper =
        q::events::PreparedDerivedOrder::prepare(*schema, 0, q::events::DerivedOrderKind::MARKET_MAKER, MAPPING);
    REQUIRE(mapper);
    auto fields = values(*schema);
    set(*schema, fields, u"MMExchange", 65);
    set(*schema, fields, u"MMID", 0x41420043);

    for (const auto expected :
         std::array<std::array<std::int32_t, 3>, 5>{{{0, 1, 0}, {4, 5, 0}, {8, 1, 8}, {16, 1, 16}, {31, 7, 27}}}) {
        const auto flags = expected[0];
        auto bid = mapper->map(u"AAPL", {schema->recordContext(0), fields}, q::events::DerivedOrderRole::UNITARY_BID,
                               VALUES, 0, flags);
        auto ask = mapper->map(u"AAPL", {schema->recordContext(0), fields}, q::events::DerivedOrderRole::UNITARY_ASK,
                               VALUES, 0, flags);
        REQUIRE(bid);
        REQUIRE(ask);
        CHECK(bid->getSourceId() == 9);
        CHECK(ask->getSourceId() == 9);
        CHECK(bid->eventFlags == expected[1]);
        CHECK(ask->eventFlags == expected[2]);
        CHECK((static_cast<std::uint64_t>(bid->index) ^ static_cast<std::uint64_t>(ask->index)) ==
              (std::uint64_t{1} << 47));
        CHECK(bid->getOrderSide() == q::events::Side::BUY);
        CHECK(ask->getOrderSide() == q::events::Side::SELL);
    }

    set(*schema, fields, u"MMExchange", 128);
    CHECK_FALSE(mapper->map(u"AAPL", {schema->recordContext(0), fields}, q::events::DerivedOrderRole::BID, VALUES));
    auto maker = q::events::PreparedMarketMaker::prepare(*schema, 0, MAPPING);
    REQUIRE(maker);
    auto raw = maker->map(u"AAPL", {schema->recordContext(0), fields}, VALUES);
    REQUIRE(raw);
    CHECK(raw->getExchangeCode() == 128);
    CHECK_FALSE(mapper->sourceId(static_cast<q::events::DerivedOrderRole>(-1)));
}

TEST_CASE("Quote-derived Order keeps literal symbols and real side time without Quote sequence") {
    auto original = q::events::defaultQuoteSchema(SCHEMA);
    REQUIRE(original);
    auto record = original->records()[0];
    record.name = u"Quote&Q";
    auto schema = q::core::LocalSchema::create(std::span(&record, 1), SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    auto mapper = q::events::PreparedDerivedOrder::prepare(*schema, 0, q::events::DerivedOrderKind::QUOTE, MAPPING);
    REQUIRE(mapper);
    auto fields = values(*schema);
    set(*schema, fields, u"Bid.Time", -3);
    set(*schema, fields, u"Ask.Time", 4);
    auto bid =
        mapper->map(u"AAPL&X", {schema->recordContext(0), fields}, q::events::DerivedOrderRole::BID, VALUES, 7, 31);
    REQUIRE(bid);
    auto ask =
        mapper->map(u"AAPL&X", {schema->recordContext(0), fields}, q::events::DerivedOrderRole::ASK, VALUES, 7, 31);
    REQUIRE(ask);
    CHECK(bid->symbol == u"AAPL&X");
    CHECK(bid->getExchangeCode() == u'Q');
    CHECK(bid->getSourceId() == 3);
    CHECK(ask->getSourceId() == 4);
    CHECK(bid->getScope() == q::events::Scope::REGIONAL);
    CHECK(bid->eventFlags == 0);
    CHECK(ask->eventFlags == 0);
    CHECK(bid->getTime() == -3000);
    CHECK(ask->getTime() == 4000);
    CHECK_FALSE(mapper->map(u"AAPL", {original->recordContext(0), fields}, q::events::DerivedOrderRole::BID, VALUES));
    CHECK_FALSE(mapper->map(u"AAPL", {schema->recordContext(0), fields}, q::events::DerivedOrderRole::BID, {3, 8, 11}));
}

TEST_CASE("MM removal context preserves exact signed index words while ordinary UTF remains strict") {
    auto schema = q::events::defaultMarketMakerSchema(SCHEMA);
    REQUIRE(schema);
    auto maker = q::events::PreparedMarketMaker::prepare(*schema, 0, MAPPING);
    REQUIRE(maker);
    auto order =
        q::events::PreparedDerivedOrder::prepare(*schema, 0, q::events::DerivedOrderKind::MARKET_MAKER, MAPPING);
    REQUIRE(order);

    for (const auto rawTime : std::array<std::int64_t, 5>{0, -1, std::numeric_limits<std::int64_t>::min(),
                                                          std::numeric_limits<std::int64_t>::max(), 4294967301LL}) {
        auto fields = values(*schema);
        const auto bits = static_cast<std::uint64_t>(rawTime);
        fields[0].raw = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32));
        fields[1].raw = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits));
        auto raw = maker->map(u"AAPL", {schema->recordContext(0), fields}, VALUES, 0, 2, rawTime);
        REQUIRE(raw);
        CHECK(raw->eventFlags == 2);
        CHECK(fields[0].raw == std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32)));

        if (rawTime == std::numeric_limits<std::int64_t>::min()) {
            CHECK(raw->index == 0);
        }

        auto derived = order->map(u"AAPL", {schema->recordContext(0), fields}, q::events::DerivedOrderRole::BID, VALUES,
                                  0, 2, rawTime);

        if (rawTime == -1 || rawTime == std::numeric_limits<std::int64_t>::max()) {
            CHECK_FALSE(derived);
        } else {
            REQUIRE(derived);
            CHECK(derived->getSourceId() == 5);
        }

        CHECK_FALSE(maker->map(u"AAPL", {schema->recordContext(0), fields}, VALUES, 0, 0, rawTime));
        fields[1].raw = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits) + 1);
        CHECK_FALSE(maker->map(u"AAPL", {schema->recordContext(0), fields}, VALUES, 0, 2, rawTime));
    }

    auto fields = values(*schema);
    fields[0].raw = std::numeric_limits<std::int32_t>::min();
    CHECK_FALSE(q::core::validFieldValue(fields[0]));
    CHECK_FALSE(maker->map(u"AAPL", {schema->recordContext(0), fields}, VALUES, 0, 2));
    fields[0].units = std::u16string{};
    CHECK_FALSE(maker->map(u"AAPL", {schema->recordContext(0), fields}, VALUES, 0, 2,
                           std::numeric_limits<std::int64_t>::min()));
    fields[0].units.reset();
    fields[0].bytes = std::vector<std::byte>{};
    CHECK_FALSE(maker->map(u"AAPL", {schema->recordContext(0), fields}, VALUES, 0, 2,
                           std::numeric_limits<std::int64_t>::min()));
    fields[0].bytes.reset();
    std::vector<q::core::FieldValueView> borrowed;

    for (const auto &field : fields) {
        borrowed.push_back({field.type, field.raw, {}, {}});
    }

    auto view = maker->mapView(u"AAPL", {schema->recordContext(0), borrowed}, VALUES, 0, 2,
                               std::numeric_limits<std::int64_t>::min());
    REQUIRE(view);
    CHECK(view->index == 0);
    borrowed[0].units = std::u16string_view{};
    CHECK_FALSE(maker->mapView(u"AAPL", {schema->recordContext(0), borrowed}, VALUES, 0, 2,
                               std::numeric_limits<std::int64_t>::min()));
    borrowed[0].units.reset();
    borrowed[0].bytes = std::span<const std::byte>{};
    CHECK_FALSE(maker->mapView(u"AAPL", {schema->recordContext(0), borrowed}, VALUES, 0, 2,
                               std::numeric_limits<std::int64_t>::min()));
    auto foreign = q::events::defaultMarketMakerSchema(SCHEMA);
    REQUIRE(foreign);
    CHECK_FALSE(maker->map(u"AAPL", {foreign->recordContext(0), fields}, VALUES, 0, 2,
                           std::numeric_limits<std::int64_t>::min()));
    fields = values(*schema);
    CHECK(maker->map(u"AAPL", {schema->recordContext(0), fields}, VALUES, 0, 2));
    CHECK(maker->map(u"AAPL", {schema->recordContext(0), fields}, VALUES, 0, 2, std::int64_t{0}));
    auto extraRecord = schema->records()[0];
    extraRecord.fields.push_back({std::to_underlying(q::core::SerialFieldType::UTF_CHAR)});
    extraRecord.lookups.push_back({u"Extra", 10});
    auto extraSchema =
        q::core::LocalSchema::create(std::span(&extraRecord, 1), SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(extraSchema);
    auto extra = q::events::PreparedMarketMaker::prepare(*extraSchema, 0, MAPPING);
    REQUIRE(extra);
    auto extraFields = values(*extraSchema);
    extraFields[0].raw = std::numeric_limits<std::int32_t>::min();
    extraFields.back().raw = -1;
    CHECK_FALSE(extra->map(u"AAPL", {extraSchema->recordContext(0), extraFields}, VALUES, 0, 2,
                           std::numeric_limits<std::int64_t>::min()));
    auto quoteSchema = q::events::defaultQuoteSchema(SCHEMA);
    REQUIRE(quoteSchema);
    auto quote = q::events::PreparedDerivedOrder::prepare(*quoteSchema, 0, q::events::DerivedOrderKind::QUOTE, MAPPING);
    REQUIRE(quote);
    auto quoteFields = values(*quoteSchema);
    CHECK_FALSE(quote->map(u"AAPL", {quoteSchema->recordContext(0), quoteFields}, q::events::DerivedOrderRole::BID,
                           VALUES, 0, 2, std::int64_t{0}));
}
