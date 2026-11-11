// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include "qd/api/feed_filter.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <stop_token>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;

constexpr q::core::SchemaLimits SCHEMA{16, 128, 256, 128, 16384};
constexpr std::int32_t SNAPSHOT_BEGIN = 4;
constexpr std::int32_t SNAPSHOT_END = 8;
constexpr std::int32_t REMOVE_EVENT = 2;

void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Construct the actual source schema shape, with a physical high VOID before the native low Index. */
q::core::LocalRecord physicalRecord(q::core::LocalRecord logical) {
    logical.hasTime = false;
    logical.indexedTimeLayout.reset();
    logical.fields.insert(logical.fields.begin(), q::core::LocalField{0});

    for (auto &lookup : logical.lookups) {
        ++lookup.fieldId;
    }

    logical.lookups.insert(logical.lookups.begin(), {u"Void", 0});

    return logical;
}

peer::Record data(const q::core::LocalRecord &physical, std::int32_t index) {
    peer::Record result{u"AAPL", {}, SNAPSHOT_BEGIN, {}, physical.id, {}};

    for (const auto &field : physical.fields) {
        result.fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
    }

    for (const auto &lookup : physical.lookups) {
        if (lookup.name == u"Index") {
            result.fields[lookup.fieldId].raw = index;
        } else if (lookup.name == u"SpreadSymbol") {
            result.fields[lookup.fieldId].units = u"AAPL-MSFT";
        }
    }

    return result;
}

void check() {
    const auto spreadSchema = q::events::defaultSpreadOrderSchema(SCHEMA);
    const auto analyticSchema = q::events::defaultAnalyticOrderSchema(SCHEMA);
    require(spreadSchema.has_value() && analyticSchema.has_value(), "variant schema creation");
    std::array records{spreadSchema->records().front(), analyticSchema->records().front()};
    records[0].name = u"SpreadOrder#ISE";
    records[1].id = 1;
    records[1].name = u"AnalyticOrder#NTV";
    const auto schema = q::core::LocalSchema::create(records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    const std::array physicalRecords{physicalRecord(records[0]), physicalRecord(records[1])};
    const auto physical =
        q::core::LocalSchema::create(physicalRecords, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(schema.has_value() && physical.has_value(), "native and physical variant schemas");
    const std::array routes{q::api::EventRegistration{q::api::EventType::SPREAD_ORDER, 0},
                            q::api::EventRegistration{q::api::EventType::ANALYTIC_ORDER, 1}};
    // An explicit registry must use the same native Index coordinate as the HISTORY collector.
    auto invalidRecords = records;
    const auto flags = std::ranges::find_if(invalidRecords[1].lookups, [](const auto &lookup) {
        return lookup.name == u"Flags";
    });
    require(flags != invalidRecords[1].lookups.end(), "variant Flags fixture");
    invalidRecords[1].indexedTimeLayout = q::core::IndexedTimeLowInt32{flags->fieldId};
    const auto invalid = q::core::LocalSchema::create(invalidRecords, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(invalid.has_value(), "generic wrong-compact-index record schema");
    {
        q::api::EventRegistryWork invalidWork;
        require(!q::api::EventRegistry::create(*invalid, routes, {}, invalidWork),
                "mismatched AnalyticOrder collector Index accepted");
    }

    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*schema, routes, {}, work);
    require(registry.has_value(), "variant registry creation");

    std::stop_source done;
    std::vector<q::events::SpreadOrder> spreadValues;
    std::vector<q::events::AnalyticOrder> analyticValues;
    std::vector<q::api::RegisteredEvent> mixedValues;
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::HISTORY;
    options.eventRegistry = *registry;
    options.maximumAttempts = 1;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto spread = feed->createSubscription<q::events::SpreadOrder>();
    auto analytic = feed->createSubscription<q::events::AnalyticOrder>();
    auto mixed = feed->createSubscription({q::api::EventType::SPREAD_ORDER, q::api::EventType::ANALYTIC_ORDER});
    const auto ise = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::ISE.indexedSource());
    const auto ntv = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::NTV.indexedSource());
    spread->setSelectors(std::array{ise});
    analytic->setSelectors(std::array{ntv});
    mixed->setSelectors(std::array{ntv, ise});

    // All listeners run on this runtime's single default worker. The creator reads copies after retirement.
    const auto onProgress = [&] {
        if (spreadValues.size() >= 2 && analyticValues.size() >= 2 && mixedValues.size() >= 4) {
            done.request_stop();
        }
    };
    spread->addEventListener([&](auto batch) {
        spreadValues.insert(spreadValues.end(), batch.begin(), batch.end());
        onProgress();
    });
    analytic->addEventListener([&](auto batch) {
        analyticValues.insert(analyticValues.end(), batch.begin(), batch.end());
        onProgress();
    });
    mixed->addEventListener([&](auto batch) {
        mixedValues.insert(mixedValues.end(), batch.begin(), batch.end());
        onProgress();
    });

    const std::array payload{
        data(physicalRecords[0], 17), peer::Record{u"AAPL", {}, REMOVE_EVENT | SNAPSHOT_END, 0, 0, {}},
        data(physicalRecords[1], 23), peer::Record{u"AAPL", {}, REMOVE_EVENT | SNAPSHOT_END, 0, 1, {}}};
    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::HISTORY;
    script.historyBound = 0;
    script.describeAllRecords = true;
    script.canonicalSymbols = true;
    script.subscriptionRecordIds = {0, 1};
    script.removeSequence = std::vector{peer::ExpectedSubscription{{u"AAPL", u"AAPL"}, {0, 1}, true}};
    script.afterFinalRemove = std::vector<peer::Record>{};
    const std::array<std::u16string, 2> expected{u"AAPL", u"AAPL"};
    peer::Peer source(*physical, payload, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    static_cast<void>(
        endpoint->awaitState(q::api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 10s, done.get_token()));
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();

    if (!done.stop_requested() || !closed.drained || source.failed() || source.adds() != 1) {
        const auto diagnostic = endpoint->getDiagnostic();
        const auto mixedDiagnostic = mixed->getDiagnostic();
        std::fprintf(stderr,
                     "HISTORY variant: done=%d drained=%d gap=%d expired=%d adds=%u peer=%d counts=%zu/%zu/%zu "
                     "endpoint=%d/%.*s/%u mixed=%d/%.*s\n",
                     done.stop_requested(), closed.drained, closed.hadGap, closed.deadlineExpired, source.adds(),
                     source.failed(), spreadValues.size(), analyticValues.size(), mixedValues.size(),
                     static_cast<int>(diagnostic.code), static_cast<int>(diagnostic.stage.size()),
                     diagnostic.stage.data(), diagnostic.detailCode, static_cast<int>(mixedDiagnostic.code),
                     static_cast<int>(mixedDiagnostic.stage.size()), mixedDiagnostic.stage.data());
    }

    require(done.stop_requested() && closed.drained && !source.failed() && source.adds() == 1,
            "variant HISTORY delivery or drain failed");
    require(spreadValues.size() == 2 && analyticValues.size() == 2 && mixedValues.size() == 4,
            "variant business delivery count");
    require(spreadValues.front().symbol == u"AAPL" && spreadValues.front().spreadSymbol == u"AAPL-MSFT" &&
                spreadValues.front().getSourceId() == q::events::OrderSource::ISE.id() &&
                spreadValues.front().eventFlags == SNAPSHOT_BEGIN &&
                spreadValues.back().eventFlags == (REMOVE_EVENT | SNAPSHOT_END),
            "SpreadOrder payload, source or snapshot flags");
    require(analyticValues.front().getSourceId() == q::events::OrderSource::NTV.id() &&
                analyticValues.front().eventFlags == SNAPSHOT_BEGIN &&
                analyticValues.back().eventFlags == (REMOVE_EVENT | SNAPSHOT_END),
            "AnalyticOrder payload, source or snapshot flags");
    std::size_t spreadCount = 0;
    std::size_t analyticCount = 0;

    for (const auto &value : mixedValues) {
        spreadCount += std::holds_alternative<q::events::SpreadOrder>(value);
        analyticCount += std::holds_alternative<q::events::AnalyticOrder>(value);
    }

    require(spreadCount == 2 && analyticCount == 2 && mixed->getStatistics().deliveredEvents.value == 4,
            "mixed listener variant or delivery statistics");

    for (const auto contract : std::array{q::api::DeliveryContract::HISTORY, q::api::DeliveryContract::TICKER,
                                          q::api::DeliveryContract::STREAM}) {
        const auto spreadFilter =
            q::api::FeedFilter::Builder(*registry, contract).withCategory(u"spread", u":SpreadOrder#ISE").build();
        const auto analyticFilter =
            q::api::FeedFilter::Builder(*registry, contract).withCategory(u"analytic", u":AnalyticOrder#NTV").build();
        const bool history = contract == q::api::DeliveryContract::HISTORY;
        require(spreadFilter.accept(q::api::EventType::SPREAD_ORDER, ise) == history &&
                    analyticFilter.accept(q::api::EventType::ANALYTIC_ORDER, ntv) == history &&
                    spreadFilter.acceptEvent(q::api::RegisteredEvent{spreadValues.front()}) == history &&
                    analyticFilter.acceptEvent(q::api::RegisteredEvent{analyticValues.front()}) == history,
                "variant filter coordinates must match the HISTORY contract");
    }
}
} // namespace

int main() {
    try {
        check();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());

        return 1;
    }
}
