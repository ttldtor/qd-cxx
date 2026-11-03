// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "field_conversion_fixture.hpp"

#include "qd/api/event_registry.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <doctest/doctest.h>
#include <limits>
#include <type_traits>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace a = org::ttldtor::qdcxx::api;
namespace fixture = field_conversion_fixture;
constexpr c::SchemaLimits SCHEMA_LIMITS{2, 128, 256, 128, 16384}; ///< Complete finite source bounds.
constexpr e::MappingLimits MAPPING_LIMITS{128, 256, 32, 64, 128, 128, 20480, 3000000}; ///< Adapter work bounds.

/** Load actual Java production observations once per test executable.
 * @return Immutable owned corpus; input errors propagate as test failures.
 */
const std::map<std::string, fixture::Scenario> &corpus() {
    static const auto result = fixture::load(ORG_TTLDTOR_QDCXX_CONVERSION_FIXTURE);

    return result;
}

/** Build the declared complete binary profile without removing extra fields.
 * @param scenario Original exact production field declarations.
 * @return Owned immutable validated schema.
 */
c::LocalSchema schema(const fixture::Scenario &scenario) {
    auto result = c::LocalSchema::create(scenario.records, SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Compare finite/infinite binary64 bits and classify NaN without depending on its arithmetic payload.
 * @param actual Public business getter result.
 * @param expected Signed raw Java double bits from the real event getter.
 */
void checkBusinessDouble(double actual, const std::string &expected) {
    const auto bits = static_cast<std::uint64_t>(std::stoll(expected));

    if (std::isnan(std::bit_cast<double>(bits))) {
        CHECK(std::isnan(actual));
    } else {
        CHECK(std::bit_cast<std::uint64_t>(actual) == bits);
    }
}

/** Compare Quote output to generated mapping and real delegate getters.
 * @param value Complete owned native event.
 * @param expected Raw captured production result cells.
 */
void checkQuote(const e::Quote &value, const std::vector<std::string> &expected) {
    REQUIRE(expected.size() == 15);
    const std::array decimals{value.bidPrice.raw, value.bidSize.raw, value.askPrice.raw, value.askSize.raw};

    for (std::size_t i = 0; i < decimals.size(); ++i) {
        CHECK(decimals[i] == std::stoll(expected[i]));
    }

    CHECK(value.bidTimeMs == std::stoll(expected[4]));
    CHECK(value.bidExchangeCode == std::stoul(expected[5]));
    CHECK(value.askTimeMs == std::stoll(expected[6]));
    CHECK(value.askExchangeCode == std::stoul(expected[7]));
    CHECK(value.timeMs == std::stoll(expected[8]));
    CHECK(value.sequence == std::stoll(expected[9]));
    CHECK(value.timeNanoPart == std::stoll(expected[10]));
    CHECK(value.symbol == u"AAPL");
    const std::array business{value.getBidPrice(), value.getBidSizeAsDouble(), value.getAskPrice(),
                              value.getAskSizeAsDouble()};

    for (std::size_t i = 0; i < business.size(); ++i) {
        checkBusinessDouble(business[i], expected[11 + i]);
    }
}

/** Compare Profile numeric accessor bits, raw physical slots and nullable text.
 * @param value Complete owned native event.
 * @param expected Captured production mapping/delegate cells.
 */
void checkProfile(const e::Profile &value, const std::vector<std::string> &expected) {
    REQUIRE(expected.size() == 26);
    const std::array decimals{value.beta.raw,
                              value.earningsPerShare.raw,
                              value.dividendFrequency.raw,
                              value.exDividendAmount.raw,
                              value.high52WeekPrice.raw,
                              value.low52WeekPrice.raw,
                              value.shares.raw,
                              value.freeFloat.raw,
                              value.highLimitPrice.raw,
                              value.lowLimitPrice.raw};

    for (std::size_t i = 0; i < decimals.size(); ++i) {
        CHECK(decimals[i] == std::stoll(expected[i]));
    }

    CHECK(value.exDividendDayId == std::stoll(expected[10]));
    CHECK(value.haltStartTimeMs == std::stoll(expected[11]));
    CHECK(value.haltEndTimeMs == std::stoll(expected[12]));
    CHECK(value.flags == std::stoll(expected[13]));
    const auto text = [&](std::size_t index) -> std::optional<std::u16string> {
        return expected[index] == "null" ? std::nullopt : std::optional(fixture::units(expected[index]));
    };
    CHECK(value.description == text(14));
    CHECK(value.statusReason == text(15));
    CHECK(value.symbol == u"AAPL");
    const std::array business{value.getBeta(),
                              value.getEarningsPerShare(),
                              value.getDividendFrequency(),
                              value.getExDividendAmount(),
                              value.getHigh52WeekPrice(),
                              value.getLow52WeekPrice(),
                              value.getShares(),
                              value.getFreeFloat(),
                              value.getHighLimitPrice(),
                              value.getLowLimitPrice()};

    for (std::size_t i = 0; i < business.size(); ++i) {
        checkBusinessDouble(business[i], expected[16 + i]);
    }
}
} // namespace

TEST_CASE("production numeric, physical-slot, string and missing accessors match owned and borrowed projections") {
    std::size_t compared = 0;

    for (const auto &[name, scenario] : corpus()) {
        if (name.starts_with("bad-") || name.starts_with("required")) {
            continue;
        }

        CAPTURE(name);
        auto local = schema(scenario);
        auto quote = e::PreparedQuote::prepare(local, 0, MAPPING_LIMITS);
        auto profile = e::PreparedProfile::prepare(local, 1, MAPPING_LIMITS);
        REQUIRE(quote);
        REQUIRE(profile);

        for (const auto &[identity, sample] : scenario.samples) {
            CAPTURE(identity.first);
            CAPTURE(identity.second);
            std::vector<c::FieldValueView> views;

            for (const auto &field : sample.values) {
                views.push_back({field.type, field.raw,
                                 field.units ? std::optional<std::u16string_view>(*field.units) : std::nullopt,
                                 field.bytes ? std::optional<std::span<const std::byte>>(*field.bytes) : std::nullopt});
            }

            if (identity.first == 0) {
                auto owned = quote->map(sample.symbol, {local.recordContext(0), sample.values}, 128);
                auto borrowed = quote->mapView(sample.symbol, {local.recordContext(0), views}, 128);
                REQUIRE(owned);
                REQUIRE(borrowed);
                checkQuote(*owned, sample.expected);
                checkQuote(*borrowed, sample.expected);
            } else {
                auto owned = profile->map(sample.symbol, {local.recordContext(1), sample.values}, {128, 128, 256});
                auto borrowed = profile->mapView(sample.symbol, {local.recordContext(1), views}, {128, 128, 256});
                REQUIRE(owned);
                REQUIRE(borrowed);
                checkProfile(*owned, sample.expected);
                checkProfile(*borrowed, sample.expected);
            }

            ++compared;
        }
    }

    CHECK(compared > 3000);
}

TEST_CASE("present unsupported representation and wrong object fields never become missing or coerced values") {
    for (const auto &[name, scenario] : corpus()) {
        if (!name.starts_with("bad-") && !name.starts_with("required")) {
            continue;
        }

        CAPTURE(name);
        auto local = schema(scenario);
        auto quote = e::PreparedQuote::prepare(local, 0, MAPPING_LIMITS);
        auto profile = e::PreparedProfile::prepare(local, 1, MAPPING_LIMITS);

        if (name.starts_with("required")) {
            REQUIRE(!quote);
            REQUIRE(!profile);
            CHECK(quote.error() == e::MappingError::MISSING_REQUIRED);
            CHECK(profile.error() == e::MappingError::MISSING_REQUIRED);
        } else if (name.starts_with("bad-numeric")) {
            REQUIRE(!quote);
            REQUIRE(!profile);
            CHECK(quote.error() == e::MappingError::INCOMPATIBLE_TYPE);
            CHECK(profile.error() == e::MappingError::INCOMPATIBLE_TYPE);
        } else {
            REQUIRE(quote);
            REQUIRE(!profile);
            CHECK(profile.error() == e::MappingError::INCOMPATIBLE_TYPE);
        }
    }

    auto original = corpus().at("numeric8");

    for (const auto type : {c::SerialFieldType::BYTE, c::SerialFieldType::SHORT, c::SerialFieldType::VOID_FIELD}) {
        auto declarations = original;

        for (auto &record : declarations.records) {
            for (const auto &lookup : record.lookups) {
                if (lookup.name == u"Bid.Price" || lookup.name == u"Beta") {
                    record.fields[lookup.fieldId].serialTypeId = std::to_underlying(type);
                }
            }
        }

        auto local = schema(declarations);
        CHECK(e::PreparedQuote::prepare(local, 0, MAPPING_LIMITS).error() == e::MappingError::INCOMPATIBLE_TYPE);
        CHECK(e::PreparedProfile::prepare(local, 1, MAPPING_LIMITS).error() == e::MappingError::INCOMPATIBLE_TYPE);
    }
}

TEST_CASE("expanded alternatives have exact finite boundaries and default registry preparation succeeds") {
    auto local = schema(corpus().at("numeric104"));
    auto limits = MAPPING_LIMITS;
    limits.typeAlternatives = 72;
    REQUIRE(e::PreparedQuote::prepare(local, 0, limits));
    --limits.typeAlternatives;
    CHECK(e::PreparedQuote::prepare(local, 0, limits).error() == e::MappingError::RESOURCE_LIMIT);
    limits.typeAlternatives = 108;
    REQUIRE(e::PreparedProfile::prepare(local, 1, limits));
    --limits.typeAlternatives;
    CHECK(e::PreparedProfile::prepare(local, 1, limits).error() == e::MappingError::RESOURCE_LIMIT);
    const std::array registrations{a::EventRegistration{a::EventType::QUOTE, 0},
                                   a::EventRegistration{a::EventType::PROFILE, 1}};
    a::EventRegistryLimits registryLimits;
    a::EventRegistryWork work{100000000, 100000000};
    auto registry = a::EventRegistry::create(local, registrations, registryLimits, work);
    REQUIRE(registry);
}

TEST_CASE("STRING conversion keeps exact finite ownership, provenance and input state on refusal") {
    auto local = schema(corpus().at("text137"));
    auto mapping = e::PreparedProfile::prepare(local, 1, MAPPING_LIMITS);
    REQUIRE(mapping);
    auto values = corpus().at("text137").samples.at({1, 2}).values;
    const auto original = values;
    const auto context = local.recordContext(1);
    auto event = mapping->map(u"AAPL", {context, values}, {4, 5, 14});
    REQUIRE(event);

    for (const auto limits :
         {e::EventValueLimits{3, 5, 14}, e::EventValueLimits{4, 4, 14}, e::EventValueLimits{4, 5, 13}}) {
        auto refused = mapping->map(u"AAPL", {context, values}, limits);
        REQUIRE(!refused);
        CHECK(refused.error() == e::MappingError::RESOURCE_LIMIT);
        CHECK(values == original);
    }

    auto equivalent = schema(corpus().at("text137"));
    auto wrong = mapping->map(u"AAPL", {equivalent.recordContext(1), values}, {0, 0, 0});
    REQUIRE(!wrong);
    CHECK(wrong.error() == e::MappingError::WRONG_CONTEXT);
    const auto text = event->description;

    for (auto &field : values) {
        if (field.units) {
            field.units->assign(u"changed");
        }
    }

    CHECK(event->description == text);
    auto registration = a::EventRegistration{a::EventType::PROFILE, 1};
    registration.mappingLimits.typeAlternatives = std::numeric_limits<std::size_t>::max();
    a::EventRegistryWork work;
    auto overflow = a::EventRegistry::create(local, std::span(&registration, 1), {}, work);
    REQUIRE(!overflow);
    CHECK(std::get<a::EventRegistryError>(overflow.error().cause) == a::EventRegistryError::RESOURCE_LIMIT);
    CHECK(local.recordContext(1).matches(context));
}

TEST_CASE("business getters preserve direct LONG values and safely handle aggregates copies and raw edits") {
    static_assert(std::is_aggregate_v<e::Quote> && std::is_aggregate_v<e::Profile>);
    static_assert(std::is_trivially_copyable_v<e::detail::BusinessNumericSnapshot>);
    auto local = schema(corpus().at("numeric104"));
    auto quoteMapping = e::PreparedQuote::prepare(local, 0, MAPPING_LIMITS);
    auto profileMapping = e::PreparedProfile::prepare(local, 1, MAPPING_LIMITS);
    REQUIRE(quoteMapping);
    REQUIRE(profileMapping);
    const auto &qSample = corpus().at("numeric104").samples.at({0, 30});
    const auto &pSample = corpus().at("numeric104").samples.at({1, 30});
    auto quote = quoteMapping->map(u"AAPL", {local.recordContext(0), qSample.values}, 128);
    auto profile = profileMapping->map(u"AAPL", {local.recordContext(1), pSample.values}, {128, 128, 256});
    REQUIRE(quote);
    REQUIRE(profile);
    CHECK(std::bit_cast<std::uint64_t>(quote->getBidPrice()) == 4854880398305394689ULL);
    CHECK(std::bit_cast<std::uint64_t>(profile->getBeta()) == 4854880398305394689ULL);
    CHECK(std::bit_cast<std::uint64_t>(quote->bidPrice.toDouble()) == 4854880398305394688ULL);
    CHECK(std::bit_cast<std::uint64_t>(profile->beta.toDouble()) == 4854880398305394688ULL);
    auto copiedQuote = *quote;
    auto copiedProfile = *profile;
    quote->bidPrice.raw = 128;
    profile->beta.raw = 128;
    CHECK(quote->getBidPrice() == 0);
    CHECK(profile->getBeta() == 0);
    CHECK(quote->getAskPrice() == copiedQuote.getAskPrice());
    CHECK(profile->getShares() == copiedProfile.getShares());
    checkQuote(copiedQuote, qSample.expected);
    checkProfile(copiedProfile, pSample.expected);
    auto movedQuote = std::move(copiedQuote);
    auto movedProfile = std::move(copiedProfile);
    checkQuote(movedQuote, qSample.expected);
    checkProfile(movedProfile, pSample.expected);
    movedQuote.businessNumericSnapshot.reset();
    movedProfile.businessNumericSnapshot.reset();
    CHECK(movedQuote.getBidPrice() == movedQuote.bidPrice.toDouble());
    CHECK(movedProfile.getBeta() == movedProfile.beta.toDouble());
    e::Quote manualQuote{};
    e::Profile manualProfile{};
    e::Quote plainQuote;
    CHECK(std::isnan(plainQuote.getBidPrice()));
    CHECK(std::isnan(plainQuote.getBidSizeAsDouble()));
    CHECK(std::isnan(plainQuote.getAskPrice()));
    CHECK(std::isnan(plainQuote.getAskSizeAsDouble()));
    CHECK_FALSE(manualQuote.businessNumericSnapshot);
    CHECK_FALSE(manualProfile.businessNumericSnapshot);
    CHECK(std::isnan(manualQuote.getBidPrice()));
    CHECK(std::isnan(manualProfile.getBeta()));
    manualQuote.bidPrice.raw = 384;
    manualProfile.beta.raw = 640;
    CHECK(manualQuote.getBidPrice() == 1);
    CHECK(manualProfile.getBeta() == 2);
    const std::array legacyFields{
        c::FieldValue{c::SerialFieldType::TIME_SECONDS, 0},   c::FieldValue{c::SerialFieldType::UTF_CHAR, 0},
        c::FieldValue{c::SerialFieldType::WIDE_DECIMAL, 384}, c::FieldValue{c::SerialFieldType::WIDE_DECIMAL, 640},
        c::FieldValue{c::SerialFieldType::TIME_SECONDS, 0},   c::FieldValue{c::SerialFieldType::UTF_CHAR, 0},
        c::FieldValue{c::SerialFieldType::WIDE_DECIMAL, 896}, c::FieldValue{c::SerialFieldType::WIDE_DECIMAL, 1152}};
    auto legacy = e::mapQuote(u"AAPL", legacyFields, 4);
    REQUIRE(legacy);
    CHECK_FALSE(legacy->businessNumericSnapshot);
    CHECK(legacy->getBidPrice() == 1);
    CHECK(legacy->getBidSizeAsDouble() == 2);
    CHECK(legacy->getAskPrice() == 3);
    CHECK(legacy->getAskSizeAsDouble() == 4);
}
