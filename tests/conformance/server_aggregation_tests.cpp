// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// CTest owns finite loopback scripts and joins all helper/runtime workers before return.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "aggregation.hpp"
#include "qd/api/endpoint_configuration.hpp"
#include "recovery_test_peer.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <doctest/doctest.h>
#include <fstream>
#include <map>
#include <sstream>
#include <thread>
using namespace org::ttldtor::qdcxx;
using namespace std::chrono_literals;

namespace {
using Info = transport::AggregationPeriodInfo;
using namespace transport::detail;

/** @return Independent finite work for one actual parser observation. */
protocol::LiveWork work() {
    return {1000, 10000000, 10000000, 10000000};
}

/** @param kind Selected row type. @return Exact raw oracle cells without CRLF framing. */
std::map<std::string, std::string> fixture(std::string_view kind) {
    std::ifstream input(ORG_TTLDTOR_QDCXX_AGGREGATION_FIXTURE);
    REQUIRE(input.good());
    std::map<std::string, std::string> result;
    std::size_t count = 0;

    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        ++count;
        std::istringstream cells(line);
        std::string type, key, value;
        std::getline(cells, type, '\t');
        std::getline(cells, key, '\t');
        std::getline(cells, value);

        if (type == kind)
            result.emplace(key, value);
    }

    REQUIRE(count == 68);

    return result;
}

/** @param hex Exact Java wire cell. @return Owned byte envelope. */
std::vector<std::byte> bytes(const std::string &hex) {
    REQUIRE(hex.size() % 2 == 0);
    std::vector<std::byte> result;

    for (std::size_t i = 0; i < hex.size(); i += 2)
        result.push_back(static_cast<std::byte>(std::stoul(hex.substr(i, 2), nullptr, 16)));

    return result;
}

/** @param action Finite call. @return API category, preserving foreign exceptions. */
template <class Action> api::ErrorCode error(Action action) {
    try {
        action();
    } catch (const api::Error &failure) {
        return failure.diagnostic().code;
    }

    return api::ErrorCode::NONE;
}

/** @param predicate Thread-safe observation. @return Success within the finite test deadline. */
template <class Predicate> bool until(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    do {
        if (predicate())
            return true;

        std::this_thread::sleep_for(1ms);
    } while (std::chrono::steady_clock::now() < deadline);

    return false;
}

/** @param captured Actual prefix. @param expected Exact delta. @return Contiguous occurrence. */
bool contains(std::span<const std::byte> captured, std::span<const std::byte> expected) {
    return std::search(captured.begin(), captured.end(), expected.begin(), expected.end()) != captured.end();
}

/** @param snapshot Detached configuration. @return Canonical requested local row. */
const api::EffectiveEndpointProperty &request(const api::EndpointConfigurationSnapshot &snapshot) {
    auto found = std::ranges::find(snapshot.properties, "dxfeed.requestedAggregationPeriod",
                                   &api::EffectiveEndpointProperty::key);

    if (found == snapshot.properties.end())
        throw std::runtime_error("missing request row");

    return *found;
}

/** @return Explicit numeric-loopback runtime policy. */
api::RuntimeOptions runtimeOptions() {
    api::RuntimeOptions result;
    result.dnsServers.push_back({{127, 0, 0, 1}});

    return result;
}
} // namespace

TEST_CASE("peer TimePeriodInfo matches production baseline and names corrected Java overflow") {
    const std::array<std::u16string_view, 17> texts{u"",
                                                    u"",
                                                    u"{\"min\":-1,\"max\":-1}",
                                                    u"{\"min\":0,\"max\":0}",
                                                    u"{\"min\":1.5,\"max\":2}",
                                                    u"{\"min\":2,\"max\":1}",
                                                    u"{\"max\":2,\"min\":1}",
                                                    u"{\"min\": 1,\"max\":2}",
                                                    u"{\"min\":1e-3,\"max\":2E-3}",
                                                    u"{\"min\":-0.0005,\"max\":0}",
                                                    u"{\"min\":-0.0006,\"max\":0}",
                                                    u"{\"min\":0.0005,\"max\":0.0015}",
                                                    u"{\"min\":1e1000,\"max\":1e1000}",
                                                    u"{\"min\":1,\"max\":2,\"future\":true}",
                                                    u"{\"min\":1,\"max\":2,\"future\":{\"x\":1}}",
                                                    u"{\"min\":1,\"max\":2,\"future\":[[1]]}",
                                                    u"{\"min\":1,\"max\":2,\"future\":\"a,b\"}"};
    const auto raw = fixture("parse");
    REQUIRE(raw.size() == texts.size());

    for (std::size_t i = 0; i < texts.size(); ++i) {
        auto budget = work();
        bool complete = false;
        const auto parsed = parseAggregationPeriodInfo(texts[i], budget, &complete);
        CHECK(complete);
        std::string observed = "-1000,-1000,true";

        if (parsed)
            observed =
                std::to_string(parsed->minimum.count()) + "," + std::to_string(parsed->maximum.count()) + ",false";

        CHECK_MESSAGE(observed == raw.at(std::to_string(i)), i);
    }

    // Preserve the actual Java result; native intentionally corrects its internal int exponent overflow.
    CHECK(fixture("compatibility").at("java-int-exponent-overflow") == "0,0,false");
    auto budget = work();
    const auto corrected = parseAggregationPeriodInfo(
        u"{\"min\":10000000000000000000e2147483647,\"max\":10000000000000000000e2147483647}", budget);
    REQUIRE(corrected);
    CHECK(*corrected == Info{std::chrono::milliseconds::max(), std::chrono::milliseconds::max()});
}

TEST_CASE("peer inspection is bounded and never publishes a partial family union") {
    constexpr auto known = u"{\"min\":1,\"max\":2}";
    auto empty = work();
    empty.visits = 0;
    bool complete = true;
    CHECK_FALSE(parseAggregationPeriodInfo(known, empty, &complete));
    CHECK_FALSE(complete);
    std::u16string wide(65537, u'x');
    auto budget = work();
    CHECK_FALSE(parseAggregationPeriodInfo(wide, budget));
    const std::array<protocol::LivePropertyView, 1> inherited{{{u"aggregationPeriodInfo", known}}};
    const std::array<protocol::LivePropertyView, 1> local{{{u"aggregationPeriodInfo", u"{\"min\":3,\"max\":4}"}}};
    const std::array<protocol::LiveMessageView, 2> messages{{{10, u"TICKER_DATA", {}}, {15, u"STREAM_DATA", local}}};
    budget = work();
    const auto united = peerAggregationPeriodInfo({inherited, messages, {}}, {true, true, false}, budget, &complete);
    REQUIRE(united);
    CHECK(complete);
    CHECK(*united == Info{1s, 4s});
    const auto spent = work().visits - budget.visits;
    REQUIRE(spent > 1);
    budget = work();
    budget.visits = spent - 1;
    CHECK_FALSE(peerAggregationPeriodInfo({inherited, messages, {}}, {true, true, false}, budget, &complete));
    CHECK_FALSE(complete);
    const std::array<protocol::LivePropertyView, 1> reset{{{u"aggregationPeriodInfo", u""}}};
    const std::array<protocol::LiveMessageView, 1> overridden{{{10, u"TICKER_DATA", reset}}};
    budget = work();
    CHECK_FALSE(peerAggregationPeriodInfo({inherited, overridden, {}}, {true, false, false}, budget));
    budget = work();
    CHECK_FALSE(peerAggregationPeriodInfo({inherited, messages, {}}, {false, false, true}, budget));
    std::u16string suffix = u"{\"min\":1,\"max\":2,";
    suffix.append(60000, u',');
    suffix += u"}";
    budget = work();
    budget.visits = 100;
    CHECK_FALSE(parseAggregationPeriodInfo(suffix, budget, &complete));
    CHECK_FALSE(complete);
}

TEST_CASE("request canonical text and public bounds match production Java") {
    for (const auto &[value, text] :
         std::array<std::pair<std::int64_t, std::u16string_view>, 7>{{{0, u"PT0S"},
                                                                      {1, u"PT0.001S"},
                                                                      {1500, u"PT1.5S"},
                                                                      {60000, u"PT1M0S"},
                                                                      {3600000, u"PT1H0S"},
                                                                      {3661123, u"PT1H1M1.123S"},
                                                                      {86400000, u"P1DT0S"}}})
        CHECK(aggregationRequestText(std::chrono::milliseconds{value}).view() == text);

    CHECK(aggregationRequestText({}).view().empty());
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint();
    auto second = runtime.createEndpoint();

    for (const auto &[value, outcome] : fixture("bound")) {
        const auto period = value == "null" ? std::optional<std::chrono::milliseconds>{}
                                            : std::optional{std::chrono::milliseconds{std::stoll(value)}};
        const auto before = endpoint->getRequestedAggregationPeriod();
        const auto failed = error([&] {
            endpoint->setRequestedAggregationPeriod(period);
        });

        if (outcome == "IllegalArgumentException") {
            CHECK(failed == api::ErrorCode::INVALID_ARGUMENT);
            CHECK(endpoint->getRequestedAggregationPeriod() == before);
        } else {
            CHECK(failed == api::ErrorCode::NONE);
            CHECK(endpoint->getRequestedAggregationPeriod() == period);
        }
    }

    CHECK_FALSE(second->getRequestedAggregationPeriod());
    CHECK_FALSE(endpoint->getAggregationPeriodInfo());
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK(endpoint->getEffectiveConfiguration().connectionRevision == 0);
    std::atomic<bool> closedCallback = false;
    const std::weak_ptr<api::Endpoint> weak = endpoint;
    static_cast<void>(endpoint->addStateChangeListener([weak, &closedCallback](auto, auto next) {
        if (next == api::EndpointState::CLOSED) {
            if (const auto selected = weak.lock())
                selected->setRequestedAggregationPeriod(5ms);

            closedCallback = true;
        }
    }));
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK(closedCallback.load());
    endpoint->setRequestedAggregationPeriod(5ms);
    CHECK(endpoint->getRequestedAggregationPeriod() == 5ms);
    CHECK_FALSE(endpoint->getAggregationPeriodInfo());
    runtime.closeAndAwaitTermination();
}

TEST_CASE("immutable initial policy and owned current request revisions are instance local") {
    const std::array defaults{api::EndpointProperty{"dxfeed.requestedAggregationPeriod", "PT5S"}};
    const std::array explicitValues{
        api::EndpointProperty{"dxfeed.address", "localhost:1234[requestedAggregationPeriod=PT2S]"}};
    auto configuration = api::EndpointConfiguration::create({}, defaults, explicitValues);
    CHECK(configuration.options().requestedAggregationPeriod == 2s);
    CHECK(request(configuration.snapshot()).effectiveValue == "PT2S");
    CHECK(request(configuration.snapshot()).origin == api::EndpointPropertyOrigin::ADDRESS_PROPERTIES);
    const std::array bare{api::EndpointProperty{"dxfeed.requestedAggregationPeriod", "1"}};
    CHECK(api::EndpointConfiguration::create({}, {}, bare).options().requestedAggregationPeriod == 1s);
    const auto original = configuration.snapshot();
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(configuration);
    auto second = runtime.createEndpoint(configuration);
    std::barrier start(2);
    std::atomic<bool> consistent = true;
    std::jthread reader([&] {
        start.arrive_and_wait();

        for (unsigned i = 0; i < 1000; ++i) {
            const auto observed = endpoint->getEffectiveConfiguration();
            const auto &row = request(observed);

            if (observed.requestedAggregationRevision == 0 ? row.effectiveValue != "PT2S"
                                                           : row.origin != api::EndpointPropertyOrigin::RUNTIME_UPDATE)
                consistent = false;
        }
    });
    start.arrive_and_wait();

    for (unsigned i = 0; i < 100; ++i)
        endpoint->setRequestedAggregationPeriod(std::chrono::milliseconds{i});

    reader.join();
    CHECK(consistent.load());
    CHECK(endpoint->getRequestedAggregationPeriod() == 99ms);
    CHECK(second->getRequestedAggregationPeriod() == 2s);
    const auto updated = endpoint->getEffectiveConfiguration();
    CHECK(updated.requestedAggregationRevision == 100);
    endpoint->setRequestedAggregationPeriod(99ms);
    CHECK(endpoint->getEffectiveConfiguration().requestedAggregationRevision == 100);
    CHECK(updated.connectionRevision == 0);

    for (const auto &row : original.properties)
        if (row.key != "dxfeed.requestedAggregationPeriod") {
            const auto found = std::ranges::find(updated.properties, row.key, &api::EffectiveEndpointProperty::key);
            REQUIRE(found != updated.properties.end());
            CHECK(found->effectiveValue == row.effectiveValue);
            CHECK(found->requestedValue == row.requestedValue);
            CHECK(found->origin == row.origin);
        }

    CHECK(configuration.snapshot().requestedAggregationRevision == 0);
    CHECK(request(configuration.snapshot()).effectiveValue == "PT2S");
    endpoint->setRequestedAggregationPeriod({});
    CHECK(request(endpoint->getEffectiveConfiguration()).effectiveValue.empty());

    for (auto value : {"-1", "P2D", "", "not-a-period"}) {
        const std::array input{api::EndpointProperty{"dxfeed.requestedAggregationPeriod", value}};
        CHECK(error([&] {
                  static_cast<void>(api::EndpointConfiguration::create({}, {}, input));
              }) == api::ErrorCode::INVALID_ARGUMENT);
    }

    const std::array reset{api::EndpointProperty{"dxfeed.address", "localhost:1234[requestedAggregationPeriod=]"}};
    CHECK_FALSE(api::EndpointConfiguration::create({}, defaults, reset).options().requestedAggregationPeriod);
    runtime.closeAndAwaitTermination();
}

TEST_CASE("native worker sends exact production deltas and fences peer info across fresh connections") {
    const auto raw = fixture("wire");
    test::TcpPeerScript first;
    first.initialBytes = bytes(raw.at("default-response"));
    first.readThroughMessage = 1;
    first.replyBytes = bytes(raw.at("5s-response"));
    first.waitForControllerReply = true;
    first.waitForClientClose = true;
    test::TcpPeerScript second;
    second.initialBytes = bytes(raw.at("legacy-fresh-no-info"));
    second.readThroughMessage = 1;
    second.replyBytes = bytes(raw.at("fresh-adapter-reconnect-response"));
    second.waitForControllerReply = true;
    second.waitForClientClose = true;
    test::OwnedTcpPeer peer({{first, second}, 1024 * 1024, 20s});
    REQUIRE(peer.port() != 0);
    api::ApiRuntime runtime(runtimeOptions());
    api::EndpointOptions options;
    options.maximumAttempts = 2; // Both physical sources consume the existing lifetime attempt budget.
    auto endpoint = runtime.createEndpoint(options);
    const auto route = "127.0.0.1:" + std::to_string(peer.port());
    endpoint->connect(route);
    REQUIRE(until([&] {
        return endpoint->getAggregationPeriodInfo() == Info{3s, 3s};
    }));
    REQUIRE(until([&] {
        return peer.snapshot().sessions[0].matched;
    }));
    endpoint->setRequestedAggregationPeriod(5s);
    CHECK(endpoint->getAggregationPeriodInfo() == Info{3s, 3s});
    peer.allowReply(0);
    REQUIRE(until([&] {
        return endpoint->getAggregationPeriodInfo() == Info{5s, 5s};
    }));

    for (const auto &[period, key] :
         std::array<std::pair<std::optional<std::chrono::milliseconds>, std::string_view>, 3>{
             {{0ms, "0s-request"}, {20s, "20s-request"}, {std::nullopt, "reset-request"}}}) {
        endpoint->setRequestedAggregationPeriod(period);
        const auto expected = bytes(raw.at(std::string(key)));
        REQUIRE(until([&] {
            return contains(peer.snapshot().sessions[0].received, expected);
        }));
        CHECK(endpoint->getAggregationPeriodInfo() == Info{5s, 5s});
    }

    CHECK(contains(peer.snapshot().sessions[0].received, bytes(raw.at("5s-request"))));
    endpoint->disconnect();
    CHECK_FALSE(endpoint->getAggregationPeriodInfo());
    endpoint->setRequestedAggregationPeriod(5s);
    endpoint->connect(route);
    REQUIRE(until([&] {
        return peer.snapshot().sessions[1].matched;
    }));
    CHECK_FALSE(endpoint->getAggregationPeriodInfo());
    const auto captured = peer.snapshot().sessions[1].received;
    const std::string marker = "requestedAggregationPeriod";
    std::vector<std::byte> keyBytes;

    for (auto c : marker)
        keyBytes.push_back(static_cast<std::byte>(c));

    keyBytes.push_back(std::byte{4});

    for (auto c : std::string_view{"PT5S"})
        keyBytes.push_back(static_cast<std::byte>(c));

    CHECK(contains(captured, keyBytes));
    peer.allowReply(1);
    REQUIRE(until([&] {
        return endpoint->getAggregationPeriodInfo() == Info{5s, 5s};
    }));
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK_FALSE(endpoint->getAggregationPeriodInfo());
    runtime.closeAndAwaitTermination();
    peer.stop();
    CHECK(peer.snapshot().error == test::TcpPeerError::NONE);
}
