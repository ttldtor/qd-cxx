// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/event_flags.hpp"
#include "qd/events/option_sale.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace c = q::core;
namespace e = q::events;
using Type = c::SerialFieldType;

constexpr c::SchemaLimits SCHEMA{1, 32, 32, 64, 1024};
constexpr e::MappingLimits MAPPING{32, 32, 32, 32, 256, 64, 2048, 20000};
constexpr e::EventValueLimits VALUES{64, 64, 192};

std::vector<c::FieldValue> zeroValues(const c::LocalSchema &schema) {
    std::vector<c::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        result.push_back({static_cast<Type>(field.serialTypeId), 0});
    }

    return result;
}
} // namespace

TEST_CASE("OptionSale retains independent index time sequence flags and owned nullable option symbol") {
    const auto schema = e::defaultOptionSaleSchema(SCHEMA);
    REQUIRE(schema);
    const auto mapper = e::PreparedOptionSale::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[0].raw = -20;
    fields[1].raw = -1;
    fields[2].raw = 0x00400005;
    fields[3].raw = u'Q';
    fields[4].raw = 0x180;
    fields[5].raw = 0x280;
    fields[8].raw = 0x4142;
    fields[9].raw = (u'Q' << 8) | (2 << 5) | (1 << 4) | (1 << 3) | (1 << 2) | 1;
    fields[13].units = u".IBM270115C100";
    std::u16string symbol = u"IBM";
    const auto event = mapper->map(symbol, {schema->recordContext(0), fields}, VALUES, 1234, 5);
    REQUIRE(event);
    CHECK(event->symbol == u"IBM");
    CHECK(event->eventTime == 1234);
    CHECK(event->eventFlags == 5);
    CHECK(event->getSource().id() == 0);
    CHECK(event->getIndex() == -20);
    CHECK(event->getTime() == -999);
    CHECK(event->getSequence() == 5);
    CHECK(event->getTimeNanoPart() == 0);
    CHECK(event->getExchangeCode() == u'Q');
    CHECK(event->getPrice() == 1);
    CHECK(event->getSize() == 2);
    CHECK(event->getExchangeSaleConditions() == std::optional<std::u16string>(u"AB"));
    CHECK(event->getOptionSymbol() == std::optional<std::u16string>(u".IBM270115C100"));
    CHECK(event->getTradeThroughExempt() == u'Q');
    CHECK(event->getAggressorSide() == e::Side::SELL);
    CHECK(event->isSpreadLeg());
    CHECK(event->isExtendedTradingHours());
    CHECK(event->isValidTick());
    CHECK(event->getType() == e::TimeAndSaleType::CORRECTION);
    CHECK(event->isCorrection());
    CHECK_FALSE(event->isCancel());
    symbol[0] = u'X';
    fields[13].units->clear();
    CHECK(event->symbol == u"IBM");
    CHECK(event->getOptionSymbol() == std::optional<std::u16string>(u".IBM270115C100"));
    const auto empty = mapper->map(symbol, {schema->recordContext(0), fields}, VALUES);
    REQUIRE(empty);
    CHECK(empty->optionSymbol == std::optional<std::u16string>(u""));
    fields[13].units.reset();
    const auto nullValue = mapper->map(symbol, {schema->recordContext(0), fields}, VALUES);
    REQUIRE(nullValue);
    CHECK_FALSE(nullValue->optionSymbol);
}

TEST_CASE("OptionSale optional nano and required value shapes have bounded ownership") {
    const auto standard = e::defaultOptionSaleSchema(SCHEMA);
    REQUIRE(standard);
    auto record = standard->records()[0];
    record.lookups.push_back({u"TimeNanoPart", record.fields.size()});
    record.fields.push_back({std::to_underlying(Type::COMPACT_INT)});
    const auto schema = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedOptionSale::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[1].raw = 1;
    fields[8].raw = std::numeric_limits<std::int32_t>::min();
    fields[13].units = u"AB";
    fields.back().raw = 999;
    const auto value = mapper->map(u"IBM", {schema->recordContext(0), fields}, VALUES);
    REQUIRE(value);
    CHECK(value->getTimeNanoPart() == 999);
    CHECK(value->getTimeNanos() == 1000000999LL);
    REQUIRE(value->exchangeSaleConditions);
    CHECK(value->exchangeSaleConditions->size() == 5);
    CHECK(mapper->map(u"IBM", {schema->recordContext(0), fields}, {64, 4, 192}) ==
          std::unexpected(e::MappingError::RESOURCE_LIMIT));
    CHECK(mapper->map(u"IBM", {schema->recordContext(0), fields}, {64, 64, 9}) ==
          std::unexpected(e::MappingError::RESOURCE_LIMIT));
    const auto unrelated = e::defaultOptionSaleSchema(SCHEMA);
    REQUIRE(unrelated);
    CHECK(mapper->map(u"IBM", {unrelated->recordContext(0), fields}, VALUES) ==
          std::unexpected(e::MappingError::WRONG_CONTEXT));
    fields[0].raw = std::numeric_limits<std::int64_t>::max();
    CHECK(mapper->map(u"IBM", {schema->recordContext(0), fields}, VALUES) ==
          std::unexpected(e::MappingError::INVALID_VALUES));
}

TEST_CASE("OptionSale requires every non-nano field and rejects wrong option-symbol type") {
    const auto standard = e::defaultOptionSaleSchema(SCHEMA);
    REQUIRE(standard);
    const auto &original = standard->records()[0];

    for (std::size_t omitted = 0; omitted < original.fields.size(); ++omitted) {
        c::LocalRecord record{0, u"OptionSale", false, {}, {}};

        for (std::size_t index = 0; index < original.fields.size(); ++index) {
            if (index != omitted) {
                record.fields.push_back(original.fields[index]);
                record.lookups.push_back({original.lookups[index].name, record.fields.size() - 1});
            }
        }

        const auto missing = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(missing);
        CHECK(e::PreparedOptionSale::prepare(*missing, 0, MAPPING) ==
              std::unexpected(e::MappingError::MISSING_REQUIRED));
    }

    auto record = original;
    record.fields.back().serialTypeId = std::to_underlying(Type::COMPACT_INT);
    const auto wrong = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(wrong);
    CHECK(e::PreparedOptionSale::prepare(*wrong, 0, MAPPING) == std::unexpected(e::MappingError::INCOMPATIBLE_TYPE));
}

TEST_CASE("OptionSale removal validates index separately from time and reserved flags match Java defaults") {
    const auto schema = e::defaultOptionSaleSchema(SCHEMA);
    REQUIRE(schema);
    const auto mapper = e::PreparedOptionSale::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);
    auto fields = zeroValues(*schema);
    fields[0].raw = -1;
    fields[1].raw = 2;
    fields[2].raw = 0x00400005;
    fields[9].raw = 3 | (3 << 5);
    const e::RecordValueView values{schema->recordContext(0), fields};
    CHECK(mapper->map(u"IBM", values, VALUES, 0, 0, -1) == std::unexpected(e::MappingError::INVALID_INPUT));
    CHECK(mapper->map(u"IBM", values, VALUES, 0, e::indexed_event_flags::REMOVE_EVENT, 0) ==
          std::unexpected(e::MappingError::INVALID_VALUES));
    const auto event = mapper->map(u"IBM", values, VALUES, 0, e::indexed_event_flags::REMOVE_EVENT, -1);
    REQUIRE(event);
    CHECK(event->getIndex() == -1);
    CHECK(event->getTime() == 2001);
    CHECK(event->getSequence() == 5);
    CHECK(event->flags == (3 | (3 << 5)));
    CHECK(event->getType() == e::TimeAndSaleType::NEW);
    CHECK(event->getAggressorSide() == e::Side::UNDEFINED);
}

namespace {
struct SaleOracle {
    c::LocalRecord record{0, u"OptionSale", true, {}, {}, c::IndexedTimeLowInt32{0}};
    std::array<std::vector<c::FieldValue>, 8> inputs;
    std::vector<std::vector<std::string>> events;
};

std::optional<std::u16string> oracleUnits(const std::string &value) {
    if (value == "null")
        return std::nullopt;

    REQUIRE(value.starts_with('u'));
    REQUIRE((value.size() - 1) % 4 == 0);
    std::u16string result;

    for (std::size_t i = 1; i < value.size(); i += 4)
        result.push_back(static_cast<char16_t>(std::stoul(value.substr(i, 4), nullptr, 16)));

    return result;
}

SaleOracle readOptionSaleOracle(const std::filesystem::path &file) {
    SaleOracle result;
    std::ifstream input(file);
    REQUIRE(input);

    for (std::string line; std::getline(input, line);) {
        std::istringstream source(line);
        std::vector<std::string> row;

        for (std::string cell; std::getline(source, cell, '\t');)
            row.push_back(std::move(cell));

        REQUIRE_FALSE(row.empty());

        if (row[0] == "int" || row[0] == "object") {
            REQUIRE(row.size() == 4);
            const auto index = result.record.fields.size();
            result.record.fields.push_back({std::stoi(row[1])});
            result.record.lookups.push_back({std::u16string(row[3].begin(), row[3].end()), index});
        } else if (row[0] == "value" || row[0] == "object-value") {
            REQUIRE(row.size() == 4);
            const auto event = std::stoul(row[1]);
            REQUIRE(event < result.inputs.size());
            auto &values = result.inputs[event];
            REQUIRE(values.size() < result.record.fields.size());
            const auto &name = result.record.lookups[values.size()].name;
            REQUIRE(name == std::u16string(row[2].begin(), row[2].end()));
            const auto type = static_cast<Type>(result.record.fields[values.size()].serialTypeId);

            if (row[0] == "value")
                values.push_back({type, std::stoll(row[3])});
            else
                values.push_back({type, 0, oracleUnits(row[3])});
        } else if (row[0] == "event") {
            REQUIRE(row.size() == 40);
            REQUIRE(std::stoul(row[1]) == result.events.size());
            result.events.push_back(std::move(row));
        }
    }

    REQUIRE(result.events.size() == result.inputs.size());

    return result;
}

void verifyOptionSaleOracle(const std::filesystem::path &file) {
    auto oracle = readOptionSaleOracle(file);
    const auto schema = c::LocalSchema::create(std::span(&oracle.record, 1), SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const auto mapper = e::PreparedOptionSale::prepare(*schema, 0, MAPPING);
    REQUIRE(mapper);

    for (std::size_t index = 0; index < oracle.events.size(); ++index) {
        const auto &row = oracle.events[index];
        INFO(file.string(), " event=", index);
        const auto number = [&](std::size_t column) {
            return std::stoll(row[column]);
        };
        const auto flag = [&](std::size_t column) {
            return row[column] == "true";
        };
        const auto symbol = oracleUnits(row[2]);
        REQUIRE(symbol);
        const auto flags = std::stoi(row[4]);
        const auto removal =
            (flags & e::indexed_event_flags::REMOVE_EVENT) ? std::optional<std::int64_t>(number(39)) : std::nullopt;
        const auto event =
            mapper->map(*symbol, {schema->recordContext(0), oracle.inputs[index]}, VALUES, number(3), flags, removal);
        REQUIRE(event);
        CHECK(event->symbol == *symbol);
        CHECK(event->eventTime == number(3));
        CHECK(event->eventFlags == flags);
        CHECK(event->getSource().id() == number(5));
        CHECK(event->getIndex() == number(6));
        CHECK(event->getTimeSequence() == number(7));
        CHECK(event->getTime() == number(8));
        CHECK(event->getTimeNanoPart() == number(9));
        CHECK(event->getTimeNanos() == number(10));
        CHECK(event->getSequence() == number(11));
        CHECK(event->getExchangeCode() == number(12));
        CHECK(event->getExchangeSaleConditions() == oracleUnits(row[17]));
        CHECK(event->flags == number(18));
        CHECK(event->getTradeThroughExempt() == number(19));
        CHECK(std::to_underlying(event->getAggressorSide()) == number(20));
        CHECK(event->isSpreadLeg() == flag(21));
        CHECK(event->isExtendedTradingHours() == flag(22));
        CHECK(event->isValidTick() == flag(23));
        CHECK(std::to_underlying(event->getType()) == number(24));
        CHECK(event->isNew() == flag(25));
        CHECK(event->isCorrection() == flag(26));
        CHECK(event->isCancel() == flag(27));
        CHECK(event->getOptionSymbol() == oracleUnits(row[31]));
        const std::array business{
            event->getPrice(),           event->getSize(),       event->getBidPrice(), event->getAskPrice(),
            event->getUnderlyingPrice(), event->getVolatility(), event->getDelta()};
        constexpr std::array columns{13U, 14U, 15U, 16U, 28U, 29U, 30U};

        for (std::size_t i = 0; i < business.size(); ++i)
            CHECK(std::bit_cast<std::int64_t>(business[i]) == number(columns[i]));
    }
}
} // namespace

TEST_CASE("OptionSale mapper matches pinned Java fields and values in all wire profiles") {
    const std::filesystem::path root(QDCXX_OPTION_SALE_FIXTURES);

    for (const auto *profile : {"standard", "tiny", "nano", "field"})
        verifyOptionSaleOracle(root / profile / "oracle.tsv");
}
