// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "tls_byte_owner.hpp"
#include "tls_test_support.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>
#include <thread>
#include <vector>

namespace t = org::ttldtor::qdcxx::transport;
namespace td = t::detail;
namespace x = org::ttldtor::qdcxx::test::tls;
namespace h = org::ttldtor::qdcxx::test;
using namespace std::chrono_literals;

namespace {

/** Make finite numeric-route native settings, independent of reference identity.
 * @param port Explicit owned peer listener port.
 * @return Native options with finite backing and deliberately short writes.
 */
t::TcpOptions nativeOptions(std::uint16_t port) {
    return {"127.0.0.1", port, {}, 53, 1s, 1s, 5s, 5s, 1ms, {64, 257, 64, 16, 65536, 8192, 8 * 1024 * 1024}};
}

/** Make explicit bounded no-provider TLS settings from unchanged ordinary fixtures.
 * @return Owned trust/reference and phase deadlines; no authentication configuration.
 */
org::ttldtor::qdcxx::client::EndpointTls settings() {
    org::ttldtor::qdcxx::client::EndpointTls result;
    result.trustPem = x::read("ca.pem");
    result.referenceIdentity = "localhost";
    result.handshakeTimeout = 2s;
    result.closeTimeout = 1s;

    return result;
}

/** Drive actual serialized TLS/native progress to an explicit condition.
 * @tparam Predicate Owner-only nullary predicate.
 * @param owner Stable byte operation.
 * @param requirements Same-image exact shape used for this attempt.
 * @param done Exact stopping condition.
 * @return True if the condition held within five seconds.
 */
template <class Predicate> bool pump(td::TlsByteOwner &owner, const t::TlsRequirements &requirements, Predicate done) {
    const auto deadline = td::Clock::now() + 5s;

    while (!done() && td::Clock::now() < deadline) {
        auto native = x::nativeTurn();
        auto copy = x::sessionTurn();
        auto tls = x::tlsTurn(requirements);
        const auto result = owner.poll(native, copy, tls);

        if (!result) {
            INFO("category=" << static_cast<int>(result.error().error) << " stage=" << result.error().stage);
            REQUIRE(result);
        }

        if (result->status == t::TcpPollStatus::IDLE || result->status == t::TcpPollStatus::YIELD) {
            std::this_thread::sleep_for(1ms);
        }
    }

    return done();
}

} // namespace

TEST_CASE("raw TLS 1.2 and 1.3 share existing verification and preserve partial byte prefixes") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto version : {h::PeerTlsVersion::TLS_1_2, h::PeerTlsVersion::TLS_1_3}) {
        auto peerConfig = x::peerOptions(version, h::PeerCloseMode::CLEAN);
        peerConfig.expectedClientBytes = std::vector<std::uint8_t>{'b', 'y', 't', 'e', '-', 'r', 'e', 'q'};
        peerConfig.replyBytes = {'r', 'a', 'w', '-', 'r', 'e', 's', 'p', 'o', 'n', 's', 'e'};
        auto peer = h::TlsTestPeer::start(peerConfig);
        REQUIRE(peer);
        auto config = settings();
        config.closeTimeout = 200ms;
        const auto options = x::options();
        const auto requirements = t::tlsRequirements(options.limits, options.host);
        REQUIRE(requirements);
        auto copy = x::sessionTurn();
        auto tls = x::tlsTurn(*requirements);
        auto made =
            td::TlsByteOwner::create(*bootstrap, nativeOptions(peer->port()), {3, 128}, config, options, copy, tls);
        REQUIRE(made);
        auto &owner = **made;
        config.trustPem.assign("changed-after-create");
        config.referenceIdentity.assign("changed.invalid");
        auto request = *peerConfig.expectedClientBytes;
        REQUIRE(owner.write(std::as_bytes(std::span{request}), copy));
        std::ranges::fill(request, std::uint8_t{0});
        REQUIRE(owner.start());
        std::vector<std::uint8_t> response;
        const auto finished = pump(owner, *requirements, [&] {
            const auto input = owner.input();
            REQUIRE(input);

            if (!input->empty()) {
                response.push_back(std::to_integer<std::uint8_t>(input->front()));
                REQUIRE(owner.consumeInput(1));
            }

            return owner.bytes().core().terminal().has_value() && owner.input()->empty();
        });
        auto &core = owner.bytes().core();
        const auto peerState = peer->snapshot();
        const auto &engine = *static_cast<td::TlsEngine *>(core.engine.get());
        INFO("state=" << static_cast<int>(core.state()) << " response=" << response.size()
                      << " appOutput=" << owner.bytes().pendingOutput() << " wire=" << core.wireBytes().size()
                      << " nativeInput=" << core.inputEnd - core.inputBegin
                      << " enginePlain=" << core.engine->plaintext().size() << " verified=" << core.engine->verified()
                      << " finished=" << core.engine->finished() << " readPending=" << core.readPending
                      << " writePending=" << core.writePending << " cause=" << owner.diagnostic().stage
                      << " engineClosing=" << engine.closing() << " enginePeerClosed=" << engine.peerClosed()
                      << " peerRunning=" << peerState.childRunning << " peerFailed=" << peerState.failed
                      << " peerRead=" << peerState.bytesRead << " peerWrite=" << peerState.bytesWritten
                      << " peerClose=" << peerState.closeNotifyComplete);
        REQUIRE(finished);
        CHECK(response == peerConfig.replyBytes);
        CHECK(owner.diagnostic().error == t::TlsError::NONE);
        CHECK(owner.bytes().core().terminal()->peerEnded);
        CHECK(owner.bytes().core().resources().pendingOperations == 0);
        CHECK(owner.bytes().core().resources().sessionBytes == 0);
        const auto beforeLatePoll = *core.terminal();
        std::this_thread::sleep_for(250ms);
        t::TcpWork noNativeWork{};
        org::ttldtor::qdcxx::protocol::LiveWork noByteWork{};
        auto noTlsWork = t::TlsWork{};
        noTlsWork.shape = requirements->shape;
        const auto late = owner.poll(noNativeWork, noByteWork, noTlsWork);
        REQUIRE(late);
        CHECK(late->status == t::TcpPollStatus::CLOSED);
        CHECK(owner.diagnostic().error == t::TlsError::NONE);
        CHECK(core.terminal()->aborted == beforeLatePoll.aborted);
        REQUIRE(owner.requestClose());
        REQUIRE(owner.poll(noNativeWork, noByteWork, noTlsWork));
        CHECK(owner.diagnostic().error == t::TlsError::NONE);
        CHECK(core.terminal()->aborted == beforeLatePoll.aborted);
        CHECK(core.terminal()->peerEnded == beforeLatePoll.peerEnded);
        CHECK(core.terminal()->reportedWriteBytes == beforeLatePoll.reportedWriteBytes);
        CHECK(core.terminal()->discardedInputBytes == beforeLatePoll.discardedInputBytes);
        CHECK(core.terminal()->pendingOutputBytes == beforeLatePoll.pendingOutputBytes);
        const auto observed = peer->stop();
        CHECK(observed.bytesRead == peerConfig.expectedClientBytes->size());
        CHECK(observed.handshakeComplete);
    }
}

TEST_CASE("raw TLS failed reference verification publishes no plaintext or application output") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peerConfig = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::HOLD);
    peerConfig.initialBytes = {'n', 'o', '-', 'l', 'e', 'a', 'k'};
    auto peer = h::TlsTestPeer::start(peerConfig);
    REQUIRE(peer);
    auto config = settings();
    config.referenceIdentity = "unrelated.invalid";
    const auto options = x::options();
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto copy = x::sessionTurn();
    auto tls = x::tlsTurn(*requirements);
    auto made = td::TlsByteOwner::create(*bootstrap, nativeOptions(peer->port()), {64, 64}, config, options, copy, tls);
    REQUIRE(made);
    auto &owner = **made;
    const std::string output = "application-only-after-verification";
    REQUIRE(owner.write(std::as_bytes(std::span{output}), copy));
    REQUIRE(owner.start());
    REQUIRE(pump(owner, *requirements, [&] {
        return owner.bytes().core().terminal().has_value();
    }));
    CHECK(owner.input()->empty());
    CHECK((owner.diagnostic().error == t::TlsError::TRUST_FAILURE ||
           owner.diagnostic().error == t::TlsError::IDENTITY_FAILURE));
    CHECK(owner.bytes().core().pendingApplicationAtClose == output.size());
    const auto observed = peer->stop();
    CHECK(observed.bytesRead == 0);
}

TEST_CASE("raw TLS truncation retains the preceding verified application prefix") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peerConfig = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::ABRUPT);
    peerConfig.replyBytes = {'v', 'e', 'r', 'i', 'f', 'i', 'e', 'd', '-', 'p', 'r', 'e', 'f', 'i', 'x'};
    auto peer = h::TlsTestPeer::start(peerConfig);
    REQUIRE(peer);
    const auto options = x::options();
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto copy = x::sessionTurn();
    auto tls = x::tlsTurn(*requirements);
    auto made =
        td::TlsByteOwner::create(*bootstrap, nativeOptions(peer->port()), {4, 64}, settings(), options, copy, tls);
    REQUIRE(made);
    auto &owner = **made;
    REQUIRE(owner.start());
    std::vector<std::uint8_t> response;
    REQUIRE(pump(owner, *requirements, [&] {
        const auto input = owner.input();
        REQUIRE(input);

        for (const auto byte : *input) {
            response.push_back(std::to_integer<std::uint8_t>(byte));
        }

        REQUIRE(owner.consumeInput(input->size()));

        return owner.bytes().core().terminal().has_value() && owner.input()->empty();
    }));
    CHECK(response == peerConfig.replyBytes);
    CHECK(owner.diagnostic().error != t::TlsError::NONE);
    CHECK_FALSE(owner.bytes().core().terminal()->peerEnded);
}

TEST_CASE("raw TLS factory and entry require exact existing work shape and preserve whole reservations") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto native = nativeOptions(1);
    auto options = x::options();
    const auto config = settings();
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto copy = x::sessionTurn();
    auto tls = x::tlsTurn(*requirements);
    auto made = td::TlsByteOwner::create(*bootstrap, native, {17, 23}, config, options, copy, tls);
    REQUIRE(made);
    const auto reservation = (*made)->resources();
    made->reset();
    options.residentBytes = reservation.totalBytes;
    copy = x::sessionTurn();
    tls = x::tlsTurn(*requirements);
    auto exact = td::TlsByteOwner::create(*bootstrap, native, {17, 23}, config, options, copy, tls);
    REQUIRE(exact);
    CHECK((*exact)->resources().totalBytes == options.residentBytes);
    exact->reset();
    --options.residentBytes;
    copy = x::sessionTurn();
    tls = x::tlsTurn(*requirements);
    const auto below = td::TlsByteOwner::create(*bootstrap, native, {17, 23}, config, options, copy, tls);
    REQUIRE_FALSE(below);
    CHECK(below.error().error == t::TlsError::RESOURCE_LIMIT);
    ++options.residentBytes;

    for (std::size_t coordinate = 0; coordinate < requirements->factoryAdmission.units.size(); ++coordinate) {
        const auto need = requirements->factoryAdmission.units[coordinate];

        if (need == 0) {
            continue;
        }

        copy = x::sessionTurn();
        tls = x::tlsTurn(*requirements);
        tls.remaining.units[coordinate] = need - 1;
        const auto rejected = td::TlsByteOwner::create(*bootstrap, native, {17, 23}, config, options, copy, tls);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == t::TlsError::WORK_LIMIT);
    }
}

TEST_CASE("raw TLS pending handshake abort is request-only and drains on its creating owner") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peerConfig = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::HOLD);
    peerConfig.pauseBeforeHandshake = 1s;
    auto peer = h::TlsTestPeer::start(peerConfig);
    REQUIRE(peer);
    auto copy = x::sessionTurn();
    const auto options = x::options();
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto tls = x::tlsTurn(*requirements);
    auto made =
        td::TlsByteOwner::create(*bootstrap, nativeOptions(peer->port()), {64, 64}, settings(), options, copy, tls);
    REQUIRE(made);
    auto &owner = **made;
    REQUIRE(owner.start());
    REQUIRE(pump(owner, *requirements, [&] {
        return owner.bytes().core().readPending;
    }));
    std::optional<t::TlsDiagnostic> wrong;
    std::jthread foreign([&] {
        const auto result = owner.requestClose();
        REQUIRE_FALSE(result);
        wrong = result.error();
    });
    foreign.join();
    REQUIRE(wrong);
    CHECK(wrong->error == t::TlsError::WRONG_THREAD);
    REQUIRE(owner.requestClose());
    CHECK_FALSE(owner.bytes().core().closeApplied);
    CHECK_FALSE(owner.bytes().core().pauseCancellation);
    REQUIRE(pump(owner, *requirements, [&] {
        return owner.bytes().core().terminal().has_value();
    }));
    CHECK(owner.bytes().core().pending == 0);
    CHECK(owner.bytes().core().callbackDepth == 0);
    CHECK(owner.bytes().core().terminal()->aborted);
    made->reset();
}

TEST_CASE("raw TLS funds H once and expires unused E without refund or SDK entry") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peerConfig = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::HOLD);
    auto peer = h::TlsTestPeer::start(peerConfig);
    REQUIRE(peer);
    const auto options = x::options();
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto copy = x::sessionTurn();
    auto tls = x::tlsTurn(*requirements);
    auto made =
        td::TlsByteOwner::create(*bootstrap, nativeOptions(peer->port()), {64, 64}, settings(), options, copy, tls);
    REQUIRE(made);
    auto &owner = **made;
    REQUIRE(owner.start());
    REQUIRE(pump(owner, *requirements, [&] {
        return owner.bytes().core().state() == t::TcpState::CONNECTED;
    }));

    for (std::size_t coordinate = 0; coordinate < requirements->initialStart.units.size(); ++coordinate) {
        const auto need = requirements->initialStart.units[coordinate];

        if (need == 0) {
            continue;
        }

        auto native = x::nativeTurn();
        tls = x::tlsTurn(*requirements);
        tls.remaining.units[coordinate] = need - 1;
        const auto rejected = owner.poll(native, copy, tls);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == t::TlsError::WORK_LIMIT);
        CHECK(owner.bytes().core().wireBytes().empty());
    }

    t::TcpWork noNativeWork{};
    tls.shape = requirements->shape;
    tls.remaining = requirements->initialStart;
    const auto admitted = owner.poll(noNativeWork, copy, tls);
    REQUIRE(admitted);
    CHECK(admitted->status == t::TcpPollStatus::YIELD);
    CHECK(owner.bytes().core().wireBytes().empty());

    for (const auto remaining : tls.remaining.units) {
        CHECK(remaining == 0);
    }

    auto native = x::nativeTurn();
    tls.shape = requirements->shape;
    tls.remaining = requirements->entry;
    const auto entered = owner.poll(native, copy, tls);
    REQUIRE(entered);
    CHECK(entered->status == t::TcpPollStatus::PROGRESS);
    CHECK_FALSE(owner.bytes().core().wireBytes().empty());
    REQUIRE(owner.requestClose());
    REQUIRE(pump(owner, *requirements, [&] {
        return owner.bytes().core().terminal().has_value();
    }));
}

TEST_CASE("raw TLS repeated orderly close cannot rearm its original phase deadline") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peerConfig = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::HOLD);
    auto peer = h::TlsTestPeer::start(peerConfig);
    REQUIRE(peer);
    auto config = settings();
    config.closeTimeout = 200ms;
    const auto options = x::options();
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto copy = x::sessionTurn();
    auto tls = x::tlsTurn(*requirements);
    auto made = td::TlsByteOwner::create(*bootstrap, nativeOptions(peer->port()), {64, 64}, config, options, copy, tls);
    REQUIRE(made);
    auto &owner = **made;
    REQUIRE(owner.start());
    REQUIRE(pump(owner, *requirements, [&] {
        return owner.bytes().core().engine->verified();
    }));
    REQUIRE(owner.requestTlsClose());
    // Reach a real shutdown WANT before retrying the public control request.
    REQUIRE(pump(owner, *requirements, [&] {
        auto &core = owner.bytes().core();

        return core.readPending && core.wireBytes().empty() &&
               !core.engine->needsStep(core.inputBegin != core.inputEnd, false);
    }));
    auto native = x::nativeTurn();
    tls = x::tlsTurn(*requirements);
    REQUIRE(owner.poll(native, copy, tls));
    std::this_thread::sleep_for(150ms);
    const auto repeated = owner.requestTlsClose();
    CHECK_FALSE(repeated);
    CHECK_FALSE(owner.bytes().core().engine->needsStep(false, false));

    if (!repeated) {
        CHECK(repeated.error().error == t::TlsError::INVALID_STATE);
    }

    std::this_thread::sleep_for(100ms);
    native = x::nativeTurn();
    tls = x::tlsTurn(*requirements);
    REQUIRE(owner.poll(native, copy, tls));
    CHECK(owner.diagnostic().error == t::TlsError::DEADLINE);
    REQUIRE(pump(owner, *requirements, [&] {
        return owner.bytes().core().terminal().has_value();
    }));
    const auto terminalBeforeLatePoll = *owner.bytes().core().terminal();
    std::this_thread::sleep_for(250ms);
    native = x::nativeTurn();
    tls = x::tlsTurn(*requirements);
    REQUIRE(owner.poll(native, copy, tls));
    CHECK(owner.bytes().core().terminal()->aborted == terminalBeforeLatePoll.aborted);
    CHECK(owner.diagnostic().error == t::TlsError::DEADLINE);
}

TEST_CASE("raw TLS local orderly terminal remains stable after the original close deadline") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peerConfig = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::CLEAN);
    peerConfig.expectedClientBytes = std::vector<std::uint8_t>{'x'};
    auto peer = h::TlsTestPeer::start(peerConfig);
    REQUIRE(peer);
    auto config = settings();
    config.closeTimeout = 200ms;
    const auto options = x::options();
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto copy = x::sessionTurn();
    auto tls = x::tlsTurn(*requirements);
    auto made = td::TlsByteOwner::create(*bootstrap, nativeOptions(peer->port()), {64, 64}, config, options, copy, tls);
    REQUIRE(made);
    auto &owner = **made;
    REQUIRE(owner.write(std::as_bytes(std::span{*peerConfig.expectedClientBytes}), copy));
    REQUIRE(owner.start());
    REQUIRE(pump(owner, *requirements, [&] {
        return owner.bytes().core().engine->verified() && owner.bytes().pendingOutput() == 0;
    }));
    REQUIRE(owner.requestTlsClose());
    REQUIRE(pump(owner, *requirements, [&] {
        return owner.bytes().core().terminal().has_value();
    }));
    REQUIRE(owner.diagnostic().error == t::TlsError::NONE);
    const auto before = *owner.bytes().core().terminal();
    CHECK_FALSE(before.aborted);
    std::this_thread::sleep_for(250ms);
    t::TcpWork native{};
    org::ttldtor::qdcxx::protocol::LiveWork noCopy{};
    t::TlsWork noTls{};
    noTls.shape = requirements->shape;
    REQUIRE(owner.poll(native, noCopy, noTls));
    CHECK(owner.diagnostic().error == t::TlsError::NONE);
    CHECK(owner.bytes().core().terminal()->aborted == before.aborted);
}

TEST_CASE("raw TLS no-provider boundary rejects alias-null external ownership before retention") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    bool destroyed = false;
    auto external = std::shared_ptr<int>(new int{0}, [&](int *value) {
        destroyed = true;
        delete value;
    });
    auto config = settings();
    config.credentials = std::shared_ptr<org::ttldtor::qdcxx::client::CredentialProvider>(external, nullptr);
    REQUIRE_FALSE(config.credentials);
    REQUIRE(config.credentials.use_count() != 0);
    const auto options = x::options();
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto copy = x::sessionTurn();
    auto tls = x::tlsTurn(*requirements);
    auto made = td::TlsByteOwner::create(*bootstrap, nativeOptions(1), {64, 64}, config, options, copy, tls);
    CHECK_FALSE(made);

    if (!made) {
        CHECK(made.error().error == t::TlsError::INVALID_CONFIGURATION);
    }

    external.reset();
    config.credentials.reset();
    CHECK(destroyed);

    if (made) {
        made->reset();
    }

    CHECK(destroyed);
}
