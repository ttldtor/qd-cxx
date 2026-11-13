// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../src/api/on_demand_playback_clock.hpp"
#include "qd/api/feed.hpp"
#include "recovery_test_peer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <doctest/doctest.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace api = q::api;
using namespace std::chrono_literals;

// Emitted by the pinned QD 3.355 ReplayRequest and ReplayResponse codecs from synthetic input.
constexpr std::string_view JAVA_REQUEST =
    "8081020708636f6e7472616374000475736572000a65787069726174696f6e1339323233333732303336383534373735383037"
    "066469676573742000000000000000000000000000000000000000000000000000000000000000000564656c617901300474"
    "696d650d31373037373438323035303030046b65797307044d4f434b0051";
constexpr std::string_view JAVA_RESPONSE =
    "31020106626c6f636b73272600044d4f434b0051f065ca2b6880419388ff058051c04c9b0a05805187da1482ee22864080a0";
constexpr std::string_view START_HEADER = "X-OnDemand-StartTime: 2024-02-12T14:30:05.000Z";
constexpr auto START = std::chrono::sys_time{std::chrono::milliseconds{1707748205000}};
constexpr auto WAIT = 8s;

std::vector<std::byte> unhex(std::string_view text) {
    std::vector<std::byte> bytes;
    const auto digit = [](char value) {
        return value <= '9' ? value - '0' : value - 'a' + 10;
    };

    for (std::size_t i = 0; i < text.size(); i += 2)
        bytes.push_back(static_cast<std::byte>(16 * digit(text[i]) + digit(text[i + 1])));

    return bytes;
}

void append(std::vector<std::byte> &bytes, std::string_view text) {
    const auto input = std::as_bytes(std::span(text));
    bytes.insert(bytes.end(), input.begin(), input.end());
}

std::vector<std::byte> response(std::string_view headers, std::vector<std::byte> body = {}) {
    std::vector<std::byte> bytes;
    append(bytes, headers);
    bytes.insert(bytes.end(), body.begin(), body.end());

    return bytes;
}

std::vector<std::byte> binaryResponse(std::string_view hex) {
    auto body = unhex(hex);
    const auto headers = std::string("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: ") +
                         std::to_string(body.size()) + "\r\n\r\n";

    return response(headers, std::move(body));
}

std::size_t requestLength() {
    constexpr std::string_view HEADER =
        "POST /MarketDataReplay HTTP/1.1\r\nHost: 127.0.0.1:0\r\n"
        "Content-Type: application/octet-stream\r\nAccept: application/octet-stream\r\n"
        "X-OnDemand-StartTime: 2024-02-12T14:30:05.000Z\r\nContent-Length: 131\r\nConnection: close\r\n\r\n";

    return HEADER.size() + JAVA_REQUEST.size() / 2;
}

q::test::TcpPeerConfig peerConfig(std::vector<std::byte> reply, bool hold = false) {
    q::test::TcpPeerConfig config;
    config.sessions.resize(1);
    auto &session = config.sessions.front();
    // Allow one through five port digits. The peer retains the remaining bytes through client EOF.
    session.clientBytesToRead = requestLength();
    session.replyBytes = std::move(reply);
    session.waitForControllerReply = hold;
    session.waitForClientClose = !hold;

    return config;
}

api::RuntimeOptions runtimeOptions() {
    api::RuntimeOptions options;
    options.dnsServers.push_back({{127, 0, 0, 1}});

    return options;
}

template <class Predicate> bool eventually(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + WAIT;

    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate())
            return true;

        std::this_thread::sleep_for(1ms);
    }

    return false;
}
} // namespace

TEST_CASE("OnDemand fetches the exact Java request and delivers two historical Quotes at 1x") {
    struct Arrival {
        q::events::Quote quote;
        std::chrono::steady_clock::time_point time;
    };

    q::test::OwnedTcpPeer peer(peerConfig(binaryResponse(JAVA_RESPONSE)));
    REQUIRE(peer.port() != 0);
    std::vector<Arrival> arrivals;
    api::ApiRuntime runtime(runtimeOptions());
    api::EndpointOptions options;
    options.pollInterval = 30s;
    const auto endpoint = runtime.createEndpoint(options);
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("MOCK");
    subscription->addEventListener([&](std::span<const q::events::Quote> quotes) {
        for (const auto &quote : quotes)
            arrivals.push_back({quote, std::chrono::steady_clock::now()});
    });
    endpoint->connectOnDemand(peer.port(), u"MOCK", START);
    const auto completed = endpoint->awaitState(api::EndpointState::CLOSED, std::chrono::steady_clock::now() + WAIT);
    INFO(completed.diagnostic.stage);
    REQUIRE(completed.status == api::WaitStatus::REACHED);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK(closed.drained);
    CHECK(closed.diagnostic.code == api::ErrorCode::NONE);
    REQUIRE(arrivals.size() == 2);
    const auto &first = arrivals.front();
    const auto &last = arrivals.back();
    CHECK(first.quote.symbol == u"MOCK");
    CHECK(first.quote.eventTime == START.time_since_epoch().count());
    CHECK(first.quote.getBidPrice() == 12.25);
    CHECK(first.quote.getAskPrice() == 12.50);
    CHECK(first.quote.getBidSizeAsDouble() == 10);
    CHECK(first.quote.getAskSizeAsDouble() == 20);
    CHECK(first.quote.bidExchangeCode == u'Q');
    CHECK(last.quote.eventTime == first.quote.eventTime + 750);
    CHECK(last.quote.getBidPrice() == 13.25);
    CHECK(last.quote.getAskPrice() == 13.50);
    CHECK(last.time - first.time >= 500ms);
    CHECK(endpoint->getStatistics().connection.sourceEpoch.has_value());

    REQUIRE(eventually([&] {
        return peer.snapshot().finished;
    }));
    const auto wire = peer.snapshot();
    CHECK(wire.error == q::test::TcpPeerError::NONE);
    REQUIRE(wire.sessions.size() == 1);
    const auto &received = wire.sessions.front().received;
    const std::string_view text(reinterpret_cast<const char *>(received.data()), received.size());
    REQUIRE(text.starts_with("POST /MarketDataReplay HTTP/1.1\r\n"));
    CHECK(text.contains(START_HEADER));
    const auto separator = text.find("\r\n\r\n");
    REQUIRE(separator != std::string_view::npos);
    const auto expected = unhex(JAVA_REQUEST);
    const auto body = std::span(received).subspan(separator + 4);
    CHECK(text.contains("Content-Length: 131\r\n"));
    const auto mismatch = std::ranges::mismatch(body, expected);
    CAPTURE(body.size());
    CAPTURE(expected.size());
    CAPTURE(mismatch.in1 - body.begin());

    if (mismatch.in1 != body.end() && mismatch.in2 != expected.end()) {
        CAPTURE(std::to_integer<unsigned>(*mismatch.in1));
        CAPTURE(std::to_integer<unsigned>(*mismatch.in2));
        CHECK(*mismatch.in1 == *mismatch.in2);
    }

    CHECK(std::ranges::equal(body, expected));
}

TEST_CASE("OnDemand refuses access, ambiguous framing and compression before a historical epoch") {
    constexpr std::string_view COMPRESSED_RESPONSE =
        "34020109626c6f636b732e677a272600044d4f434b0051f065ca2b6880419388ff058051c04c9b0a05805187da1482ee22864080a0";
    const std::array cases{
        std::pair{response("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n"), "ondemand-access-refused"},
        std::pair{response("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 1\r\n"
                           "content-length: 1\r\n\r\nX"),
                  "ondemand-http-framing"},
        std::pair{binaryResponse(COMPRESSED_RESPONSE), "ondemand-compression"}};

    for (const auto &[bytes, stage] : cases) {
        q::test::OwnedTcpPeer peer(peerConfig(bytes));
        REQUIRE(peer.port() != 0);
        std::atomic<unsigned> callbacks{0};
        api::ApiRuntime runtime(runtimeOptions());
        const auto endpoint = runtime.createEndpoint();
        const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addSymbols("MOCK");
        subscription->addEventListener([&](auto) {
            ++callbacks;
        });
        CHECK_THROWS_AS(endpoint->connectOnDemand(peer.port(), u"*", START), api::Error);
        CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
        endpoint->connectOnDemand(peer.port(), u"MOCK", START);
        REQUIRE(endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + WAIT).status ==
                api::WaitStatus::REACHED);
        const auto closed = endpoint->closeAndAwaitTermination();
        CHECK(closed.diagnostic.stage == stage);
        CHECK_FALSE(endpoint->getStatistics().connection.sourceEpoch);
        CHECK(callbacks.load() == 0);
    }
}

TEST_CASE("OnDemand catches up a pre-cut Quote and fences the next Quote on callback close") {
    q::test::OwnedTcpPeer peer(peerConfig(binaryResponse(JAVA_RESPONSE)));
    REQUIRE(peer.port() != 0);
    std::vector<std::int64_t> times;
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint();
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("MOCK");
    subscription->addEventListener([&times, weakEndpoint = std::weak_ptr(endpoint)](auto quotes) {
        for (const auto &quote : quotes)
            times.push_back(quote.eventTime);

        if (const auto owner = weakEndpoint.lock())
            owner->close();
    });
    endpoint->connectOnDemand(peer.port(), u"MOCK", START + 250ms);
    const auto completed = endpoint->awaitState(api::EndpointState::CLOSED, std::chrono::steady_clock::now() + WAIT);
    INFO(completed.diagnostic.stage);
    REQUIRE(completed.status == api::WaitStatus::REACHED);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK_FALSE(closed.deadlineExpired);
    CHECK(closed.diagnostic.code == api::ErrorCode::NONE);
    REQUIRE(times.size() == 1);
    CHECK(times.front() == START.time_since_epoch().count());
}

TEST_CASE("OnDemand close while the owned peer holds its response retires without a Quote") {
    q::test::OwnedTcpPeer peer(peerConfig({}, true));
    REQUIRE(peer.port() != 0);
    std::atomic<unsigned> callbacks{0};
    api::ApiRuntime runtime(runtimeOptions());
    api::EndpointOptions options;
    options.pollInterval = 30s;
    const auto endpoint = runtime.createEndpoint(options);
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("MOCK");
    subscription->addEventListener([&](auto) {
        ++callbacks;
    });
    endpoint->connectOnDemand(peer.port(), u"MOCK", START);
    REQUIRE(eventually([&] {
        const auto snapshot = peer.snapshot();

        return !snapshot.sessions.empty() && snapshot.sessions.front().matched;
    }));
    CHECK(endpoint->getState() == api::EndpointState::CONNECTING);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK(closed.diagnostic.code == api::ErrorCode::NONE);
    CHECK_FALSE(closed.deadlineExpired);
    CHECK_FALSE(endpoint->getStatistics().connection.sourceEpoch);
    CHECK(callbacks.load() == 0);
}

TEST_CASE("OnDemand honors response deadline with a slow requested polling cadence") {
    q::test::OwnedTcpPeer peer(peerConfig({}, true));
    REQUIRE(peer.port() != 0);
    api::ApiRuntime runtime(runtimeOptions());
    api::EndpointOptions options;
    options.pollInterval = 30s;
    options.readTimeout = 20s;
    options.connectTimeout = 20s;
    options.writeTimeout = 20s;
    const auto endpoint = runtime.createEndpoint(options);
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("MOCK");
    const auto began = std::chrono::steady_clock::now();
    endpoint->connectOnDemand(peer.port(), u"MOCK", START);
    const auto result = endpoint->awaitState(api::EndpointState::FAILED, began + WAIT);
    INFO(result.diagnostic.stage);
    REQUIRE(result.status == api::WaitStatus::REACHED);
    CHECK(result.diagnostic.stage == "ondemand-response-deadline");
    CHECK(std::chrono::steady_clock::now() - began >= 4s);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK_FALSE(closed.deadlineExpired);
    const auto snapshot = peer.snapshot();
    REQUIRE(snapshot.sessions.size() == 1);
    CHECK(snapshot.sessions.front().matched);
}

TEST_CASE("OnDemand clock preserves fractional progress and applies rates to elapsed intervals") {
    q::api::detail::OnDemandPlaybackClock clock;
    const auto origin = std::chrono::steady_clock::time_point{1s};
    constexpr auto LIMIT = 2000;

    clock.advance(0.5, origin, LIMIT);
    clock.advance(0.5, origin + 10s, LIMIT);
    CHECK(clock.position() == 0);
    clock.start(origin + 10s);
    clock.advance(0.5, origin + 10s + 1ms, LIMIT);
    CHECK(clock.position() == 0);
    clock.advance(0.5, origin + 10s + 2ms, LIMIT);
    CHECK(clock.position() == 1);
    clock.advance(0, origin + 10s + 200ms, LIMIT);
    CHECK(clock.position() == 100);
    clock.advance(2, origin + 11s, LIMIT);
    CHECK(clock.position() == 100);
    const auto wake = clock.wakeBefore(110, origin + 11s, origin + 11s + 10ms);
    CHECK(wake >= origin + 11s + 5ms);
    CHECK(wake <= origin + 11s + 5ms + 1us);
    clock.advance(2, origin + 11s + 500ms, LIMIT);
    CHECK(clock.position() == 1100);
    clock.advance(0, origin + 14s, LIMIT);
    CHECK(clock.position() == LIMIT);
}

TEST_CASE("OnDemand preserves its cursor through initial pause, rate changes and terminal EOF") {
    constexpr std::string_view CONTROL_RESPONSE =
        "31020106626c6f636b73272600044d4f434b0051f065ca2b6880419388ff058051c04c9b0a05805187da1487d022864080a0";
    q::test::OwnedTcpPeer peer(peerConfig(binaryResponse(CONTROL_RESPONSE)));
    REQUIRE(peer.port() != 0);
    std::vector<api::OnDemandPlaybackChange> observed;
    std::vector<std::int64_t> times;
    std::atomic<unsigned> delivered{0};
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint();
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("MOCK");
    subscription->addEventListener([&](auto quotes) {
        for (const auto &quote : quotes) {
            times.push_back(quote.eventTime);
            ++delivered;
        }
    });
    endpoint->addOnDemandPlaybackListener([&](const auto &change) {
        observed.push_back(change);
    });
    CHECK_FALSE(endpoint->getOnDemandPlayback());
    CHECK_THROWS_AS(endpoint->pauseOnDemand(), api::Error);
    CHECK_THROWS_AS(endpoint->connectOnDemand(peer.port(), u"MOCK", START, -1), api::Error);
    endpoint->connectOnDemand(peer.port(), u"MOCK", START, 0);
    REQUIRE(eventually([&] {
        return delivered.load() == 1;
    }));
    const auto initial = endpoint->getOnDemandPlayback();
    REQUIRE(initial);
    CHECK(initial->time == START);
    CHECK(initial->speed == 0);
    CHECK(initial->covered);
    const auto epoch = endpoint->getStatistics().connection.sourceEpoch;
    REQUIRE(epoch);

    endpoint->setOnDemandSpeed(0.5);
    REQUIRE(eventually([&] {
        return endpoint->getOnDemandPlayback()->time >= START + 100ms;
    }));
    endpoint->pauseOnDemand();
    REQUIRE(eventually([&] {
        return endpoint->getOnDemandPlayback()->speed == 0;
    }));
    const auto paused = *endpoint->getOnDemandPlayback();
    CHECK(paused.time < START + 2000ms);
    std::this_thread::sleep_for(125ms);
    CHECK(endpoint->getOnDemandPlayback() == paused);
    CHECK(delivered.load() == 1);

    for (double speed : {-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                         api::MAXIMUM_ON_DEMAND_SPEED + 1})
        CHECK_THROWS_AS(endpoint->setOnDemandSpeed(speed), api::Error);

    endpoint->pauseOnDemand();
    endpoint->setOnDemandSpeed(2);
    REQUIRE(endpoint->awaitState(api::EndpointState::CLOSED, std::chrono::steady_clock::now() + WAIT).status ==
            api::WaitStatus::REACHED);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK(closed.drained);
    const std::vector<std::int64_t> expectedTimes{START.time_since_epoch().count(),
                                                  (START + 2000ms).time_since_epoch().count()};
    CHECK(times == expectedTimes);
    CHECK(endpoint->getStatistics().connection.sourceEpoch == epoch);
    CHECK(peer.snapshot().sessions.size() == 1);
    const auto terminal = endpoint->getOnDemandPlayback();
    REQUIRE(terminal);
    CHECK_FALSE(terminal->active);
    CHECK(terminal->speed == 0);
    CHECK(terminal->time == START + 2000ms);
    REQUIRE_FALSE(observed.empty());
    CHECK(observed.back().current == *terminal);
    std::vector<double> rates;

    for (const auto &change : observed) {
        CHECK(change.previous.time <= change.current.time);

        if (change.previous.speed != change.current.speed)
            rates.push_back(change.current.speed);
    }

    const std::vector<double> expectedRates{0.5, 0, 2, 0};
    CHECK(rates == expectedRates);
}

TEST_CASE("OnDemand pre-coverage speed callbacks can remove themselves and close the owned request") {
    q::test::OwnedTcpPeer peer(peerConfig({}, true));
    REQUIRE(peer.port() != 0);
    std::vector<api::OnDemandPlaybackChange> selected;
    std::atomic<unsigned> quotes{0};
    std::atomic<unsigned> retirementGuarded{0};
    api::Endpoint::OnDemandPlaybackListenerId self{};
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint();
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("MOCK");
    subscription->addEventListener([&](auto) {
        ++quotes;
    });
    self = endpoint->addOnDemandPlaybackListener(
        [&selected, &self, &retirementGuarded, weakEndpoint = std::weak_ptr(endpoint)](const auto &change) {
            selected.push_back(change);
            const auto owner = weakEndpoint.lock();

            if (!owner)
                return;

            owner->removeOnDemandPlaybackListener(self);
            try {
                static_cast<void>(owner->closeAndAwaitTermination());
            } catch (const api::Error &error) {
                if (error.diagnostic().code == api::ErrorCode::WRONG_THREAD)
                    ++retirementGuarded;
            }
            owner->close();
            throw std::runtime_error("advisory failure");
        });
    endpoint->connectOnDemand(peer.port(), u"MOCK", START, 0);
    REQUIRE(eventually([&] {
        const auto snapshot = peer.snapshot();

        return !snapshot.sessions.empty() && snapshot.sessions.front().matched;
    }));
    endpoint->setOnDemandSpeed(2);
    REQUIRE(endpoint->awaitState(api::EndpointState::CLOSED, std::chrono::steady_clock::now() + WAIT).status ==
            api::WaitStatus::REACHED);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK(closed.diagnostic.code == api::ErrorCode::NONE);
    CHECK(endpoint->getOnDemandPlaybackDiagnostic().code == api::ErrorCode::CALLBACK_FAILURE);
    CHECK(retirementGuarded.load() == 1);
    CHECK(quotes.load() == 0);
    REQUIRE(selected.size() == 1);
    CHECK(selected.front().current.time == START);
    CHECK(selected.front().current.speed == 2);
    CHECK_FALSE(selected.front().current.covered);
    const auto terminal = endpoint->getOnDemandPlayback();
    REQUIRE(terminal);
    CHECK_FALSE(terminal->active);
    CHECK(terminal->time == START);
    CHECK(terminal->speed == 0);
}
