// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "ipf_http_peer.hpp"
#include "profile_access.hpp"
#include "qd/ipf/instrument_profile_connection.hpp"
#include "transport/tls_test_support.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <doctest/doctest.h>
#include <fstream>
#include <mutex>
#include <new>
#include <thread>

namespace {
thread_local bool rejectAllocation = false;
thread_local std::size_t rejectedAllocations = 0;
} // namespace

/** Instrument only the test caller; fixture/SDK workers keep their ordinary allocator.
 * @param bytes Requested storage.
 * @return Ordinary storage, or bad_alloc at the armed admission boundary.
 */
void *operator new(std::size_t bytes) {
    if (rejectAllocation) {
        ++rejectedAllocations;
        throw std::bad_alloc{};
    }

    if (auto result = std::malloc(bytes ? bytes : 1)) {
        return result;
    }

    throw std::bad_alloc{};
}

/** @param bytes Array storage request.
 * @return Same instrumented allocation.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** @param memory Matching owned allocation. */
void operator delete(void *memory) noexcept {
    std::free(memory);
}

/** @param memory Matching owned allocation.
 * @param bytes Original size, unused.
 */
void operator delete(void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
    std::free(memory);
}

/** @param memory Matching array allocation. */
void operator delete[](void *memory) noexcept {
    std::free(memory);
}

/** @param memory Matching array allocation.
 * @param bytes Original size, unused.
 */
void operator delete[](void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
    std::free(memory);
}
namespace i = org::ttldtor::qdcxx::ipf;
namespace t = org::ttldtor::qdcxx::transport;
namespace h = org::ttldtor::qdcxx::test;
using namespace std::chrono_literals;

namespace {
/** Wait only in finite tests, never as the connection's application contract.
 * @param predicate Detached thread-safe observation.
 * @return True when reached before the four-second fixture deadline.
 */
template <class Predicate> bool reached(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 4s;

    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }

    return predicate();
}

/** Build a bounded synthetic response.
 * @param body Exact IPF bytes.
 * @param fields Complete additional header lines.
 * @return Independent finite HTTP bytes.
 */
std::string response(std::string_view body, std::string_view fields = {}) {
    return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + std::string(fields) + "\r\n" +
           std::string(body);
}

/** Build a numeric controlled source.
 * @param port Owned loopback endpoint.
 * @return Source without ambient DNS.
 */
std::string url(std::uint16_t port) {
    return "http://127.0.0.1:" + std::to_string(port) + "/profiles.ipf";
}

/** Own an independent collector.
 * @return Shared explicit repository retained through worker retirement.
 */
std::shared_ptr<i::InstrumentProfileCollector> collector() {
    return i::InstrumentProfileCollector::create();
}

/** Prepare one borrowed worker runtime.
 * @param bootstrap Explicit main/pre-thread owner.
 * @param options Finite controlled settings.
 * @return Creator-owned runtime.
 */
std::unique_ptr<i::InstrumentProfileConnectionRuntime> runtime(t::RuntimeBootstrap &bootstrap,
                                                               i::IpfConnectionRuntimeOptions options = {}) {
    auto created = i::InstrumentProfileConnectionRuntime::create(bootstrap, options);

    if (!created) {
        INFO("runtime stage: ", created.error().transfer.stage);
    }

    REQUIRE(created);

    return std::move(*created);
}

/** Capture sorted symbols without retaining facade borrows.
 * @param value Explicit repository.
 * @return Owning current symbols.
 */
std::vector<std::u16string> symbols(const std::shared_ptr<i::InstrumentProfileCollector> &value) {
    auto batch = value->snapshot();
    REQUIRE(batch);
    std::vector<std::u16string> result;

    for (const auto &revision : (*batch)->profiles()) {
        result.emplace_back(revision->profile.getSymbol());
    }

    return result;
}

constexpr std::string_view HEADER = "#STOCK::=TYPE,SYMBOL\n";
} // namespace

TEST_CASE("live connection publishes before HTTP EOF and callback can delete facade") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    options.limits.batchProfiles = 1;
    auto owner = runtime(*bootstrap, options);
    const std::string body = std::string(HEADER) + "STOCK,A\n";
    h::IpfHttpPeer peer({"HTTP/1.1 200 OK\r\nX-Live: provided\r\nContent-Length: 9999\r\n\r\n" + body}, 7, true, 1ms);
    auto model = collector();
    std::atomic<std::shared_ptr<i::InstrumentProfileConnection>> handle;
    std::atomic<bool> delivered{false};
    auto listener = model->addUpdateListener([&](auto batch) {
        if (batch->profiles().empty()) {
            return;
        }

        auto facade = handle.exchange({});

        if (facade) {
            facade->close();
        }

        delivered.store(true);
    });
    REQUIRE(listener);
    auto prepared = owner->createConnection(url(peer.port()), model);
    REQUIRE(prepared);
    handle.store(std::move(*prepared));
    auto detached = handle.load();
    auto started = detached->start();
    REQUIRE(started);
    detached.reset();
    CHECK(reached([&] {
        return delivered.load();
    }));
    CHECK(owner->closeAndAwaitTermination());
    CHECK(symbols(model) == std::vector<std::u16string>{u"A"});
    CHECK_FALSE(handle.load());
    auto requests = peer.requests();
    REQUIRE(requests.size() == 1);
    CHECK(requests.front().find("X-Live: yes\r\n") != std::string::npos);
    CHECK(requests.front().find("Accept-Encoding: identity\r\n") != std::string::npos);
}

TEST_CASE("live COMPLETE orders flush removal and sticky phase independently of EOF") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    options.limits.batchProfiles = 2;
    auto owner = runtime(*bootstrap, options);
    h::IpfHttpPeer peer({response(std::string(HEADER) + "STOCK,A\n##COMPLETE\nSTOCK,B\n##\n", "X-Live: provided\r\n")},
                        9);
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()), model);
    REQUIRE(created);
    auto connection = *created;
    std::mutex mutex;
    std::vector<i::IpfConnectionState> states;
    auto listener = connection->addStateChangeListener([&](auto, auto state) {
        std::lock_guard lock(mutex);
        states.push_back(state);
    });
    REQUIRE(listener);
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return symbols(model) == std::vector<std::u16string>{u"A", u"B"};
    }));
    CHECK(connection->observation().initialSnapshotComplete);
    CHECK(connection->getState() == i::IpfConnectionState::COMPLETED);
    connection->close();
    CHECK(owner->closeAndAwaitTermination());
    CHECK(connection->observation().retired);
    CHECK(connection->observation().initialSnapshotComplete);
    CHECK(states ==
          std::vector<i::IpfConnectionState>{i::IpfConnectionState::CONNECTING, i::IpfConnectionState::CONNECTED,
                                             i::IpfConnectionState::COMPLETED, i::IpfConnectionState::CLOSED});
    CHECK_FALSE(connection->start());
    CHECK_FALSE(owner->createConnection(url(peer.port()), model));
}

TEST_CASE("failed live pending profiles carry into next attempt generation") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    options.limits.batchProfiles = 1000;
    options.limits.attempts = 2;
    auto owner = runtime(*bootstrap, options);
    h::IpfHttpPeer peer(
        {response(std::string(HEADER) + "STOCK,A\n##\nSTOCK,B\nSTOCK,too,many\n", "X-Live: provided\r\n"),
         response(std::string(HEADER) + "STOCK,C\n##COMPLETE\n", "X-Live: provided\r\n")},
        17);
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()) + "[update=0]", model);
    REQUIRE(created);
    auto connection = *created;
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(owner->closeAndAwaitTermination());
    CHECK(connection->observation().attempts == 2);
    CHECK(symbols(model) == std::vector<std::u16string>{u"B", u"C"});
    CHECK(connection->observation().initialSnapshotComplete);
    auto requests = peer.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[1].find("If-Modified-Since:") == std::string::npos);
}

TEST_CASE("fallback unchanged date and conditional304 do not fabricate generations") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    options.limits.attempts = 3;
    auto owner = runtime(*bootstrap, options);
    constexpr std::string_view date = "Last-Modified: Sun, 06 Nov 1994 08:49:38 GMT\r\n";
    h::IpfHttpPeer peer({response(std::string(HEADER) + "STOCK,A\n", date),
                         response("not valid IPF", std::string(date) + "X-Live: provided\r\n"),
                         "HTTP/1.1 304 Not Modified\r\n\r\n"},
                        31);
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()) + "[update=0]", model);
    REQUIRE(created);
    auto connection = *created;
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(owner->closeAndAwaitTermination());
    CHECK(symbols(model) == std::vector<std::u16string>{u"A"});
    CHECK(connection->observation().capability == i::IpfLiveSupport::ABSENT);
    CHECK(connection->getLastModified().has_value());
    auto requests = peer.requests();
    REQUIRE(requests.size() == 3);
    CHECK(requests[0].find("If-Modified-Since:") == std::string::npos);
    CHECK(requests[1].find("If-Modified-Since: Sun, 06 Nov 1994 08:49:38 GMT\r\n") != std::string::npos);
    CHECK(requests[2].find("If-Modified-Since: Sun, 06 Nov 1994 08:49:38 GMT\r\n") != std::string::npos);
}

TEST_CASE("connection close from another thread cuts off blocked header wait") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = runtime(*bootstrap);
    h::IpfHttpPeer peer({""}, 1, true);
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()), model);
    REQUIRE(created);
    auto connection = *created;
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return peer.accepted() == 1;
    }));
    std::thread producer([connection] {
        connection->close();
    });
    producer.join();
    CHECK(owner->closeAndAwaitTermination());
    CHECK(connection->observation().retired);
    CHECK(connection->getState() == i::IpfConnectionState::CLOSED);
    CHECK_FALSE(connection->observation().initialSnapshotComplete);
    REQUIRE(connection->observation().diagnostic);
    CHECK(connection->observation().diagnostic->transfer.error == i::IpfTransferError::CANCELLED);
}

TEST_CASE("connection source periods follow actual bounded Java grammar") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const std::pair<std::string_view, std::int64_t> cases[] = {{"0", 0},
                                                               {"1", 1000},
                                                               {"1ms", -1},
                                                               {"1s", 1000},
                                                               {"0.5s", 500},
                                                               {"1m", 60000},
                                                               {"1h", 3600000},
                                                               {"2d", 172800000},
                                                               {"00:00:01", -1},
                                                               {"1S", 1000},
                                                               {"1.5m", -1},
                                                               {"-1s", -1},
                                                               {"1m30s", 90000},
                                                               {"PT1S", 1000},
                                                               {"inf", 9223372036854775807LL},
                                                               {"2147483647d", 185542587100800000LL},
                                                               {"9223372036854776s", 9223372036854775807LL},
                                                               {"1d9223372036854776s", -1},
                                                               {"\xd9\xa1"
                                                                "d",
                                                                86400000},
                                                               {"\xd9\xa1"
                                                                "s",
                                                                -1},
                                                               {"1\xc5\xbf", 1000},
                                                               {"\xc4\xb1nf", 9223372036854775807LL}};

    for (const auto &[text, millis] : cases) {
        CAPTURE(text);
        auto owner = runtime(*bootstrap);
        auto created = owner->createConnection("http://127.0.0.1:1/x[update=" + std::string(text) + "]", collector());
        CHECK(bool(created) == (millis >= 0));

        if (created) {
            CHECK((*created)->getUpdatePeriod() == std::chrono::milliseconds(millis));
        }

        CHECK(owner->closeAndAwaitTermination());
    }
}

TEST_CASE("live connection verified HTTPS commits only after clean native retirement") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    namespace x = h::tls;
    i::IpfConnectionRuntimeOptions options;
    options.transfer.trustPem = x::read("ca.pem");
    options.transfer.tls = x::options();
    options.transfer.tls.residentBytes += options.transfer.nativeLimits.residentBytes;
    options.transfer.tlsWork.units.fill(std::numeric_limits<std::uint64_t>::max());
    options.limits.attempts = 1;
    auto owner = runtime(*bootstrap, options);
    auto server = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::CLEAN);
    server.readHttpHeaders = true;
    const auto reply = response(std::string(HEADER) + "STOCK,A\n##COMPLETE\n",
                                "X-Live: provided\r\nLast-Modified: Sun, 06 Nov 1994 08:49:38 GMT\r\n");
    server.replyBytes.assign(reply.begin(), reply.end());
    auto peer = h::TlsTestPeer::start(server);
    REQUIRE(peer);
    auto model = collector();
    auto created =
        owner->createConnection("https://127.0.0.1:" + std::to_string(peer->port()) + "/profiles.ipf[update=0]", model);
    REQUIRE(created);
    auto connection = *created;
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(owner->closeAndAwaitTermination());
    CHECK(symbols(model) == std::vector<std::u16string>{u"A"});
    CHECK(connection->observation().initialSnapshotComplete);
    CHECK(connection->getLastModified());
    REQUIRE(connection->observation().lastAttempt);
    const auto attempt = *connection->observation().lastAttempt;
    CHECK(attempt.httpFinished);
    CHECK(attempt.nativeRetired);
    CHECK(attempt.ipfFinished);
    CHECK(attempt.envelope.finished);
    CHECK(attempt.status == i::IpfTransferStatus::SUCCESS);
}

TEST_CASE("TLS error after COMPLETE preserves profile but refuses metadata commit") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    namespace x = h::tls;
    i::IpfConnectionRuntimeOptions options;
    options.transfer.trustPem = x::read("ca.pem");
    options.transfer.tls = x::options();
    options.transfer.tls.residentBytes += options.transfer.nativeLimits.residentBytes;
    options.transfer.tlsWork.units.fill(std::numeric_limits<std::uint64_t>::max());
    options.limits.attempts = 1;
    auto owner = runtime(*bootstrap, options);
    auto server = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::ABRUPT);
    server.readHttpHeaders = true;
    const auto reply = response(std::string(HEADER) + "STOCK,A\n##COMPLETE\n",
                                "X-Live: provided\r\nLast-Modified: Sun, 06 Nov 1994 08:49:38 GMT\r\n");
    server.replyBytes.assign(reply.begin(), reply.end());
    auto peer = h::TlsTestPeer::start(server);
    REQUIRE(peer);
    auto model = collector();
    auto created =
        owner->createConnection("https://127.0.0.1:" + std::to_string(peer->port()) + "/profiles.ipf[update=0]", model);
    REQUIRE(created);
    auto connection = *created;
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(owner->closeAndAwaitTermination());
    CHECK(symbols(model) == std::vector<std::u16string>{u"A"});
    CHECK(connection->observation().initialSnapshotComplete);
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::time_point::min()) ==
          i::IpfCompletionWaitStatus::COMPLETED);
    CHECK_FALSE(connection->getLastModified());
    REQUIRE(connection->observation().lastAttempt);
    CHECK(connection->observation().lastAttempt->nativeRetired);
    CHECK(connection->observation().lastAttempt->status == i::IpfTransferStatus::FAILED);
}

TEST_CASE("connection pending unit exact and one-below grants apply before copy") {
    for (const auto units : {std::size_t(5), std::size_t(6)}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        i::IpfConnectionRuntimeOptions options;
        options.limits.batchProfiles = 1;
        options.limits.pendingUnits = units;
        auto owner = runtime(*bootstrap, options);
        h::IpfHttpPeer peer({response(std::string(HEADER) + "STOCK,A\n##COMPLETE\n", "X-Live: provided\r\n")});
        auto model = collector();
        auto created = owner->createConnection(url(peer.port()) + "[update=0]", model);
        REQUIRE(created);
        auto connection = *created;
        auto token = connection->addStateChangeListener([&](auto, auto state) {
            if (state == i::IpfConnectionState::COMPLETED) {
                connection->close();
            }
        });
        REQUIRE(token);
        REQUIRE(connection->start());
        CHECK(reached([&] {
            return connection->observation().retired;
        }));
        CHECK(owner->closeAndAwaitTermination());
        CHECK(symbols(model).size() == (units == 6 ? 1 : 0));

        if (units == 5) {
            REQUIRE(connection->observation().diagnostic);
            CHECK(connection->observation().diagnostic->transfer.stage == "connection-pending-units");
        }
    }
}

TEST_CASE("connection lifetime cuts off live header wait without helper polling") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    options.limits.lifetime = 30ms;
    auto owner = runtime(*bootstrap, options);
    h::IpfHttpPeer peer({""}, 1, true);
    auto created = owner->createConnection(url(peer.port()), collector());
    REQUIRE(created);
    auto connection = *created;
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(owner->closeAndAwaitTermination());
    REQUIRE(connection->observation().diagnostic);
    CHECK(connection->observation().diagnostic->transfer.error == i::IpfTransferError::WORK_LIMIT);
    CHECK(connection->observation().diagnostic->transfer.stage == "connection-lifetime");
}

TEST_CASE("borrowed connection runtime survives bootstrap wrapper move and old destruction") {
    auto initialized = t::RuntimeBootstrap::create();
    REQUIRE(initialized);
    auto original = std::make_unique<t::RuntimeBootstrap>(std::move(*initialized));
    auto owner = runtime(*original);
    t::RuntimeBootstrap moved(std::move(*original));
    original.reset();
    h::IpfHttpPeer peer({response(std::string(HEADER) + "STOCK,A\n##COMPLETE\n")});
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()), model);
    REQUIRE(created);
    auto connection = *created;
    auto token = connection->addStateChangeListener([&](auto, auto state) {
        if (state == i::IpfConnectionState::COMPLETED) {
            connection->close();
        }
    });
    REQUIRE(token);
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(owner->closeAndAwaitTermination());
    owner.reset();
    CHECK(symbols(model) == std::vector<std::u16string>{u"A"});
}

TEST_CASE("connection start OOM preserves unstarted state and request close destruction allocate nothing") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = runtime(*bootstrap);
    auto created = owner->createConnection("http://127.0.0.1:1/x", collector());
    REQUIRE(created);
    auto connection = std::move(*created);
    rejectedAllocations = 0;
    rejectAllocation = true;
    auto result = connection->start();
    rejectAllocation = false;
    REQUIRE_FALSE(result);
    CHECK(rejectedAllocations == 1);
    CHECK(result.error().transfer.error == i::IpfTransferError::ALLOCATION_FAILURE);
    CHECK(connection->getState() == i::IpfConnectionState::NOT_CONNECTED);
    rejectedAllocations = 0;
    rejectAllocation = true;
    connection->close();
    auto joined = owner->closeAndAwaitTermination();
    owner.reset();
    connection.reset();
    rejectAllocation = false;
    CHECK(joined);
    CHECK(rejectedAllocations == 0);
}

TEST_CASE("runtime actual resident preflight exact and one-below reject without allocations") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    std::size_t low = 0, high = options.limits.residentBytes;

    while (low < high) {
        const auto middle = low + (high - low) / 2;
        options.limits.residentBytes = middle;
        auto candidate = i::InstrumentProfileConnectionRuntime::create(*bootstrap, options);

        if (candidate) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }

    REQUIRE(low > 0);
    options.limits.residentBytes = low;
    auto exact = i::InstrumentProfileConnectionRuntime::create(*bootstrap, options);
    REQUIRE(exact);
    CHECK((*exact)->closeAndAwaitTermination());
    exact->reset();
    options.limits.residentBytes = low - 1;
    rejectedAllocations = 0;
    rejectAllocation = true;
    auto below = i::InstrumentProfileConnectionRuntime::create(*bootstrap, options);
    rejectAllocation = false;
    REQUIRE_FALSE(below);
    CHECK(below.error().transfer.stage == "connection-owner-backing");
    CHECK(rejectedAllocations == 0);
}

TEST_CASE("connection cumulative copy control work exact and one-below preserve published prefix") {
    for (const auto grant : {std::size_t(9), std::size_t(10)}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        i::IpfConnectionRuntimeOptions options;
        options.limits.batchProfiles = 1;
        options.limits.workUnits = grant;
        auto owner = runtime(*bootstrap, options);
        h::IpfHttpPeer peer({response(std::string(HEADER) + "STOCK,A\n##COMPLETE\n", "X-Live: provided\r\n")});
        auto model = collector();
        auto created = owner->createConnection(url(peer.port()), model);
        REQUIRE(created);
        auto connection = *created;
        auto listener = connection->addStateChangeListener([&](auto, auto state) {
            if (state == i::IpfConnectionState::COMPLETED) {
                connection->close();
            }
        });
        REQUIRE(listener);
        REQUIRE(connection->start());
        CHECK(reached([&] {
            return connection->observation().retired;
        }));
        CHECK(owner->closeAndAwaitTermination());
        CHECK(symbols(model) == std::vector<std::u16string>{u"A"});
        CHECK(connection->observation().workUnits == grant);
        CHECK(connection->observation().initialSnapshotComplete == (grant == 10));

        if (grant == 9) {
            REQUIRE(connection->observation().diagnostic);
            CHECK(connection->observation().diagnostic->transfer.stage == "connection-copy-control-work");
        }
    }
}

TEST_CASE("independent runtime first generations cannot remove another connection profiles") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto firstOwner = runtime(*bootstrap);
    auto secondOwner = runtime(*bootstrap);
    h::IpfHttpPeer firstPeer({response(std::string(HEADER) + "STOCK,A\n##\n", "X-Live: provided\r\n"),
                              response(std::string(HEADER) + "STOCK,C\n##COMPLETE\n", "X-Live: provided\r\n")});
    h::IpfHttpPeer secondPeer({response(std::string(HEADER) + "STOCK,B\n##COMPLETE\n", "X-Live: provided\r\n")});
    auto model = collector();
    auto preparedFirst = firstOwner->createConnection(url(firstPeer.port()) + "[update=0]", model);
    REQUIRE(preparedFirst);
    auto preparedSecond = secondOwner->createConnection(url(secondPeer.port()), model);
    REQUIRE(preparedSecond);
    auto first = *preparedFirst;
    auto second = *preparedSecond;
    std::atomic<bool> launched{false}, secondComplete{false}, nestedAccepted{false}, barrierPassed{false};
    auto listener = model->addUpdateListener([&](auto batch) {
        if (batch->profiles().empty() || batch->profiles().front()->profile.getSymbol() != u"A" ||
            launched.exchange(true)) {
            return;
        }

        nestedAccepted.store(bool(second->start()));
        barrierPassed.store(reached([&] {
            return secondComplete.load();
        }));
    });
    REQUIRE(listener);
    auto secondState = second->addStateChangeListener([&](auto, auto state) {
        if (state == i::IpfConnectionState::COMPLETED) {
            secondComplete.store(true);
            second->close();
        }
    });
    REQUIRE(secondState);
    auto firstState = first->addStateChangeListener([&](auto, auto state) {
        if (state == i::IpfConnectionState::COMPLETED) {
            first->close();
        }
    });
    REQUIRE(firstState);
    REQUIRE(first->start());
    CHECK(reached([&] {
        return first->observation().retired;
    }));
    CHECK(firstOwner->closeAndAwaitTermination());
    CHECK(secondOwner->closeAndAwaitTermination());
    CHECK(nestedAccepted.load());
    CHECK(barrierPassed.load());
    CHECK(symbols(model) == std::vector<std::u16string>{u"B", u"C"});
}

TEST_CASE("coalesced rows preserve flush COMPLETE profile order without record-limit replacement") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = runtime(*bootstrap);
    h::IpfHttpPeer peer(
        {response(std::string(HEADER) + "STOCK,A\n##\nSTOCK,B\n##COMPLETE\nSTOCK,C\n", "X-Live: provided\r\n")},
        1048576);
    auto model = collector();
    std::mutex mutex;
    std::vector<std::u16string> events;
    auto listener = model->addUpdateListener([&](auto batch) {
        std::lock_guard lock(mutex);

        for (const auto &revision : batch->profiles()) {
            events.emplace_back(revision->profile.getSymbol());
        }
    });
    REQUIRE(listener);
    auto created = owner->createConnection(url(peer.port()), model);
    REQUIRE(created);
    auto connection = *created;
    auto phase = connection->addStateChangeListener([&](auto, auto state) {
        if (state == i::IpfConnectionState::COMPLETED) {
            std::lock_guard lock(mutex);
            events.emplace_back(u"COMPLETE");
        }
    });
    REQUIRE(phase);
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return symbols(model) == std::vector<std::u16string>{u"A", u"B", u"C"};
    }));
    connection->close();
    CHECK(owner->closeAndAwaitTermination());
    CHECK(events == std::vector<std::u16string>{u"A", u"B", u"COMPLETE", u"C"});
}

TEST_CASE("gzip trailer error after marker preserves live owning prefix and separate completion") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    options.limits.attempts = 1;
    auto owner = runtime(*bootstrap, options);
    std::ifstream input(std::filesystem::path(ORG_TTLDTOR_QDCXX_IPF_COMPRESSION_FIXTURES) / "gzip-bad-crc.bin",
                        std::ios::binary);
    REQUIRE(input);
    std::string body{std::istreambuf_iterator<char>(input), {}};
    h::IpfHttpPeer peer({response(body, "X-Live: provided\r\nContent-Encoding: gzip\r\n")}, 1);
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()) + "[update=0]", model);
    REQUIRE(created);
    auto connection = *created;
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(owner->closeAndAwaitTermination());
    auto snapshot = model->snapshot();
    REQUIRE(snapshot);
    CHECK((*snapshot)->profiles().size() == 1);
    CHECK(connection->observation().initialSnapshotComplete);
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::time_point::min()) ==
          i::IpfCompletionWaitStatus::COMPLETED);
    REQUIRE(connection->observation().lastAttempt);
    CHECK_FALSE(connection->observation().lastAttempt->envelope.finished);
    CHECK(connection->observation().lastAttempt->nativeRetired);
    REQUIRE(connection->observation().diagnostic);
    CHECK(connection->observation().diagnostic->transfer.error == i::IpfTransferError::CONTAINER_FAILURE);
}

TEST_CASE("authorization callback closes deletes facade without output or worker self join") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    namespace x = h::tls;
    i::IpfConnectionRuntimeOptions options;
    options.transfer.trustPem = x::read("ca.pem");
    options.transfer.tls = x::options();
    options.transfer.tls.residentBytes += options.transfer.nativeLimits.residentBytes;
    options.transfer.tlsWork.units.fill(std::numeric_limits<std::uint64_t>::max());
    auto owner = runtime(*bootstrap, options);
    auto server = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::HOLD);
    server.captureUntilClose = true;
    auto peer = h::TlsTestPeer::start(server);
    REQUIRE(peer);
    std::atomic<std::shared_ptr<i::InstrumentProfileConnection>> handle;
    std::atomic<unsigned> calls{0};
    std::atomic<bool> joinRefused{false};

    struct Provider final : i::HttpAuthorizationProvider {
        std::function<void()> action;

        /** @param request Sanitized verified origin, not retained.
         * @param destination Bounded output, unused after cutoff.
         * @return Explicit unavailable after the request-only close action.
         */
        std::expected<std::size_t, i::HttpAuthorizationError>
        authorization([[maybe_unused]] i::HttpAuthorizationRequest request,
                      [[maybe_unused]] std::span<char> destination) override {
            action();

            return std::unexpected(i::HttpAuthorizationError::UNAVAILABLE);
        }
    };

    auto provider = std::make_shared<Provider>();
    provider->action = [&] {
        ++calls;
        auto result = owner->closeAndAwaitTermination();
        joinRefused.store(!result && result.error().transfer.error == i::IpfTransferError::WRONG_THREAD);
        auto connection = handle.exchange({});

        if (connection) {
            connection->close();
        }
    };
    i::IpfUrlRequest request;
    request.provider = provider;
    auto created = owner->createConnection("https://127.0.0.1:" + std::to_string(peer->port()) + "/profiles.ipf",
                                           collector(), request);
    REQUIRE(created);
    handle.store(std::move(*created));
    auto connection = handle.load();
    REQUIRE(connection->start());
    connection.reset();
    CHECK(reached([&] {
        return calls.load() == 1;
    }));
    CHECK(owner->closeAndAwaitTermination());
    CHECK_FALSE(handle.load());
    CHECK(joinRefused.load());
    CHECK(peer->snapshot().bytesRead == 0);
}

TEST_CASE("populated dense pending batch refuses unfunded requested copy backing before publication") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    options.limits.batchProfiles = 4;
    std::string header = "#STOCK::=TYPE,SYMBOL";
    std::string fields;
    std::size_t units = 6;

    for (unsigned n = 0; n < 254; ++n) {
        auto name = std::string("X") + char('0' + n / 100) + char('0' + (n / 10) % 10) + char('0' + n % 10);
        header += "," + name;
        fields += ",v";
        units += name.size() + 1;
    }

    header += '\n';
    options.limits.pendingUnits = units * 4;
    std::size_t low = 0, high = options.limits.residentBytes;

    while (low < high) {
        const auto middle = low + (high - low) / 2;
        options.limits.residentBytes = middle;
        auto candidate = i::InstrumentProfileConnectionRuntime::create(*bootstrap, options);

        if (candidate) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }

    options.limits.residentBytes = low;
    auto owner = runtime(*bootstrap, options);
    std::string body = header;

    for (char symbol = 'A'; symbol <= 'D'; ++symbol) {
        body += "STOCK," + std::string(1, symbol) + fields + "\n";
    }

    body += "##COMPLETE\n";
    h::IpfHttpPeer peer({response(body, "X-Live: provided\r\n")});
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()), model);
    REQUIRE(created);
    auto connection = *created;
    auto listener = connection->addStateChangeListener([&](auto, auto state) {
        if (state == i::IpfConnectionState::COMPLETED) {
            connection->close();
        }
    });
    REQUIRE(listener);
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(owner->closeAndAwaitTermination());
    CHECK(symbols(model).empty());
    REQUIRE(connection->observation().diagnostic);
    CHECK(connection->observation().diagnostic->transfer.stage == "connection-pending-backing");
}

TEST_CASE("populated pending copy resident exact and one-below grants include actual dense backing") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::InstrumentProfile prototype;
    REQUIRE(prototype.setType(u"STOCK"));
    REQUIRE(prototype.setSymbol(u"A"));
    std::string header = "#STOCK::=TYPE,SYMBOL";
    std::string fields;

    for (unsigned n = 0; n < 254; ++n) {
        const std::u16string name{u'X', char16_t(u'0' + n / 100), char16_t(u'0' + (n / 10) % 10),
                                  char16_t(u'0' + n % 10)};
        REQUIRE(prototype.setField(name, u"v"));
        std::string ascii;

        for (const auto unit : name) {
            ascii.push_back(static_cast<char>(unit));
        }

        header += "," + ascii;
        fields += ",v";
    }

    header += '\n';
    const auto copyBytes = i::collector_detail::CollectorProfileAccess::copyBacking(prototype);
    REQUIRE(copyBytes);
    i::IpfConnectionRuntimeOptions options;
    options.limits.batchProfiles = 4;
    options.limits.pendingUnits = prototype.retainedUnits() * 4;
    std::size_t low = 0, high = options.limits.residentBytes;

    while (low < high) {
        const auto middle = low + (high - low) / 2;
        options.limits.residentBytes = middle;
        auto candidate = i::InstrumentProfileConnectionRuntime::create(*bootstrap, options);

        if (candidate) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }

    const auto fixed = low - options.limits.pendingUnits * sizeof(char16_t);

    for (const auto deficit : {std::size_t(1), std::size_t(0)}) {
        options.limits.residentBytes = fixed + *copyBytes * 4 - deficit;
        auto owner = runtime(*bootstrap, options);
        std::string body = header;

        for (char symbol = 'A'; symbol <= 'E'; ++symbol) {
            body += "STOCK," + std::string(1, symbol) + fields + "\n";
        }

        body += "##COMPLETE\n";
        h::IpfHttpPeer peer({response(body, "X-Live: provided\r\n")});
        auto model = collector();
        auto created = owner->createConnection(url(peer.port()), model);
        REQUIRE(created);
        auto connection = *created;
        auto listener = connection->addStateChangeListener([&](auto, auto state) {
            if (state == i::IpfConnectionState::COMPLETED) {
                connection->close();
            }
        });
        REQUIRE(listener);
        REQUIRE(connection->start());
        CHECK(reached([&] {
            return connection->observation().retired;
        }));
        CHECK(owner->closeAndAwaitTermination());
        CHECK(symbols(model).size() == (deficit == 0 ? 5 : 0));

        if (deficit != 0) {
            REQUIRE(connection->observation().diagnostic);
            CHECK(connection->observation().diagnostic->transfer.stage == "connection-pending-backing");
        }
    }
}

TEST_CASE("request-only close wakes an infinite retry wait before creator shutdown") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = runtime(*bootstrap);
    auto model = collector();
    h::IpfHttpPeer peer({response(std::string(HEADER) + "STOCK,A\n##COMPLETE\n")});
    auto prepared = owner->createConnection(url(peer.port()) + "[update=inf]", model);
    REQUIRE(prepared);
    auto connection = std::move(*prepared);
    REQUIRE(connection->start());
    CHECK(reached([&] {
        const auto observed = connection->observation();

        return observed.lastAttempt.has_value() && !observed.activeAttempt;
    }));
    CHECK_FALSE(connection->observation().retired);
    std::thread closer([connection] {
        connection->close();
    });
    closer.join();
    // Observe worker retirement before creator shutdown can send its own notification.
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(connection->observation().state == i::IpfConnectionState::CLOSED);
    CHECK(owner->closeAndAwaitTermination());
    CHECK(symbols(model) == std::vector<std::u16string>{u"A"});
}

TEST_CASE("public completion wait preserves Java terminal and caller cancellation precedence") {
    using S = i::IpfCompletionWaitStatus;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = runtime(*bootstrap);
    auto created = owner->createConnection("http://127.0.0.1:1/unused", collector());
    REQUIRE(created);
    auto connection = std::move(*created);
    std::stop_source stop;
    stop.request_stop();
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::time_point::min()) == S::TIMED_OUT);
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::time_point::min(), stop.get_token()) == S::CANCELLED);
    CHECK(connection->getState() == i::IpfConnectionState::NOT_CONNECTED);
    CHECK_FALSE(connection->observation().diagnostic);
    connection->close();
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::time_point::min(), stop.get_token()) == S::CLOSED);
    CHECK_FALSE(connection->observation().retired);
    CHECK(owner->closeAndAwaitTermination());
    owner.reset();
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::time_point::max()) == S::CLOSED);
}

TEST_CASE("first snapshot wait completes before live HTTP EOF and remains sticky after close") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = runtime(*bootstrap);
    const auto body = std::string(HEADER) + "STOCK,A\n##COMPLETE\n";
    h::IpfHttpPeer peer({"HTTP/1.1 200 OK\r\nX-Live: provided\r\nContent-Length: 9999\r\n\r\n" + body}, 5, true);
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()), model);
    REQUIRE(created);
    auto connection = std::move(*created);
    REQUIRE(connection->start());
    auto completed = connection->awaitCompleted(std::chrono::steady_clock::now() + 3s);
    CHECK(completed == i::IpfCompletionWaitStatus::COMPLETED);
    const auto observed = connection->observation();
    CHECK(observed.activeAttempt);
    CHECK_FALSE(observed.lastAttempt);
    CHECK_FALSE(observed.retired);
    CHECK(symbols(model) == std::vector<std::u16string>{u"A"});
    connection->close();
    CHECK(owner->closeAndAwaitTermination());
    std::stop_source stop;
    stop.request_stop();
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::time_point::min(), stop.get_token()) ==
          i::IpfCompletionWaitStatus::COMPLETED);
}

TEST_CASE("caller cancellation ends only the snapshot wait during a real blocked header attempt") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = runtime(*bootstrap);
    h::IpfHttpPeer peer({""}, 1, true);
    auto created = owner->createConnection(url(peer.port()), collector());
    REQUIRE(created);
    auto connection = std::move(*created);
    REQUIRE(connection->start());
    CHECK(reached([&] {
        return peer.accepted() == 1;
    }));
    std::stop_source stop;
    std::atomic<bool> entered{false};
    std::jthread cancellation([&] {
        while (!entered.load()) {
            std::this_thread::yield();
        }

        stop.request_stop();
    });
    entered.store(true);
    const auto result = connection->awaitCompleted(std::chrono::steady_clock::time_point::max(), stop.get_token());
    cancellation.join();
    CHECK(result == i::IpfCompletionWaitStatus::CANCELLED);
    CHECK(connection->getState() != i::IpfConnectionState::CLOSED);
    CHECK(connection->observation().activeAttempt);
    CHECK_FALSE(connection->observation().initialSnapshotComplete);
    connection->close();
    CHECK(owner->closeAndAwaitTermination());
}

TEST_CASE("request-only close wakes a pre-start snapshot waiter before creator join") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = runtime(*bootstrap);
    auto created = owner->createConnection("http://127.0.0.1:1/unused", collector());
    REQUIRE(created);
    auto connection = std::move(*created);
    std::optional<std::expected<i::IpfCompletionWaitStatus, i::IpfConnectionDiagnostic>> result;
    std::jthread waiter([&] {
        result = connection->awaitCompleted(std::chrono::steady_clock::now() + 3s);
    });
    connection->close();
    waiter.join();
    REQUIRE(result);
    CHECK(*result == i::IpfCompletionWaitStatus::CLOSED);
    CHECK_FALSE(connection->observation().retired);
    CHECK(owner->closeAndAwaitTermination());
}

TEST_CASE("worker callback refuses blocking self wait but can query completion and close") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = runtime(*bootstrap);
    h::IpfHttpPeer peer({response(std::string(HEADER) + "STOCK,A\n##COMPLETE\n")});
    auto created = owner->createConnection(url(peer.port()), collector());
    REQUIRE(created);
    auto connection = std::move(*created);
    std::atomic<bool> refused{false}, queried{false}, wrongJoin{false};
    auto listener = connection->addStateChangeListener([&](auto, auto state) {
        if (state == i::IpfConnectionState::CONNECTED) {
            const auto wait = connection->awaitCompleted(std::chrono::steady_clock::now() + 1s);
            refused.store(!wait && wait.error().transfer.stage == "connection-wait-self");
            const auto join = owner->closeAndAwaitTermination();
            wrongJoin.store(!join && join.error().transfer.error == i::IpfTransferError::WRONG_THREAD);
        } else if (state == i::IpfConnectionState::COMPLETED) {
            queried.store(connection->awaitCompleted(std::chrono::steady_clock::time_point::min()) ==
                          i::IpfCompletionWaitStatus::COMPLETED);
            connection->close();
        }
    });
    REQUIRE(listener);
    REQUIRE(connection->start());
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::now() + 3s) == i::IpfCompletionWaitStatus::COMPLETED);
    CHECK(owner->closeAndAwaitTermination());
    CHECK(refused.load());
    CHECK(queried.load());
    CHECK(wrongJoin.load());
    CHECK(connection->getState() == i::IpfConnectionState::CLOSED);
}

TEST_CASE("zero wait grants refuse only pending waits and backing overflow is rejected before startup") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (bool turns : {false, true}) {
        i::IpfConnectionRuntimeOptions options;
        options.limits.completionWaiters = turns ? 1 : 0;
        options.limits.completionWaitTurns = turns ? 0 : 1;
        auto owner = runtime(*bootstrap, options);
        auto created = owner->createConnection("http://127.0.0.1:1/unused", collector());
        REQUIRE(created);
        auto connection = std::move(*created);
        const auto pending = connection->awaitCompleted(std::chrono::steady_clock::time_point::max());
        REQUIRE_FALSE(pending);
        CHECK(pending.error().transfer.error ==
              (turns ? i::IpfTransferError::WORK_LIMIT : i::IpfTransferError::RESOURCE_LIMIT));
        CHECK(connection->awaitCompleted(std::chrono::steady_clock::time_point::min()) ==
              i::IpfCompletionWaitStatus::TIMED_OUT);
        CHECK_FALSE(connection->observation().diagnostic);
        CHECK(owner->closeAndAwaitTermination());
    }

    i::IpfConnectionRuntimeOptions overflowing;
    overflowing.limits.completionWaiters = std::numeric_limits<std::size_t>::max();
    auto failed = i::InstrumentProfileConnectionRuntime::create(*bootstrap, overflowing);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().transfer.stage == "connection-owner-backing");
}

TEST_CASE("sticky first snapshot wait coexists with failed retry304 equal date and completed replacement") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    options.limits.attempts = 5;
    options.limits.batchProfiles = 1;
    auto owner = runtime(*bootstrap, options);
    const std::string format = "#STOCK::=TYPE,SYMBOL,DESCRIPTION\n";
    const std::string date1 = "Last-Modified: Sun, 06 Nov 1994 08:49:37 GMT\r\n";
    const std::string date2 = "Last-Modified: Sun, 06 Nov 1994 08:49:38 GMT\r\n";
    const std::string date3 = "Last-Modified: Sun, 06 Nov 1994 08:49:40 GMT\r\n";
    h::IpfHttpPeer peer({response(format + "STOCK,A,old\nSTOCK,B,retained\n", date1),
                         response(format + "STOCK,A,new\nSTOCK,bad,too,many\n", date2),
                         "HTTP/1.1 304 Not Modified\r\n\r\n", response("not IPF", date1),
                         response(format + "STOCK,C,replacement\n", date3)},
                        23);
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()) + "[update=0]", model);
    REQUIRE(created);
    auto connection = std::move(*created);
    std::shared_ptr<const i::ProfileBatch> firstSnapshot;
    std::atomic<bool> pendingRetainsOld{false}, stickyDuringRetry{false};
    auto stateListener = connection->addStateChangeListener([&](auto, auto state) {
        if (state == i::IpfConnectionState::COMPLETED) {
            auto snapshot = model->snapshot();

            if (snapshot) {
                firstSnapshot = *snapshot;
            }
        }
    });
    REQUIRE(stateListener);
    auto updates = model->addUpdateListener([&](auto batch) {
        for (const auto &revision : batch->profiles()) {
            if (revision->profile.getSymbol() == u"A" && revision->profile.getDescription() == u"new") {
                pendingRetainsOld.store(symbols(model) == std::vector<std::u16string>{u"A", u"B"});
                stickyDuringRetry.store(connection->awaitCompleted(std::chrono::steady_clock::time_point::min()) ==
                                        i::IpfCompletionWaitStatus::COMPLETED);
            }
        }
    });
    REQUIRE(updates);
    REQUIRE(connection->start());
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::now() + 3s) == i::IpfCompletionWaitStatus::COMPLETED);
    CHECK(reached([&] {
        return connection->observation().retired;
    }));
    CHECK(owner->closeAndAwaitTermination());
    CHECK(connection->observation().attempts == 5);
    CHECK(pendingRetainsOld.load());
    CHECK(stickyDuringRetry.load());
    CHECK(symbols(model) == std::vector<std::u16string>{u"C"});
    REQUIRE(firstSnapshot);
    REQUIRE(firstSnapshot->profiles().size() == 2);
    CHECK(firstSnapshot->profiles()[1]->profile.getSymbol() == u"B");
    CHECK(firstSnapshot->profiles()[1]->profile.getDescription() == u"retained");
    const auto expected = std::chrono::sys_days{std::chrono::year{1994} / 11 / 6} + 8h + 49min + 40s;
    CHECK(connection->getLastModified() == expected);
    auto requests = peer.requests();
    REQUIRE(requests.size() == 5);

    for (std::size_t index = 1; index < requests.size(); ++index) {
        CHECK(requests[index].find("If-Modified-Since: Sun, 06 Nov 1994 08:49:37 GMT\r\n") != std::string::npos);
    }

    CHECK(connection->awaitCompleted(std::chrono::steady_clock::time_point::min()) ==
          i::IpfCompletionWaitStatus::COMPLETED);
}

TEST_CASE("live partial UTF8 control and gzip header need input without exhausting drain turns") {
    const std::string format = "#STOCK::=TYPE,SYMBOL,DESCRIPTION\n";
    const std::array<std::string, 3> prefixes = {format + "STOCK,A,\xe2", std::string(HEADER) + "##COMP",
                                                 std::string("\x1f\x8b\x08", 3)};

    for (const auto &prefix : prefixes) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        i::IpfConnectionRuntimeOptions options;
        options.transfer.limits.turns = 4096;
        auto owner = runtime(*bootstrap, options);
        h::IpfHttpPeer peer({"HTTP/1.1 200 OK\r\nX-Live: provided\r\nContent-Length: 9999\r\n\r\n" + prefix}, 4096,
                            true);
        auto created = owner->createConnection(url(peer.port()), collector());
        REQUIRE(created);
        auto connection = std::move(*created);
        REQUIRE(connection->start());
        CHECK(reached([&] {
            return peer.sentBytes() != 0;
        }));
        CHECK(connection->awaitCompleted(std::chrono::steady_clock::now() + 50ms) ==
              i::IpfCompletionWaitStatus::TIMED_OUT);
        CHECK(connection->observation().activeAttempt);
        CHECK_FALSE(connection->observation().diagnostic);
        connection->close();
        CHECK(owner->closeAndAwaitTermination());
    }
}

TEST_CASE("live UTF8 row and COMPLETE straddled over single byte input satisfy first snapshot") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfConnectionRuntimeOptions options;
    options.transfer.limits.turns = 8192;
    auto owner = runtime(*bootstrap, options);
    const std::string body = "#STOCK::=TYPE,SYMBOL,DESCRIPTION\nSTOCK,A,\xe2\x82\xac\n##COMPLETE\n";
    h::IpfHttpPeer peer({"HTTP/1.1 200 OK\r\nX-Live: provided\r\nContent-Length: 9999\r\n\r\n" + body}, 1, true, 1ms);
    auto model = collector();
    auto created = owner->createConnection(url(peer.port()), model);
    REQUIRE(created);
    auto connection = std::move(*created);
    REQUIRE(connection->start());
    CHECK(connection->awaitCompleted(std::chrono::steady_clock::now() + 3s) == i::IpfCompletionWaitStatus::COMPLETED);
    auto snapshot = model->snapshot();
    REQUIRE(snapshot);
    REQUIRE((*snapshot)->profiles().size() == 1);
    CHECK((*snapshot)->profiles().front()->profile.getDescription() == u"\u20ac");
    CHECK(connection->observation().activeAttempt);
    CHECK_FALSE(connection->observation().lastAttempt);
    connection->close();
    CHECK(owner->closeAndAwaitTermination());
}

TEST_CASE("empty fallback snapshots complete and a later empty replacement removes stale profiles") {
    for (bool initialEmpty : {true, false}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        i::IpfConnectionRuntimeOptions options;
        options.limits.attempts = initialEmpty ? 1 : 2;
        auto owner = runtime(*bootstrap, options);
        std::vector<std::string> replies;

        if (!initialEmpty) {
            replies.push_back(
                response(std::string(HEADER) + "STOCK,A\n", "Last-Modified: Sun, 06 Nov 1994 08:49:37 GMT\r\n"));
        }

        replies.push_back(response({}, "Last-Modified: Sun, 06 Nov 1994 08:49:40 GMT\r\n"));
        h::IpfHttpPeer peer(std::move(replies));
        auto model = collector();
        auto created = owner->createConnection(url(peer.port()) + "[update=0]", model);
        REQUIRE(created);
        auto connection = std::move(*created);
        REQUIRE(connection->start());
        CHECK(connection->awaitCompleted(std::chrono::steady_clock::now() + 3s) ==
              i::IpfCompletionWaitStatus::COMPLETED);
        CHECK(reached([&] {
            return connection->observation().retired;
        }));
        CHECK(owner->closeAndAwaitTermination());
        CHECK(symbols(model).empty());
        CHECK(connection->observation().initialSnapshotComplete);
        CHECK(connection->observation().attempts == (initialEmpty ? 1 : 2));
    }
}
