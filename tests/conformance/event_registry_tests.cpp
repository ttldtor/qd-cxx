// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: immutable registry construction, no bootstrap, native state or network.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/event_registry.hpp"

#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"

#include "event_registry_defaults.hpp"
#include "receiving_profile.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace api = root::api;
namespace core = root::core;
namespace events = root::events;
constexpr core::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384}; ///< Complete finite test declarations.

/** Construct generic, Profile and two same-kind Quote routes with reordered fields and explicit aliases.
 * @param missingPrice Remove both measured business aliases while retaining an unrelated complete lookup.
 * @param wrongType Give the required price a supported schema type rejected by the Quote adapter.
 * @return Independent immutable caller schema with dense IDs0..3.
 */
core::LocalSchema customSchema(bool missingPrice = false, bool wrongType = false) {
    auto quote = events::defaultQuoteSchema(SCHEMA_LIMITS);
    auto profile = events::defaultProfileSchema(SCHEMA_LIMITS);
    REQUIRE(quote);
    REQUIRE(profile);

    auto configured = quote->records()[0];
    const auto fieldCount = configured.fields.size();
    std::ranges::reverse(configured.fields);

    for (auto &lookup : configured.lookups) {
        lookup.fieldId = fieldCount - 1 - lookup.fieldId;
    }

    const auto bidPrice = std::ranges::find_if(configured.lookups, [](const auto &lookup) {
                              return lookup.name == u"BidPrice";
                          })->fieldId;
    configured.lookups.insert(configured.lookups.begin(), {u"WirePrice", bidPrice});
    configured.fields.push_back({std::to_underlying(core::SerialFieldType::COMPACT_INT)});
    configured.lookups.push_back({u"GenericExtra", configured.fields.size() - 1});
    configured.name = u"ConfiguredQuote";
    configured.id = 2;

    if (missingPrice) {
        std::erase_if(configured.lookups, [](const auto &lookup) {
            return lookup.name == u"BidPrice" || lookup.name == u"Bid.Price";
        });
    }

    if (wrongType) {
        configured.fields[bidPrice].serialTypeId = std::to_underlying(core::SerialFieldType::UTF_CHAR_ARRAY);
    }

    auto second = configured;
    second.id = 3;
    second.name = u"OtherQuote";

    for (auto &field : second.fields) {
        if (field.serialTypeId == std::to_underlying(core::SerialFieldType::WIDE_DECIMAL)) {
            field.serialTypeId = std::to_underlying(core::SerialFieldType::COMPACT_INT);
        }
    }

    auto configuredProfile = profile->records()[0];
    configuredProfile.id = 1;
    configuredProfile.name = u"ConfiguredProfile";

    const std::array records{core::LocalRecord{0, u"Generic", false, {{8}}, {{u"Value", 0}}},
                             std::move(configuredProfile), std::move(configured), std::move(second)};
    auto result = core::LocalSchema::create(records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Register one Profile and both distinct same-kind Quote routes.
 * @param schema Validated caller input.
 * @return Owning immutable registry preserving the caller's exact origin.
 */
api::EventRegistry registry(const core::LocalSchema &schema) {
    const std::array registrations{api::EventRegistration{api::EventType::PROFILE, 1},
                                   api::EventRegistration{api::EventType::QUOTE, 2},
                                   api::EventRegistration{api::EventType::QUOTE, 3}};
    api::EventRegistryWork work;
    auto result = api::EventRegistry::create(schema, registrations, {}, work);
    REQUIRE(result);

    return std::move(*result);
}

/** Create complete canonical values, including the generic extra field.
 * @param record Complete registered record.
 * @return Owning values in the configured local order.
 */
std::vector<core::FieldValue> values(const core::LocalRecord &record) {
    std::vector<core::FieldValue> result;

    for (const auto &field : record.fields) {
        const auto type = static_cast<core::SerialFieldType>(field.serialTypeId);
        result.push_back({type, type == core::SerialFieldType::UTF_CHAR ? u'X' : 3});
    }

    const std::array assignments{std::pair{u"BidTime", 3},
                                 std::pair{u"AskTime", 7},
                                 std::pair{u"BidExchangeCode", static_cast<int>(u'X')},
                                 std::pair{u"AskExchangeCode", static_cast<int>(u'Y')},
                                 std::pair{u"BidPrice", 5},
                                 std::pair{u"BidSize", 11},
                                 std::pair{u"AskPrice", 17},
                                 std::pair{u"AskSize", 23}};

    for (const auto &[name, raw] : assignments) {
        const auto lookup = std::ranges::find_if(record.lookups, [name](const auto &entry) {
            return entry.name == name;
        });

        if (lookup != record.lookups.end()) {
            auto &value = result[lookup->fieldId];
            // Wide integer encoding uses an eight-bit rank; rank128 has decimal exponent zero.
            value.raw =
                value.type == core::SerialFieldType::WIDE_DECIMAL ? (static_cast<std::int64_t>(raw) << 8) + 128 : raw;
        }
    }

    return result;
}
} // namespace

TEST_CASE("built-in registry preserves existing ordinals and appends text events") {
    api::EventRegistryWork work;
    auto result = api::EventRegistry::builtIn({}, work);
    REQUIRE(result);
    CHECK(result->valid());
    REQUIRE(result->registrations().size() == 9);
    CHECK(result->registrations()[0].type == api::EventType::QUOTE);
    CHECK(result->registrations()[0].localRecordId == 0);
    CHECK(result->registrations()[1].type == api::EventType::PROFILE);
    CHECK(result->registrations()[1].localRecordId == 1);
    CHECK(result->schema().records()[0].name == u"Quote");
    CHECK(result->schema().records()[0].fields.size() == 8);
    CHECK(result->schema().records()[1].name == u"Profile");
    CHECK(result->schema().records()[1].fields.size() == 16);
    CHECK(result->registrations()[2].type == api::EventType::TRADE);
    CHECK(result->schema().records()[2].name == u"Trade");
    CHECK(result->schema().records()[2].fields.size() == 13);
    CHECK(result->registrations()[3].type == api::EventType::TRADE_ETH);
    CHECK(result->schema().records()[3].name == u"TradeETH");
    CHECK(result->schema().records()[3].fields.size() == 12);
    CHECK(result->registrations()[4].type == api::EventType::SUMMARY);
    CHECK(result->schema().records()[4].name == u"Summary");
    CHECK(result->schema().records()[4].fields.size() == 10);
    CHECK(result->registrations()[5].type == api::EventType::TEXT_MESSAGE);
    CHECK(result->schema().records()[5].name == u"TextMessage");
    CHECK(result->schema().records()[5].fields.size() == 3);
    CHECK(result->registrations()[6].type == api::EventType::TEXT_CONFIGURATION);
    CHECK(result->schema().records()[6].name == u"TextConfiguration");
    CHECK(result->schema().records()[6].fields.size() == 4);
    CHECK_FALSE(result->versionField(5));
    REQUIRE(result->versionField(6));
    CHECK(*result->versionField(6) == 2);
    CHECK(result->registrations()[7].type == api::EventType::MESSAGE);
    CHECK(result->registrations()[7].localRecordId == 7);
    CHECK(result->schema().records()[7].name == u"Message");
    REQUIRE(result->schema().records()[7].fields.size() == 1);
    CHECK(result->schema().records()[7].fields[0].serialTypeId == 249);
    CHECK_FALSE(result->versionField(7));
    CHECK(result->registrations()[8].type == api::EventType::CONFIGURATION);
    CHECK(result->registrations()[8].localRecordId == 8);
    CHECK(result->schema().records()[8].name == u"Configuration");
    REQUIRE(result->schema().records()[8].fields.size() == 2);
    CHECK(result->schema().records()[8].fields[0].serialTypeId == 8);
    CHECK(result->schema().records()[8].fields[1].serialTypeId == 249);
    CHECK(result->versionField(8) == 0);
    CHECK_FALSE(result->versionField(99));
    CHECK(result->storageBytes() <= api::EventRegistryLimits{}.storageBytes);
    CHECK(work.visits < api::EventRegistryWork{}.visits);
    CHECK(work.preparation < api::EventRegistryWork{}.preparation);
}

TEST_CASE("TextConfiguration registry retains the prepared optional version selection") {
    core::LocalRecord record{0, u"Custom", false, {{10}, {8}, {8}}, {{u"Text", 0}, {u"Extra", 1}, {u"Version", 2}}};
    auto schema = core::LocalSchema::create(std::span(&record, 1), SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const api::EventRegistration route{api::EventType::TEXT_CONFIGURATION, 0};
    api::EventRegistryWork work;
    auto registry = api::EventRegistry::create(*schema, std::span(&route, 1), {}, work);
    REQUIRE(registry);
    REQUIRE(registry->versionField(0));
    CHECK(*registry->versionField(0) == 2);
    record.lookups.back().name = u"UnusedVersion";
    auto withoutVersion =
        core::LocalSchema::create(std::span(&record, 1), SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(withoutVersion);
    api::EventRegistryWork secondWork;
    auto second = api::EventRegistry::create(*withoutVersion, std::span(&route, 1), {}, secondWork);
    REQUIRE(second);
    CHECK_FALSE(second->versionField(0));
}

TEST_CASE("custom registry preserves aliases extra fields generic records and multiple same-kind routes") {
    auto schema = customSchema();
    const auto origin = schema.identity();
    auto configured = registry(schema);
    auto copy = configured;
    CHECK(configured.schema().identity().matches(origin));
    CHECK(&copy.schema() == &configured.schema());
    REQUIRE(configured.registrations().size() == 3);
    CHECK(configured.registrations()[1].type == configured.registrations()[2].type);
    CHECK(configured.registrations()[1].localRecordId == 2);
    CHECK(configured.registrations()[2].localRecordId == 3);
    REQUIRE(configured.schema().records().size() == 4);
    CHECK(configured.schema().records()[0].name == u"Generic");
    CHECK(configured.schema().records()[2].fields.size() == 9);
    CHECK(configured.schema().records()[2].lookups[0].name == u"WirePrice");

    const auto mapping =
        events::PreparedQuote::prepare(configured.schema(), 2, configured.registrations()[1].mappingLimits);
    REQUIRE(mapping);

    auto fields = values(configured.schema().records()[2]);
    auto projected = mapping->map(u"OWNED", {schema.recordContext(2), fields}, 128);
    REQUIRE(projected);
    CHECK(projected->symbol == u"OWNED");
    CHECK(projected->bidTimeMs == 3000);
    CHECK(projected->askTimeMs == 7000);
    CHECK(projected->bidExchangeCode == u'X');
    CHECK(projected->askExchangeCode == u'Y');
    CHECK(projected->bidPrice.toDouble() == 5.0);
    CHECK(projected->askPrice.toDouble() == 17.0);

    const auto integerMapping =
        events::PreparedQuote::prepare(configured.schema(), 3, configured.registrations()[2].mappingLimits);
    REQUIRE(integerMapping);

    auto integerFields = values(configured.schema().records()[3]);
    auto integerProjection = integerMapping->map(u"OWNED", {configured.schema().recordContext(3), integerFields}, 128);
    REQUIRE(integerProjection);
    CHECK(integerProjection->bidPrice.toDouble() == 5.0);

    auto independent = customSchema();
    auto wrong = mapping->map(u"OWNED", {independent.recordContext(2), fields}, 128);
    REQUIRE_FALSE(wrong);
    CHECK(wrong.error() == events::MappingError::WRONG_CONTEXT);
}

TEST_CASE("registry owns its schema and routes after caller inputs retire") {
    auto configured = [] {
        auto source = customSchema();

        return registry(source);
    }();
    const auto context = configured.schema().recordContext(3);
    auto copy = configured;
    auto moved = std::move(configured);
    CHECK_FALSE(configured.valid());
    CHECK(configured.storageBytes() == 0);
    CHECK(copy.valid());
    CHECK(context.matches(moved.schema().recordContext(3)));
    CHECK(copy.schema().sharesIdentityWith(moved.schema()));
}

TEST_CASE("projected event remains owning after registry schema and generic values retire") {
    auto event = [] {
        auto source = customSchema();
        auto configured = registry(source);
        auto mapping =
            events::PreparedQuote::prepare(configured.schema(), 2, configured.registrations()[1].mappingLimits);
        REQUIRE(mapping);

        auto fields = values(configured.schema().records()[2]);
        auto projected = mapping->map(u"RETAINED", {configured.schema().recordContext(2), fields}, 128);
        REQUIRE(projected);

        return std::move(*projected);
    }();
    CHECK(event.symbol == u"RETAINED");
    CHECK(event.bidTimeMs == 3000);
    CHECK(event.askPrice.toDouble() == 17.0);
}

TEST_CASE("registry rejects unknown enum invalid record and duplicate exact route atomically") {
    auto schema = customSchema();

    for (const auto registration : std::array{api::EventRegistration{static_cast<api::EventType>(999), 2},
                                              api::EventRegistration{api::EventType::QUOTE, -1},
                                              api::EventRegistration{api::EventType::QUOTE, 4}}) {
        api::EventRegistryWork work;
        auto result = api::EventRegistry::create(schema, std::span(&registration, 1), {}, work);
        REQUIRE_FALSE(result);
        CHECK(std::get<api::EventRegistryError>(result.error().cause) == api::EventRegistryError::INVALID_INPUT);
        CHECK(result.error().registrationIndex == 0);
    }

    std::array duplicate{api::EventRegistration{api::EventType::QUOTE, 2},
                         api::EventRegistration{api::EventType::PROFILE, 2}};
    api::EventRegistryWork work;
    auto result = api::EventRegistry::create(schema, duplicate, {}, work);
    REQUIRE_FALSE(result);
    CHECK(std::get<api::EventRegistryError>(result.error().cause) == api::EventRegistryError::INVALID_INPUT);
    CHECK(result.error().registrationIndex == 1);

    duplicate[0].eventTimePolicy = static_cast<api::EventTimePolicy>(999);
    work = {};
    result = api::EventRegistry::create(schema, duplicate, {}, work);
    REQUIRE_FALSE(result);
    CHECK(std::get<api::EventRegistryError>(result.error().cause) == api::EventRegistryError::INVALID_INPUT);
    CHECK(result.error().registrationIndex == 0);
}

TEST_CASE("registry preserves mapper missing-field exact-type and finite-preparation failures") {
    const api::EventRegistration registration{api::EventType::QUOTE, 2};

    for (const auto missing : {false, true}) {
        auto schema = customSchema(missing, !missing);
        api::EventRegistryWork work;
        const auto result = api::EventRegistry::create(schema, std::span(&registration, 1), {}, work);
        REQUIRE_FALSE(result);
        CHECK(std::get<events::MappingError>(result.error().cause) ==
              (missing ? events::MappingError::MISSING_REQUIRED : events::MappingError::INCOMPATIBLE_TYPE));
        CHECK(result.error().registrationIndex == 0);
    }

    auto schema = customSchema();
    auto bounded = registration;
    bounded.mappingLimits.fields = 0;
    api::EventRegistryWork work;
    const auto result = api::EventRegistry::create(schema, std::span(&bounded, 1), {}, work);
    REQUIRE_FALSE(result);
    CHECK(std::get<events::MappingError>(result.error().cause) == events::MappingError::RESOURCE_LIMIT);
}

TEST_CASE("registry separately enforces route schema storage and attempted work bounds") {
    auto schema = customSchema();
    const api::EventRegistration registration{api::EventType::QUOTE, 2};

    for (const auto kind : {0, 1, 2, 3, 4}) {
        api::EventRegistryLimits limits;
        api::EventRegistryWork work;

        if (kind == 0) {
            limits.registrations = 0;
        } else if (kind == 1) {
            limits.schema.fields = 0;
        } else if (kind == 2) {
            limits.storageBytes = schema.storageBytes();
        } else if (kind == 3) {
            work.visits = 0;
        } else {
            work.preparation = 0;
        }

        const auto result = api::EventRegistry::create(schema, std::span(&registration, 1), limits, work);
        REQUIRE_FALSE(result);

        if (kind == 1) {
            CHECK(std::get<core::SchemaError>(result.error().cause) == core::SchemaError::RESOURCE_LIMIT);
        } else {
            CHECK(std::get<api::EventRegistryError>(result.error().cause) ==
                  (kind >= 3 ? api::EventRegistryError::WORK_LIMIT : api::EventRegistryError::RESOURCE_LIMIT));
        }
    }

    auto moved = std::move(schema);
    api::EventRegistryWork work;
    const auto invalid = api::EventRegistry::create(schema, {}, {}, work);
    REQUIRE_FALSE(invalid);
    CHECK(std::get<api::EventRegistryError>(invalid.error().cause) == api::EventRegistryError::INVALID_INPUT);
    CHECK(moved.identity().valid());
}

TEST_CASE("generic-only immutable registry does not invent typed routes") {
    auto schema = customSchema();
    api::EventRegistryWork work;
    const auto result = api::EventRegistry::create(schema, {}, {}, work);
    REQUIRE(result);
    CHECK(result->registrations().empty());
    CHECK(result->schema().records().size() == schema.records().size());
}

TEST_CASE("receiving preparation selects all explicit same-kind routes and derives aggregate finite reservations") {
    auto source = customSchema();
    auto configured = registry(source);
    api::RuntimeOptions runtime;
    runtime.maximumSubscriptionsPerEndpoint = 2;
    runtime.maximumSymbolsPerSubscription = 5;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    api::EndpointOptions options;
    options.maximumAttempts = 1;
    options.eventRegistry = configured;
    auto profile = api::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
    CHECK(profile.schema.sharesIdentityWith(source));
    REQUIRE(profile.bindings.size() == 3);
    CHECK(profile.bindings[0].localId == 2);
    CHECK(profile.bindings[1].localId == 3);
    CHECK(profile.bindings[2].localId == 1);
    const auto &quotes = api::detail::receivingEventPlan(profile, api::detail::EventSelection::QUOTE);
    const auto &profiles = api::detail::receivingEventPlan(profile, api::detail::EventSelection::PROFILE);
    CHECK(quotes.bindingCount == 2);
    CHECK(quotes.limits.bindings == 2);
    CHECK(quotes.limits.shapeFields == 18);
    CHECK(profiles.bindingIndices[0] == 2);
    CHECK(profiles.bindingCount == 1);
    CHECK(profile.eventPlans.size() == 4);
    CHECK(api::detail::receivingEventPlan(profile, api::detail::EventSelection::ANALYTIC_ORDER).bindingCount == 0);
    CHECK(profile.clientLimits.commandEntries == 15);
    CHECK(profile.endpoint.settings().budgets.desired.entries == 30);
    CHECK(profile.endpoint.settings().budgets.desired.totalSymbolUnits == 30 * runtime.maximumSymbolUnits);

    auto queueWork = profile.queueWork;
    auto queue = api::detail::createReceivingQueue(profile, queueWork);
    REQUIRE(queue);

    /** Inert complete-batch consumer used only for native-free receiver preparation. */
    class Consumer final : public root::client::SubscriptionConsumer {
        public:
        /** Accept any complete callback without retaining its borrow.
         * @param batch Callback-only complete values, unused by this preparation check.
         * @return Continue normal independent delivery.
         */
        root::mux::BatchAction onBatch(const root::client::SubscriptionBatch &batch) override {
            static_cast<void>(batch);

            return root::mux::BatchAction::CONTINUE;
        }
    };

    const auto consumer = std::make_shared<Consumer>();
    auto plan = api::detail::receivingEventPlan(profile, api::detail::EventSelection::QUOTE);
    auto handle = root::client::openSubscriptionReceiver(
        *queue, profile.schema, profile.family, core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1,
        std::span(profile.bindings).subspan(plan.bindingIndices[0], plan.bindingCount), consumer, plan.limits,
        profile.clientLimits, plan.work, queueWork);
    REQUIRE(handle);
}

TEST_CASE("receiving preparation refuses valid registries beyond its qualified schema ceiling") {
    auto limits = SCHEMA_LIMITS;
    limits.fields = limits.lookups = 285;
    core::LocalRecord record{0, u"Generic", false, {}, {}};

    for (std::size_t index = 0; index < limits.fields; ++index) {
        auto name = u"Field" +
                    std::u16string{static_cast<char16_t>(u'A' + index / 26), static_cast<char16_t>(u'A' + index % 26)};
        record.fields.push_back({8});
        record.lookups.push_back({std::move(name), index});
    }

    auto source = core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(source);

    api::EventRegistryLimits registryLimits;
    registryLimits.schema = limits;
    api::EventRegistryWork work;
    auto configured = api::EventRegistry::create(*source, {}, registryLimits, work);
    REQUIRE(configured);

    api::RuntimeOptions runtime;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    api::EndpointOptions options;
    options.eventRegistry = *configured;
    try {
        static_cast<void>(api::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345));
        FAIL("A valid larger registry must reject before native activation");
    } catch (const api::Error &error) {
        CHECK(error.diagnostic().code == api::ErrorCode::UNSUPPORTED_PROFILE);
        CHECK(error.diagnostic().stage == "profile-schema");
    }
}

TEST_CASE("maximum same-kind route list receives complete duplicate-inspection preparation credits") {
    std::vector<core::LocalRecord> records;
    std::vector<api::EventRegistration> registrations;

    for (std::int32_t index = 0; index < 16; ++index) {
        auto name = u"Quote" + std::u16string(1, static_cast<char16_t>(u'A' + index));
        records.push_back({index,
                           std::move(name),
                           false,
                           {{8}, {8}, {8}, {8}},
                           {{u"BidPrice", 0}, {u"BidSize", 1}, {u"AskPrice", 2}, {u"AskSize", 3}}});
        registrations.push_back({api::EventType::QUOTE, index});
    }

    auto source = core::LocalSchema::create(records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(source);

    api::EventRegistryWork work;
    auto configured = api::EventRegistry::create(*source, registrations, {}, work);
    REQUIRE(configured);

    api::RuntimeOptions runtime;
    runtime.maximumSubscriptionsPerEndpoint = runtime.maximumSymbolsPerSubscription = 1;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    api::EndpointOptions options;
    options.eventRegistry = *configured;
    auto profile = api::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
    const auto &plan = api::detail::receivingEventPlan(profile, api::detail::EventSelection::QUOTE);
    REQUIRE(plan.bindingCount == 16);
    // Existing receiver visits once, twice per route and once per preceding route during duplicate checks.
    CHECK(plan.work.visits >= 1 + 2 * plan.bindingCount + plan.bindingCount * (plan.bindingCount - 1) / 2);
    CHECK(plan.limits.shapeFields == 64);
    CHECK(profile.clientLimits.commandEntries == 16);
    CHECK(profile.endpoint.settings().budgets.desired.entries == 16);
}

TEST_CASE("overflowing declared mapper allowances refuse without wrapping or changing schema provenance") {
    auto source = customSchema();
    auto route = api::EventRegistration{api::EventType::QUOTE, 2};
    route.mappingLimits.fields = std::numeric_limits<std::size_t>::max();
    api::EventRegistryWork work;
    const auto origin = source.identity();
    auto result = api::EventRegistry::create(source, std::span(&route, 1), {}, work);
    REQUIRE_FALSE(result);
    CHECK(std::get<api::EventRegistryError>(result.error().cause) == api::EventRegistryError::RESOURCE_LIMIT);
    CHECK(source.identity().matches(origin));
    CHECK(work.visits < api::EventRegistryWork{}.visits);
}

TEST_CASE("registry byte ceiling includes actual shared state and control allocation request") {
    auto source = customSchema();
    auto configured = registry(source);
    api::EventRegistryLimits limits;
    limits.storageBytes = configured.storageBytes();
    api::EventRegistryWork work;
    auto exact = api::EventRegistry::create(source, configured.registrations(), limits, work);
    REQUIRE(exact);
    CHECK(exact->storageBytes() == limits.storageBytes);

    --limits.storageBytes;
    work = {};
    const auto refused = api::EventRegistry::create(source, configured.registrations(), limits, work);
    REQUIRE_FALSE(refused);
    CHECK(std::get<api::EventRegistryError>(refused.error().cause) == api::EventRegistryError::RESOURCE_LIMIT);
    CHECK_FALSE(refused.error().registrationIndex);
    CHECK(work.preparation < api::EventRegistryWork{}.preparation);
}

TEST_CASE("SDK defaults retain all real regional records and explicit physical delegate families") {
    constexpr std::size_t TICKER_RECORDS = 39;
    constexpr std::size_t TICKER_ROUTES = 66;
    auto ticker = api::detail::defaultReceivingRegistry(api::DeliveryContract::TICKER);
    REQUIRE(ticker);
    REQUIRE(ticker->schema().records().size() == TICKER_RECORDS);
    REQUIRE(ticker->registrations().size() == TICKER_ROUTES);
    std::size_t fields = 0, lookups = 0;

    for (const auto &record : ticker->schema().records()) {
        fields += record.fields.size();
        lookups += record.lookups.size();
    }

    CHECK(fields == 257);
    CHECK(lookups == 639);

    for (char16_t exchange = u'A'; exchange <= u'Z'; ++exchange) {
        const auto name = u"Quote&" + std::u16string(1, exchange);
        CHECK(std::ranges::count_if(ticker->schema().records(), [&](const auto &record) {
                  return record.name == name;
              }) == 1);
    }

    constexpr std::array imbalanceNames{u"OrderImbalance", u"OrderImbalance#NTV", u"OrderImbalance#ARCA",
                                        u"OrderImbalance#NUAM"};

    for (std::size_t i = 0; i < imbalanceNames.size(); ++i) {
        const auto recordId = TICKER_RECORDS - imbalanceNames.size() + i;
        const auto &record = ticker->schema().records()[recordId];
        const auto &route = ticker->registrations()[TICKER_ROUTES - imbalanceNames.size() + i];
        CHECK(record.name == imbalanceNames[i]);
        CHECK_FALSE(record.hasTime);
        CHECK(route.type == api::EventType::ORDER_IMBALANCE);
        CHECK(route.localRecordId == static_cast<std::int32_t>(recordId));
    }

    auto history = api::detail::defaultReceivingRegistry(api::DeliveryContract::HISTORY);
    REQUIRE(history);
    REQUIRE(history->schema().records().size() == 6);
    REQUIRE(history->registrations().size() == 7);
    CHECK(history->schema().records()[2].name == u"SpreadOrder");
    CHECK(history->schema().records()[3].name == u"AnalyticOrder");
    CHECK(history->schema().records()[4].name == u"OtcMarketsOrder");
    CHECK(history->schema().records()[5].name == u"OtcMarketsOrder#pink");
    CHECK(history->registrations()[5].type == api::EventType::OTC_MARKETS_ORDER);
    CHECK(history->registrations()[6].type == api::EventType::OTC_MARKETS_ORDER);
    auto defaults = api::EventRegistryLimits{};
    api::EventRegistryWork work;
    auto unchanged = api::EventRegistry::builtIn(defaults, work);
    REQUIRE(unchanged);
    CHECK(unchanged->schema().records().size() == 9);
    CHECK(unchanged->registrations().size() == 9);

    api::RuntimeOptions runtime;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    const auto profile = api::detail::prepareReceivingProfile(runtime, {}, "127.0.0.1", 12345);
    CHECK(profile.physicalRouteCount == TICKER_RECORDS);
    CHECK(profile.clientLimits.commandEntries == 16 * TICKER_RECORDS);
    CHECK(profile.interests.entries == 8 * 16 * TICKER_RECORDS);
    CHECK(profile.endpoint.settings().budgets.schema.records == 42);
    CHECK(profile.endpoint.settings().budgets.layout.records == 42);
    CHECK(profile.endpoint.settings().budgets.session.context.descriptions == 42);
    CHECK(profile.eventPlans.size() == 512);
    CHECK(profile.routing.retainedMetadataBytes >=
          profile.eventPlans.capacity() * sizeof(api::detail::ReceivingEventPlan));
    std::cout << "DERIVED_DEFAULT_STORAGE registry=" << ticker->storageBytes()
              << " profile=" << sizeof(api::detail::ReceivingProfile)
              << " plan=" << sizeof(api::detail::ReceivingEventPlan)
              << " retained=" << profile.routing.retainedMetadataBytes << '\n';
}

TEST_CASE("one physical record admits only one native and one compatible derived group") {
    auto source = events::defaultQuoteSchema(SCHEMA_LIMITS);
    REQUIRE(source);
    api::EventRegistration derived{api::EventType::ORDER, 0};
    derived.projection = events::EventProjection::QUOTE_ORDERS;
    api::EventRegistryWork work;
    auto accepted = api::EventRegistry::create(
        *source, std::array{api::EventRegistration{api::EventType::QUOTE, 0}, derived}, {}, work);
    REQUIRE(accepted);
    CHECK(accepted->registrations().size() == 2);
    auto refuse = [&](auto routes) {
        api::EventRegistryWork attempt;
        auto result = api::EventRegistry::create(*source, routes, {}, attempt);
        REQUIRE_FALSE(result);
        REQUIRE(std::holds_alternative<api::EventRegistryError>(result.error().cause));
        CHECK(std::get<api::EventRegistryError>(result.error().cause) == api::EventRegistryError::INVALID_INPUT);
    };
    refuse(
        std::array{api::EventRegistration{api::EventType::QUOTE, 0}, api::EventRegistration{api::EventType::ORDER, 0}});
    auto otherDerived = derived;
    otherDerived.projection = events::EventProjection::MARKET_MAKER_ORDERS;
    refuse(std::array{derived, otherDerived});
    refuse(std::array{api::EventRegistration{api::EventType::QUOTE, 0}, otherDerived});
}
