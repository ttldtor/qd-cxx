// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: explicit synthetic settings, no sockets or credential acquisition.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/client/endpoint_options.hpp"
#include "qd/events/quote.hpp"

#include <atomic>
#include <climits>
#include <doctest/doctest.h>
#include <limits>
#include <thread>
#include <vector>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace d = root::client;
using namespace std::chrono_literals;

/** Construct a full caller schema independent of a runtime.
 * @return Complete validated default Quote with its own immutable provenance.
 */
c::LocalSchema schema() {
    auto value = root::events::defaultQuoteSchema({16, 128, 256, 128, 16384});
    REQUIRE(value);

    return std::move(*value);
}

/** Prepare explicit controlled settings for a synthetic numeric endpoint.
 * @return Value inputs with exact current example defaults and explicit host/port.
 */
d::EndpointSettings settings() {
    auto value = d::controlledStreamSettings();
    REQUIRE(value);
    value->host = "127.0.0.1";
    value->port = 4321;

    return std::move(*value);
}

/** Synthetic provider whose callback must never be invoked by configuration. */
class Provider final : public d::CredentialProvider {
    std::atomic<unsigned> &calls_; ///< Borrowed test counter, outliving every retained provider.
    public:
    /** Retain an externally owned counter.
     * @param calls Counter kept alive through provider destruction.
     */
    explicit Provider(std::atomic<unsigned> &calls) : calls_(calls) {
    }

    /** Observe forbidden invocation without producing any credential.
     * @param destination Borrowed output, deliberately untouched.
     * @return Static unavailable status.
     */
    std::expected<std::size_t, d::CredentialError> basicToken(std::span<char16_t> destination) override {
        (void)destination;
        ++calls_;

        return std::unexpected(d::CredentialError::UNAVAILABLE);
    }
};

/** Prepare explicit structurally valid TLS handoff using synthetic non-certificate text.
 * @param calls External callback counter.
 * @return Settings that must never be mistaken for cryptographic verification.
 */
d::EndpointSettings secure(std::atomic<unsigned> &calls) {
    auto value = settings();
    value.security = d::EndpointSecurity::TLS;
    value.tls.emplace();
    value.tls->referenceIdentity = "example.invalid";
    value.tls->trustPem = "synthetic-not-a-certificate";
    value.tls->handshakeTimeout = 200ms;
    value.tls->closeTimeout = 300ms;
    value.tls->authenticationTimeout = 400ms;
    value.tls->tokenUnits = 128;
    value.tls->credentials = std::make_shared<Provider>(calls);

    return value;
}
} // namespace

TEST_CASE("endpoint snapshots own independent settings and schema across actual thread release") {
    auto local = schema();
    auto input = settings();
    auto first = d::EndpointOptions::create(local, input);
    REQUIRE(first);
    input.host = "::1";
    input.port = 7654;
    input.timeouts.read = 77ms;
    auto second = d::EndpointOptions::create(local, input);
    REQUIRE(second);
    input.host.clear();
    CHECK(first->settings().host == "127.0.0.1");
    CHECK(second->settings().host == "::1");
    CHECK(first->schema().recordContext(0).matches(local.recordContext(0)));
    std::atomic<unsigned> reads{0};
    std::vector<std::thread> threads;

    for (unsigned index = 0; index < 4; ++index) {
        threads.emplace_back([copy = *first, &reads]() mutable {
            for (unsigned pass = 0; pass < 1000; ++pass) {
                auto retained = copy;

                if (retained.settings().port == 4321 && retained.schema().records().size() == 1) {
                    ++reads;
                }
            }
        });
    }

    first = std::unexpected(d::EndpointDiagnostic{});

    for (auto &thread : threads) {
        thread.join();
    }

    CHECK(reads == 4000);
    CHECK(second->settings().timeouts.read == 77ms);
}

TEST_CASE("security handoff owns finite values but never acquires or diagnoses secrets") {
    auto local = schema();
    std::atomic<unsigned> calls{0};
    auto input = secure(calls);
    std::weak_ptr<d::CredentialProvider> provider = input.tls->credentials;
    auto snapshot = d::EndpointOptions::create(local, input);
    REQUIRE(snapshot);
    CHECK_FALSE(snapshot->requirePlainStream());
    input.tls.reset();
    CHECK_FALSE(provider.expired());
    auto copy = *snapshot;
    snapshot = std::unexpected(d::EndpointDiagnostic{});
    CHECK(copy.settings().tls->trustPem == "synthetic-not-a-certificate");
    CHECK(calls == 0);
    auto invalid = settings();
    invalid.tls = copy.settings().tls;
    auto rejected = d::EndpointOptions::create(local, invalid);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().stage == "endpoint-security");
}

TEST_CASE("TLS identity and applicable explicit bounds validate before any copied state") {
    auto local = schema();
    std::atomic<unsigned> calls{0};
    auto input = secure(calls);

    for (auto identity : {"", "*.example", "user@example", "host%zone", "[::1]", "http://host", "a..b", "a{p}"}) {
        input.tls->referenceIdentity = identity;
        CHECK_FALSE(d::EndpointOptions::create(local, input));
    }

    input.tls->referenceIdentity = std::string(255, 'a');
    CHECK_FALSE(d::EndpointOptions::create(local, input));
    input.tls->referenceIdentity = std::string("a\0b", 3);
    CHECK_FALSE(d::EndpointOptions::create(local, input));
    input.tls->referenceIdentity = std::string(63, 'a') + "." + std::string(63, 'b') + "." + std::string(63, 'c') +
                                   "." + std::string(61, 'd') + ".";
    REQUIRE(input.tls->referenceIdentity.size() == 254);
    CHECK(d::EndpointOptions::create(local, input));
    input.tls->handshakeTimeout.reset();
    CHECK_FALSE(d::EndpointOptions::create(local, input));
    input.tls->handshakeTimeout = 1ms;
    input.tls->credentials.reset();
    CHECK_FALSE(d::EndpointOptions::create(local, input));
    input.tls->authenticationTimeout.reset();
    input.tls->tokenUnits.reset();
    CHECK(d::EndpointOptions::create(local, input));
    CHECK(calls == 0);
}

TEST_CASE("native units arithmetic and handoff unsupported profiles are truthful") {
    auto local = schema();
    auto input = settings();

    for (auto timeout : {0ms, -1ms, std::chrono::milliseconds(static_cast<std::int64_t>(INT_MAX) + 1),
                         std::chrono::milliseconds::max()}) {
        input.timeouts.connect = timeout;
        CHECK_FALSE(d::EndpointOptions::create(local, input));
    }

    input.timeouts.connect = std::chrono::milliseconds(INT_MAX);
    CHECK(d::EndpointOptions::create(local, input));
    input = settings();
    input.budgets.client.commandSlots = std::numeric_limits<std::size_t>::max();
    CHECK_FALSE(d::EndpointOptions::create(local, input));
    input = settings();
    input.host = "example.invalid";
    auto dns = d::EndpointOptions::create(local, input);
    REQUIRE(dns);
    CHECK_FALSE(dns->requirePlainStream());
    input = settings();
    input.retry = {2, 10ms};
    auto retry = d::EndpointOptions::create(local, input);
    REQUIRE(retry);
    CHECK_FALSE(retry->requirePlainStream());
    input.security = static_cast<d::EndpointSecurity>(999);
    CHECK_FALSE(d::EndpointOptions::create(local, input));
    CHECK_FALSE(d::controlledStreamSettings(0ms));
    CHECK(d::controlledStreamSettings(600000ms));
}
