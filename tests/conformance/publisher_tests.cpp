// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/publisher.hpp"

#include "connect_peer.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/order_source.hpp"
#include "qd/protocol/incoming_subscription.hpp"
#include "qd/protocol/live_session.hpp"
#include "qd/transport/tcp_runtime.hpp"
#include "recovery_test_peer.hpp"

#include <array>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <condition_variable>
#include <doctest/doctest.h>
#include <limits>
#include <mutex>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
using asio::ip::tcp;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr q::core::SchemaLimits QUOTE_SCHEMA_LIMITS{1, 16, 48, 64, 1024};
constexpr std::array<q::protocol::LiveMessageView, 2> QUOTE_SEND{
    {{11, u"TICKER_ADD_SUBSCRIPTION", {}}, {12, u"TICKER_REMOVE_SUBSCRIPTION", {}}}};
constexpr std::array<q::protocol::LiveMessageView, 1> QUOTE_RECEIVE{{{10, u"TICKER_DATA", {}}}};

/** Create a receiver that preserves the two optional fields declared on the publisher's wire. */
q::api::EventRegistry quoteRegistry() {
    constexpr auto PROFILE = q::core::LocalProfile::BINARY_FIELDS_V1;
    const auto defaults = q::events::defaultQuoteSchema(QUOTE_SCHEMA_LIMITS);
    REQUIRE(defaults);
    auto record = defaults->records().front();
    record.lookups.push_back({u"Sequence", record.fields.size()});
    record.fields.push_back({std::to_underlying(q::core::SerialFieldType::SEQUENCE)});
    record.lookups.push_back({u"TimeNanoPart", record.fields.size()});
    record.fields.push_back({std::to_underlying(q::core::SerialFieldType::COMPACT_INT)});
    const auto schema = q::core::LocalSchema::create(std::span(&record, 1), QUOTE_SCHEMA_LIMITS, PROFILE);
    REQUIRE(schema);
    q::api::EventRegistryWork work;
    constexpr q::api::EventRegistration route{q::api::EventType::QUOTE, 0};
    auto registry = q::api::EventRegistry::create(*schema, std::span(&route, 1), {}, work);
    REQUIRE(registry);

    return std::move(*registry);
}

/** Source publication with negative-independent distinct bid/ask business time and exact price bits. */
q::events::Quote makeQuote(std::int64_t value = 100) {
    q::events::Quote quote{};
    quote.symbol = u"IBM";
    quote.bidTimeMs = 3000;
    quote.askTimeMs = 4000;
    quote.timeMs = 4321;
    quote.bidExchangeCode = u'Q';
    quote.askExchangeCode = u'N';
    quote.bidPrice.raw = (value << 8) | 128;
    quote.askPrice.raw = ((value + 1) << 8) | 128;
    quote.bidSize.raw = (10 << 8) | 128;
    quote.askSize.raw = (12 << 8) | 128;
    quote.sequence = 17;
    quote.timeNanoPart = -300;

    return quote;
}

/** Thread-safe receiver observation retained by each callback capture. */
struct Observation {
    std::mutex mutex;
    std::condition_variable ready;
    std::optional<q::events::Quote> latest;

    void capture(std::span<const q::events::Quote> batch) {
        const std::lock_guard lock(mutex);

        if (!batch.empty()) {
            latest = batch.back();
            ready.notify_all();
        }
    }

    std::optional<q::events::Quote> wait(std::int64_t price) {
        std::unique_lock lock(mutex);

        if (!ready.wait_for(lock, 5s, [&] {
                return latest && latest->bidPrice.raw == price;
            }))
            return {};

        return latest;
    }
};

struct Client {
    std::shared_ptr<Observation> events = std::make_shared<Observation>();
    std::shared_ptr<q::api::Endpoint> endpoint;
    std::shared_ptr<q::api::Subscription<q::events::Quote>> subscription;

    Client(q::api::ApiRuntime &runtime, std::uint16_t port, const q::api::EventRegistry *registry = nullptr) {
        q::api::EndpointOptions options;
        options.maximumAttempts = 1;

        if (registry)
            options.eventRegistry = *registry;

        endpoint = runtime.createEndpoint(options);
        subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addEventListener([events = events](std::span<const q::events::Quote> batch) {
            events->capture(batch);
        });
        subscription->addSymbols("IBM");
        endpoint->connect("127.0.0.1:" + std::to_string(port));
    }
};

void requireServer(const std::expected<std::unique_ptr<q::api::PublisherEndpoint>, q::api::Diagnostic> &server) {
    if (!server) {
        INFO(server.error().stage, " category ", static_cast<int>(server.error().code), " detail ",
             server.error().detailCode, " native ", server.error().nativeCode);
        REQUIRE(server);
    }
}

/** One ordinary peer that sends only its own Quote definition and never reads server output. */
class NonReadingPeer {
    asio::io_context context_;
    tcp::socket socket_{context_};

    void send(std::expected<q::protocol::LivePendingOutput, q::protocol::LiveError> output) {
        REQUIRE(output);
        asio::error_code error;
        auto remaining = output->pendingBytes();

        while (!remaining.empty()) {
            const auto count = socket_.write_some(asio::buffer(remaining.data(), remaining.size()), error);
            REQUIRE_FALSE(error);
            REQUIRE(count > 0);
            remaining = remaining.subspan(count);
        }
    }

    public:
    explicit NonReadingPeer(std::uint16_t port) {
        namespace p = q::protocol;
        auto schema = q::events::defaultQuoteSchema(QUOTE_SCHEMA_LIMITS);
        REQUIRE(schema);
        constexpr std::int32_t SENDER_ID = 81;
        p::OutgoingRecord layout{SENDER_ID, 0, u"Quote", {}};

        for (std::size_t i = 0; i < q::events::QUOTE_FIELD_NAMES.size(); ++i) {
            const auto name = q::events::QUOTE_FIELD_NAMES[i];
            layout.fields.push_back({std::u16string(name.begin(), name.end()),
                                     std::to_underlying(q::events::QUOTE_FIELD_TYPES[i]),
                                     static_cast<std::int32_t>(i)});
        }

        p::SubscriptionWriterWork writerWork{100, 8192, 8192, 65536};
        const p::LayoutLimits layoutLimits{QUOTE_SCHEMA_LIMITS, 1, 16, 64, 1024};
        auto writer = p::SubscriptionWriter::create(*schema, std::span(&layout, 1), layoutLimits,
                                                    {8192, 8201, 128, 256, {8192, 1024}, 65536, 65536, 4}, writerWork);
        REQUIRE(writer);
        const q::core::SubscriptionRequest request{{schema->recordContext(0), u"SLOW"}, {}};
        auto add = writer->compose(p::SubscriptionMessage::TICKER_ADD, std::span(&request, 1), writerWork);
        REQUIRE(add);
        const std::vector<std::byte> addBytes(add->bytes().begin(), add->bytes().end());
        add = std::unexpected(p::SubscriptionWriterError::INVALID_STATE);

        p::LiveLimits limits{};
        limits.frame = {8192, 8201};
        limits.protocol = {64, 16, {8192, 1024}, 8192};
        limits.descriptions = {8192, 16, 16, 128, {8192, 1024}, 8192};
        limits.context = {QUOTE_SCHEMA_LIMITS, 16, {128, 1024, 8192}};
        limits.residentBytes = 1024 * 1024;
        limits.owners = 8;
        limits.snapshotBytes = 65536;
        limits.scratchBytes = 65536;
        limits.eventBytes = 8192;
        limits.outputBytes = 8192;
        p::LiveWork work{100, 65536, 65536, 1024 * 1024};
        auto session = p::LiveSession::create(std::move(*schema), std::move(*writer), limits, work);
        REQUIRE(session);

        asio::error_code error;
        socket_.connect({asio::ip::address_v4::loopback(), port}, error);
        REQUIRE_FALSE(error);
        send(session->composeProtocol({{}, QUOTE_SEND, QUOTE_RECEIVE}, work));
        send(session->describe(std::span(&SENDER_ID, 1), work));
        const auto written = asio::write(socket_, asio::buffer(addBytes.data(), addBytes.size()), error);
        REQUIRE_FALSE(error);
        REQUIRE(written == addBytes.size());
    }
};

} // namespace

TEST_CASE("publisher serves two typed TCP Feed connections with a late TICKER value and local admission") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto server = q::api::PublisherEndpoint::create(*bootstrap);
    requireServer(server);
    const auto port = (*server)->port();
    REQUIRE(port != 0);
    const auto publisher = (*server)->getPublisher();
    q::api::ApiRuntime runtime(*bootstrap);
    const auto registry = quoteRegistry();
    Client first(runtime, port, &registry);
    REQUIRE((*server)->awaitDemand(u"IBM", true, Clock::now() + 5s));

    auto value = makeQuote();
    const auto earlyPrice = value.bidPrice.raw;
    REQUIRE(publisher.publish(value));
    value.symbol[0] = u'X';
    const auto received = first.events->wait(earlyPrice);
    REQUIRE(received);
    CHECK(received->symbol == u"IBM");
    CHECK(received->bidTimeMs == 3000);
    CHECK(received->askTimeMs == 4000);
    CHECK(received->timeMs == 4321);
    CHECK(received->sequence == 17);
    CHECK(received->timeNanoPart == -300);
    CHECK(received->bidExchangeCode == u'Q');
    CHECK(received->askExchangeCode == u'N');

    Client late(runtime, port); // The ordinary built-in catalog receives this exact composite record too.
    REQUIRE(late.events->wait(earlyPrice));
    const auto next = makeQuote(200);
    REQUIRE(publisher.publish(next));
    REQUIRE(first.events->wait(next.bidPrice.raw));
    REQUIRE(late.events->wait(next.bidPrice.raw));

    (void)first.endpoint->closeAndAwaitTermination();
    CHECK((*server)->awaitDemand(u"IBM", true, Clock::now() + 5s));
    (void)late.endpoint->closeAndAwaitTermination();
    CHECK((*server)->awaitDemand(u"IBM", false, Clock::now() + 5s));

    auto wrong = makeQuote();
    wrong.timeMs = (std::numeric_limits<std::int64_t>::min)();
    const auto refused = publisher.publish(wrong);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code == q::api::ErrorCode::INVALID_ARGUMENT);
    (*server)->closeAndAwaitTermination();
    CHECK((*server)->getDiagnostic().code == q::api::ErrorCode::NONE);
    server->reset();
    const auto closed = publisher.publish(next);
    REQUIRE_FALSE(closed);
    CHECK(closed.error().code == q::api::ErrorCode::CLOSED);
}

TEST_CASE("non-reading peer with a different record ID does not block a typed client") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    q::api::PublisherOptions options;
    options.listener.maxWriteBytes = 3;
    auto server = q::api::PublisherEndpoint::create(*bootstrap, options);
    requireServer(server);
    const auto publisher = (*server)->getPublisher();
    NonReadingPeer slow((*server)->port());
    REQUIRE((*server)->awaitDemand(u"SLOW", true, Clock::now() + 5s));
    q::api::ApiRuntime runtime(*bootstrap);
    Client fast(runtime, (*server)->port());
    REQUIRE((*server)->awaitDemand(u"IBM", true, Clock::now() + 5s));

    for (std::int64_t i = 0; i < 32; ++i) {
        auto value = makeQuote(i);
        value.symbol = u"SLOW";
        REQUIRE(publisher.publish(value));
    }

    const auto target = makeQuote(300);
    REQUIRE(publisher.publish(target));
    REQUIRE(fast.events->wait(target.bidPrice.raw));
    CHECK((*server)->awaitDemand(u"SLOW", true, Clock::now() + 5s));
}

TEST_CASE("remote QTP publisher reconciles overlapping demand across source generations") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = q::events::defaultQuoteSchema(QUOTE_SCHEMA_LIMITS);
    REQUIRE(schema);
    constexpr auto family = q::core::SubscriptionFamily::TICKER;
    constexpr auto add = std::to_underlying(q::protocol::SubscriptionMessage::TICKER_ADD);
    constexpr auto remove = std::to_underlying(q::protocol::SubscriptionMessage::TICKER_REMOVE);
    const auto initial = makeQuote(100);
    const auto current = makeQuote(300);
    const auto makeData = [&](const q::events::Quote &quote) {
        const std::array<std::int64_t, 8> raw{quote.bidTimeMs / 1000, quote.bidExchangeCode,  quote.bidPrice.raw,
                                              quote.bidSize.raw,      quote.askTimeMs / 1000, quote.askExchangeCode,
                                              quote.askPrice.raw,     quote.askSize.raw};
        q::connect_tests::Record record{u"IBM", {}};

        for (std::size_t i = 0; i < raw.size(); ++i)
            record.fields.push_back({q::events::QUOTE_FIELD_TYPES[i], raw[i]});

        return q::connect_tests::fixturePeerData(std::span(&record, 1), family);
    };
    q::test::TcpPeerConfig script;
    script.lifetime = 20s;
    script.sessions.resize(2);

    for (auto &connection : script.sessions) {
        connection.initialBytes = q::connect_tests::fixturePeerPrefix(*schema, family);
        connection.readThroughMessage = add;
    }

    script.sessions[0].replyBytes = makeData(initial);
    script.sessions[0].stagedBytes.push_back(makeData(makeQuote(200)));
    script.sessions[0].waitForControllerClose = true;
    script.sessions[1].replyBytes = makeData(current);
    script.sessions[1].waitForClientClose = true;
    q::test::OwnedTcpPeer source(std::move(script));
    REQUIRE(source.port() != 0);
    q::api::PublisherOptions relayOptions;
    relayOptions.source = q::api::PublisherSourceOptions{"127.0.0.1", source.port(), 20ms};
    auto relay = q::api::PublisherEndpoint::create(*bootstrap, relayOptions);
    requireServer(relay);
    q::api::ApiRuntime runtime(*bootstrap);
    Client first(runtime, (*relay)->port());
    REQUIRE((*relay)->awaitSourceGeneration(0, Clock::now() + 5s));
    const auto initialGeneration = (*relay)->getSourceStatus().generation;
    REQUIRE(initialGeneration > 0);
    REQUIRE(first.events->wait(initial.bidPrice.raw));
    Client second(runtime, (*relay)->port());
    REQUIRE(second.events->wait(initial.bidPrice.raw));

    (void)first.endpoint->closeAndAwaitTermination();
    source.allowStages(0, 1);
    REQUIRE(second.events->wait(makeQuote(200).bidPrice.raw));
    source.allowClose(0);
    REQUIRE((*relay)->awaitSourceGeneration(initialGeneration, Clock::now() + 5s));
    CHECK((*relay)->getSourceStatus().generation > initialGeneration);
    REQUIRE(second.events->wait(current.bidPrice.raw));

    (void)second.endpoint->closeAndAwaitTermination();
    const auto countDemand = [&](std::span<const std::byte> input, std::int32_t type) {
        std::size_t count = 0;
        q::protocol::FrameReader frames({8192, 8201});
        const std::array mapping{q::protocol::IncomingSubscriptionRecord{0, 0}};
        const q::protocol::IncomingSubscriptionLimits limits{8192, 1, 1, 32, 256, 8192};
        q::protocol::IncomingSubscriptionWork work{32, 65536, 65536, 65536};

        while (!input.empty()) {
            const auto framed = frames.feed(input);
            REQUIRE(framed.status != q::protocol::FrameStatus::FAILED);
            REQUIRE(framed.accepted > 0);
            input = input.subspan(framed.accepted);

            if (framed.frame && framed.frame->type == type) {
                const auto batch =
                    q::protocol::decodeIncomingSubscription(*framed.frame, *schema, mapping, limits, work);
                REQUIRE(batch);

                for (const auto &entry : batch->entries) {
                    CHECK(entry.symbol == u"IBM");
                    ++count;
                }
            }
        }

        return count;
    };
    const auto deadline = Clock::now() + 5s;

    while (Clock::now() < deadline && countDemand(source.snapshot().sessions[1].received, remove) == 0)
        std::this_thread::sleep_for(1ms);

    const auto captured = source.snapshot();
    CHECK(captured.error == q::test::TcpPeerError::NONE);
    CHECK(countDemand(captured.sessions[0].received, add) == 1);
    CHECK(countDemand(captured.sessions[1].received, add) == 1);
    CHECK(countDemand(captured.sessions[1].received, remove) == 1);

    for (const auto &connection : captured.sessions) {
        auto bytes = std::span(connection.received);
        q::protocol::FrameReader frames({8192, 8201});
        bool described = false;

        while (!bytes.empty()) {
            const auto framed = frames.feed(bytes);
            REQUIRE(framed.status != q::protocol::FrameStatus::FAILED);
            REQUIRE(framed.accepted > 0);
            bytes = bytes.subspan(framed.accepted);

            if (framed.frame && framed.frame->type == 2)
                described = true;

            if (framed.frame && framed.frame->type == add)
                CHECK(described);
        }
    }

    (*relay)->closeAndAwaitTermination();
    CHECK((*relay)->getDiagnostic().code == q::api::ErrorCode::NONE);
}

TEST_CASE("mixed publisher preserves STREAM occurrences and indexed HISTORY on one connection") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    q::api::PublisherOptions serverOptions;
    serverOptions.mixed = true;
    serverOptions.listener.maxWriteBytes = 11;
    auto source = q::api::PublisherEndpoint::create(*bootstrap, serverOptions);
    requireServer(source);
    serverOptions.source = q::api::PublisherSourceOptions{"127.0.0.1", (*source)->port(), 20ms};
    auto server = q::api::PublisherEndpoint::create(*bootstrap, serverOptions);
    requireServer(server);
    q::api::ApiRuntime runtime(*bootstrap);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::MIXED;
    options.maximumAttempts = 1;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription(
        {q::api::EventType::QUOTE, q::api::EventType::TIME_AND_SALE, q::api::EventType::ORDER});

    struct Events {
        std::mutex mutex;
        std::condition_variable ready;
        std::vector<q::api::RegisteredEvent> values;
    };

    auto events = std::make_shared<Events>();
    subscription->addEventListener([events](std::span<const q::api::RegisteredEvent> batch) {
        const std::lock_guard lock(events->mutex);
        events->values.insert(events->values.end(), batch.begin(), batch.end());
        events->ready.notify_all();
    });
    subscription->addSymbols("IBM");
    endpoint->connect("127.0.0.1:" + std::to_string((*server)->port()));

    auto second = runtime.createEndpoint(options);
    auto secondSubscription = second->getFeed()->createSubscription(
        {q::api::EventType::QUOTE, q::api::EventType::TIME_AND_SALE, q::api::EventType::ORDER});
    auto secondEvents = std::make_shared<Events>();
    secondSubscription->addEventListener([secondEvents](std::span<const q::api::RegisteredEvent> batch) {
        const std::lock_guard lock(secondEvents->mutex);
        secondEvents->values.insert(secondEvents->values.end(), batch.begin(), batch.end());
        secondEvents->ready.notify_all();
    });
    secondSubscription->addSymbols({"IBM", "MSFT"});

    for (const auto family : {q::core::SubscriptionFamily::TICKER, q::core::SubscriptionFamily::STREAM,
                              q::core::SubscriptionFamily::HISTORY}) {
        INFO("contract ", static_cast<int>(family));
        const auto observed = (*server)->awaitDemand(family, u"IBM", true, Clock::now() + 5s);
        const auto serverDiagnostic = (*server)->getDiagnostic();
        const auto endpointDiagnostic = endpoint->getDiagnostic();
        INFO("publisher ", serverDiagnostic.stage, " category ", static_cast<int>(serverDiagnostic.code), " detail ",
             serverDiagnostic.detailCode, " feed ", endpointDiagnostic.stage, " category ",
             static_cast<int>(endpointDiagnostic.code), " detail ", endpointDiagnostic.detailCode);
        REQUIRE(observed);
        REQUIRE((*source)->awaitDemand(family, u"IBM", true, Clock::now() + 5s));
    }

    second->connect("127.0.0.1:" + std::to_string((*server)->port()));

    for (const auto family : {q::core::SubscriptionFamily::TICKER, q::core::SubscriptionFamily::STREAM,
                              q::core::SubscriptionFamily::HISTORY}) {
        REQUIRE((*source)->awaitDemand(family, u"MSFT", true, Clock::now() + 5s));
    }

    auto publisher = (*source)->getPublisher();
    REQUIRE(publisher.publish(makeQuote()));
    q::events::TimeAndSale sale;
    sale.symbol = u"IBM";
    sale.index = (std::int64_t{7} << 32) + 42;
    sale.exchangeCode = u'Q';
    sale.price.raw = (103 << 8) | 128;
    sale.size.raw = (4 << 8) | 128;
    REQUIRE(publisher.publish(sale));
    REQUIRE(publisher.publish(sale));

    namespace flags = q::events::indexed_event_flags;
    q::events::Order order;
    order.symbol = u"IBM";
    order.index = 7;
    order.price.raw = (105 << 8) | 128;
    order.size.raw = (4 << 8) | 128;
    order.flags = 4; // BUY in the native Order wire flag word.
    order.eventFlags = flags::SNAPSHOT_BEGIN | flags::TX_PENDING;
    REQUIRE(publisher.publish(order));
    order.index = 0;
    order.eventFlags = flags::SNAPSHOT_END | flags::REMOVE_EVENT;
    REQUIRE(publisher.publish(order));

    const auto defaultOrder = [](const q::api::RegisteredEvent &event) {
        const auto *order = std::get_if<q::events::Order>(&event);

        return order && order->getSourceId() == q::events::OrderSource::DEFAULT.id() ? order : nullptr;
    };
    const auto counts = [&] {
        std::array<unsigned, 3> result{};

        for (const auto &event : events->values) {
            result[0] += std::holds_alternative<q::events::Quote>(event);
            result[1] += std::holds_alternative<q::events::TimeAndSale>(event);
            result[2] += defaultOrder(event) != nullptr;
        }

        return result;
    };
    {
        std::unique_lock lock(events->mutex);
        const auto complete = events->ready.wait_for(lock, 5s, [&] {
            const auto observed = counts();

            return observed[0] >= 1 && observed[1] >= 2 && observed[2] >= 2;
        });
        const auto observed = counts();
        const auto diagnostic = (*server)->getDiagnostic();
        INFO("counts ", observed[0], " ", observed[1], " ", observed[2], " publisher ", diagnostic.stage, " category ",
             static_cast<int>(diagnostic.code), " detail ", diagnostic.detailCode);
        REQUIRE(complete);
        CHECK(counts() == std::array<unsigned, 3>{1, 2, 2});
        std::vector<q::events::Order> orders;

        for (const auto &event : events->values) {
            if (const auto *observed = std::get_if<q::events::TimeAndSale>(&event)) {
                CHECK(observed->getIndex() == sale.index);
                CHECK(observed->price.raw == sale.price.raw);
            }

            if (const auto *observed = defaultOrder(event))
                orders.push_back(*observed);
        }

        REQUIRE(orders.size() == 2);
        CHECK(orders[0].index == 7);
        CHECK((orders[0].eventFlags & flags::SNAPSHOT_BEGIN) != 0);
        CHECK(orders[1].index == 0);
        CHECK((orders[1].eventFlags & flags::SNAPSHOT_END) != 0);
    }
    CHECK(endpoint->getStatistics().connection.establishedConnections.value == 1);
    {
        std::unique_lock lock(secondEvents->mutex);
        REQUIRE(secondEvents->ready.wait_for(lock, 5s, [&] {
            return std::ranges::count_if(secondEvents->values,
                                         [](const auto &event) {
                                             return std::holds_alternative<q::events::TimeAndSale>(event);
                                         }) == 2 &&
                   std::ranges::count_if(secondEvents->values, [&](const auto &event) {
                       return defaultOrder(event) != nullptr;
                   }) == 2;
        }));
    }
    order.index = 7;
    order.eventFlags = flags::REMOVE_EVENT;
    REQUIRE(publisher.publish(order));
    {
        std::unique_lock lock(events->mutex);
        REQUIRE(events->ready.wait_for(lock, 5s, [&] {
            return counts()[2] >= 3;
        }));
        const auto *removed = defaultOrder(events->values.back());
        REQUIRE(removed);
        CHECK(removed->index == 7);
        CHECK((removed->eventFlags & flags::REMOVE_EVENT) != 0);
    }
    (void)endpoint->closeAndAwaitTermination();
    REQUIRE((*source)->awaitDemand(q::core::SubscriptionFamily::HISTORY, u"MSFT", true, Clock::now() + 5s));
    (void)second->closeAndAwaitTermination();
    REQUIRE((*source)->awaitDemand(q::core::SubscriptionFamily::HISTORY, u"IBM", false, Clock::now() + 5s));
    (*server)->closeAndAwaitTermination();
    CHECK((*server)->getDiagnostic().code == q::api::ErrorCode::NONE);
}
