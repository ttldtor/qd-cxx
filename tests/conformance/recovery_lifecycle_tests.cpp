// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: actual public recovery lifetime/cutoff over finite owned native loopback peers.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

#include <memory>
#include <optional>
#include <utility>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace m = root::mux;
namespace t = root::transport;
namespace r = root::test::recovery;
using namespace std::chrono_literals;

/** Create a real finite STREAM source with one-record callbacks for the lifetime assertions.
 * @param local Exact owned schema provenance shared with the eventual native session.
 * @return Unbound queue; no data, callback or synthetic source event has been admitted.
 */
m::MuxQueue stream(const c::LocalSchema &local) {
    auto work = r::queueWork();
    auto queue = m::MuxQueue::createStream(
        local, {16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024}, {16, 32, 128, 4096, 64, 8192, 64},
        {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
        {4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024}, work);
    REQUIRE(queue);

    return std::move(*queue);
}

/** Callback action selected explicitly before the recovery owner adopts this registration. */
enum class OwnerAction { DESTROY, REPLACE };

/** Observe callback lifetime without throwing test assertions through the production consumer boundary. */
struct LifecycleConsumer final : m::MuxConsumer {
    std::optional<t::TcpRecovery> &owner;       ///< Actual public handle used by the enclosing in-flight poll.
    std::optional<t::TcpRecovery> &replacement; ///< Separately configured replacement, unused for DESTROY.
    OwnerAction action;                         ///< Exact one-shot operation requested by the scenario.
    m::OwnedMuxBatch retained;       ///< Immutable application-owned result, surviving old Core/queue destruction.
    m::QueueIdentity deliveredEpoch; ///< Exact valid callback epoch, captured before public owner mutation.
    std::size_t callbacks = 0;       ///< Actual invocations; every supplied batch has at most one record.
    t::RecoveryError recursivePoll = t::RecoveryError::NONE;  ///< Actual nested poll category.
    t::RecoveryError recursiveStart = t::RecoveryError::NONE; ///< Actual nested start category.
    bool stopAccepted = false;       ///< Actual request-only cutoff result during the executing callback.
    bool ownerPresent = false;       ///< Whether the callback was entered with a live public owner.
    bool replacementPresent = false; ///< Whether the explicit replacement was available when requested.

    /** Bind only externally owned public handles; no recovery ownership cycle is created.
     * @param current Public owner whose synchronous poll will dispatch the callback.
     * @param next Optional independent configured owner, adopted only in REPLACE mode.
     * @param selected Explicit one-shot callback operation.
     */
    LifecycleConsumer(std::optional<t::TcpRecovery> &current, std::optional<t::TcpRecovery> &next,
                      OwnerAction selected) noexcept
        : owner(current), replacement(next), action(selected) {
    }

    /** Retain the already built detached batch, then invalidate or replace the executing public handle.
     * @param batch Callback-only borrow, never retained as a view or pointer.
     * @return Continue acknowledges this batch; the owner cutoff must gate every later invocation.
     */
    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        ++callbacks;

        if (callbacks != 1) {
            return m::BatchAction::CONTINUE;
        }

        retained = batch.retain();
        deliveredEpoch = retained.epoch();
        ownerPresent = owner.has_value();

        if (!ownerPresent) {
            return m::BatchAction::CONTINUE;
        }

        if (action == OwnerAction::DESTROY) {
            owner.reset();

            return m::BatchAction::CONTINUE;
        }

        auto work = r::turn();
        const auto polled = owner->poll(work);
        recursivePoll = polled ? t::RecoveryError::NONE : polled.error().error;
        const auto started = owner->start();
        recursiveStart = started ? t::RecoveryError::NONE : started.error().error;
        stopAccepted = owner->requestStop().has_value();
        replacementPresent = replacement.has_value();

        if (replacementPresent) {
            *owner = std::move(*replacement);
            replacement.reset();
        }

        return m::BatchAction::CONTINUE;
    }
};

/** Reserve the consumer's finite detached domain and submit one ordinary desired symbol.
 * @param queue Real source that will be adopted by recovery after this preparation.
 * @param consumer Externally owned callback with fixed result fields.
 * @param local Exact subscription provenance.
 * @return Producer-safe public handle retained through old owner destruction.
 */
m::MuxHandle subscribe(m::MuxQueue &queue, const std::shared_ptr<LifecycleConsumer> &consumer,
                       const c::LocalSchema &local) {
    auto work = r::queueWork();
    auto handle =
        queue.open({32, 512 * 1024, 8, 128 * 1024, 1, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, consumer, work);
    REQUIRE(handle);
    const std::array<c::SubscriptionRequest, 1> desired{{{{local.recordContext(0), u"A"}, {}}}};
    m::QueueSubmitWork submission{10000, 1000000, 1000000, 1000000};
    auto accepted = handle->replace(desired, submission);
    REQUIRE(accepted);

    return std::move(*handle);
}

/** Supply real protocol/schema input followed by two independently framed data points.
 * @return Finite byte-only script; first callback cutoff must prevent a second callback.
 */
root::test::TcpPeerScript deliveringScript() {
    root::test::TcpPeerScript script;
    script.initialBytes = r::ready();
    r::append(script.initialBytes, r::description(41));
    script.clientBytesToRead = 1;
    script.replyBytes = r::data(41, u"A", 101);
    r::append(script.replyBytes, r::data(41, u"A", 102));
    script.waitForControllerClose = true;

    return script;
}

/** Advance only the currently retained public owner under a finite real monotonic deadline.
 * @tparam Predicate Actual detached/public observation checked before each turn.
 * @param owner Actual public owner, allowed to become empty inside its poll callback.
 * @param peer Actual owned peer supplying detached finite native-byte observations on failure.
 * @param predicate Required real stopping condition.
 * @param timeout Finite test deadline, unrelated to SDK hard-time bounds.
 * @return Last detached old-operation status; failure is asserted outside the consumer boundary.
 */
template <class Predicate>
t::RecoveryStatus until(std::optional<t::TcpRecovery> &owner, const root::test::OwnedTcpPeer &peer, Predicate predicate,
                        std::chrono::milliseconds timeout = 4s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    t::RecoveryStatus last;

    while (!predicate() && owner && std::chrono::steady_clock::now() < deadline) {
        auto work = r::turn();
        auto progressed = owner->poll(work);
        REQUIRE(progressed);
        last = *progressed;
        std::this_thread::sleep_for(1ms);
    }

    const auto snapshot = peer.snapshot();
    INFO("last phase=", static_cast<int>(last.phase), " attempts=", last.attempts, " first stage=", last.first.stage,
         " recovery error=", static_cast<int>(last.first.error),
         " transport error=", static_cast<int>(last.first.transport.error),
         " protocolError=", static_cast<int>(last.first.transport.protocolError),
         " secondary stage=", last.secondary.stage,
         " secondary protocolError=", static_cast<int>(last.secondary.transport.protocolError));
    INFO("peer error=", static_cast<int>(snapshot.error), " native code=", snapshot.nativeCode,
         " first accepted=", snapshot.sessions[0].accepted,
         " first initial bytes=", snapshot.sessions[0].initialBytesWritten,
         " first reply bytes=", snapshot.sessions[0].replyBytesWritten,
         " first client bytes=", snapshot.sessions[0].received.size(),
         " second accepted=", snapshot.sessions[1].accepted,
         " second initial bytes=", snapshot.sessions[1].initialBytesWritten,
         " second reply bytes=", snapshot.sessions[1].replyBytesWritten,
         " second client bytes=", snapshot.sessions[1].received.size());
    REQUIRE(predicate());

    return last;
}

/** Check detached application data after its live native/queue owners have retired.
 * @param batch Actual retained callback result.
 * @param deliveredEpoch Exact callback epoch copied before destroying or replacing its live owner.
 */
void checkRetained(const m::OwnedMuxBatch &batch, const m::QueueIdentity &deliveredEpoch) {
    const auto records = batch.records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].symbol == u"A");
    REQUIRE(records[0].typedValues.size() == 3);
    CHECK(records[0].typedValues[2].raw == 101);
    REQUIRE(records[0].source);
    CHECK(records[0].source->senderId == 41);
    CHECK(batch.epoch().matches(deliveredEpoch));
}
} // namespace

TEST_CASE("recovery successor connects after bootstrap facade moves during actual backoff") {
    auto initialized = t::RuntimeBootstrap::create();
    REQUIRE(initialized);
    std::optional<t::RuntimeBootstrap> bootstrap(std::move(*initialized));
    std::optional<t::RuntimeBootstrap> movedBootstrap;
    root::test::TcpPeerConfig scripts;
    scripts.lifetime = 20s;
    scripts.sessions.resize(2);
    scripts.sessions[0].waitForControllerClose = true;
    scripts.sessions[1].initialBytes = r::ready();
    scripts.sessions[1].waitForControllerClose = true;
    root::test::OwnedTcpPeer peer(std::move(scripts));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto input = r::settings(peer.port(), 2, 300ms);
    input.timeouts.read = 5s;
    auto endpoint = r::endpoint(local, input);
    auto options = r::options();
    options.protocolInactivity = 2s;
    auto preparation = r::preparation();
    t::RecoveryQueues queues;
    auto created =
        t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), options, preparation);
    REQUIRE(created);
    std::optional<t::TcpRecovery> owner(std::move(*created));
    REQUIRE(owner->start());
    until(
        owner, peer,
        [&] {
            return owner->status().phase == t::RecoveryPhase::BACKOFF;
        },
        8s);
    REQUIRE(owner->status().attempts == 1);
    CHECK(owner->status().first.stage == "recovery-protocol-inactivity");
    movedBootstrap.emplace(std::move(*bootstrap));
    bootstrap.reset();
    CHECK_FALSE(bootstrap);
    peer.allowClose(0);
    until(
        owner, peer,
        [&] {
            return owner->status().attempts == 2 && owner->status().phase == t::RecoveryPhase::ACTIVE;
        },
        8s);
    CHECK(peer.snapshot().sessions[0].accepted);
    CHECK(peer.snapshot().sessions[1].accepted);
    CHECK(peer.snapshot().error == root::test::TcpPeerError::NONE);
    r::stop(*owner);
    CHECK(owner->status().attempts == 2);
    owner.reset();
    peer.stop();
}

TEST_CASE("consumer destruction of public recovery lets the old poll return with detached data retained") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::TcpPeerConfig scripts;
    scripts.sessions = {deliveringScript(), {}};
    root::test::OwnedTcpPeer peer(std::move(scripts));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    std::optional<t::TcpRecovery> replacement;
    std::optional<t::TcpRecovery> owner;
    auto consumer = std::make_shared<LifecycleConsumer>(owner, replacement, OwnerAction::DESTROY);
    auto queue = stream(local);
    auto handle = subscribe(queue, consumer, local);
    t::RecoveryQueues queues;
    queues.stream = std::move(queue);
    auto endpoint = r::endpoint(local, r::settings(peer.port(), 2, 20ms));
    auto preparation = r::preparation();
    auto created =
        t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), r::options(), preparation);
    REQUIRE(created);
    owner.emplace(std::move(*created));
    REQUIRE(owner->start());
    const auto returned = until(owner, peer, [&] {
        return consumer->callbacks != 0;
    });
    REQUIRE(consumer->ownerPresent);
    CHECK_FALSE(owner);
    CHECK(returned.stopped);
    CHECK(returned.attempts == 1);
    CHECK(consumer->callbacks == 1);
    const auto terminal = handle.terminal();
    REQUIRE(terminal);
    CHECK(terminal->counts.callbackStartedBatches == 1);
    CHECK(terminal->counts.callbackCompletedBatches == 1);
    handle = {};
    const auto callbacks = consumer->callbacks;
    std::this_thread::sleep_for(50ms);
    CHECK(consumer->callbacks == callbacks);
    CHECK_FALSE(peer.snapshot().sessions[1].accepted);
    peer.stop();
    checkRetained(consumer->retained, consumer->deliveredEpoch);
}

TEST_CASE("consumer move replacement rejects old reentry and cannot start the configured successor implicitly") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::TcpPeerConfig scripts;
    scripts.sessions = {deliveringScript(), {}};
    scripts.sessions[1].initialBytes = r::ready();
    scripts.sessions[1].waitForControllerClose = true;
    root::test::OwnedTcpPeer peer(std::move(scripts));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto endpoint = r::endpoint(local, r::settings(peer.port(), 2, 20ms));
    std::optional<t::TcpRecovery> replacement;
    std::optional<t::TcpRecovery> owner;
    auto consumer = std::make_shared<LifecycleConsumer>(owner, replacement, OwnerAction::REPLACE);
    auto queue = stream(local);
    auto handle = subscribe(queue, consumer, local);
    t::RecoveryQueues newQueues;
    auto preparation = r::preparation();
    auto prepared =
        t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(newQueues), r::options(), preparation);
    REQUIRE(prepared);
    replacement.emplace(std::move(*prepared));
    t::RecoveryQueues oldQueues;
    oldQueues.stream = std::move(queue);
    preparation = r::preparation();
    auto created =
        t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(oldQueues), r::options(), preparation);
    REQUIRE(created);
    owner.emplace(std::move(*created));
    REQUIRE(owner->start());
    const auto oldReturned = until(owner, peer, [&] {
        return consumer->callbacks != 0;
    });
    REQUIRE(consumer->ownerPresent);
    REQUIRE(consumer->replacementPresent);
    CHECK(consumer->recursivePoll == t::RecoveryError::REENTRANT);
    CHECK(consumer->recursiveStart == t::RecoveryError::REENTRANT);
    CHECK(consumer->stopAccepted);
    CHECK(oldReturned.stopped);
    CHECK(oldReturned.attempts == 1);
    REQUIRE(owner);
    CHECK_FALSE(replacement);
    CHECK(owner->status().phase == t::RecoveryPhase::CONFIGURED);
    CHECK(owner->status().attempts == 0);
    CHECK_FALSE(owner->status().stopped);

    for (unsigned turn = 0; turn != 4; ++turn) {
        auto work = r::turn();
        auto status = owner->poll(work);
        REQUIRE(status);
        CHECK(status->phase == t::RecoveryPhase::CONFIGURED);
        CHECK(status->attempts == 0);
    }

    CHECK_FALSE(peer.snapshot().sessions[1].accepted);
    CHECK(consumer->callbacks == 1);
    peer.allowClose(0);
    REQUIRE(owner->start());
    until(owner, peer, [&] {
        return owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    CHECK(owner->status().attempts == 1);
    CHECK(peer.snapshot().sessions[1].accepted);
    CHECK(peer.snapshot().error == root::test::TcpPeerError::NONE);
    r::stop(*owner);
    owner.reset();
    CHECK(consumer->callbacks == 1);
    const auto terminal = handle.terminal();
    REQUIRE(terminal);
    CHECK(terminal->counts.callbackCompletedBatches == 1);
    handle = {};
    peer.stop();
    checkRetained(consumer->retained, consumer->deliveredEpoch);
}

TEST_CASE("repeated public stop requests monotonically close an unstarted STREAM recovery owner") {
    constexpr unsigned MAX_STOP_TURNS = 200;
    constexpr unsigned POST_CLOSE_STOP_TURNS = 4;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto local = r::schema();
    auto endpoint = r::endpoint(local, r::settings(4321));
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto preparation = r::preparation();
    auto owner =
        t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), r::options(), preparation);
    REQUIRE(owner);
    CHECK(owner->status().phase == t::RecoveryPhase::CONFIGURED);
    CHECK(owner->status().attempts == 0);
    CHECK_FALSE(owner->status().stopped);
    bool closed = false;

    for (unsigned turn = 0; turn != MAX_STOP_TURNS; ++turn) {
        REQUIRE(owner->requestStop());
        CHECK(owner->status().stopped);
        CHECK(owner->status().attempts == 0);
        auto work = r::turn();
        const auto status = owner->poll(work);
        REQUIRE(status);
        CHECK(status->stopped);
        CHECK(status->attempts == 0);

        if (status->phase == t::RecoveryPhase::CLOSED) {
            closed = true;

            break;
        }
    }

    REQUIRE(closed);

    for (unsigned turn = 0; turn != POST_CLOSE_STOP_TURNS; ++turn) {
        REQUIRE(owner->requestStop());
        auto work = r::turn();
        const auto status = owner->poll(work);
        REQUIRE(status);
        CHECK(status->phase == t::RecoveryPhase::CLOSED);
        CHECK(status->stopped);
        CHECK(status->attempts == 0);
    }
}
