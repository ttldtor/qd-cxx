// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/endpoint_configuration.hpp"
#include "qd/api/feed.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"

#include "../../src/protocol/live_authorization.hpp"
#include "../../src/transport/tls_credential_gate.hpp"
#include "connect_peer.hpp"
#include "transport/tls_test_support.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace api = q::api;
namespace peer = q::connect_tests;
namespace test = q::test;
namespace tls = test::tls;
using namespace std::chrono_literals;

constexpr std::u16string_view SYNTHETIC_TOKEN = u"Basic dGVzdDpmaXh0dXJl";

class Provider final : public q::client::CredentialProvider {
    public:
    enum class Mode { VALID, THROW, OVERSIZE, HOLD, RECONNECT };

    Mode mode = Mode::VALID;
    std::weak_ptr<api::Endpoint> endpoint;
    std::atomic<unsigned> calls{0};
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;

    std::expected<std::size_t, q::client::CredentialError> basicToken(std::span<char16_t> output) override {
        const auto call = ++calls;

        if (mode == Mode::THROW)
            throw std::runtime_error("synthetic-provider-secret");

        if (mode == Mode::OVERSIZE)
            return output.size() + 1;

        if (mode == Mode::HOLD && call == 1) {
            std::unique_lock lock(mutex);
            entered = true;
            changed.notify_all();

            if (!changed.wait_for(lock, 6s, [&] {
                    return released;
                }))
                return std::unexpected(q::client::CredentialError::UNAVAILABLE);
        }

        if (mode == Mode::RECONNECT && call == 1) {
            if (const auto owner = endpoint.lock())
                owner->reconnect();
        }

        if (output.size() < SYNTHETIC_TOKEN.size())
            return std::unexpected(q::client::CredentialError::REJECTED);

        std::ranges::copy(SYNTHETIC_TOKEN, output.begin());

        return SYNTHETIC_TOKEN.size();
    }

    bool awaitEntry() {
        std::unique_lock lock(mutex);

        return changed.wait_for(lock, 8s, [&] {
            return entered;
        });
    }

    void release() {
        std::lock_guard lock(mutex);
        released = true;
        changed.notify_all();
    }
};

api::EndpointOptions endpointOptions() {
    auto resources = tls::options();
    constexpr std::size_t FEED_NATIVE_ALLOWANCE = 128 * 1024 * 1024; // Includes Feed's DNS and TCP/session backing.
    api::EndpointOptions result;
    result.maximumAttempts = 1;
    result.tls.emplace(api::VerifiedTlsOptions{"localhost", tls::read("ca.pem"), 5s, 1s, resources.limits,
                                               resources.host, resources.sdkInstanceBytes, resources.sdkSharedBytes,
                                               resources.residentBytes + FEED_NATIVE_ALLOWANCE});

    return result;
}

api::RuntimeOptions runtimeOptions() {
    api::RuntimeOptions result;
    result.dnsServers.push_back({{127, 0, 0, 1}});

    return result;
}

api::EventRegistry quoteRegistry() {
    const auto schema = tls::schema();
    const api::EventRegistration route{api::EventType::QUOTE, schema.records().front().id};
    api::EventRegistryWork work;
    auto registry = api::EventRegistry::create(schema, std::span(&route, 1), {}, work);
    REQUIRE(registry);

    return std::move(*registry);
}

std::vector<std::uint8_t> quoteBytes() {
    const auto schema = tls::schema();
    const auto family = q::core::SubscriptionFamily::TICKER;
    auto bytes = peer::fixturePeerPrefix(schema, family);
    peer::Record quote{u"IBM", {}};

    for (const auto &field : schema.records().front().fields)
        quote.fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});

    const auto data = peer::fixturePeerData(std::span(&quote, 1), family, true);
    bytes.insert(bytes.end(), data.begin(), data.end());

    std::vector<std::uint8_t> result;

    for (const auto byte : bytes)
        result.push_back(std::to_integer<std::uint8_t>(byte));

    return result;
}

test::TlsTestPeer quotePeer() {
    auto script = tls::peerOptions();
    script.pauseBeforeHandshake = 100ms;
    script.initialBytes = quoteBytes();

    auto value = test::TlsTestPeer::start(std::move(script));
    REQUIRE(value);

    return std::move(*value);
}

void appendNumber(std::vector<std::uint8_t> &output, std::int32_t value) {
    std::array<std::byte, 5> encoded{};
    const auto count = q::encoding::writeCompactInt(value, encoded);
    REQUIRE(count.status == q::encoding::Status::OK);

    for (const auto byte : std::span(encoded).first(count.written))
        output.push_back(std::to_integer<std::uint8_t>(byte));
}

void appendText(std::vector<std::uint8_t> &output, std::u16string_view text) {
    std::array<std::byte, 1024> encoded{};
    const auto count = q::encoding::writeUtfString(text, encoded, {encoded.size(), 128});
    REQUIRE(count.status == q::encoding::Status::OK);

    for (const auto byte : std::span(encoded).first(count.written))
        output.push_back(std::to_integer<std::uint8_t>(byte));
}

std::vector<std::uint8_t> frame(std::int32_t type, std::vector<std::uint8_t> body) {
    std::vector<std::uint8_t> result;
    appendNumber(result, static_cast<std::int32_t>(body.size() + 1));
    appendNumber(result, type);
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

std::vector<std::uint8_t> property(std::u16string_view key, std::u16string_view value) {
    std::vector<std::uint8_t> body{'D', 'X', 'P', '3', 1};
    appendText(body, key);
    appendText(body, value);
    body.insert(body.end(), {0, 0});

    return frame(1, std::move(body));
}

/** Exact independent local descriptor and single selected Quote description before challenge. */
std::vector<std::uint8_t> feedNegotiation() {
    std::vector<std::uint8_t> body{'D', 'X', 'P', '3', 0};
    const auto appendCommon = [&] {
        constexpr std::array<std::u16string_view, 3> names{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS"};

        for (std::size_t i = 0; i < names.size(); ++i) {
            appendNumber(body, static_cast<std::int32_t>(i));
            appendText(body, names[i]);
            body.push_back(0);
        }
    };
    body.push_back(5);
    appendCommon();
    body.push_back(11);
    appendText(body, u"TICKER_ADD_SUBSCRIPTION");
    body.insert(body.end(), {0, 12});
    appendText(body, u"TICKER_REMOVE_SUBSCRIPTION");
    body.insert(body.end(), {0, 4});
    appendCommon();
    body.push_back(10);
    appendText(body, u"TICKER_DATA");
    body.push_back(1);
    appendText(body, u"requestedAggregationPeriod");
    appendText(body, u"");

    auto result = frame(1, std::move(body));
    const auto schema = tls::schema();
    const auto &quote = schema.records().front();
    body.clear();
    appendNumber(body, quote.id);
    appendText(body, quote.name);
    appendNumber(body, static_cast<std::int32_t>(quote.fields.size()));

    for (std::size_t i = 0; i < quote.fields.size(); ++i) {
        const auto field = std::ranges::find_if(quote.lookups, [i](const auto &value) {
            return value.fieldId == i;
        });
        REQUIRE(field != quote.lookups.end());
        appendText(body, field->name);
        appendNumber(body, quote.fields[i].serialTypeId);
    }

    const auto description = frame(2, std::move(body));
    result.insert(result.end(), description.begin(), description.end());

    return result;
}

/** Use the wire peer to prove the exact authorization is sent before the source can publish Quote. */
test::TlsTestPeer authenticatedPeer(bool expectFirst, bool successor = false) {
    auto script = tls::peerOptions();
    script.captureUntilClose = true;
    script.readTimeout = 10s;
    script.initialBytes = property(u"authentication", u"LOGIN synthetic-realm");
    const auto authorization = property(u"authorization", SYNTHETIC_TOKEN);
    auto ready = property(u"authentication", u"");
    const auto quotes = quoteBytes();
    ready.insert(ready.end(), quotes.begin(), quotes.end());

    if (expectFirst) {
        script.expectedClientBytes = feedNegotiation();
        script.replyBytes = std::exchange(script.initialBytes, {});
        script.expectedReplyClientBytes = authorization;
        script.finalBytes = ready;
    }

    if (successor) {
        script.successor.emplace();
        script.successor->replyBytes = script.initialBytes;
        script.successor->expectedClientBytes = feedNegotiation();
        script.successor->expectedReplyClientBytes = authorization;
        script.successor->finalBytes = std::move(ready);
    }

    auto value = test::TlsTestPeer::start(std::move(script));
    REQUIRE(value);

    return std::move(*value);
}

bool containsAuthorization(const std::vector<std::uint8_t> &bytes) {
    return std::ranges::search(bytes, std::string_view("authorization")).begin() != bytes.end();
}

void reportPeer(const test::PeerSnapshot &snapshot, const std::vector<std::uint8_t> &bytes) {
    const auto expected = feedNegotiation();
    const auto mismatch = std::ranges::mismatch(bytes, expected);
    const auto offset = static_cast<std::size_t>(mismatch.in1 - bytes.begin());
    MESSAGE("matched sessions=", snapshot.matchedSessions, " handshakes=", snapshot.successfulHandshakes,
            " peer failed=", snapshot.failed, " timed out=", snapshot.timedOut, " received=", bytes.size(),
            " initial expected=", expected.size(), " first difference offset=", offset,
            " contains authorization=", containsAuthorization(bytes));
}

std::shared_ptr<std::atomic<bool>> observeQuote(api::Endpoint &endpoint,
                                                std::shared_ptr<api::Subscription<q::events::Quote>> &subscription) {
    auto received = std::make_shared<std::atomic<bool>>(false);
    subscription = endpoint.getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([received](auto quotes) {
        if (!quotes.empty() && quotes.front().symbol == u"IBM")
            received->store(true);
    });

    return received;
}

bool awaitQuote(const std::atomic<bool> &received, std::chrono::steady_clock::time_point deadline) {
    while (!received.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(2ms);

    return received.load();
}
} // namespace

TEST_CASE("Feed owns explicit TLS inputs and delivers Quote only from the verified source") {
    auto server = quotePeer();
    api::ApiRuntime runtime(runtimeOptions());
    auto options = endpointOptions();
    auto endpoint = runtime.createEndpoint(options);
    options.tls.reset();
    CHECK_THROWS_AS(endpoint->connectTape("unused.qtp"), api::Error);
    auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    const auto received = std::make_shared<std::atomic<bool>>(false);
    subscription->addEventListener([received](auto quotes) {
        if (!quotes.empty() && quotes.front().symbol == u"IBM")
            received->store(true);
    });
    subscription->addSymbols("IBM");
    endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
    const auto deadline = std::chrono::steady_clock::now() + 8s;
    const auto connected = endpoint->awaitState(api::EndpointState::CONNECTED, deadline);
    INFO("endpoint stage=" << endpoint->getDiagnostic().stage);

    REQUIRE(connected.status == api::WaitStatus::REACHED);

    while (!received->load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(2ms);

    REQUIRE(received->load());
    const auto stats = endpoint->getStatistics().connection;
    CHECK(stats.sourceEpoch == 1);
    CHECK(stats.readBytes.value > 0);
    CHECK(stats.readMessages.availability == q::core::StatisticAvailability::UNSUPPORTED);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK(closed.drained);
    CHECK_FALSE(closed.hadGap);
    CHECK_FALSE(closed.deadlineExpired);
    const auto final = endpoint->getStatistics();
    CHECK(final.final);
    CHECK(final.connection.closedConnections.value == 1);
    CHECK_FALSE(final.connection.sourceEpoch);
    const auto peer = server.stop();
    CHECK(peer.handshakeComplete);
    CHECK(peer.sniLocalhost);
}

TEST_CASE("Feed TLS rejects an untrusted chain or mismatched SAN before delivery") {
    for (const bool wrongIdentity : {false, true}) {
        CAPTURE(wrongIdentity);
        auto server = quotePeer();
        api::ApiRuntime runtime(runtimeOptions());
        auto options = endpointOptions();

        if (wrongIdentity)
            options.tls->referenceIdentity = "wrong.example";
        else
            options.tls->trustPem = tls::read("other-ca.pem");

        auto endpoint = runtime.createEndpoint(std::move(options));
        const auto received = std::make_shared<std::atomic<bool>>(false);
        auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addSymbols("IBM");
        subscription->addEventListener([received](auto) {
            received->store(true);
        });
        endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
        const auto failed = endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 8s);
        REQUIRE(failed.status == api::WaitStatus::REACHED);
        const auto diagnostic = endpoint->getDiagnostic();
        INFO("endpoint stage=" << diagnostic.stage);
        CHECK(diagnostic.code == api::ErrorCode::PROTOCOL_FAILURE);
        CHECK(diagnostic.stage.starts_with("tls-"));
        CHECK_FALSE(received->load());
        CHECK_FALSE(endpoint->getStatistics().connection.sourceEpoch);
        CHECK(endpoint->getStatistics().connection.establishedConnections.value == 0);
        static_cast<void>(endpoint->closeAndAwaitTermination());
    }
}

TEST_CASE("Feed TLS refuses an insufficient whole-run allowance before native connect") {
    auto server = quotePeer();
    api::ApiRuntime runtime(runtimeOptions());
    auto options = endpointOptions();
    options.tls->residentBytes = 1;
    auto endpoint = runtime.createEndpoint(std::move(options));
    endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
    const auto failed = endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 8s);
    REQUIRE(failed.status == api::WaitStatus::REACHED);
    const auto diagnostic = endpoint->getDiagnostic();
    CHECK(diagnostic.code == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(diagnostic.stage == "tls-resident-allowance");
    CHECK_FALSE(server.stop().accepted);
    static_cast<void>(endpoint->closeAndAwaitTermination());
}

TEST_CASE("Feed Basic sends an exact authorization only after verified TLS and waits for peer acknowledgement") {
    auto server = authenticatedPeer(true);
    auto provider = std::make_shared<Provider>();
    api::ApiRuntime runtime(runtimeOptions());
    auto options = endpointOptions();
    options.eventRegistry = quoteRegistry();
    options.tls->credentials = api::BasicTlsCredentials{provider};
    const auto configuration = api::EndpointConfiguration::create(options);
    CHECK(provider->calls == 0);
    auto endpoint = runtime.createEndpoint(configuration);
    CHECK(provider->calls == 0);
    options.tls.reset();
    std::shared_ptr<api::Subscription<q::events::Quote>> subscription;
    const auto received = observeQuote(*endpoint, subscription);
    endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
    const auto deadline = std::chrono::steady_clock::now() + 8s;
    const auto connected = endpoint->awaitState(api::EndpointState::CONNECTED, deadline);
    INFO("endpoint stage=" << endpoint->getDiagnostic().stage);

    if (connected.status != api::WaitStatus::REACHED) {
        static_cast<void>(endpoint->closeAndAwaitTermination());
        const auto peer = server.stop();
        reportPeer(peer, peer.firstClientBytes);
    }

    REQUIRE(connected.status == api::WaitStatus::REACHED);
    CHECK(awaitQuote(*received, deadline));
    CHECK(provider->calls == 1);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    const auto observed = server.stop();
    CHECK(observed.successfulHandshakes == 1);
    CHECK(containsAuthorization(observed.firstClientBytes));
}

TEST_CASE("Feed Basic rejects provider errors without diagnostics or wire bytes carrying the token") {
    for (const auto mode : {Provider::Mode::THROW, Provider::Mode::OVERSIZE}) {
        auto server = authenticatedPeer(false);
        auto provider = std::make_shared<Provider>();
        provider->mode = mode;
        api::ApiRuntime runtime(runtimeOptions());
        auto options = endpointOptions();
        options.tls->credentials = api::BasicTlsCredentials{provider};
        auto endpoint = runtime.createEndpoint(options);
        endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
        REQUIRE(endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 8s).status ==
                api::WaitStatus::REACHED);
        const auto diagnostic = endpoint->getDiagnostic();
        CHECK(diagnostic.code == api::ErrorCode::PROTOCOL_FAILURE);
        CHECK(diagnostic.stage == "tls-credential-result");
        CHECK(provider->calls == 1);
        static_cast<void>(endpoint->closeAndAwaitTermination());
        const auto observed = server.stop();
        CHECK(observed.successfulHandshakes == 1);
        CHECK_FALSE(containsAuthorization(observed.firstClientBytes));
    }
}

TEST_CASE("Feed Basic never invokes a provider for a certificate failure or an unchallenged peer") {
    for (const bool wrongTrust : {false, true}) {
        auto server = wrongTrust ? authenticatedPeer(false) : quotePeer();
        auto provider = std::make_shared<Provider>();
        api::ApiRuntime runtime(runtimeOptions());
        auto options = endpointOptions();
        options.tls->credentials = api::BasicTlsCredentials{provider};

        if (wrongTrust)
            options.tls->trustPem = tls::read("other-ca.pem");

        auto endpoint = runtime.createEndpoint(options);
        std::shared_ptr<api::Subscription<q::events::Quote>> subscription;
        const auto received = observeQuote(*endpoint, subscription);
        endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
        REQUIRE(endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 8s).status ==
                api::WaitStatus::REACHED);

        if (!wrongTrust)
            CHECK(endpoint->getDiagnostic().stage == "tls-authentication-ack-required");

        static_cast<void>(endpoint->closeAndAwaitTermination());
        CHECK(provider->calls == 0);
        CHECK_FALSE(received->load());
    }
}

TEST_CASE("Feed Basic drops a held provider result when endpoint close is admitted") {
    auto server = authenticatedPeer(false);
    auto provider = std::make_shared<Provider>();
    provider->mode = Provider::Mode::HOLD;
    api::ApiRuntime runtime(runtimeOptions());
    auto options = endpointOptions();
    options.tls->credentials = api::BasicTlsCredentials{provider};
    auto endpoint = runtime.createEndpoint(options);
    endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
    const bool entered = provider->awaitEntry();
    endpoint->close();
    provider->release();
    REQUIRE(entered);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    const auto observed = server.stop();
    CHECK(observed.successfulHandshakes == 1);
    CHECK_FALSE(containsAuthorization(observed.firstClientBytes));
    CHECK(provider->calls == 1);
    CHECK(endpoint->getDiagnostic().stage != "tls-credential-result");
}

TEST_CASE("Feed Basic drops reentrant stale output and authenticates the fresh verified attempt") {
    auto server = authenticatedPeer(false, true);
    auto provider = std::make_shared<Provider>();
    provider->mode = Provider::Mode::RECONNECT;
    api::ApiRuntime runtime(runtimeOptions());
    auto options = endpointOptions();
    options.eventRegistry = quoteRegistry();
    options.maximumAttempts = 2;
    options.retryDelay = 1ms;
    options.tls->credentials = api::BasicTlsCredentials{provider};
    auto endpoint = runtime.createEndpoint(options);
    provider->endpoint = endpoint;
    std::shared_ptr<api::Subscription<q::events::Quote>> subscription;
    const auto received = observeQuote(*endpoint, subscription);
    endpoint->connect("127.0.0.1:" + std::to_string(server.port()));
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    const auto connected = endpoint->awaitState(api::EndpointState::CONNECTED, deadline);
    INFO("endpoint stage=" << endpoint->getDiagnostic().stage);

    if (connected.status != api::WaitStatus::REACHED) {
        static_cast<void>(endpoint->closeAndAwaitTermination());
        const auto peer = server.stop();
        reportPeer(peer, peer.secondClientBytes);
    }

    REQUIRE(connected.status == api::WaitStatus::REACHED);
    CHECK(awaitQuote(*received, deadline));
    CHECK(provider->calls == 2);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    const auto observed = server.stop();
    CHECK(observed.successfulHandshakes == 2);
    CHECK_FALSE(containsAuthorization(observed.firstClientBytes));
    CHECK(containsAuthorization(observed.secondClientBytes));
}

TEST_CASE("A late credential cutoff discards prepared bytes without committing protocol state") {
    auto schema = tls::schema();
    auto limits = tls::settings(1).budgets;
    limits.session.credentials = q::protocol::LiveCredentialPolicy::VERIFIED_TLS_BASIC;
    auto composer = tls::writer(schema, limits);
    auto work = tls::sessionTurn();
    auto session = q::protocol::LiveSession::create(std::move(schema), std::move(composer), limits.session, work);
    REQUIRE(session);
    const auto challenge = property(u"authentication", u"LOGIN synthetic-realm");
    REQUIRE(session->feed(std::as_bytes(std::span(challenge)), work).event);
    REQUIRE(session->authentication() == q::protocol::LiveAuthentication::REQUIRED);

    {
        const auto prepared = q::protocol::detail::LiveAuthorization::prepare(*session, SYNTHETIC_TOKEN, work);
        REQUIRE(prepared);
        CHECK(session->authentication() == q::protocol::LiveAuthentication::REQUIRED);
        bool cutoff = false;
        const q::transport::detail::TlsCredentialAttempt attempt{
            &cutoff,
            [](void *owner, std::uint64_t, void *context,
               q::transport::detail::TlsCredentialAttempt::Action action) noexcept {
                if (*static_cast<const bool *>(owner))
                    return false;

                if (action)
                    action(context);

                return true;
            },
            1};
        REQUIRE(attempt.admit());
        cutoff = true;
        CHECK_FALSE(attempt.admit(&*session, [](void *owner) noexcept {
            q::protocol::detail::LiveAuthorization::commit(*static_cast<q::protocol::LiveSession *>(owner));
        }));
    }

    CHECK(session->authentication() == q::protocol::LiveAuthentication::REQUIRED);
    const auto publicOutput = session->authorize(SYNTHETIC_TOKEN, work);
    REQUIRE(publicOutput);
    CHECK(session->authentication() == q::protocol::LiveAuthentication::AUTHORIZATION_EMITTED);
}

TEST_CASE("Feed TLS refuses the plaintext OnDemand profile without opening a connection") {
    auto server = quotePeer();
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint(endpointOptions());
    const auto initial = endpoint->getState();

    try {
        endpoint->connectOnDemand(server.port(), u"IBM", std::chrono::sys_days{std::chrono::year{2024} / 2 / 12});
        FAIL("TLS endpoint accepted plaintext OnDemand");
    } catch (const api::Error &failure) {
        CHECK(failure.diagnostic().code == api::ErrorCode::UNSUPPORTED_PROFILE);
        CHECK(failure.diagnostic().stage == "ondemand-profile");
    }

    CHECK(endpoint->getState() == initial);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK_FALSE(server.stop().accepted);
}
