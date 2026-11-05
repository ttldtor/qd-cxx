// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Standalone public consumer: actual loopback QTP peer, owned callbacks and no private runtime access.
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include "qd/transport/recovery.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;

/** @param condition Required public observation.
 * @param message Failure retained by the bounded test process.
 */
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Test-only finite observation wait; SDK progress remains library-owned.
 * @tparam Predicate Nonblocking observation function.
 * @param predicate Success or terminal fault predicate.
 * @return Whether the predicate became true within five seconds.
 */
template <class Predicate> bool eventually(Predicate predicate) {
    const auto end = std::chrono::steady_clock::now() + 5s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= end) {
            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

/** Preserve only the exact controlled-peer native retirement limits already qualified by Feed tests.
 * @param endpoint Public owner after actual retirement.
 * @param result Complete original close observation; this check never claims drained cancellation.
 */
void verifyClose(const std::shared_ptr<q::api::Endpoint> &endpoint, const q::api::CloseResult &result) {
    const auto &diagnostic = result.diagnostic;
    bool knownNativeEnd = diagnostic.stage == "recovery-peer-end" && diagnostic.nativeCode == 0;
#ifdef _WIN32
    knownNativeEnd = knownNativeEnd || (diagnostic.stage == "socket-read" && diagnostic.nativeCode == 1236);
#endif

    if (diagnostic.code == q::api::ErrorCode::NONE) {
        require(endpoint->getState() == q::api::EndpointState::CLOSED, "error-free retirement did not close endpoint");
    } else {
        require(knownNativeEnd && diagnostic.code == q::api::ErrorCode::TRANSPORT_FAILURE &&
                    diagnostic.detailCode ==
                        static_cast<std::uint32_t>(q::transport::RecoveryError::TRANSPORT_FAILURE) &&
                    endpoint->getState() == q::api::EndpointState::FAILED,
                "unexpected native retirement failure");
    }

    require(!result.deadlineExpired, "native retirement exceeded the drain deadline");
    std::cout << "CLOSE drained=" << result.drained << " gap=" << result.hadGap
              << " deadline=" << result.deadlineExpired << " code=" << static_cast<int>(diagnostic.code)
              << " stage=" << diagnostic.stage << " native=" << diagnostic.nativeCode
              << " detail=" << diagnostic.detailCode << '\n';
}

/** Detached copies survive every callback, subscription, endpoint and runtime owner. */
template <class Event> struct Observation {
    std::mutex mutex;
    std::vector<Event> values;
    std::atomic<std::size_t> count{0};

    void append(std::span<const Event> batch) {
        std::lock_guard lock(mutex);
        values.insert(values.end(), batch.begin(), batch.end());
        count.store(values.size(), std::memory_order_release);
    }
};

/** Explicit caller schema keeps extra fields and an unregistered custom record.
 * @param versionType Exact Version full identity, or zero to omit Version entirely.
 * @return Complete immutable ordinary binary schema with Configuration0/Message1/Custom2.
 */
q::core::LocalSchema schema(int versionType) {
    auto config = q::events::defaultConfigurationSchema({16, 128, 256, 128, 16384});
    auto message = q::events::defaultMessageSchema({16, 128, 256, 128, 16384});
    require(bool(config) && bool(message), "default misc schema refused");
    auto configuration = config->records()[0];

    if (!versionType) {
        configuration.fields.erase(configuration.fields.begin());
        configuration.lookups = {{u"Configuration", 0}};
    } else {
        configuration.fields[0].serialTypeId = versionType;
    }

    configuration.lookups.push_back({u"VendorBlob", configuration.fields.size() - 1});
    configuration.lookups.push_back({u"Extra", configuration.fields.size()});
    configuration.fields.push_back({8});
    auto msg = message->records()[0];
    msg.id = 1;
    msg.lookups.push_back({u"Extra", 1});
    msg.fields.push_back({8});
    const std::array records{configuration, msg, q::core::LocalRecord{2, u"Custom", false, {{8}}, {{u"Value", 0}}}};
    auto result =
        q::core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(result), "caller misc schema refused");

    return std::move(*result);
}

/** Encode an exact independent value with deliberately opaque bytes and actual FIELD metadata.
 * @param version Signed first physical int slot, unrelated to codec versions.
 * @param ordinal Source case identity retained in the opaque body and event timestamp.
 * @param versionType Exact declared Version identity, zero for absence.
 * @return Configuration source record with an extra generic field.
 */
peer::Record configuration(std::int32_t version, unsigned ordinal, int versionType) {
    std::vector<q::core::FieldValue> fields;

    if (versionType) {
        const auto raw =
            versionType == 104
                ? std::bit_cast<std::int64_t>((std::uint64_t{static_cast<std::uint32_t>(version)} << 32) | 123)
                : static_cast<std::int64_t>(version);
        fields.push_back({static_cast<q::core::SerialFieldType>(versionType), raw});
    }

    fields.push_back({q::core::SerialFieldType::SERIAL_OBJECT,
                      0,
                      {},
                      std::vector{std::byte{0xff}, static_cast<std::byte>(ordinal)}});
    fields.push_back({q::core::SerialFieldType::COMPACT_INT, static_cast<std::int64_t>(ordinal)});

    return {u"CONFIG", std::move(fields), 0, {}, 0, q::protocol::FieldEventTime{1600000000 + ordinal, 123LL << 22}};
}

/** Exercise typed/mixed public delivery, shared cache replay and independent explicit decode failures.
 * @param ticker Select TICKER rather than exact STREAM order.
 * @param versionType Version8, Version104 or absent0.
 * @param firstOnly Select one first-value boundary rather than a complete intra-batch version sequence.
 * @param seed Signed first version for the first-value boundary.
 */
void check(bool ticker, int versionType, bool firstOnly, std::int32_t seed = -7) {
    auto local = schema(versionType);
    const std::array routes{q::api::EventRegistration{q::api::EventType::CONFIGURATION, 0},
                            q::api::EventRegistration{q::api::EventType::MESSAGE, 1}};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(local, routes, {}, work);
    require(bool(registry), "typed misc registry refused");
    require(registry->versionField(0).has_value() == bool(versionType), "version selection not retained");
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.maximumSubscriptionsPerEndpoint = 5;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(runtimeOptions);
    q::api::EndpointOptions options;
    options.contract = ticker ? q::api::DeliveryContract::TICKER : q::api::DeliveryContract::STREAM;
    options.eventRegistry = *registry;
    options.maximumAttempts = 1;
    options.readTimeout = 10000ms;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();

    for (const auto unknown : {-1, std::numeric_limits<int>::max()}) {
        bool refused = false;
        try {
            static_cast<void>(feed->createSubscription({static_cast<q::api::EventType>(unknown)}));
        } catch (const q::api::Error &error) {
            refused = error.diagnostic().code == q::api::ErrorCode::INVALID_ARGUMENT;
        }
        require(refused, "unknown native event enum accepted");
    }

    if (ticker) {
        bool refused = false;
        try {
            static_cast<void>(feed->createSubscription<q::events::Message>());
        } catch (const q::api::Error &error) {
            refused = error.diagnostic().code == q::api::ErrorCode::UNSUPPORTED_PROFILE;
        }
        require(refused, "Message accepted outside its STREAM contract");
    }

    auto typed = feed->createSubscription<q::events::Configuration>();
    auto observed = std::make_shared<Observation<q::events::Configuration>>();
    auto emptyCodecs = q::events::AttachmentCodecRegistry::create({}, {0, 0, 4096});
    require(bool(emptyCodecs), "empty codec registry refused");
    typed->addEventListener([observed, codecs = *emptyCodecs](auto batch) {
        for (const auto &event : batch) {
            const auto result =
                codecs.template decode<std::string>(event.attachment, "unknown", 0, {4096, 4096, 4096, 4096});
            require(!result && result.error() == q::events::AttachmentError::UNKNOWN_CODEC,
                    "unknown codec did not return independent typed error");
        }

        observed->append(batch);
    });
    typed->setSymbols({"CONFIG"});
    auto mixed = feed->createSubscription({q::api::EventType::CONFIGURATION});
    auto mixedValues = std::make_shared<Observation<q::api::RegisteredEvent>>();
    mixed->addEventListener([mixedValues](auto batch) {
        mixedValues->append(batch);
    });
    mixed->setSymbols({"CONFIG"});
    const std::array versions{-7,
                              -8,
                              0,
                              5,
                              7,
                              6,
                              7,
                              std::numeric_limits<std::int32_t>::min(),
                              std::numeric_limits<std::int32_t>::max(),
                              std::numeric_limits<std::int32_t>::min()};
    std::vector<peer::Record> data;

    if (firstOnly) {
        data.push_back(configuration(seed, 0, versionType));
    } else {
        for (unsigned i = 0; i < versions.size(); ++i) {
            data.push_back(configuration(versions[i], i, versionType));
        }
    }

    peer::PeerScript script;
    script.family = ticker ? q::core::SubscriptionFamily::TICKER : q::core::SubscriptionFamily::STREAM;
    script.fieldMetadata = true;
    script.canonicalSymbols = true;
    script.describeAllRecords = true;
    script.waitForDataRelease = true;
    script.afterRemove = std::vector<peer::Record>{};
    const std::array symbols{std::u16string(u"CONFIG")};
    peer::Peer source(local, data, symbols, 7, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    source.releaseData();
    const auto expectedCount = ticker || firstOnly ? 1U : static_cast<unsigned>(versions.size());
    const auto ready = eventually([&] {
        return observed->count.load() >= expectedCount || source.failed() ||
               typed->getDiagnostic().code != q::api::ErrorCode::NONE;
    });

    if (!ready || typed->getDiagnostic().code != q::api::ErrorCode::NONE) {
        std::cerr << "DETAIL ticker=" << ticker << " versionType=" << versionType << " count=" << observed->count
                  << " typed=" << typed->getDiagnostic().stage << ':' << typed->getDiagnostic().detailCode
                  << " endpoint=" << endpoint->getDiagnostic().stage << ':' << endpoint->getDiagnostic().detailCode
                  << " adds=" << source.adds() << " peerFailed=" << source.failed() << '\n';
    }

    require(ready && !source.failed(), "misc typed delivery timed out or peer failed");
    require(eventually([&] {
                return mixedValues->count.load() >= expectedCount;
            }),
            "mixed delivery incomplete");
    require(typed->getDiagnostic().code == q::api::ErrorCode::NONE, "decode failure incorrectly failed feed");
    require(observed->count == expectedCount && mixedValues->count == expectedCount,
            "unexpected version delivery count");
    {
        std::lock_guard lock(observed->mutex);

        for (unsigned i = 0; i < expectedCount; ++i) {
            const auto index = ticker ? firstOnly ? 0U : versionType ? 8U : 9U : i;
            const auto version = versionType ? firstOnly ? seed : versions[index] : 0;
            const bool defaultValue = ticker && versionType && firstOnly && seed < 0;
            const auto &event = observed->values[i];
            require(event.symbol == u"CONFIG", "Configuration symbol differs");
            require(event.version == (defaultValue ? 0 : version), "Configuration signed version differs");
            require(event.eventTime == (defaultValue ? 0 : 1600000000123LL + index * 1000LL),
                    "Configuration event time differs");
            require(event.attachment.isNull() == defaultValue, "fresh default attachment nullness differs");

            if (!defaultValue) {
                const auto bytes = *event.attachment.bytes();
                require(bytes.size() == 2 && bytes[1] == static_cast<std::byte>(index),
                        "opaque attachment changed or old version won");
            }
        }
    }

    std::shared_ptr<q::api::Subscription<q::events::Configuration>> late;

    if (ticker) {
        late = feed->createSubscription<q::events::Configuration>();
        auto replay = std::make_shared<Observation<q::events::Configuration>>();
        late->addEventListener([replay](auto batch) {
            replay->append(batch);
        });
        late->setSymbols({"CONFIG"});
        require(eventually([&] {
                    return replay->count == 1 || late->getDiagnostic().code != q::api::ErrorCode::NONE;
                }),
                "late cache replay missing");
        std::scoped_lock lock(observed->mutex, replay->mutex);
        require(replay->values[0].version == observed->values.back().version &&
                    replay->values[0].attachment == observed->values.back().attachment &&
                    replay->values[0].eventTime == observed->values.back().eventTime,
                "late replay exposed rejected input");
    }

    require(endpoint->getDiagnostic().code == q::api::ErrorCode::NONE &&
                typed->getDiagnostic().code == q::api::ErrorCode::NONE &&
                mixed->getDiagnostic().code == q::api::ErrorCode::NONE,
            "ordinary Configuration owner failed before close");
    const auto closed = endpoint->closeAndAwaitTermination();
    verifyClose(endpoint, closed);
    runtime.closeAndAwaitTermination();
    require(observed->count == expectedCount, "owned observations invalidated after runtime close");
    std::cout << "PASS misc ticker=" << ticker << " versionType=" << versionType << " firstOnly=" << firstOnly
              << " seed=" << seed << '\n';
}

/** Exercise Message STREAM, mixed variants, callback close/throw isolation and capture retirement. */
void checkMessages(bool callbackFaults) {
    auto local = schema(8);
    const std::array routes{q::api::EventRegistration{q::api::EventType::MESSAGE, 1},
                            q::api::EventRegistration{q::api::EventType::CONFIGURATION, 0}};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(local, routes, {}, work);
    require(bool(registry), "Message registry refused");
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(runtimeOptions);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::STREAM;
    options.eventRegistry = *registry;
    options.maximumAttempts = 1;
    options.readTimeout = 10000ms;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto typed = feed->createSubscription<q::events::Message>();
    auto observed = std::make_shared<Observation<q::events::Message>>();
    auto capture = std::make_shared<int>(42);
    std::weak_ptr<int> lifetime = capture;
    typed->addEventListener([observed, capture](auto batch) {
        observed->append(batch);
    });
    capture.reset();
    require(!lifetime.expired(), "Message listener capture was not retained");
    typed->setSymbols({"MSG"});
    std::shared_ptr<q::api::Subscription<q::events::Message>> throwing;

    if (callbackFaults) {
        throwing = feed->createSubscription<q::events::Message>();
        throwing->addEventListener([](auto) {
            throw std::runtime_error("deliberate Message callback fault");
        });
        throwing->setSymbols({"MSG"});
    }

    const auto make = [](std::optional<std::vector<std::byte>> bytes, unsigned ordinal) {
        return peer::Record{u"MSG",
                            {{q::core::SerialFieldType::SERIAL_OBJECT, 0, {}, std::move(bytes)},
                             {q::core::SerialFieldType::COMPACT_INT, ordinal}},
                            0,
                            {},
                            1,
                            q::protocol::FieldEventTime{1600000000 + ordinal, 123LL << 22}};
    };
    const std::vector<std::byte> javaString{std::byte{0xac}, std::byte{0xed}, std::byte{0},   std::byte{5},
                                            std::byte{0x74}, std::byte{0},    std::byte{5},   std::byte{'h'},
                                            std::byte{'e'},  std::byte{'l'},  std::byte{'l'}, std::byte{'o'}};
    const std::array initial{make({}, 0), make(std::vector<std::byte>{}, 1), make(javaString, 2)};
    auto config = configuration(7, 3, 8);
    config.symbol = u"MSG";
    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::STREAM;
    script.subscriptionRecordId = 1;
    script.describeAllRecords = true;
    script.canonicalSymbols = true;
    script.fieldMetadata = true;
    script.waitForDataRelease = true;
    script.followupAdd = peer::ExpectedSubscription{{u"MSG"}, {0}, false};
    script.afterFollowupAdd = std::vector{make(std::vector{std::byte{0xff}}, 4), config};
    // Callback-close retires only Configuration; the independent typed Message keeps its physical interest.
    script.removeSequence = callbackFaults ? std::vector{peer::ExpectedSubscription{{u"MSG"}, {0}, false},
                                                         peer::ExpectedSubscription{{u"MSG"}, {1}, false}}
                                           : std::vector{peer::ExpectedSubscription{{u"MSG", u"MSG"}, {0, 1}, true}};
    script.afterRemove = std::vector<peer::Record>{};
    const std::array symbols{std::u16string(u"MSG")};
    peer::Peer source(local, initial, symbols, 7, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    source.releaseData();

    if (!eventually([&] {
            return observed->count == 3 || source.failed() || typed->getDiagnostic().code != q::api::ErrorCode::NONE;
        })) {
        std::cerr << "Message wait: adds=" << source.adds() << " sent=" << source.initialDataSent() << '\n';
    }

    if (typed->getDiagnostic().code != q::api::ErrorCode::NONE) {
        std::cerr << "Message diagnostic=" << typed->getDiagnostic().stage << ':' << typed->getDiagnostic().detailCode
                  << '\n';
    }

    require(eventually([&] {
                return observed->count == 3 || source.failed() ||
                       typed->getDiagnostic().code != q::api::ErrorCode::NONE;
            }),
            "Message initial delivery missing");
    require(!source.failed() && observed->count == 3, "Message source failed or record count differs");

    if (throwing) {
        require(eventually([&] {
                    return throwing->getDiagnostic().code == q::api::ErrorCode::CALLBACK_FAILURE;
                }),
                "Message callback throw was not isolated");
    }

    auto mixed = feed->createSubscription({q::api::EventType::MESSAGE, q::api::EventType::CONFIGURATION});
    std::weak_ptr<q::api::Subscription<q::api::RegisteredEvent>> weakMixed = mixed;
    auto mixedValues = std::make_shared<Observation<q::api::RegisteredEvent>>();
    mixed->addEventListener([mixedValues, weakMixed, callbackFaults](auto batch) {
        mixedValues->append(batch);

        if (callbackFaults && mixedValues->count >= 2) {
            if (auto subscription = weakMixed.lock()) {
                subscription->close();
            }
        }
    });
    mixed->setSymbols({"MSG"});
    require(eventually([&] {
                return (observed->count == 4 && mixedValues->count == 2) || source.failed();
            }),
            "mixed Message/Configuration followup missing");
    require(!source.failed() && typed->getDiagnostic().code == q::api::ErrorCode::NONE,
            "unrelated Message subscription failed");
    {
        std::lock_guard lock(observed->mutex);
        require(observed->values[0].attachment.isNull(), "Message null became empty");
        require(!observed->values[1].attachment.isNull() && observed->values[1].attachment.bytes()->empty(),
                "Message empty became null");
        require(std::ranges::equal(*observed->values[2].attachment.bytes(), javaString),
                "serialized Java String changed");
        require(observed->values[3].attachment.bytes()->front() == std::byte{0xff},
                "opaque malformed Message was decoded/rejected");

        for (unsigned i = 0; i < observed->values.size(); ++i) {
            const auto ordinal = i == 3 ? 4 : i;
            require(observed->values[i].symbol == u"MSG" &&
                        observed->values[i].eventTime == 1600000000123LL + ordinal * 1000LL,
                    "Message symbol/event time differs");
        }
    }
    {
        std::lock_guard lock(mixedValues->mutex);
        require(std::holds_alternative<q::events::Message>(mixedValues->values[0]) &&
                    std::holds_alternative<q::events::Configuration>(mixedValues->values[1]),
                "mixed variants/order differ");
    }
    require(endpoint->getDiagnostic().code == q::api::ErrorCode::NONE &&
                typed->getDiagnostic().code == q::api::ErrorCode::NONE,
            "callback ABORT/throw escaped into a healthy owner before endpoint close");

    if (callbackFaults) {
        require(eventually([&] {
                    return source.removes() >= 1 || source.failed();
                }) &&
                    !source.failed(),
                "callback-close did not emit the exact independent Configuration REMOVE");
    }

    const auto closed = endpoint->closeAndAwaitTermination();
    verifyClose(endpoint, closed);
    runtime.closeAndAwaitTermination();
    require(lifetime.expired(), "Message callback captures survived actual endpoint/runtime retirement");
    require(observed->count == 4, "owning Message values invalidated by shutdown");
    std::cout << "PASS Message STREAM mixed capture-retirement\n";
}
} // namespace

int main() {
    try {
        checkMessages(false);
        checkMessages(true);

        for (const auto type : {8, 104, 0}) {
            check(false, type, false);
            check(true, type, false);
        }

        for (const auto version :
             {-7, 0, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            check(true, 8, true, version);
        }

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
