// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Standalone public Feed consumer driven by an explicitly owned deterministic HISTORY peer.
#include "connect_peer.hpp"
#include "order_fixture.hpp"
#include "qd/api/feed.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <limits>
#include <mutex>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace peer = q::connect_tests;
namespace f = order_fixture;
using namespace std::chrono_literals;
constexpr q::core::SchemaLimits SCHEMA{16, 128, 256, 128, 16384};
constexpr std::int32_t TX_PENDING = 1;
constexpr std::int32_t REMOVE_EVENT = 2;
constexpr std::int32_t SNAPSHOT_BEGIN = 4;
constexpr std::int32_t SNAPSHOT_END = 8;

/** @param condition Required observable behavior.
 * @param message Static test failure, with no peer input in diagnostic text.
 */
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Wait for one bounded public observation, without controlling another test's work.
 * @tparam Predicate Nonblocking completion observation.
 * @param predicate Public/peer state check.
 * @return True when observed within ten seconds.
 */
template <class Predicate> bool eventually(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 10s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

/** Independent callback copies; no callback span, schema or runtime owner is retained. */
struct Observation {
    mutable std::mutex mutex;
    std::vector<q::events::Order> values;
    std::atomic<std::size_t> count{0};

    /** @param batch Complete owning values borrowed only through this callback. */
    void append(std::span<const q::events::Order> batch) {
        std::lock_guard lock(mutex);
        values.insert(values.end(), batch.begin(), batch.end());
        count.store(values.size(), std::memory_order_release);
    }

    /** @return Independent event copies, surviving native and callback retirement. */
    std::vector<q::events::Order> copy() const {
        std::lock_guard lock(mutex);

        return values;
    }
};

/** Reproduce the original source integer slots, including high VOID and long tails, for the wire peer.
 * The receiving registry retains only logical fields with explicit IndexedTimeLowInt32.
 * @param scenario Actual production physical schema observations.
 * @return Independently owned test-source declarations; no collector or receiver uses this physical schema.
 */
q::core::LocalSchema physicalSchema(const f::Scenario &scenario) {
    std::vector<q::core::LocalRecord> records;

    for (const auto &logical : scenario.records) {
        q::core::LocalRecord physical{logical.id, logical.name, false, {}, {}};

        for (const auto &row : scenario.routes) {
            if (std::stoi(row.at(2)) != logical.id) {
                continue;
            }

            const auto slot = std::stoull(row.at(3));
            require(slot == physical.fields.size(), "Physical Order source slots changed");
            physical.fields.push_back({std::stoi(row.at(4))});
            physical.lookups.push_back({std::u16string(row.at(5).begin(), row.at(5).end()), slot});
        }

        require(physical.fields.size() >= 2 && physical.fields[0].serialTypeId == 0 &&
                    physical.fields[1].serialTypeId == 8,
                "Original physical VOID/Index source identity lost");
        records.push_back(std::move(physical));
    }

    auto result = q::core::LocalSchema::create(records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(result), "Original physical Order source schema refused");

    return std::move(*result);
}

/** Copy one actual cursor's logical values into its original physical source field declarations.
 * @param scenario Original production logical/physical schema.
 * @param physical Test-source declarations containing physical VOID slots.
 * @param id Exact source record identity.
 * @param sample Original actual parser/delegate sample.
 * @param flags Selected real HISTORY source flags for this scenario.
 * @param removal Exact raw removal index, absent for ordinary data.
 * @param metadata Whether to attach independent explicit FIELD words.
 * @return Complete owned deterministic source record.
 */
peer::Record sourceRecord(const f::Scenario &scenario, const q::core::LocalSchema &physical, std::size_t id,
                          std::size_t sample, std::int32_t flags, std::optional<std::int64_t> removal, bool metadata) {
    std::vector<q::core::FieldValue> fields;
    const auto &source = physical.records()[id];
    const auto &native = scenario.records[id];
    const auto &values = scenario.samples.at({id, sample}).values;

    for (std::size_t slot = 0; slot < source.fields.size(); ++slot) {
        const auto type = static_cast<q::core::SerialFieldType>(source.fields[slot].serialTypeId);
        q::core::FieldValue value{type, 0};

        if (type != q::core::SerialFieldType::VOID_FIELD) {
            const auto physicalKey = std::ranges::find_if(source.lookups, [slot](const auto &lookup) {
                return lookup.fieldId == slot;
            });
            const auto logical = std::ranges::find_if(native.lookups, [&](const auto &lookup) {
                return lookup.name == physicalKey->name;
            });
            require(logical != native.lookups.end(), "Physical Order field has no exact logical projection");
            value = values[logical->fieldId];
        }

        fields.push_back(std::move(value));
    }

    return {u"AAPL",
            std::move(fields),
            flags,
            removal,
            static_cast<std::int32_t>(id),
            metadata ? std::optional{q::protocol::FieldEventTime{7, 0x7fffffff, 0}} : std::nullopt};
}

/** @param scenario Actual production standard-source declarations.
 * @return One immutable native logical registry retaining every standard field and each source separately.
 */
q::api::EventRegistry registry(const f::Scenario &scenario) {
    auto schema = q::core::LocalSchema::create(scenario.records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Actual indexed Order logical schema refused");
    const std::array routes{q::api::EventRegistration{q::api::EventType::ORDER, 0},
                            q::api::EventRegistration{q::api::EventType::ORDER, 1},
                            q::api::EventRegistration{q::api::EventType::ORDER, 2}};
    q::api::EventRegistryWork work;
    auto result = q::api::EventRegistry::create(*schema, routes, {}, work);
    require(bool(result), "Complete actual Order registry refused");

    return std::move(*result);
}

/** Verify real full-field HISTORY callbacks, logical overlap, actual TX continuation and reentry retirement.
 * @param scenario Actual default/tiny/int/FOB production source observations.
 * @param metadata Attach explicit independent FIELD metadata for the full standard-source path.
 */
void check(const f::Scenario &scenario, bool metadata) {
    auto physical = physicalSchema(scenario);
    q::api::RuntimeOptions limits;
    limits.maximumSubscriptionsPerEndpoint = 4;
    limits.maximumSymbolsPerSubscription = 4;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::HISTORY;
    options.eventRegistry = registry(scenario);
    options.maximumAttempts = 1;
    options.readTimeout = 15000ms;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto typed = feed->createSubscription<q::events::Order>();
    auto mixed = feed->createSubscription({q::api::EventType::ORDER, q::api::EventType::ORDER});
    auto throwing = feed->createSubscription<q::events::Order>();
    auto typedValues = std::make_shared<Observation>();
    auto mixedValues = std::make_shared<Observation>();
    auto callbackClosed = std::make_shared<std::atomic<bool>>(false);
    const auto finalTime = std::stoll(scenario.samples.at({1, 11}).expected.at(6));
    const std::weak_ptr<q::api::Subscription<q::events::Order>> weakTyped = typed;
    typed->addEventListener([typedValues, callbackClosed, weakTyped, finalTime](auto batch) {
        typedValues->append(batch);

        for (const auto &order : batch) {
            if (order.getSourceId() == q::events::OrderSource::NTV.id() && order.getTime() == finalTime &&
                order.eventFlags == 0) {
                if (auto subscription = weakTyped.lock()) {
                    subscription->close();
                }

                callbackClosed->store(true, std::memory_order_release);
            }
        }
    });
    mixed->addEventListener([mixedValues](auto batch) {
        for (const auto &value : batch) {
            require(std::holds_alternative<q::events::Order>(value), "Wrong mixed Order projection");
            const auto &order = std::get<q::events::Order>(value);
            mixedValues->append(std::span(&order, 1));
        }
    });
    const std::weak_ptr<q::api::Subscription<q::api::RegisteredEvent>> weakMixed = mixed;
    auto listenerIdentity = std::make_shared<std::atomic<std::uint64_t>>(0);
    auto selfCalls = std::make_shared<std::atomic<unsigned>>(0);
    listenerIdentity->store(mixed->addEventListener([weakMixed, listenerIdentity, selfCalls](auto) {
        selfCalls->fetch_add(1, std::memory_order_relaxed);

        if (auto subscription = weakMixed.lock()) {
            subscription->removeEventListener(listenerIdentity->load());
        }
    }));
    throwing->addEventListener([](auto) {
        throw std::runtime_error("Owned Order listener failure");
    });
    const auto plain = q::api::SubscriptionSelector::plain(u"AAPL");
    const auto ntv = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::NTV.indexedSource());
    const auto bzx = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::BZX.indexedSource());
    typed->setSelectors(std::array{plain, ntv});
    mixed->setSelectors(std::array{ntv});
    throwing->setSelectors(std::array{bzx});
    std::vector<peer::Record> initial;
    std::vector<peer::Record> afterRemove;

    for (std::size_t id = 0; id < 3; ++id) {
        initial.push_back(sourceRecord(scenario, physical, id, 3, SNAPSHOT_BEGIN, {}, metadata));
        initial.push_back(sourceRecord(scenario, physical, id, 0, REMOVE_EVENT | SNAPSHOT_END, 0, metadata));
        afterRemove.push_back(sourceRecord(scenario, physical, id, 7, id == 1 ? TX_PENDING : 0, {}, metadata));
    }

    const std::vector<peer::Record> endTransaction{sourceRecord(scenario, physical, 1, 11, 0, {}, metadata)};
    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::HISTORY;
    script.historyBound = 0;
    script.describeAllRecords = true;
    script.canonicalSymbols = true;
    script.fieldMetadata = metadata;
    script.waitForDataRelease = true;
    script.subscriptionRecordIds = {0, 1, 2};
    script.removeSequence = std::vector{peer::ExpectedSubscription{{u"AAPL", u"AAPL"}, {0, 2}, true},
                                        peer::ExpectedSubscription{{u"AAPL", u"MARKER"}, {1, 1}, true}};
    script.afterRemove = afterRemove;
    script.afterFinalRemove = std::vector<peer::Record>{};
    script.followupAdd = peer::ExpectedSubscription{{u"MARKER"}, {1}};
    script.afterFollowupAdd = endTransaction;
    const std::array<std::u16string, 3> expected{u"AAPL", u"AAPL", u"AAPL"};
    peer::Peer source(physical, initial, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return source.adds() == 1 || source.failed();
            }),
            "Order physical ADD timeout");
    require(!source.failed(), "Overlapping Order selectors duplicated or misrouted physical demand");
    source.releaseData();
    require(eventually([&] {
                return (typedValues->count.load() >= 6 && mixedValues->count.load() >= 2) || source.failed();
            }),
            "Initial Order snapshot timeout");
    require(!source.failed(), "Order peer rejected initial HISTORY demand");
    require(eventually([&] {
                return throwing->getState() == q::api::SubscriptionState::FAILED;
            }),
            "Order listener throw did not isolate its subscription");
    typed->removeSelector(plain);
    const auto removed = eventually([&] {
        return source.removes() == 1 || source.failed();
    });

    if (!removed) {
        const auto diagnostic = typed->getDiagnostic();
        const auto endpointDiagnostic = endpoint->getDiagnostic();
        const auto statistics = typed->getStatistics();
        std::fprintf(stderr,
                     "overlap diagnostic: state=%d code=%d stage=%.*s detail=%u endpoint=%d/%.*s adds=%u removes=%u "
                     "typed=%zu mixed=%zu desired=%llu active=%llu\n",
                     static_cast<int>(typed->getState()), static_cast<int>(diagnostic.code),
                     static_cast<int>(diagnostic.stage.size()), diagnostic.stage.data(), diagnostic.detailCode,
                     static_cast<int>(endpointDiagnostic.code), static_cast<int>(endpointDiagnostic.stage.size()),
                     endpointDiagnostic.stage.data(), source.adds(), source.removes(), typedValues->count.load(),
                     mixedValues->count.load(), static_cast<unsigned long long>(typed->getSnapshot().revision),
                     static_cast<unsigned long long>(statistics.activeDesiredRevision.value_or(0)));
    }

    require(removed, "Order overlap removal timeout");
    require(!source.failed(), "Removing plain selector lost retained NTV source demand");
    const auto pending = eventually([&] {
        return typedValues->count.load() >= 7 && mixedValues->count.load() >= 3;
    });

    if (!pending) {
        const auto diagnostic = typed->getDiagnostic();
        const auto endpointDiagnostic = endpoint->getDiagnostic();
        std::fprintf(stderr,
                     "pending diagnostic: state=%d code=%d stage=%.*s detail=%u endpoint=%d/%.*s adds=%u removes=%u "
                     "typed=%zu mixed=%zu sent=%d peerfailed=%d\n",
                     static_cast<int>(typed->getState()), static_cast<int>(diagnostic.code),
                     static_cast<int>(diagnostic.stage.size()), diagnostic.stage.data(), diagnostic.detailCode,
                     static_cast<int>(endpointDiagnostic.code), static_cast<int>(endpointDiagnostic.stage.size()),
                     endpointDiagnostic.stage.data(), source.adds(), source.removes(), typedValues->count.load(),
                     mixedValues->count.load(), source.afterRemoveSent(), source.failed());
    }

    require(pending, "Order TX_PENDING callback timeout");
    require(typedValues->copy().back().eventFlags == TX_PENDING && mixedValues->copy().back().eventFlags == TX_PENDING,
            "Actual Order transaction continuation flag lost");
    const auto marker = q::api::SubscriptionSelector::indexed(u"MARKER", q::events::OrderSource::NTV.indexedSource());
    mixed->addSelectors(std::array{marker});
    require(eventually([&] {
                return callbackClosed->load() && typedValues->count.load() >= 8 && mixedValues->count.load() >= 4;
            }),
            "Order transaction completion or callback close timeout");
    require(!source.failed(), "Order transaction followup demand failed");
    require(selfCalls->load() == 1, "Order listener removal did not fence later callbacks");
    auto values = typedValues->copy();
    require(values.size() == 8, "Order overlap duplicated typed events");

    for (std::size_t id = 0; id < 3; ++id) {
        auto expectedSample = scenario.samples.at({id, 3});
        expectedSample.expected[4] = "4";
        expectedSample.expected[1] = metadata ? "7511" : "0";
        const auto sourceId = std::stoi(expectedSample.expected[3]);
        const auto found = std::ranges::find_if(values, [&](const auto &order) {
            return order.getSourceId() == sourceId && order.eventFlags == SNAPSHOT_BEGIN;
        });
        require(found != values.end() && f::matches(*found, expectedSample),
                "Full standard Order fields changed on Feed path");
        require(std::ranges::count_if(values,
                                      [&](const auto &order) {
                                          return order.getSourceId() == sourceId &&
                                                 (order.eventFlags & REMOVE_EVENT) != 0 &&
                                                 (order.eventFlags & SNAPSHOT_END) != 0 &&
                                                 static_cast<std::uint32_t>(order.index) == 0;
                                      }) == 1,
                "Order source-specific remove/snapshot boundary lost");
    }

    require(values.back().eventFlags == 0 && values.back().getSourceId() == q::events::OrderSource::NTV.id(),
            "Order transaction completion lost source identity");
    mixed->setSelectors(std::span<const q::api::SubscriptionSelector>{});
    require(eventually([&] {
                return source.removes() == 2 || source.failed();
            }),
            "Final Order source/marker removal timeout");
    require(!source.failed(), "Independent Order removal tuples changed");
    const auto clearedRevision = mixed->getSnapshot().revision;
    require(eventually([&] {
                const auto state = mixed->getStatistics();

                return state.activeDesiredRevision == clearedRevision && state.pendingEvents == 0 &&
                       state.inFlightEvents == 0 && typed->getStatistics().final && throwing->getStatistics().final;
            }),
            "Order final desired replacement or callback retirement did not apply");
    require(mixedValues->count.load() == 4, "Duplicate mixed Order event type or source demand duplicated callbacks");
    require(endpoint->getState() == q::api::EndpointState::CONNECTED &&
                endpoint->getDiagnostic().code == q::api::ErrorCode::NONE,
            "Healthy Order subscribers lost their endpoint before local callback cutoff");
    const auto closed = endpoint->closeAndAwaitTermination();
    std::fprintf(stderr, "Order callback cutoff: drained=%d gap=%d expired=%d code=%d stage=%.*s native=%d\n",
                 closed.drained, closed.hadGap, closed.deadlineExpired, static_cast<int>(closed.diagnostic.code),
                 static_cast<int>(closed.diagnostic.stage.size()), closed.diagnostic.stage.data(),
                 closed.diagnostic.nativeCode);
    require(!closed.drained && !closed.deadlineExpired && closed.hadGap,
            "Order aborted incomplete snapshot gap or callback retirement evidence was lost");
    // The throwing subscriber aborts on SNAPSHOT_BEGIN before its boundary; HISTORY retains that incomplete range.
    const auto expectedCutoff =
        closed.diagnostic.code == q::api::ErrorCode::NONE ||
        (closed.diagnostic.code == q::api::ErrorCode::TRANSPORT_FAILURE &&
         ((closed.diagnostic.stage == "socket-read" && closed.diagnostic.nativeCode == 1236) ||
          (closed.diagnostic.stage == "recovery-peer-end" && closed.diagnostic.nativeCode == 0)));
    require(expectedCutoff, "Unexpected Order diagnostic after explicit healthy endpoint cutoff");
    require(typed->getStatistics().final && throwing->getStatistics().final,
            "Order callback owners did not reach final retirement");
    require(values[0].symbol == u"AAPL" && values[0].businessNumericSnapshot.has_value(),
            "Retained Order copy changed after shutdown");
}

/** Receive an actual ordinary Order using SDK-owned HISTORY defaults and observe passive range publication.
 * @param scenario Independent actual standard source declarations and values.
 */
void checkDefaultRegistry(const f::Scenario &scenario) {
    auto physical = physicalSchema(scenario);
    q::api::RuntimeOptions limits;
    limits.maximumSubscriptionsPerEndpoint = 1;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::HISTORY;
    options.maximumAttempts = 1;
    auto endpoint = runtime.createEndpoint(options);
    bool rejected = false;
    try {
        (void)endpoint->getFeed()->createSubscription<q::events::Quote>();
    } catch (const q::api::Error &error) {
        rejected = error.diagnostic().code == q::api::ErrorCode::UNSUPPORTED_PROFILE;
    }
    require(rejected, "Unsupported HISTORY event was admitted into the default registry");
    auto subscription = endpoint->getFeed()->createSubscription<q::events::Order>();
    auto observed = std::make_shared<Observation>();
    auto callbackSafe = std::make_shared<std::atomic<bool>>(true);
    auto callbackReads = std::make_shared<std::atomic<unsigned>>(0);
    auto matchingCallbacks = std::make_shared<std::atomic<unsigned>>(0);
    auto callbackStates = std::make_shared<std::atomic<unsigned>>(0);
    const std::weak_ptr<q::api::Subscription<q::events::Order>> weak = subscription;
    subscription->addEventListener(
        [observed, callbackSafe, callbackReads, matchingCallbacks, callbackStates, weak](auto batch) {
            if (const auto owner = weak.lock()) {
                const auto cut = owner->getHistorySnapshot();

                if (cut.state == q::api::HistoryObservationState::CURRENT ||
                    std::ranges::any_of(cut.ranges, [](const auto &range) {
                        return range.status.completeDeliveredRange;
                    })) {
                    callbackSafe->store(false);
                }

                // Earlier facade qualifiers can suppress CURRENT before the active DATA callback fence is examined.
                const bool matchingCut =
                    cut.publicationSerial > 0 && !cut.final && cut.logicalRevision == cut.acceptedLogicalRevision &&
                    cut.mappingLogicalRevision && *cut.mappingLogicalRevision == cut.acceptedLogicalRevision &&
                    cut.connectionRevision == cut.acceptedConnectionRevision &&
                    cut.clearRevision == cut.acceptedClearRevision && cut.clearedRevision >= cut.acceptedClearRevision;

                if (matchingCut) {
                    matchingCallbacks->fetch_add(1);

                    if (cut.state != q::api::HistoryObservationState::CALLBACK_PENDING) {
                        callbackSafe->store(false);
                    }
                }

                callbackReads->fetch_add(1);
                callbackStates->fetch_or(1U << static_cast<unsigned>(cut.state));
            }

            observed->append(batch);
        });
    const auto symbol = q::api::SubscriptionSelector::plain(u"AAPL");
    const auto marker = q::api::SubscriptionSelector::plain(u"MARKER");
    subscription->setSelectors(std::array{symbol});
    const std::array initial{sourceRecord(scenario, physical, 0, 3, SNAPSHOT_BEGIN, {}, false)};
    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::HISTORY;
    script.historyBound = 0;
    script.canonicalSymbols = true;
    script.followupAdd = peer::ExpectedSubscription{{u"MARKER"}, {0}};
    script.afterFollowupAdd =
        std::vector{sourceRecord(scenario, physical, 0, 0, REMOVE_EVENT | SNAPSHOT_END, 0, false)};
    script.removeSequence = std::vector{peer::ExpectedSubscription{{u"AAPL", u"MARKER"}, {0, 0}, true}};
    script.afterFinalRemove = std::vector<peer::Record>{};
    const std::array<std::u16string, 1> expected{u"AAPL"};
    peer::Peer source(physical, initial, expected, 1, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return observed->count.load() >= 1 || source.failed();
            }),
            "Default Order HISTORY registry delivery timeout");
    require(!source.failed(), "Default Order registry sent an unexpected physical source tuple");
    q::api::HistorySubscriptionSnapshot partial;
    require(eventually([&] {
                partial = subscription->getHistorySnapshot();

                return partial.state == q::api::HistoryObservationState::CURRENT && partial.ranges.size() == 1 &&
                       partial.ranges[0].status.source &&
                       partial.ranges[0].status.source->snapshot == q::collectors::HistorySnapshotState::RECEIVING;
            }),
            "Actual Order partial HISTORY publication timeout");
    const auto statistics = subscription->getStatistics();
    require(partial.ownerIdentity == statistics.ownerIdentity && partial.sourceEpoch == statistics.sourceEpoch &&
                partial.sourceEpoch && partial.publicationSerial > 0 && partial.ranges[0].context.valid() &&
                partial.logicalRevision == subscription->getSnapshot().revision &&
                partial.mappingLogicalRevision == partial.logicalRevision &&
                partial.acceptedLogicalRevision == partial.logicalRevision &&
                partial.connectionRevision == partial.acceptedConnectionRevision &&
                partial.clearRevision == partial.clearedRevision && !partial.sourceResetPending,
            "Order HISTORY publication lost owner mapping revision or source scope");
    const auto &partialRange = partial.ranges[0];
    require(partialRange.symbol == u"AAPL" && partialRange.selector == q::core::SubscriptionSelectorKind::EXACT &&
                partialRange.status.requestedFrom == 0 && partialRange.status.source->requestedFrom == 0 &&
                partialRange.status.source->sourceFrontier == std::numeric_limits<std::int32_t>::max() &&
                !partialRange.status.source->completeRequestedRange && !partialRange.status.completeDeliveredRange,
            "Order partial HISTORY range invented completion or converted raw Index to time");
    subscription->addSelectors(std::array{marker});
    const auto updatedRevision = subscription->getSnapshot().revision;
    require(partial.logicalRevision != updatedRevision && partial.ranges.size() == 1,
            "Retained owning Order cut changed after logical mutation");
    require(eventually([&] {
                return observed->count.load() >= 2 || source.failed();
            }),
            "Actual Order snapshot boundary delivery timeout");
    q::api::HistorySubscriptionSnapshot complete;
    require(eventually([&] {
                complete = subscription->getHistorySnapshot();
                const auto range = std::ranges::find_if(complete.ranges, [](const auto &item) {
                    return item.symbol == u"AAPL";
                });

                return complete.state == q::api::HistoryObservationState::CURRENT &&
                       complete.logicalRevision == updatedRevision && range != complete.ranges.end() &&
                       range->status.completeDeliveredRange;
            }),
            "Actual Order delivered HISTORY completion publication timeout");
    const auto range = std::ranges::find_if(complete.ranges, [](const auto &item) {
        return item.symbol == u"AAPL";
    });
    require(!source.failed() && complete.ranges.size() == 2 && complete.ownerIdentity == partial.ownerIdentity &&
                complete.sourceEpoch == partial.sourceEpoch && complete.mappingLogicalRevision == updatedRevision &&
                complete.acceptedLogicalRevision == updatedRevision &&
                complete.publicationSerial > partial.publicationSerial &&
                range->context.matches(partialRange.context) && range->status.source &&
                range->status.requestedFrom == 0 && range->status.source->requestedFrom == 0 &&
                range->status.source->sourceFrontier == 0 && range->status.source->completeRequestedRange &&
                !range->status.source->sourceTransactionPending && !range->status.source->initialPending &&
                !range->status.source->viewTransactionDirty && callbackSafe->load() && callbackReads->load() == 2,
            "Order completion or passive callback HISTORY getter qualification changed");
    std::fprintf(stderr, "Order HISTORY getter callbacks=%u matchingFences=%u observedStateMask=%u\n",
                 callbackReads->load(), matchingCallbacks->load(), callbackStates->load());
    auto values = observed->copy();
    auto expectedValue = scenario.samples.at({0, 3});
    expectedValue.expected[4] = "4";
    expectedValue.expected[1] = "0";
    require(values.size() == 2 && f::matches(values.front(), expectedValue),
            "Default Order registry changed actual source fields");
    subscription->setSelectors(std::span<const q::api::SubscriptionSelector>{});
    require(eventually([&] {
                return source.removes() == 1 || source.failed();
            }),
            "Default Order registry physical removal timeout");
    require(!source.failed(), "Default Order registry removal changed physical identity");
    require(endpoint->closeAndAwaitTermination().drained, "Default Order registry owners did not retire");
    const auto retired = subscription->getHistorySnapshot();
    require(retired.state == q::api::HistoryObservationState::RETIRED && retired.final &&
                retired.ownerIdentity == partial.ownerIdentity &&
                partial.state == q::api::HistoryObservationState::CURRENT && partial.ranges[0].symbol == u"AAPL" &&
                partial.ranges[0].context.matches(range->context) && !partial.ranges[0].status.completeDeliveredRange &&
                values.front().symbol == u"AAPL",
            "Owning Order HISTORY cut or callback copy did not survive shutdown unchanged");
}

/** Exercise two distinct negative logical source IDs through actual aggregate demand and callbacks.
 * @param scenario Complete actual standard source observations.
 */
void checkNegativeSources(const f::Scenario &scenario) {
    auto physical = physicalSchema(scenario);
    q::api::RuntimeOptions limits;
    limits.maximumSymbolsPerSubscription = 4;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::HISTORY;
    options.eventRegistry = registry(scenario);
    options.maximumAttempts = 1;
    options.readTimeout = 15000ms;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription<q::events::Order>();
    auto observed = std::make_shared<Observation>();
    subscription->addEventListener([observed](auto batch) {
        observed->append(batch);
    });
    const q::events::IndexedEventSource all(-1, std::nullopt);
    const q::events::IndexedEventSource other(std::numeric_limits<std::int32_t>::min(), std::nullopt);
    const auto first = q::api::SubscriptionSelector::indexed(u"AAPL", all);
    const auto retained = q::api::SubscriptionSelector::indexed(u"AAPL", other);
    subscription->setSelectors(std::array{first, retained});
    std::vector<peer::Record> initial;
    std::vector<peer::Record> updated;

    for (std::size_t id = 0; id < 3; ++id) {
        initial.push_back(sourceRecord(scenario, physical, id, 3, SNAPSHOT_BEGIN, {}, false));
        initial.push_back(sourceRecord(scenario, physical, id, 0, REMOVE_EVENT | SNAPSHOT_END, 0, false));
        updated.push_back(sourceRecord(scenario, physical, id, 7, 0, {}, false));
    }

    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::HISTORY;
    script.historyBound = 0;
    script.describeAllRecords = true;
    script.canonicalSymbols = true;
    script.subscriptionRecordIds = {0, 1, 2};
    script.followupAdd = peer::ExpectedSubscription{{u"MARKER", u"MARKER", u"MARKER"}, {0, 1, 2}};
    script.afterFollowupAdd = updated;
    script.removeSequence = std::vector{peer::ExpectedSubscription{
        {u"AAPL", u"MARKER", u"AAPL", u"MARKER", u"AAPL", u"MARKER"}, {0, 0, 1, 1, 2, 2}, true}};
    script.afterFinalRemove = std::vector<peer::Record>{};
    const std::array<std::u16string, 3> expected{u"AAPL", u"AAPL", u"AAPL"};
    peer::Peer source(physical, initial, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return observed->count.load() >= 6 || source.failed();
            }),
            "Negative Order source initial delivery timeout");
    require(!source.failed() && observed->count.load() == 6,
            "Negative Order sources did not select exact available union");
    subscription->removeSelector(first);
    const auto revision = subscription->getSnapshot().revision;
    require(subscription->getSnapshot().symbols == std::vector{retained},
            "Distinct negative source logical identity collapsed");
    require(eventually([&] {
                return subscription->getStatistics().activeDesiredRevision == revision;
            }),
            "Negative Order retained selector did not apply");
    const auto marker = q::api::SubscriptionSelector::indexed(u"MARKER", other);
    subscription->addSelectors(std::array{marker});
    require(eventually([&] {
                return (source.adds() == 2 && observed->count.load() >= 9) || source.failed();
            }),
            "Negative Order retained demand did not receive followup data");
    require(!source.failed() && observed->count.load() == 9 && source.removes() == 0,
            "Removing one negative Order source lost another's physical demand");
    subscription->setSelectors(std::span<const q::api::SubscriptionSelector>{});
    require(eventually([&] {
                return source.removes() == 1 || source.failed();
            }),
            "Negative Order final physical removal timeout");
    require(!source.failed(), "Negative Order exact removal union changed");
    require(endpoint->closeAndAwaitTermination().drained, "Negative Order owners did not retire");
}

/** Retain typed Order identities through a real fresh protocol/source epoch and desired replay.
 * @param scenario Complete actual standard source observations.
 */
void checkReconnect(const f::Scenario &scenario) {
    auto physical = physicalSchema(scenario);
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::HISTORY;
    options.eventRegistry = registry(scenario);
    options.maximumAttempts = 2;
    options.retryDelay = 1ms;
    options.readTimeout = 15000ms;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription<q::events::Order>();
    auto observed = std::make_shared<Observation>();
    subscription->addEventListener([observed](auto batch) {
        observed->append(batch);
    });
    const auto plain = q::api::SubscriptionSelector::plain(u"AAPL");
    subscription->setSelectors(std::array{plain});
    std::vector<peer::Record> initial;

    for (std::size_t id = 0; id < 3; ++id) {
        initial.push_back(sourceRecord(scenario, physical, id, 3, SNAPSHOT_BEGIN, {}, false));
        initial.push_back(sourceRecord(scenario, physical, id, 0, REMOVE_EVENT | SNAPSHOT_END, 0, false));
    }

    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::HISTORY;
    script.historyBound = 0;
    script.describeAllRecords = true;
    script.canonicalSymbols = true;
    script.subscriptionRecordIds = {0, 1, 2};
    script.afterReconnect = initial;
    script.removeSequence = std::vector{peer::ExpectedSubscription{{u"AAPL", u"AAPL", u"AAPL"}, {0, 1, 2}, true}};
    script.afterFinalRemove = std::vector<peer::Record>{};
    const std::array<std::u16string, 3> expected{u"AAPL", u"AAPL", u"AAPL"};
    peer::Peer source(physical, initial, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return observed->count.load() >= 6 || source.failed();
            }),
            "Order first source epoch timeout");
    const auto firstEpoch = subscription->getStatistics().sourceEpoch;
    endpoint->reconnect();
    require(eventually([&] {
                return (source.adds() == 2 && observed->count.load() >= 12) || source.failed();
            }),
            "Order fresh source replay timeout");
    require(!source.failed(), "Order desired replay changed source tuples");
    const auto secondEpoch = subscription->getStatistics().sourceEpoch;
    require(firstEpoch && secondEpoch && *secondEpoch > *firstEpoch, "Order source epoch did not advance");
    const auto values = observed->copy();
    require(values.size() == 12, "Order source reset duplicated or lost projected events");

    for (const auto &order : values) {
        require(order.symbol == u"AAPL" &&
                    (order.getSourceId() == 0 || order.getSourceId() == q::events::OrderSource::NTV.id() ||
                     order.getSourceId() == q::events::OrderSource::BZX.id()),
                "Order source identity changed across protocol epochs");
    }

    subscription->removeSelector(plain);
    require(eventually([&] {
                return source.removes() == 1 || source.failed();
            }),
            "Order replay final remove timeout");
    require(!source.failed(), "Order replay removal changed physical identities");
    const auto closed = endpoint->closeAndAwaitTermination();
    require(!closed.drained && closed.hadGap && !closed.deadlineExpired,
            "Order explicit reconnect gap or final retirement evidence was lost");
    require(subscription->getStatistics().final, "Order replay subscription did not retire");
}
} // namespace

int main(int argc, char **argv) {
    try {
        require(argc == 2, "Expected one Order oracle fixture");
        const auto corpus = f::load(argv[1]);

        for (const auto &name : std::array{"default120", "scheme-tiny120", "scheme-int120", "scheme-fob120"}) {
            check(corpus.at(name), name == std::string_view("default120"));
        }

        checkDefaultRegistry(corpus.at("default120"));
        checkNegativeSources(corpus.at("default120"));
        checkReconnect(corpus.at("default120"));

        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());

        return 1;
    }
}
