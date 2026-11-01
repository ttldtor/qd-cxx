// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: finite owned loopback peers and private stable-bootstrap assembly only.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "bootstrap_lease.hpp"
#include "qd/events/quote.hpp"
#include "recovery_test_peer.hpp"
#include "tcp_runtime_factory.hpp"

#ifdef ORG_TTLDTOR_QDCXX_BOOTSTRAP_TEST_TLS
#    include "tls_runtime_factory.hpp"
#    include "transport/tls_test_support.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <doctest/doctest.h>
#include <limits>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

namespace {
namespace t = org::ttldtor::qdcxx::transport;
namespace td = org::ttldtor::qdcxx::transport::detail;
namespace p = org::ttldtor::qdcxx::protocol;
namespace c = org::ttldtor::qdcxx::core;
namespace h = org::ttldtor::qdcxx::test;
using namespace std::chrono_literals;

static_assert(!std::is_copy_constructible_v<td::BootstrapLease>);
static_assert(std::is_nothrow_move_constructible_v<td::BootstrapLease>);
static_assert(std::is_nothrow_move_assignable_v<td::BootstrapLease>);
static_assert(sizeof(td::BootstrapLease) == sizeof(void *));

/** Matched owning schema/composer pair, independent of the process lifetime. */
struct Configuration {
    c::LocalSchema schema;          ///< Immutable eight-field Quote schema.
    p::SubscriptionWriter composer; ///< Exact corresponding outgoing layout.
};

/** Prepare ordinary owned protocol inputs without native activation.
 * @return Matched schema/composer that remain usable after factory refusal.
 */
Configuration configuration() {
    const c::SchemaLimits bounds{16, 128, 256, 128, 16384};
    auto defaults = org::ttldtor::qdcxx::events::defaultQuoteSchema(bounds);
    REQUIRE(defaults);
    auto schema = c::LocalSchema::create(defaults->records(), bounds, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    p::OutgoingRecord record{17, 0, u"Quote", {}};
    const auto &source = schema->records()[0];

    for (std::size_t field = 0; field < source.fields.size(); ++field) {
        const auto name = std::ranges::find_if(source.lookups, [field](const auto &lookup) {
            return lookup.fieldId == field;
        });
        REQUIRE(name != source.lookups.end());
        record.fields.push_back({name->name, source.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
    }

    p::SubscriptionWriterWork work{10000, 1000000, 1000000, 1000000};
    auto composer = p::SubscriptionWriter::create(*schema, std::span(&record, 1), {bounds, 16, 128, 128, 16384},
                                                  {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, work);
    REQUIRE(composer);

    return {std::move(*schema), std::move(*composer)};
}

/** Supply finite ordinary session limits from the supported controlled stream profile.
 * @return Credential-free plain session settings.
 */
p::LiveLimits sessionLimits() {
    p::LiveLimits result{};
    result.frame = {8192, 8201};
    result.protocol = {128, 64, {8192, 1024}, 16384};
    result.descriptions = {8192, 32, 32, 256, {8192, 1024}, 8192};
    result.context = {{16, 128, 256, 128, 16384}, 32, {512, 1024, 16384}};
    result.data = {8192, 64,   512,   64,    512,   8192, {8192, 128, 128}, {8192, 1024}, 8192, 8192, 8192,
                   8192, 8192, 16384, 16384, 16384, 64};
    result.residentBytes = 4 * 1024 * 1024;
    result.owners = 128;
    result.snapshotBytes = 65536;
    result.scratchBytes = 65536;
    result.eventBytes = 65536;
    result.outputBytes = 8192;
    result.credentials = p::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    result.fieldTime = p::FieldTimePolicy::DISABLED;
    result.events = p::DataEventPolicy::BINARY_CATALOG_V1;
    result.historySnapshot = true;

    return result;
}

/** Supply finite explicit numeric-loopback settings.
 * @param port Owned peer listener port, or an unused numeric test port.
 * @return Ordinary native settings without DNS discovery.
 */
t::TcpOptions options(std::uint16_t port = 4321) {
    return {"127.0.0.1", port, {}, 53, 1s, 1s, 5s, 5s, 5ms, {64, 7, 64, 16, 65536, 8192, 8 * 1024 * 1024}};
}

/** Supply fresh finite pure-session/native preparation credits.
 * @return Independent caller-owned credits.
 */
p::LiveWork sessionWork() {
    return {10000, 10000000, 10000000, 10000000};
}

/** Advance a real native run until its predicate succeeds or its finite test deadline expires.
 * @tparam Predicate Checked stopping condition.
 * @param runtime Owned run on its creating thread.
 * @param predicate Condition observed before every caller-driven turn.
 */
template <typename Predicate> void until(t::TcpRuntime &runtime, Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        t::TcpWork native{128, 128, 10000000, 128, 2, 131070, 1000000000};
        auto protocol = sessionWork();
        REQUIRE(runtime.poll(native, protocol));
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(predicate());
}

/** Restore a deliberately saturated counter even when a test assertion aborts its current scope. */
struct CounterRestore {
    std::atomic_size_t &count; ///< Test-owned access to stable internal process bookkeeping.
    std::size_t previous;      ///< Real count restored before token destruction.

    /** Restore without SDK calls or callbacks. */
    ~CounterRestore() {
        count.store(previous);
    }
};
} // namespace

TEST_CASE("stable bootstrap lease moves and assignment transfer exactly one counted borrow") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto acquired = td::BootstrapLease::acquire(*bootstrap);
    REQUIRE(acquired);
    auto *state = acquired->state();
    REQUIRE(state);
    CHECK(state->borrowers.load() == 1);
    {
        auto token = std::move(*acquired);
        CHECK_FALSE(static_cast<bool>(*acquired));
        CHECK(acquired->state() == nullptr);
        CHECK(token.state() == state);
        token = std::move(token);
        CHECK(token.state() == state);
        auto replacement = td::BootstrapLease::acquire(*bootstrap);
        REQUIRE(replacement);
        CHECK(state->borrowers.load() == 2);
        *replacement = std::move(token);
        CHECK_FALSE(static_cast<bool>(token));
        CHECK(replacement->state() == state);
        CHECK(state->borrowers.load() == 1);
    }
    CHECK(state->borrowers.load() == 0);
}

TEST_CASE("stable bootstrap lease spans a facade move with no native attempt then a real TCP connection") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    std::optional<t::RuntimeBootstrap> moved;
    auto replacement = t::RuntimeBootstrap::create();
    REQUIRE(replacement);
    {
        auto lease = td::BootstrapLease::acquire(*bootstrap);
        REQUIRE(lease);
        auto *state = lease->state();
        moved.emplace(std::move(*bootstrap));
        *replacement = std::move(*moved);
        CHECK(state->borrowers.load() == 1);
        CHECK_FALSE(td::BootstrapLease::acquire(*bootstrap));
        CHECK_FALSE(td::BootstrapLease::acquire(*moved));
        h::TcpPeerConfig peerConfig;
        peerConfig.sessions.push_back({{}, {}, 0, {}, true});
        h::OwnedTcpPeer peer(std::move(peerConfig));
        REQUIRE(peer.port() != 0);
        {
            auto source = configuration();
            auto work = sessionWork();
            auto runtime = td::TcpRuntimeFactory::create(*lease, std::move(source.schema), std::move(source.composer),
                                                         sessionLimits(), options(peer.port()), work);
            REQUIRE(runtime);
            CHECK(state->borrowers.load() == 2);
            REQUIRE(runtime->start());
            until(*runtime, [&] {
                return runtime->state() == t::TcpState::CONNECTED && peer.snapshot().sessions[0].accepted;
            });
            REQUIRE(runtime->requestClose());
            until(*runtime, [&] {
                return runtime->state() == t::TcpState::CLOSED;
            });
            CHECK(state->borrowers.load() == 2);
        }
        CHECK(state->borrowers.load() == 1);
    }
}

TEST_CASE("stable bootstrap lease preserves TCP refusal order and caller inputs after facade or token move") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = t::RuntimeBootstrap::create();
    REQUIRE(owner);
    auto acquired = td::BootstrapLease::acquire(*bootstrap);
    REQUIRE(acquired);
    auto token = std::move(*acquired);
    *owner = std::move(*bootstrap);
    auto source = configuration();
    auto noWork = p::LiveWork{};
    auto rejected = t::TcpRuntime::create(*bootstrap, std::move(source.schema), std::move(source.composer),
                                          sessionLimits(), options(), noWork);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().stage == "runtime-create-attempt");
    CHECK(source.composer.matchesSchema(source.schema));
    auto work = sessionWork();
    const auto before = work;
    rejected = td::TcpRuntimeFactory::create(*acquired, std::move(source.schema), std::move(source.composer),
                                             sessionLimits(), options(), work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().stage == "runtime-configuration");
    CHECK(work.operations == before.operations - 1);
    CHECK(work.bytes == before.bytes);
    CHECK(work.visits == before.visits);
    CHECK(source.composer.matchesSchema(source.schema));
    auto config = options();
    config.limits.contextBytes = 1;
    work = sessionWork();
    rejected = td::TcpRuntimeFactory::create(token, std::move(source.schema), std::move(source.composer),
                                             sessionLimits(), config, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == t::TcpError::ALLOCATION_FAILURE);
    CHECK(token.state()->borrowers.load() == 1);
    CHECK(source.composer.matchesSchema(source.schema));
}

TEST_CASE("stable bootstrap lease refuses saturated counts without wrapping or consuming inputs") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto lease = td::BootstrapLease::acquire(*bootstrap);
    REQUIRE(lease);
    auto &count = lease->state()->borrowers;
    CounterRestore restore{count, count.load()};
    count.store(std::numeric_limits<std::size_t>::max());
    const auto rejected = td::BootstrapLease::acquire(*bootstrap);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().stage == "bootstrap-lease-borrowers");
    CHECK(count.load() == std::numeric_limits<std::size_t>::max());
    auto source = configuration();
    auto work = sessionWork();
    auto native = td::TcpRuntimeFactory::create(*lease, std::move(source.schema), std::move(source.composer),
                                                sessionLimits(), options(), work);
    REQUIRE_FALSE(native);
    CHECK(native.error().stage == "bootstrap-lease-borrowers");
    CHECK(count.load() == std::numeric_limits<std::size_t>::max());
    CHECK(source.composer.matchesSchema(source.schema));
}

TEST_CASE("stable bootstrap permits a different serialized native runtime owner thread") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto lease = td::BootstrapLease::acquire(*bootstrap);
    REQUIRE(lease);
    auto source = configuration();
    bool prepared = false;
    std::thread worker([&] {
        auto work = sessionWork();
        auto runtime = td::TcpRuntimeFactory::create(*lease, std::move(source.schema), std::move(source.composer),
                                                     sessionLimits(), options(), work);
        prepared = runtime.has_value();
    });
    worker.join();
    CHECK(prepared);
    CHECK(lease->state()->borrowers.load() == 1);
}

#ifdef ORG_TTLDTOR_QDCXX_BOOTSTRAP_TEST_TLS
TEST_CASE("stable bootstrap lease creates verified TLS after a facade move during an idle attempt gap") {
    namespace x = org::ttldtor::qdcxx::test::tls;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    std::optional<t::RuntimeBootstrap> owner;
    auto movedOwner = t::RuntimeBootstrap::create();
    REQUIRE(movedOwner);
    auto lease = td::BootstrapLease::acquire(*bootstrap);
    REQUIRE(lease);
    auto *state = lease->state();
    owner.emplace(std::move(*bootstrap));
    *movedOwner = std::move(*owner);
    auto peer = h::TlsTestPeer::start(x::peerOptions());
    REQUIRE(peer);
    {
        auto local = x::schema();
        auto input = x::settings(peer->port());
        auto endpoint = x::d::EndpointOptions::create(local, input);
        REQUIRE(endpoint);
        auto composer = x::writer(local, input.budgets);
        const auto config = x::options();
        const auto need = t::tlsRequirements(config.limits, config.host);
        REQUIRE(need);
        auto tlsWork = x::tlsTurn(*need);
        auto work = x::sessionTurn();
        auto runtime = td::TlsRuntimeFactory::create(*lease, *endpoint, std::move(composer), config, tlsWork, work);
        REQUIRE(runtime);
        CHECK(state->borrowers.load() == 2);
        REQUIRE(runtime->start());
        x::until(*runtime, [&] {
            return runtime->state() == t::TlsState::CONNECTED && peer->snapshot().handshakeComplete;
        });
        x::close(*runtime);
        CHECK(state->borrowers.load() == 2);
    }
    CHECK(state->borrowers.load() == 1);
}

TEST_CASE("TLS moved-bootstrap late refusal keeps exact TLS factory debit and usable composer") {
    namespace x = org::ttldtor::qdcxx::test::tls;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = std::move(*bootstrap);
    auto local = x::schema();
    auto input = x::settings(4321);
    auto endpoint = x::d::EndpointOptions::create(local, input);
    REQUIRE(endpoint);
    auto composer = x::writer(local, input.budgets);
    const auto config = x::options();
    const auto need = t::tlsRequirements(config.limits, config.host);
    REQUIRE(need);
    auto tlsWork = t::TlsWork{need->shape, need->factoryAdmission};
    auto work = x::sessionTurn();
    const auto before = work;
    auto result = t::TlsRuntime::create(*bootstrap, *endpoint, std::move(composer), config, tlsWork, work);
    REQUIRE_FALSE(result);
    CHECK(result.error().stage == "runtime-configuration");
    CHECK(work.operations == before.operations - 1);
    CHECK(work.bytes == before.bytes);
    CHECK(work.visits == before.visits);
    CHECK(tlsWork.remaining == t::TlsWorkVector{});
    auto writerWork = input.budgets.writerPreparation;
    const std::array<std::int32_t, 1> ids{17};
    CHECK(composer.describe(ids, writerWork));
}
#endif
