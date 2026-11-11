// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "connect_peer.hpp"
#include "event_registry_defaults.hpp"
#include "qd/api/feed.hpp"
#include "qd/api/feed_filter.hpp"
#include "qd/events/event_flags.hpp"

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
constexpr auto LOCAL_PROFILE = q::core::LocalProfile::BINARY_FIELDS_V1;

/** @param ok Required observation.
 * @param message Diagnostic for an unmet observation.
 */
void require(bool ok, const char *message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

/** @param options Endpoint options for the expected delivery contract.
 * @param selector Event type to try on the endpoint.
 * @return Whether the public API refuses the type with UNSUPPORTED_PROFILE.
 */
bool rejects(const q::api::EndpointOptions &options, q::api::EventType selector) {
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint(options);

    try {
        static_cast<void>(endpoint->getFeed()->createSubscription({selector}));
    } catch (const q::api::Error &error) {
        return error.diagnostic().code == q::api::ErrorCode::UNSUPPORTED_PROFILE;
    }

    return false;
}

/** @param record Exact local record to project into an independent peer tuple.
 * @param symbol Exact wire symbol.
 * @return Owning peer tuple with canonical zero values.
 */
peer::Record data(const q::core::LocalRecord &record, std::u16string symbol) {
    peer::Record result{std::move(symbol), {}, 0, {}, record.id, {}};

    for (const auto &field : record.fields) {
        result.fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
    }

    return result;
}

/** @param record Actual source record with an explicit field lookup.
 * @param tuple Matching peer data to mutate.
 * @param key Field to modify.
 * @param raw Complete raw scalar.
 */
void set(const q::core::LocalRecord &record, peer::Record &tuple, std::u16string_view key, std::int64_t raw) {
    for (const auto &lookup : record.lookups) {
        if (lookup.name == key) {
            tuple.fields[lookup.fieldId].raw = raw;

            return;
        }
    }

    throw std::runtime_error("missing peer field");
}

/** Prove exact OTC #pink HISTORY tuples, data type and independent removal flags. */
void history() {
    using namespace q::events::indexed_event_flags;
    const auto schema = q::events::defaultOtcMarketsOrderSchema(SCHEMA);
    require(schema.has_value(), "default OTC schema");
    auto pink = schema->records()[1];
    pink.id = 0;
    const auto local = q::core::LocalSchema::create(std::span(&pink, 1), SCHEMA, LOCAL_PROFILE);
    require(local.has_value(), "OTC local schema");
    const std::array routes{q::api::EventRegistration{q::api::EventType::OTC_MARKETS_ORDER, 0}};
    auto wrongIndex = pink;
    wrongIndex.hasTime = false;
    wrongIndex.indexedTimeLayout.reset();
    const auto wrongSchema = q::core::LocalSchema::create(std::span(&wrongIndex, 1), SCHEMA, LOCAL_PROFILE);
    require(wrongSchema.has_value(), "OTC shape without indexed coordinate");
    q::api::EventRegistryWork wrongWork;
    require(!q::api::EventRegistry::create(*wrongSchema, routes, {}, wrongWork),
            "OTC registry accepted a missing collector coordinate");
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*local, routes, {}, work);
    require(registry.has_value(), "OTC registry");

    pink.hasTime = false;
    pink.indexedTimeLayout.reset();
    pink.fields.insert(pink.fields.begin(), q::core::LocalField{0});

    for (auto &lookup : pink.lookups) {
        ++lookup.fieldId;
    }

    pink.lookups.insert(pink.lookups.begin(), {u"Void", 0});
    const auto wire = q::core::LocalSchema::create(std::span(&pink, 1), SCHEMA, LOCAL_PROFILE);
    require(wire.has_value(), "OTC physical high VOID schema");
    auto first = data(pink, u"AAPL");
    first.flags = SNAPSHOT_BEGIN;
    set(pink, first, u"Index", 21);
    set(pink, first, u"QuoteAccessPayment", -17);
    set(pink, first, u"OtcMarketsFlags", 0x2b);
    const std::array payload{first, peer::Record{u"AAPL", {}, REMOVE_EVENT | SNAPSHOT_END, 0, 0, {}}};

    std::stop_source done;
    std::vector<q::events::OtcMarketsOrder> events;
    std::vector<q::api::RegisteredEvent> variants;
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::HISTORY;
    options.eventRegistry = std::move(*registry);
    options.maximumAttempts = 1;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto subscription = feed->createSubscription<q::events::OtcMarketsOrder>();
    auto mixed = feed->createSubscription({q::api::EventType::OTC_MARKETS_ORDER});
    const auto selector =
        q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::PINK_LOWER.indexedSource());
    subscription->setSelectors(std::array{selector});
    mixed->setSelectors(std::array{selector});
    subscription->addEventListener([&](auto batch) {
        events.insert(events.end(), batch.begin(), batch.end());

        if (events.size() >= 2 && variants.size() >= 2) {
            done.request_stop();
        }
    });
    mixed->addEventListener([&](auto batch) {
        variants.insert(variants.end(), batch.begin(), batch.end());

        if (events.size() >= 2 && variants.size() >= 2) {
            done.request_stop();
        }
    });

    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::HISTORY;
    script.historyBound = 0;
    script.canonicalSymbols = true;
    script.afterRemove = std::vector<peer::Record>{};
    peer::Peer source(*wire, payload, std::array<std::u16string, 1>{u"AAPL"}, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    static_cast<void>(
        endpoint->awaitState(q::api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 10s, done.get_token()));
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();

    require(done.stop_requested() && closed.drained && !source.failed() && source.adds() == 1,
            "OTC HISTORY delivery and retirement");
    require(events.size() == 2 && variants.size() == 2 &&
                std::holds_alternative<q::events::OtcMarketsOrder>(variants[0]) &&
                std::holds_alternative<q::events::OtcMarketsOrder>(variants[1]),
            "OTC owning registered variant");
    require(events.front().order.symbol == u"AAPL" &&
                events.front().getSource() == q::events::OrderSource::PINK_LOWER &&
                static_cast<std::uint32_t>(events.front().getIndex()) == 21 &&
                events.front().getQuoteAccessPayment() == -17 && events.front().otcMarketsFlags == 0x2b &&
                events.front().order.eventFlags == SNAPSHOT_BEGIN &&
                events.back().order.eventFlags == (REMOVE_EVENT | SNAPSHOT_END),
            "OTC source, payload and snapshot/removal flags");
    require(mixed->getStatistics().deliveredEvents.value == 2, "OTC registered statistics");
}

/** Prove TICKER source selection keeps two records for the same symbol distinct. */
void ticker() {
    const auto schema = q::events::defaultOrderImbalanceSchema(SCHEMA);
    require(schema.has_value(), "default imbalance schema");
    const std::array records{schema->records()[0], schema->records()[1]};
    const auto local = q::core::LocalSchema::create(records, SCHEMA, LOCAL_PROFILE);
    require(local.has_value(), "imbalance local schema");
    const std::array routes{q::api::EventRegistration{q::api::EventType::ORDER_IMBALANCE, 0},
                            q::api::EventRegistration{q::api::EventType::ORDER_IMBALANCE, 1}};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*local, routes, {}, work);
    require(registry.has_value(), "imbalance registry");
    auto first = data(records[0], u"AAPL");
    auto second = data(records[1], u"AAPL");
    set(records[0], first, u"Time", 17);
    set(records[0], first, u"Flags", 0x11);
    set(records[1], second, u"Time", -23);
    set(records[1], second, u"Flags", 0x22);
    const std::array payload{first, second};

    std::stop_source done;
    std::vector<q::events::OrderImbalance> selected;
    std::vector<q::api::RegisteredEvent> all;
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::TICKER;
    options.eventRegistry = std::move(*registry);
    options.maximumAttempts = 1;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto exact = feed->createSubscription<q::events::OrderImbalance>();
    auto plain = feed->createSubscription({q::api::EventType::ORDER_IMBALANCE});
    const auto ntv = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::NTV.indexedSource());
    exact->setSelectors(std::array{ntv});
    plain->setSelectors(std::array{q::api::SubscriptionSelector::plain(u"AAPL")});
    exact->addEventListener([&](auto batch) {
        selected.insert(selected.end(), batch.begin(), batch.end());

        if (selected.size() >= 1 && all.size() >= 2) {
            done.request_stop();
        }
    });
    plain->addEventListener([&](auto batch) {
        all.insert(all.end(), batch.begin(), batch.end());

        if (selected.size() >= 1 && all.size() >= 2) {
            done.request_stop();
        }
    });

    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::TICKER;
    script.describeAllRecords = true;
    script.canonicalSymbols = true;
    // The first subscription adds NTV. The plain subscription adds DEFAULT to that existing demand.
    script.subscriptionRecordIds = {1, 0};
    script.removeSequence = std::vector{peer::ExpectedSubscription{{u"AAPL", u"AAPL"}, {0, 1}, true}};
    script.afterFinalRemove = std::vector<peer::Record>{};
    peer::Peer source(*local, payload, std::array<std::u16string, 2>{u"AAPL", u"AAPL"}, 32, peer::PeerMode::HOLD,
                      script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    static_cast<void>(
        endpoint->awaitState(q::api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 10s, done.get_token()));
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();

    require(done.stop_requested() && closed.drained && !source.failed() && source.adds() == 1,
            "imbalance TICKER delivery and retirement");
    require(selected.size() == 1 && selected.front().symbol == u"AAPL" &&
                selected.front().getSource() == q::events::OrderSource::NTV && selected.front().getTime() == -23000 &&
                selected.front().flags == 0x22,
            "explicit imbalance source selection");
    require(all.size() == 2 && std::holds_alternative<q::events::OrderImbalance>(all[0]) &&
                std::holds_alternative<q::events::OrderImbalance>(all[1]) &&
                std::get<q::events::OrderImbalance>(all[0]).getSourceId() !=
                    std::get<q::events::OrderImbalance>(all[1]).getSourceId() &&
                plain->getStatistics().deliveredEvents.value == 2,
            "plain imbalance selects both distinct sources");
}

/** Exercise the default catalogs and reject a subscription on the wrong collector. */
void contracts() {
    const auto tickerRegistry = q::api::detail::defaultReceivingRegistry(q::api::DeliveryContract::TICKER);
    const auto historyRegistry = q::api::detail::defaultReceivingRegistry(q::api::DeliveryContract::HISTORY);
    require(tickerRegistry.has_value() && historyRegistry.has_value(), "default receiving catalogs");
    require(tickerRegistry->schema().records().size() == 42 && historyRegistry->schema().records().size() == 10,
            "default named source variants");

    const auto exactPink =
        q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::PINK_LOWER.indexedSource());
    const auto exactNtv = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::NTV.indexedSource());
    const auto exactDefault =
        q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::DEFAULT.indexedSource());
    const auto historyFilter = q::api::FeedFilter::Builder(*historyRegistry, q::api::DeliveryContract::HISTORY)
                                   .withCategory(u"pink", u":OtcMarketsOrder#pink")
                                   .build();
    const auto tickerFilter = q::api::FeedFilter::Builder(*tickerRegistry, q::api::DeliveryContract::TICKER)
                                  .withCategory(u"ntv", u":OrderImbalance#NTV")
                                  .build();
    require(historyFilter.accept(q::api::EventType::OTC_MARKETS_ORDER, exactPink) &&
                !historyFilter.accept(q::api::EventType::OTC_MARKETS_ORDER, exactDefault),
            "HISTORY filter selected the wrong OTC source");
    require(tickerFilter.accept(q::api::EventType::ORDER_IMBALANCE, exactNtv) &&
                !tickerFilter.accept(q::api::EventType::ORDER_IMBALANCE, exactDefault),
            "TICKER filter selected the wrong imbalance source");

    q::events::OtcMarketsOrder order;
    order.order.symbol = u"AAPL";
    order.order.index = static_cast<std::int64_t>(q::events::OrderSource::PINK_LOWER.id()) << 32;
    q::events::OrderImbalance imbalance;
    imbalance.symbol = u"AAPL";
    imbalance.sourceId = q::events::OrderSource::NTV.id();
    require(historyFilter.acceptEvent(q::api::RegisteredEvent{order}) &&
                tickerFilter.acceptEvent(q::api::RegisteredEvent{imbalance}),
            "FeedFilter rejected the owning event's exact source");

    for (const auto contract : std::array{q::api::DeliveryContract::HISTORY, q::api::DeliveryContract::STREAM}) {
        const auto filter =
            q::api::FeedFilter::Builder(*tickerRegistry, contract).withCategory(u"ntv", u":OrderImbalance#NTV").build();
        require(!filter.accept(q::api::EventType::ORDER_IMBALANCE, exactNtv) &&
                    !filter.acceptEvent(q::api::RegisteredEvent{imbalance}),
                "Filter admitted TICKER-only imbalance on another contract");
    }

    order.order.index = 0;
    imbalance.sourceId = q::events::OrderSource::DEFAULT.id();
    require(!historyFilter.acceptEvent(q::api::RegisteredEvent{order}) &&
                !tickerFilter.acceptEvent(q::api::RegisteredEvent{imbalance}),
            "FeedFilter accepted owning events from an unselected source");

    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::TICKER;
    require(rejects(options, q::api::EventType::OTC_MARKETS_ORDER), "OTC on TICKER");
    options.contract = q::api::DeliveryContract::HISTORY;
    require(rejects(options, q::api::EventType::ORDER_IMBALANCE), "imbalance on HISTORY");
}
} // namespace

int main() {
    try {
        contracts();
        history();
        ticker();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());

        return 1;
    }
}
