// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: actual bounded loopback TCP recovery; no external service or process.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

namespace {
namespace root = org::ttldtor::qdcxx;
namespace t = root::transport;
namespace r = root::test::recovery;
using namespace std::chrono_literals;
} // namespace

TEST_CASE("plain recovery requires actual protocol input even with empty demand and exhausts finite silence retries") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::TcpPeerConfig peerConfig;
    peerConfig.sessions.resize(2);

    for (auto &script : peerConfig.sessions)
        script.waitForControllerClose = true;

    root::test::OwnedTcpPeer peer(std::move(peerConfig));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto input = r::settings(peer.port(), 2);
    auto endpoint = r::endpoint(local, input);
    auto blueprint = r::blueprint();
    auto options = r::options();
    options.protocolInactivity = 80ms;
    options.heartbeatInterval = 10ms;
    auto preparation = r::preparation();
    t::RecoveryQueues queues;
    auto owner =
        t::TcpRecovery::create(*bootstrap, endpoint, std::move(blueprint), std::move(queues), options, preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    bool active = false;
    bool backoff = false;
    const auto deadline = std::chrono::steady_clock::now() + 4s;

    while (std::chrono::steady_clock::now() < deadline) {
        auto work = r::turn();
        auto progressed = owner->poll(work);
        REQUIRE(progressed);
        active = active || progressed->phase == t::RecoveryPhase::ACTIVE;

        if (progressed->phase == t::RecoveryPhase::BACKOFF) {
            backoff = true;
            peer.allowClose(0);
        }

        if (progressed->phase == t::RecoveryPhase::EXHAUSTED || progressed->phase == t::RecoveryPhase::FAILED)
            break;

        std::this_thread::sleep_for(1ms);
    }

    const auto status = owner->status();
    INFO("phase=", static_cast<int>(status.phase), " attempts=", status.attempts,
         " error=", static_cast<int>(status.first.error), " stage=", status.first.stage);
    CHECK_FALSE(active);
    CHECK(backoff);
    CHECK(status.phase == t::RecoveryPhase::EXHAUSTED);
    CHECK(status.attempts == 2);
    CHECK(status.first.stage == "recovery-protocol-inactivity");
    peer.stop();
    CHECK(peer.snapshot().sessions[0].accepted);
    CHECK(peer.snapshot().sessions[1].accepted);
}

TEST_CASE("plain recovery cancels a long backoff without entering another factory") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::TcpPeerConfig peerConfig;
    peerConfig.sessions.resize(2);
    peerConfig.sessions[0].waitForControllerClose = true;
    root::test::OwnedTcpPeer peer(std::move(peerConfig));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto endpoint = r::endpoint(local, r::settings(peer.port(), 3, 5s));
    auto blueprint = r::blueprint();
    auto options = r::options();
    options.protocolInactivity = 60ms;
    auto preparation = r::preparation();
    t::RecoveryQueues queues;
    auto owner =
        t::TcpRecovery::create(*bootstrap, endpoint, std::move(blueprint), std::move(queues), options, preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    const auto deadline = std::chrono::steady_clock::now() + 3s;

    while (owner->status().phase != t::RecoveryPhase::BACKOFF && std::chrono::steady_clock::now() < deadline) {
        auto work = r::turn();
        REQUIRE(owner->poll(work));
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(owner->status().phase == t::RecoveryPhase::BACKOFF);
    CHECK(owner->status().attempts == 1);
    const auto stoppedAt = std::chrono::steady_clock::now();
    r::stop(*owner);
    CHECK(std::chrono::steady_clock::now() - stoppedAt < 1s);
    CHECK(owner->status().attempts == 1);
    peer.stop();
    CHECK_FALSE(peer.snapshot().sessions[1].accepted);
}

TEST_CASE("plain recovery explicit disconnect suspends without a fake failure and resumes the same stable owner") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::TcpPeerConfig peerConfig;
    peerConfig.sessions.resize(2);

    for (auto &script : peerConfig.sessions) {
        script.initialBytes = r::ready();
        script.waitForControllerClose = true;
    }

    root::test::OwnedTcpPeer peer(std::move(peerConfig));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto endpoint = r::endpoint(local, r::settings(peer.port(), 2));
    auto blueprint = r::blueprint();
    auto options = r::options();
    auto preparation = r::preparation();
    t::RecoveryQueues queues;
    auto owner =
        t::TcpRecovery::create(*bootstrap, endpoint, std::move(blueprint), std::move(queues), options, preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    auto cutoff = std::chrono::steady_clock::now() + 2s;

    while (owner->status().phase != t::RecoveryPhase::ACTIVE && std::chrono::steady_clock::now() < cutoff) {
        auto work = r::turn();
        REQUIRE(owner->poll(work));
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(owner->status().phase == t::RecoveryPhase::ACTIVE);
    CHECK(owner->status().attempts == 1);
    REQUIRE(owner->requestDisconnect());
    cutoff = std::chrono::steady_clock::now() + 2s;

    while (owner->status().phase != t::RecoveryPhase::SUSPENDED && std::chrono::steady_clock::now() < cutoff) {
        auto work = r::turn();
        REQUIRE(owner->poll(work));
        std::this_thread::sleep_for(1ms);
    }

    const auto paused = owner->status();
    CAPTURE(paused.first.stage);
    CAPTURE(paused.first.transport.error);
    REQUIRE(paused.phase == t::RecoveryPhase::SUSPENDED);
    CHECK(paused.first.error == t::RecoveryError::NONE);
    CHECK_FALSE(paused.terminal);
    CHECK(paused.attempts == 1);
    peer.allowClose(0);
    REQUIRE(owner->requestReconnect());
    cutoff = std::chrono::steady_clock::now() + 2s;

    while (owner->status().phase != t::RecoveryPhase::ACTIVE && std::chrono::steady_clock::now() < cutoff) {
        auto work = r::turn();
        REQUIRE(owner->poll(work));
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(owner->status().phase == t::RecoveryPhase::ACTIVE);
    CHECK(owner->status().attempts == 2);
    CHECK(owner->status().first.error == t::RecoveryError::NONE);
    CHECK(owner->status().first.stage == "none");
    const auto refused = owner->requestReconnect();
    REQUIRE_FALSE(refused);
    CHECK(refused.error().error == t::RecoveryError::RESOURCE_LIMIT);
    r::stop(*owner);
    peer.stop();
    const auto snapshot = peer.snapshot();
    CHECK(snapshot.sessions[0].accepted);
    CHECK(snapshot.sessions[1].accepted);
}
