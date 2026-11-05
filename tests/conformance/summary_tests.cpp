// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/summary.hpp"
#include "receiving_profile.hpp"
#include "summary_fixture.hpp"
#include "trade_eth_fixture.hpp"
#include "trade_fixture.hpp"
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
namespace f = summary_fixture;
constexpr c::SchemaLimits SCHEMA{16, 128, 256, 128, 16384}; ///< Existing finite complete receiving schema.
constexpr e::MappingLimits MAPPING{128, 256, 32, 64, 128, 128, 20480, 3000000}; ///< Finite complete mapping allowances.

/** @return Process-owned actual production observations, initialized once. */
const auto &corpus() {
    static const auto result = f::load(ORG_TTLDTOR_QDCXX_SUMMARY_FIXTURE);

    return result;
}

/** Construct an explicit eight-record, five-kind fixture within existing schema caps.
 * @return Complete immutable Summary/ETH/Trade composite+regional and Quote/Profile registry.
 */
a::EventRegistry mixedRegistry() {
    auto records = corpus().at("default120").records;
    const auto eth = trade_eth_fixture::load(ORG_TTLDTOR_QDCXX_SUMMARY_ETH_FIXTURE);
    const auto regular = trade_fixture::load(ORG_TTLDTOR_QDCXX_SUMMARY_TRADE_FIXTURE);

    for (auto record : eth.at("default120").records) {
        record.id += 2;
        records.push_back(std::move(record));
    }

    for (auto record : regular.at("default120").records) {
        record.id += 4;
        records.push_back(std::move(record));
    }

    auto quote = e::defaultQuoteSchema(SCHEMA);
    auto profile = e::defaultProfileSchema(SCHEMA);
    REQUIRE(quote);
    REQUIRE(profile);
    records.push_back(quote->records()[0]);
    records.back().id = 6;
    records.push_back(profile->records()[0]);
    records.back().id = 7;
    auto schema = c::LocalSchema::create(records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const std::array registrations{
        a::EventRegistration{a::EventType::SUMMARY, 0},   a::EventRegistration{a::EventType::SUMMARY, 1},
        a::EventRegistration{a::EventType::TRADE_ETH, 2}, a::EventRegistration{a::EventType::TRADE_ETH, 3},
        a::EventRegistration{a::EventType::TRADE, 4},     a::EventRegistration{a::EventType::TRADE, 5},
        a::EventRegistration{a::EventType::QUOTE, 6},     a::EventRegistration{a::EventType::PROFILE, 7}};
    a::EventRegistryWork work;
    auto registry = a::EventRegistry::create(*schema, registrations, {}, work);
    REQUIRE(registry);

    return *registry;
}
} // namespace

/** Complete actual getter parity includes defaults, physical high words and classified business-long correction. */
TEST_CASE("complete Summary projections match production values and classified long OI reference") {
    std::size_t observations = 0;

    for (const auto &[name, scenario] : corpus()) {
        CAPTURE(name);

        if (name.starts_with("required-") || name.starts_with("bad")) {
            continue;
        }

        auto schema = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::int32_t id = 0; id < 2; ++id) {
            auto mapping = e::PreparedSummary::prepare(*schema, id, MAPPING);
            REQUIRE(mapping);

            for (const auto &[identity, sample] : scenario.samples) {
                if (identity.first != static_cast<std::size_t>(id)) {
                    continue;
                }

                CAPTURE(identity.second);
                std::vector<c::FieldValueView> views;

                for (const auto &field : sample.values) {
                    views.push_back({field.type, field.raw});
                }

                const auto metadata = std::stoll(sample.expected[1]);
                auto owned = mapping->map(sample.symbol, {schema->recordContext(id), sample.values}, 128, metadata);
                auto borrowed = mapping->mapView(sample.symbol, {schema->recordContext(id), views}, 128, metadata);
                REQUIRE(owned);
                REQUIRE(borrowed);
                CHECK(f::matches(*owned, sample.expected));
                CHECK(f::matches(*borrowed, sample.expected));
                ++observations;
            }
        }
    }

    CHECK(observations > 4000);
}

TEST_CASE("Summary corrected OpenInterest preserves finite wide counts and regional absence") {
    const auto &scenario = corpus().at("oi-long120");
    auto schema = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    auto mapping = e::PreparedSummary::prepare(*schema, 0, MAPPING);
    REQUIRE(mapping);
    const auto &threeBillion = scenario.samples.at({0, 14});
    REQUIRE(threeBillion.expected[4] == "3000000000");
    auto value = mapping->map(threeBillion.symbol, {schema->recordContext(0), threeBillion.values}, 128);
    REQUIRE(value);
    CHECK(value->getOpenInterest() == 3000000000LL);
    const auto &twoTo32 = scenario.samples.at({0, 16});
    REQUIRE(twoTo32.expected[4] == "4294967296");
    value = mapping->map(twoTo32.symbol, {schema->recordContext(0), twoTo32.values}, 128);
    REQUIRE(value);
    CHECK(value->getOpenInterest() == 4294967296LL);
    auto regional = e::PreparedSummary::prepare(*schema, 1, MAPPING);
    REQUIRE(regional);
    const auto &absent = scenario.samples.at({1, 14});
    auto regionalValue = regional->map(absent.symbol, {schema->recordContext(1), absent.values}, 128);
    REQUIRE(regionalValue);
    CHECK(regionalValue->getOpenInterest() == 0);
}

TEST_CASE("Summary refuses required absence incompatible types and complete invalid shapes atomically") {
    for (const auto &[name, scenario] : corpus()) {
        if (!name.starts_with("required-") && !name.starts_with("bad")) {
            continue;
        }

        CAPTURE(name);
        auto schema = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);

        for (std::int32_t id = 0; id < 2; ++id) {
            auto mapping = e::PreparedSummary::prepare(*schema, id, MAPPING);

            if (name.starts_with("bad-interest") && id == 1) {
                REQUIRE(mapping);
                const auto &sample = scenario.samples.at({1, 0});
                auto mapped = mapping->map(sample.symbol, {schema->recordContext(1), sample.values}, 128,
                                           std::stoll(sample.expected[1]));
                REQUIRE(mapped);
                CHECK(f::matches(*mapped, sample.expected));
                continue;
            }

            REQUIRE_FALSE(mapping);
            CHECK(mapping.error() == (name.starts_with("required-") ? e::MappingError::MISSING_REQUIRED
                                                                    : e::MappingError::INCOMPATIBLE_TYPE));
        }
    }

    const auto &scenario = corpus().at("default120");
    const auto &sample = scenario.samples.at({0, 0});
    auto schema = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    auto mapping = e::PreparedSummary::prepare(*schema, 0, MAPPING);
    REQUIRE(mapping);
    auto damaged = sample.values;
    damaged.pop_back();
    auto missing = mapping->map(sample.symbol, {schema->recordContext(0), damaged}, 128);
    REQUIRE_FALSE(missing);
    CHECK(missing.error() == e::MappingError::INVALID_VALUES);
    damaged = sample.values;
    damaged[0].type = c::SerialFieldType::STRING;
    REQUIRE_FALSE(mapping->map(sample.symbol, {schema->recordContext(0), damaged}, 128));
    auto foreign = e::defaultSummarySchema(SCHEMA);
    REQUIRE(foreign);
    auto wrong = mapping->map(sample.symbol, {foreign->recordContext(0), sample.values}, 128);
    REQUIRE_FALSE(wrong);
    CHECK(wrong.error() == e::MappingError::WRONG_CONTEXT);
    REQUIRE_FALSE(mapping->map(sample.symbol, {schema->recordContext(0), sample.values}, 3));
}

TEST_CASE("Summary plain default initialization snapshots and exact75 versus74 limits remain finite") {
    static_assert(std::is_aggregate_v<e::Summary>);
    e::Summary plain;
    CHECK(std::isnan(plain.getDayOpenPrice()));
    CHECK(std::isnan(plain.getDayHighPrice()));
    CHECK(std::isnan(plain.getDayLowPrice()));
    CHECK(std::isnan(plain.getDayClosePrice()));
    CHECK(std::isnan(plain.getPrevDayClosePrice()));
    CHECK(std::isnan(plain.getPrevDayVolume()));
    CHECK(plain.getOpenInterest() == 0);
    CHECK(plain.getDayId() == 0);
    CHECK(plain.getPrevDayId() == 0);
    CHECK(plain.getDayClosePriceType() == e::PriceType::REGULAR);
    CHECK(plain.getPrevDayClosePriceType() == e::PriceType::REGULAR);
    auto defaults = e::defaultSummarySchema(SCHEMA);
    REQUIRE(defaults);
    auto exact = MAPPING;
    exact.typeAlternatives = 75;
    REQUIRE(e::PreparedSummary::prepare(*defaults, 0, exact));
    --exact.typeAlternatives;
    auto refused = e::PreparedSummary::prepare(*defaults, 0, exact);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == e::MappingError::RESOURCE_LIMIT);
    const auto &scenario = corpus().at("numeric104");
    auto schema = c::LocalSchema::create(scenario.records, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    auto mapping = e::PreparedSummary::prepare(*schema, 0, MAPPING);
    REQUIRE(mapping);
    bool witness = false;

    for (const auto &[identity, sample] : scenario.samples) {
        if (identity.first) {
            continue;
        }

        const auto raw = std::stoll(sample.expected[8]);

        if (std::bit_cast<std::int64_t>(c::WideDecimal{raw}.toDouble()) == std::stoll(sample.expected[14])) {
            continue;
        }

        auto result = mapping->map(sample.symbol, {schema->recordContext(0), sample.values}, 128);
        REQUIRE(result);
        auto copy = *result;
        auto moved = std::move(*result);
        CHECK(copy.getDayOpenPrice() == moved.getDayOpenPrice());
        CHECK(std::bit_cast<std::int64_t>(moved.getDayOpenPrice()) == std::stoll(sample.expected[14]));
        moved.dayOpenPrice = {128};
        CHECK(moved.getDayOpenPrice() == 0);
        CHECK(std::bit_cast<std::int64_t>(moved.getDayHighPrice()) == std::stoll(sample.expected[15]));
        moved.businessNumericSnapshot.reset();
        CHECK(moved.getDayHighPrice() == moved.dayHighPrice.toDouble());
        witness = true;
        break;
    }

    CHECK(witness);
    a::EventRegistryWork work;
    auto builtin = a::EventRegistry::builtIn({}, work);
    REQUIRE(builtin);
    CHECK(builtin->registrations().size() == 9);
    CHECK(builtin->schema().records().size() == 9);
    CHECK(builtin->registrations()[4].type == a::EventType::SUMMARY);
    std::size_t fields = 0;

    for (const auto &record : builtin->schema().records()) {
        fields += record.fields.size();
    }

    CHECK(fields == 69);
}

TEST_CASE("all finite receiving selections include only registered kinds and prepay binding copies atomically") {
    auto registry = std::optional<a::EventRegistry>{mixedRegistry()};
    REQUIRE(registry);
    a::EndpointOptions options;
    options.eventRegistry = *registry;
    a::RuntimeOptions runtime;
    runtime.maximumSubscriptionsPerEndpoint = 33;
    runtime.maximumSymbolsPerSubscription = 2;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    auto profile = a::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);

    for (unsigned mask = 1; mask <= 1023; ++mask) {
        const auto &plan = a::detail::receivingEventPlan(profile, static_cast<a::detail::EventSelection>(mask));
        CHECK(plan.bindingCount == ((mask & 1 ? 1 : 0) + (mask & 2 ? 1 : 0) + (mask & 4 ? 2 : 0) + (mask & 8 ? 2 : 0) +
                                    (mask & 16 ? 2 : 0)));
        std::array<q::client::PreparedSubscriptionBinding, a::detail::MAXIMUM_RECEIVING_RECORDS> selected{};
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

        for (std::size_t i = 0; i < plan.bindingCount; ++i) {
            CHECK((mask & (1U << static_cast<unsigned>(selected[i].kind))) != 0);
        }

        auto seed = plan.work;

        REQUIRE(a::detail::copyReceivingBindings(profile, plan, selected, seed));
        CHECK((seed.preparationWork > 0) == (plan.bindingCount > 0));
        CHECK(seed.visits >= plan.bindingCount);
    }
}

TEST_CASE("fresh receiving grant funds a whole atomic fanout and real select billing has exact one-below boundary") {
    auto registry = std::optional<a::EventRegistry>{mixedRegistry()};
    REQUIRE(registry);
    a::EndpointOptions options;
    options.eventRegistry = *registry;
    a::RuntimeOptions runtime;
    runtime.maximumSubscriptionsPerEndpoint = 33;
    runtime.maximumSymbolsPerSubscription = 2;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    auto profile = a::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
    CHECK(profile.eventPlans.size() == 1023);
    CHECK(profile.schema.records().size() == 8);
    CHECK(profile.interests.clients == 33);
    std::size_t fieldCount = 0;

    for (const auto &record : profile.schema.records()) {
        fieldCount += record.fields.size();
    }

    CHECK(fieldCount == 91);
    std::cout << "RESOURCE Summary=" << sizeof(e::Summary) << " TradeETH=" << sizeof(e::TradeETH)
              << " Trade=" << sizeof(e::Trade) << " RegisteredEvent=" << sizeof(a::RegisteredEvent)
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
