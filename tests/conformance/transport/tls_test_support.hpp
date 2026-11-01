// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/quote.hpp"
#include "qd/transport/tls_runtime.hpp"
#include "tls_test_peer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <thread>
#include <utility>

namespace org::ttldtor::qdcxx::test::tls {

namespace t = transport;
namespace p = protocol;
namespace c = core;
namespace d = client;
using namespace std::chrono_literals;

/** Resolve one explicitly configured immutable ordinary fixture.
 * @param name Relative filename within the configured fixture directory.
 * @return Absolute fixture path; no ambient configuration or source lookup.
 */
inline std::filesystem::path fixture(std::string_view name) {
    return std::filesystem::path(ORG_TTLDTOR_QDCXX_TLS_TEST_FIXTURES) / name;
}

/** Read an ordinary explicit fixture unchanged; no fixture or credential content is logged.
 * @param name Relative ordinary fixture filename.
 * @return Owned exact binary bytes.
 */
inline std::string read(std::string_view name) {
    std::ifstream input(fixture(name), std::ios::binary);
    REQUIRE(input.good());

    return {std::istreambuf_iterator<char>(input), {}};
}

/** Make synthetic finite same-image ceilings for this controlled test executable only.
 * Every SDK user is this test's serialized factory/runtime path; the server runs in another image.
 * These deliberately loose declarations are test inputs, never detected maxima or product defaults.
 * @return Explicit twenty-coordinate host bounds, including finite failed-creation history.
 */
inline t::TlsHostLimits host() noexcept {
    return {5000, 16, 64, 64, 128, 32768, 512, 8, 128, 64, 8, 16, 4, 8, 65536, 4096, 65536, 64, 8, 2048};
}

/** Compute explicit proof/input bounds and whole-run allowances for the controlled tests.
 * @return Checked finite allowances; logical allowance is not an actual allocation/RSS measurement.
 */
inline t::TlsOptions options() {
    t::TlsOptions result;
    result.limits = {524288, 256, 65536, 65536, 255};
    result.host = host();
    const auto need = t::tlsRequirements(result.limits, result.host);
    REQUIRE(need);
    result.sdkInstanceBytes = need->sdk.instanceBytes;
    result.sdkSharedBytes = need->sdk.sharedBytes;
    result.residentBytes = need->sdk.totalBytes + 32 * 1024 * 1024;

    return result;
}

/** Supply finite maximal coordinate values for a single explicitly renewed test turn.
 * @param requirements Independently recomputed/owned runtime qualification.
 * @return Credits with the exact owned shape; UINT64_MAX is a finite test allowance.
 */
inline t::TlsWork tlsTurn(const t::TlsRequirements &requirements) noexcept {
    t::TlsWork result;
    result.shape = requirements.shape;
    result.remaining.units.fill(std::numeric_limits<std::uint64_t>::max());

    return result;
}

/** Supply an explicit fresh native progress turn.
 * @return Finite native/DNS credits, renewed only by the test caller.
 */
inline t::TcpWork nativeTurn() noexcept {
    return {128, 128, 10000000, 128, 2, 131070, 1000000000};
}

/** Supply an explicit fresh pure-session turn.
 * @return Finite preparation/control/input credits.
 */
inline p::LiveWork sessionTurn() noexcept {
    return {10000, 10000000, 10000000, 100 * 1024 * 1024};
}

/** Prepare actual eight-field Quote configuration and real TLS snapshot values.
 * @param port Owned loopback listener port.
 * @param provider Optional explicit provider; never invoked here.
 * @return Structurally validated settings input with ordinary explicit trust and DNS reference identity.
 */
inline d::EndpointSettings settings(std::uint16_t port, std::shared_ptr<d::CredentialProvider> provider = {}) {
    auto source = d::controlledStreamSettings(5s);
    REQUIRE(source);
    source->host = "127.0.0.1";
    source->port = port;
    source->security = d::EndpointSecurity::TLS;
    source->tls.emplace();
    source->tls->referenceIdentity = "localhost";
    source->tls->trustPem = read("ca.pem");
    source->tls->handshakeTimeout = 2s;
    source->tls->closeTimeout = 1s;
    source->tls->credentials = std::move(provider);

    if (source->tls->credentials) {
        source->tls->authenticationTimeout = 2s;
        source->tls->tokenUnits = 128;
    }

    source->budgets.session.events = p::DataEventPolicy::BINARY_CATALOG_V1;
    source->budgets.session.historySnapshot = true;

    return std::move(*source);
}

/** Build a matched outgoing writer from every real Quote field; no reduced schema is substituted.
 * @param schema Complete owned validated schema, borrowed during preparation only.
 * @param bounds Explicit settings/composer bounds.
 * @return Usable independent writer sharing the immutable schema provenance.
 */
inline p::SubscriptionWriter writer(const c::LocalSchema &schema, const d::EndpointBudgets &bounds) {
    p::OutgoingRecord declaration{17, 0, u"Quote", {}};
    const auto &record = schema.records()[0];

    for (std::size_t field = 0; field < record.fields.size(); ++field) {
        const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
            return entry.fieldId == field;
        });
        REQUIRE(lookup != record.lookups.end());
        declaration.fields.push_back(
            {lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
    }

    auto work = bounds.writerPreparation;
    auto result = p::SubscriptionWriter::create(schema, std::span(&declaration, 1), bounds.layout, bounds.writer, work);
    REQUIRE(result);

    return std::move(*result);
}

/** Prepare the actual immutable binary Quote schema.
 * @return Independently owned complete eight-field schema.
 */
inline c::LocalSchema schema() {
    constexpr c::SchemaLimits bounds{16, 128, 256, 128, 16384};
    auto defaults = events::defaultQuoteSchema(bounds);
    REQUIRE(defaults);
    auto result = c::LocalSchema::create(defaults->records(), bounds, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Construct a real unactivated runtime using only explicit immutable inputs.
 * @param bootstrap Application process owner outliving the returned runtime.
 * @param input Explicit settings; external provider state remains caller-owned.
 * @param config Explicit TLS/DNS allowances; the default is the finite controlled test fixture.
 * @return Move-only prepared TLS attempt; no network or provider call occurs here.
 */
inline t::TlsRuntime runtime(t::RuntimeBootstrap &bootstrap, const d::EndpointSettings &input,
                             t::TlsOptions config = options()) {
    auto local = schema();
    auto endpoint = d::EndpointOptions::create(local, input);
    REQUIRE(endpoint);
    auto composer = writer(local, input.budgets);
    const auto requirements = t::tlsRequirements(config.limits, config.host);
    REQUIRE(requirements);
    auto tlsWork = tlsTurn(*requirements);
    auto sessionWork = sessionTurn();

    if (!config.dns.servers.empty()) {
        const auto dns = t::TcpRuntime::dnsPreparationWorkRequirement(config.dns.servers.size());
        REQUIRE(dns);
        REQUIRE(dns->bytes <= std::numeric_limits<std::size_t>::max() - sessionWork.bytes);
        REQUIRE(dns->visits <= std::numeric_limits<std::size_t>::max() - sessionWork.visits);
        sessionWork.bytes += dns->bytes;
        sessionWork.visits += dns->visits;
    }

    auto result = t::TlsRuntime::create(bootstrap, *endpoint, std::move(composer), config, tlsWork, sessionWork);

    if (!result) {
        INFO("TLS factory category=" << static_cast<int>(result.error().error) << " stage=" << result.error().stage
                                     << " sdk=" << result.error().sdkCode << " verify=" << result.error().verifyCode);
        REQUIRE(result);
    }

    return std::move(*result);
}

/** Prepare explicit ordinary isolated-peer settings; callers may customize scripts/deadlines.
 * @param version Exact TLS version for the server.
 * @param mode Finite post-script behavior.
 * @return Owned helper inputs; no process is started until TlsTestPeer::start.
 */
inline PeerOptions peerOptions(PeerTlsVersion version = PeerTlsVersion::TLS_1_3,
                               PeerCloseMode mode = PeerCloseMode::HOLD) {
    const auto control = std::filesystem::path(ORG_TTLDTOR_QDCXX_TLS_TEST_CONTROL);
    std::filesystem::create_directories(control);
    PeerOptions result;
    result.childExecutable = std::filesystem::path(ORG_TTLDTOR_QDCXX_TLS_TEST_PEER);
    result.certificateFile = fixture("server.pem");
    result.privateKeyFile = fixture("server.key");
    result.controlRoot = control;
    result.tlsVersion = version;
    result.closeMode = mode;
    result.overallTimeout = 10s;

    return result;
}

/** Perform one real caller-selected progress turn, rejecting unexpected static/numeric failures.
 * @param value Current runtime, retained by the caller through the operation.
 * @return Owning single-poll result.
 */
inline t::TlsPollResult poll(t::TlsRuntime &value) {
    auto native = nativeTurn();
    const auto dns = value.dnsProgressWorkRequirement();
    native.dnsCalls = (std::max)(native.dnsCalls, dns.dnsCalls);
    native.dnsPackets = (std::max)(native.dnsPackets, dns.dnsPackets);
    native.dnsBytes = (std::max)(native.dnsBytes, dns.dnsBytes);
    native.dnsUnits = (std::max)(native.dnsUnits, dns.dnsUnits);
    auto tls = tlsTurn(value.requirements());
    auto session = sessionTurn();
    auto result = value.poll(native, tls, session);

    if (!result) {
        INFO("TLS poll category=" << static_cast<int>(result.error().error) << " stage=" << result.error().stage);
        REQUIRE(result);
    }

    return std::move(*result);
}

/** Pump real runtime progress until a test condition or finite deadline.
 * @tparam Predicate Callable returning the explicit stopping condition.
 * @param value Owned runtime on its serialized owner.
 * @param predicate Condition checked before each turn and once after timeout.
 * @param timeout Finite test deadline, unrelated to an SDK hard-time promise.
 */
template <typename Predicate>
inline void until(t::TlsRuntime &value, Predicate predicate, std::chrono::milliseconds timeout = 5s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        (void)poll(value);
        std::this_thread::sleep_for(1ms);
    }

    if (!predicate()) {
        const auto terminal = value.terminal();
        INFO("TLS final state=" << static_cast<int>(value.state())
                                << " category=" << (terminal ? static_cast<int>(terminal->primary.error) : -1)
                                << " stage=" << (terminal ? terminal->primary.stage : "active"));
        REQUIRE(predicate());
    }
}

/** Request explicit abort and observe native callback quiescence.
 * @param value Owned runtime on its serialized owner.
 */
inline void close(t::TlsRuntime &value) {
    REQUIRE(value.requestClose());
    until(value, [&] {
        return value.state() == t::TlsState::CLOSED;
    });
    REQUIRE(value.terminal());
}

} // namespace org::ttldtor::qdcxx::test::tls
