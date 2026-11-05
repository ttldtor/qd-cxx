// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace q = org::ttldtor::qdcxx;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

template <typename Event>
void check(bool configuration, bool ticker = false, bool equalUpdate = false, bool versionEnabled = true,
           bool firstNegative = false) {
    constexpr q::core::SchemaLimits limits{16, 128, 256, 128, 16384};
    auto schema =
        configuration ? q::events::defaultTextConfigurationSchema(limits) : q::events::defaultTextMessageSchema(limits);
    require(bool(schema), "schema");

    if (configuration && !versionEnabled) {
        auto record = schema->records()[0];

        for (auto &lookup : record.lookups) {
            if (lookup.fieldId == 2)
                lookup.name = lookup.name == u"Version" ? u"ExtraVersion" : u"TextConfiguration.ExtraVersion";
        }

        schema = q::core::LocalSchema::create(std::span(&record, 1), limits, q::core::LocalProfile::BINARY_FIELDS_V1);
        require(bool(schema), "schema without selected Version");
    }

    const q::api::EventRegistration registration{
        configuration ? q::api::EventType::TEXT_CONFIGURATION : q::api::EventType::TEXT_MESSAGE, 0};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*schema, std::span(&registration, 1), {}, work);
    require(bool(registry), "registry");
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(runtimeOptions);
    q::api::EndpointOptions options;
    options.contract = ticker ? q::api::DeliveryContract::TICKER : q::api::DeliveryContract::STREAM;
    options.eventRegistry = *registry;
    options.readTimeout = 5000ms;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription<Event>();
    std::mutex mutex;
    std::vector<Event> received;
    std::atomic<bool> done{false};
    subscription->addEventListener([&](std::span<const Event> batch) {
        std::lock_guard lock(mutex);
        received.insert(received.end(), batch.begin(), batch.end());
        done.store(true, std::memory_order_release);
    });
    subscription->addSymbols({"A&N&Q"});
    std::vector<q::core::FieldValue> fields;

    for (const auto &field : schema->records()[0].fields) {
        fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
    }

    fields[0].raw = -1;
    fields[1].raw = (123 << 22) | 9;

    if (configuration)
        fields[2].raw = ticker ? (firstNegative ? -7 : 3) : -7;

    fields.back().units = std::u16string{u'A', u'\0', char16_t{0xd800}};
    std::vector<peer::Record> records;

    if (ticker && !firstNegative) {
        auto lower = fields;
        lower[2].raw = 1;
        lower.back().units = versionEnabled
                                 ? std::optional<std::u16string>(u"older")
                                 : std::optional<std::u16string>(std::u16string{u'A', u'\0', char16_t{0xd800}});
        auto equal = fields;
        equal.back().units = std::u16string{u'A', u'\0', char16_t{0xd800}};
        fields.back().units = equalUpdate
                                  ? std::optional<std::u16string>(u"first")
                                  : std::optional<std::u16string>(std::u16string{u'A', u'\0', char16_t{0xd800}});
        records.push_back({u"A&N&Q", std::move(fields)});
        records.push_back({u"A&N&Q", std::move(lower)});

        if (equalUpdate)
            records.push_back({u"A&N&Q", std::move(equal)});
    } else {
        records.push_back({u"A&N&Q", std::move(fields)});
    }

    const std::array<std::u16string, 1> expected{u"A&N&Q"};
    peer::PeerScript script;
    script.family = ticker ? q::core::SubscriptionFamily::TICKER : q::core::SubscriptionFamily::STREAM;
    script.canonicalSymbols = ticker;

    if (firstNegative) {
        script.fieldMetadata = true;

        for (auto &record : records)
            record.fieldTime = q::protocol::FieldEventTime{12, 0, 0};
    }

    peer::Peer source(*schema, records, expected, 1, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    const auto deadline = std::chrono::steady_clock::now() + 10s;

    while (!done.load(std::memory_order_acquire) && !source.failed() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }

    require(done.load(std::memory_order_acquire) && !source.failed(), "delivery timeout or peer failure");
    std::atomic<bool> replayed{false};
    std::optional<Event> replay;
    std::mutex replayMutex;
    std::shared_ptr<q::api::Subscription<Event>> late;

    if (ticker) {
        late = endpoint->getFeed()->createSubscription<Event>();
        late->addEventListener([&](std::span<const Event> batch) {
            std::lock_guard lock(replayMutex);

            if (!batch.empty())
                replay = batch.front();

            replayed.store(true, std::memory_order_release);
        });
        late->addSymbols({"A&N&Q"});
        const auto replayDeadline = std::chrono::steady_clock::now() + 5s;

        while (!replayed.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < replayDeadline) {
            std::this_thread::sleep_for(1ms);
        }

        require(replayed.load(std::memory_order_acquire), "cached replay timeout");
    }

    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();
    require(!closed.deadlineExpired, "close deadline");
    std::lock_guard lock(mutex);
    require(received.size() == 1, "count");
    require(received[0].symbol == u"A&N&Q", "nonmarket symbol");

    if (firstNegative)
        require(received[0].eventTime == 0, "default FIELD event time");

    require(received[0].getTime() == (firstNegative ? 0 : -877) && received[0].getSequence() == (firstNegative ? 0 : 9),
            "time and sequence");

    if (firstNegative)
        require(!received[0].text, "negative first produces default text");
    else
        require(received[0].text && *received[0].text == std::u16string({u'A', u'\0', char16_t{0xd800}}), "owned text");

    if constexpr (std::is_same_v<Event, q::events::TextConfiguration>) {
        require(received[0].version == (firstNegative ? 0 : (versionEnabled ? (ticker ? 3 : -7) : 0)), "version");

        if (ticker) {
            std::lock_guard replayLock(replayMutex);
            require(replay && replay->version == (firstNegative ? 0 : (versionEnabled ? 3 : 0)) &&
                        replay->text == received[0].text,
                    "cached replay");
        }
    }
}

void checkTextMessageRejectsTicker() {
    q::api::RuntimeOptions runtimeOptions;
    q::api::ApiRuntime runtime(runtimeOptions);
    auto endpoint = runtime.createEndpoint();
    bool rejected = false;
    try {
        static_cast<void>(endpoint->getFeed()->createSubscription<q::events::TextMessage>());
    } catch (const q::api::Error &error) {
        rejected = error.diagnostic().code == q::api::ErrorCode::UNSUPPORTED_PROFILE;
    }
    require(rejected, "TextMessage TICKER must reject");
    static_cast<void>(endpoint->closeAndAwaitTermination());
    runtime.closeAndAwaitTermination();
}

int main() {
    try {
        checkTextMessageRejectsTicker();
        check<q::events::TextMessage>(false);
        check<q::events::TextConfiguration>(true);
        check<q::events::TextConfiguration>(true, true);
        check<q::events::TextConfiguration>(true, true, true);
        check<q::events::TextConfiguration>(true, true, false, false);
        check<q::events::TextConfiguration>(true, true, false, true, true);

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
