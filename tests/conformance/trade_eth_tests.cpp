// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/trade_eth.hpp"
#include "receiving_profile.hpp"
#include "trade_eth_fixture.hpp"
#include "trade_fixture.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>
#include <iostream>
#include <type_traits>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace e = q::events;
namespace c = q::core;
namespace a = q::api;
namespace f = trade_eth_fixture;
constexpr c::SchemaLimits SCHEMA{16, 128, 256, 128,
                                 16384}; ///< Complete bounded catalog, including regional declarations.
constexpr e::MappingLimits MAPPING{128, 256, 32,    64,
                                   128, 128, 20480, 3000000}; ///< Explicit finite preparation credits.

/** @return Read-only owned actual Java production corpus, initialized once per test process. */
const auto &corpus() {
    static const auto value = f::load(ORG_TTLDTOR_QDCXX_TRADE_ETH_FIXTURE);

    return value;
}

/** Build the actual six-record mixed fixture registry with finite existing schema limits.
 * @return Owning ETH/regular/composite/regional routes and independent Quote/Profile interop.
 */
a::EventRegistry mixedRegistry() {
    auto records = corpus().at("default120").records;
    const auto regular = trade_fixture::load(ORG_TTLDTOR_QDCXX_TRADE_INTEROP_FIXTURE);

    for (auto record : regular.at("default120").records) {
        record.id += 2;
        records.push_back(std::move(record));
    }

    auto quote = e::defaultQuoteSchema(SCHEMA);
    auto profile = e::defaultProfileSchema(SCHEMA);
    REQUIRE(quote);
    REQUIRE(profile);
    records.push_back(quote->records()[0]);
    records.back().id = 4;
    records.push_back(profile->records()[0]);
    records.back().id = 5;
    auto schema = c::LocalSchema::create(records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const std::array registrations{
        a::EventRegistration{a::EventType::TRADE_ETH, 0}, a::EventRegistration{a::EventType::TRADE_ETH, 1},
        a::EventRegistration{a::EventType::TRADE, 2},     a::EventRegistration{a::EventType::TRADE, 3},
        a::EventRegistration{a::EventType::QUOTE, 4},     a::EventRegistration{a::EventType::PROFILE, 5}};
    a::EventRegistryWork work;
    auto result = a::EventRegistry::create(*schema, registrations, {}, work);
    REQUIRE(result);

    return *result;
}

/** Compare actual business binary64 bits, with NaN classification rather than arithmetic payload promises.
 * @param value Actual native business getter.
 * @param expected Signed Java binary64 bits.
 */
void business(double value, const std::string &expected) {
    const auto bits = static_cast<std::uint64_t>(std::stoll(expected));

    if (std::isnan(std::bit_cast<double>(bits)))
        CHECK(std::isnan(value));
    else
        CHECK(std::bit_cast<std::uint64_t>(value) == bits);
}

/** Compare the complete typed event with independent actual generated mapping/delegate observations.
 * @param value Complete owning native TradeETH.
 * @param expected Exact production result cells.
 */
void compare(const e::TradeETH &value, const std::vector<std::string> &expected) {
    REQUIRE(expected.size() == 25);
    CHECK(value.symbol == f::units(expected[0]));
    const std::array<std::int64_t, 12> scalars{value.eventTime,
                                               value.timeSequence,
                                               value.getTime(),
                                               value.getTimeNanos(),
                                               value.getSequence(),
                                               value.timeNanoPart,
                                               value.exchangeCode,
                                               value.tradeId,
                                               value.dayId,
                                               value.flags,
                                               static_cast<std::int64_t>(value.getTickDirection()),
                                               value.isExtendedTradingHours() ? 1 : 0};

    for (std::size_t i = 0; i < scalars.size(); ++i)
        CHECK(scalars[i] == std::stoll(expected[1 + i]));

    const std::array wide{value.price.raw, value.size.raw, value.change.raw, value.dayVolume.raw,
                          value.dayTurnover.raw};
    const std::array doubles{value.getPrice(), value.getSizeAsDouble(), value.getChange(), value.getDayVolumeAsDouble(),
                             value.getDayTurnover()};

    for (std::size_t i = 0; i < wide.size(); ++i) {
        CHECK(wide[i] == std::stoll(expected[13 + i]));
        business(doubles[i], expected[18 + i]);
    }

    CHECK(value.getSize() == std::stoll(expected[23]));
    CHECK(value.getDayVolume() == std::stoll(expected[24]));
}
} // namespace

TEST_CASE("complete owning and borrowed TradeETH match actual composite and regional production getters") {
    std::size_t observations = 0;

    for (const auto &[name, scenario] : corpus()) {
        CAPTURE(name);

        if (name.starts_with("required") || name.starts_with("bad") || name.starts_with("id-one"))
            continue;

        auto schema = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::int32_t id = 0; id < 2; ++id) {
            auto mapping = e::PreparedTradeETH::prepare(*schema, id, MAPPING);
            REQUIRE(mapping);

            for (const auto &[identity, sample] : scenario.samples) {
                if (identity.first != static_cast<std::size_t>(id))
                    continue;

                CAPTURE(identity.second);
                std::vector<c::FieldValueView> views;

                for (const auto &field : sample.values)
                    views.push_back({field.type, field.raw});

                const auto metadata = std::stoll(sample.expected[1]);
                auto owned = mapping->map(sample.symbol, {schema->recordContext(id), sample.values}, 128, metadata);
                auto borrowed = mapping->mapView(sample.symbol, {schema->recordContext(id), views}, 128, metadata);
                REQUIRE(owned);
                REQUIRE(borrowed);
                compare(*owned, sample.expected);
                compare(*borrowed, sample.expected);
                ++observations;
            }
        }
    }

    CHECK(observations > 2000);
}

TEST_CASE("actual ETH-to-RTH flags preserve the last extended-hours business snapshot") {
    const auto &scenario = corpus().at("transition120");
    auto schema = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    for (std::int32_t id = 0; id < 2; ++id) {
        auto mapping = e::PreparedTradeETH::prepare(*schema, id, MAPPING);
        REQUIRE(mapping);
        std::array<e::TradeETH, 2> values;

        for (std::size_t sampleIndex = 0; sampleIndex < values.size(); ++sampleIndex) {
            const auto &sample = scenario.samples.at({static_cast<std::size_t>(id), sampleIndex});
            auto value = mapping->map(sample.symbol, {schema->recordContext(id), sample.values}, 128);
            REQUIRE(value);
            values[sampleIndex] = *value;
        }

        CHECK(values[0].isExtendedTradingHours());
        CHECK_FALSE(values[1].isExtendedTradingHours());
        CHECK(values[0].timeSequence == values[1].timeSequence);
        CHECK(values[0].timeNanoPart == values[1].timeNanoPart);
        CHECK(values[0].tradeId == values[1].tradeId);
        CHECK(values[0].dayId == values[1].dayId);
        CHECK(values[0].price.raw == values[1].price.raw);
        CHECK(values[0].size.raw == values[1].size.raw);
        CHECK(values[0].change.raw == values[1].change.raw);
        CHECK(values[0].dayVolume.raw == values[1].dayVolume.raw);
        CHECK(values[0].dayTurnover.raw == values[1].dayTurnover.raw);
    }
}

TEST_CASE("TradeETH preparation refuses missing required fields unsupported numeric types and one-slot TradeId") {
    for (const auto &[name, scenario] : corpus()) {
        if (!name.starts_with("required") && !name.starts_with("bad") && !name.starts_with("id-one"))
            continue;

        CAPTURE(name);
        auto schema = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::int32_t id = 0; id < 2; ++id) {
            auto mapped = e::PreparedTradeETH::prepare(*schema, id, MAPPING);
            REQUIRE_FALSE(mapped);
            CHECK(mapped.error() == (name.starts_with("required") ? e::MappingError::MISSING_REQUIRED
                                                                  : e::MappingError::INCOMPATIBLE_TYPE));
        }
    }
}

TEST_CASE("TradeETH defaults snapshots raw edits and exact preparation boundaries remain owning and finite") {
    static_assert(std::is_aggregate_v<e::TradeETH>);
    static_assert(!std::is_convertible_v<e::TradeETH, e::Trade>);
    e::TradeETH plain;
    CHECK(std::isnan(plain.getPrice()));
    CHECK(std::isnan(plain.getSizeAsDouble()));
    CHECK(std::isnan(plain.getChange()));
    CHECK(std::isnan(plain.getDayVolumeAsDouble()));
    CHECK(std::isnan(plain.getDayTurnover()));
    CHECK(plain.getSize() == 0);
    CHECK(plain.getDayVolume() == 0);
    CHECK(plain.getTime() == 0);
    CHECK(plain.getTickDirection() == e::Direction::UNDEFINED);
    CHECK_FALSE(plain.isExtendedTradingHours());
    auto schema = e::defaultTradeETHSchema(SCHEMA);
    REQUIRE(schema);
    auto exact = MAPPING;
    exact.typeAlternatives = 99;
    REQUIRE(e::PreparedTradeETH::prepare(*schema, 0, exact));
    --exact.typeAlternatives;
    auto refused = e::PreparedTradeETH::prepare(*schema, 0, exact);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == e::MappingError::RESOURCE_LIMIT);
    const auto &scenario = corpus().at("numeric104");
    auto local = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    auto mapping = e::PreparedTradeETH::prepare(*local, 0, MAPPING);
    REQUIRE(mapping);
    bool found = false;

    for (const auto &[identity, sample] : scenario.samples) {
        if (identity.first || sample.expected[18] == std::to_string(std::bit_cast<std::int64_t>(
                                                         c::WideDecimal{std::stoll(sample.expected[13])}.toDouble())))
            continue;

        auto event = mapping->map(sample.symbol, {local->recordContext(0), sample.values}, 128);
        REQUIRE(event);
        found = true;
        business(event->getPrice(), sample.expected[18]);
        const auto copy = *event;
        auto moved = std::move(*event);
        CHECK(copy.getPrice() == moved.getPrice());
        moved.price = {128};
        CHECK(moved.getPrice() == 0);
        business(moved.getSizeAsDouble(), sample.expected[19]);
        moved.businessNumericSnapshot.reset();
        CHECK(moved.getSizeAsDouble() == moved.size.toDouble());
        auto wrong = mapping->map(sample.symbol, {schema->recordContext(0), sample.values}, 128);
        REQUIRE_FALSE(wrong);
        auto shortSymbol = mapping->map(sample.symbol, {local->recordContext(0), sample.values}, 3);
        REQUIRE_FALSE(shortSymbol);
        CHECK(shortSymbol.error() == e::MappingError::RESOURCE_LIMIT);
        break;
    }

    CHECK(found);
    a::EventRegistryWork work;
    auto builtin = a::EventRegistry::builtIn({}, work);
    REQUIRE(builtin);
    REQUIRE(builtin->registrations().size() == 9);
    CHECK(builtin->registrations()[3].type == a::EventType::TRADE_ETH);
}

TEST_CASE("all fifteen finite receiving selections include only their kinds and prepay binding copies atomically") {
    auto registry = std::optional<a::EventRegistry>{mixedRegistry()};
    REQUIRE(registry);
    a::EndpointOptions options;
    options.eventRegistry = *registry;
    a::RuntimeOptions runtime;
    runtime.maximumSubscriptionsPerEndpoint = 17;
    runtime.maximumSymbolsPerSubscription = 2;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    auto profile = a::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);

    for (unsigned mask = 1; mask <= 15; ++mask) {
        const auto &plan = a::detail::receivingEventPlan(profile, static_cast<a::detail::EventSelection>(mask));
        CHECK(plan.bindingCount == ((mask & 1 ? 1 : 0) + (mask & 2 ? 1 : 0) + (mask & 4 ? 2 : 0) + (mask & 8 ? 2 : 0)));
        std::array<q::client::PreparedSubscriptionBinding, a::detail::MAXIMUM_RECEIVING_BINDINGS> selected{};
        selected[0].localId = -1;
        q::client::ReceiverWork credits{2 * plan.bindingCount, a::detail::RECEIVING_BINDING_COPY_WORK - 1};
        CHECK_FALSE(a::detail::copyReceivingBindings(profile, plan, selected, credits));
        CHECK(credits.preparationWork == a::detail::RECEIVING_BINDING_COPY_WORK - 1);
        CHECK(credits.visits == 2 * plan.bindingCount);
        CHECK(selected[0].localId == -1);
        ++credits.preparationWork;
        REQUIRE(a::detail::copyReceivingBindings(profile, plan, selected, credits));
        CHECK(credits.visits == 0);
        CHECK(credits.preparationWork == 0);

        for (std::size_t i = 0; i < plan.bindingCount; ++i)
            CHECK((mask & (1U << static_cast<unsigned>(selected[i].kind))) != 0);

        auto seed = plan.work;
        REQUIRE(a::detail::copyReceivingBindings(profile, plan, selected, seed));
        CHECK(seed.preparationWork > 0);
        CHECK(seed.visits >= plan.bindingCount);
    }
}

TEST_CASE("fresh receiving grant funds a whole atomic fanout and real select billing has exact one-below boundary") {
    auto registry = std::optional<a::EventRegistry>{mixedRegistry()};
    REQUIRE(registry);
    a::EndpointOptions options;
    options.eventRegistry = *registry;
    a::RuntimeOptions runtime;
    runtime.maximumSubscriptionsPerEndpoint = 17;
    runtime.maximumSymbolsPerSubscription = 2;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    auto profile = a::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
    CHECK(profile.eventPlans.size() == 16);
    CHECK(profile.schema.records().size() == 6);
    CHECK(profile.interests.clients == 17);
    std::size_t fieldCount = 0;

    for (const auto &record : profile.schema.records()) {
        fieldCount += record.fields.size();
    }

    CHECK(fieldCount == 72);
    std::cout << "RESOURCE TradeETH=" << sizeof(e::TradeETH) << " Trade=" << sizeof(e::Trade)
              << " RegisteredEvent=" << sizeof(a::RegisteredEvent)
              << " SubscriptionProjection=" << sizeof(q::client::SubscriptionProjection)
              << " ReceivingSymbol=" << sizeof(a::detail::ReceivingSymbol) << " plans=" << profile.eventPlans.size()
              << " planBytes=" << sizeof(profile.eventPlans)
              << " retainedMetadata=" << profile.routing.retainedMetadataBytes
              << " clients=" << profile.interests.clients << " records=" << profile.schema.records().size()
              << " fields=" << fieldCount << " grantVisits=" << profile.queueWork.interest.visits
              << " grantUnits=" << profile.queueWork.interest.symbolUnits << '\n';
    auto creation = profile.queueWork.interest;
    auto interest = q::mux::MuxState::create(profile.schema, profile.family, profile.interests, creation,
                                             c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1);
    REQUIRE(interest);
    auto client = interest->createClient(creation);
    REQUIRE(client);
    const c::SubscriptionKey key{profile.schema.recordContext(0), u"AAPL"};
    auto probe = profile.queueWork.interest;
    const auto before = probe;
    REQUIRE(interest->select(*client, key, {}, probe));
    const auto queryVisits = before.visits - probe.visits;
    const auto queryUnits = before.symbolUnits - probe.symbolUnits;
    REQUIRE(queryVisits > 0);
    REQUIRE(queryUnits > 0);
    CHECK(before.bytes == probe.bytes);
    const auto queries = profile.queueLimits.routeRecords * profile.interests.clients;
    q::mux::MuxWork exact{queries, queryVisits * queries, queryUnits * queries, 0};
    auto oneBelow = exact;
    --oneBelow.symbolUnits;
    auto fresh = profile.queueWork.interest;

    for (std::size_t index = 0; index < queries; ++index) {
        REQUIRE(interest->select(*client, key, {}, exact));
        REQUIRE(interest->select(*client, key, {}, fresh));
        auto result = interest->select(*client, key, {}, oneBelow);

        if (index + 1 == queries) {
            REQUIRE_FALSE(result);
            CHECK(result.error() == q::mux::MuxError::WORK_LIMIT);
        } else {
            REQUIRE(result);
        }
    }

    CHECK(exact.operations == 0);
    CHECK(exact.visits == 0);
    CHECK(exact.symbolUnits == 0);
    CHECK(oneBelow.operations == 0);
    CHECK(oneBelow.visits == 0);
    CHECK(oneBelow.symbolUnits == queryUnits - 1);
}
