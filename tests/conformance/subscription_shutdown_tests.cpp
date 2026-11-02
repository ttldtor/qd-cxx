// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: composed local-prefix shutdown over owned, finite native peers.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace m = root::mux;
namespace t = root::transport;
namespace r = root::test::recovery;
using namespace std::chrono_literals;

/** Create a shared source with one-record callbacks and a two-record routing quantum.
 * @param local Exact provenance subsequently adopted by recovery.
 * @return Unactivated finite queue.
 */
m::MuxQueue stream(const c::LocalSchema &local) {
    auto work = r::queueWork();
    auto queue = m::MuxQueue::createStream(
        local, {16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024}, {16, 32, 128, 4096, 64, 8192, 64},
        {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
        {4, 128, 16, 2, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024}, work);
    REQUIRE(queue);

    return std::move(*queue);
}

/** Test consumer keeps only owning values; assertions remain outside production callback boundaries. */
struct Receiver final : m::MuxConsumer {
    std::vector<std::int64_t> values;       ///< Finite four-record observation.
    std::vector<m::OwnedMuxBatch> retained; ///< Actual independently retained payloads.
    std::function<void()> action;           ///< Explicit one-shot owner operation.
    std::function<void()> observe;          ///< Optional bounded detached phase observation at callback entry.
    bool pause = false;                     ///< Pause after the first accepted callback.
    bool throws = false;                    ///< Actual non-cancellation callback failure.
    bool shapeValid = true;                 ///< Checked after callback return.

    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        if (observe)
            observe();

        for (const auto &record : batch.records()) {
            if (record.typedValues.size() != 3) {
                shapeValid = false;

                return m::BatchAction::PAUSE;
            }

            values.push_back(record.typedValues[2].raw);
        }

        retained.push_back(batch.retain());

        if (action) {
            auto once = std::move(action);
            once();
        }

        if (throws)
            throw std::runtime_error("controlled shutdown consumer failure");

        return pause ? m::BatchAction::PAUSE : m::BatchAction::CONTINUE;
    }
};

/** Register one actual desired membership, leaving application to the serialized recovery owner.
 * @param queue Source to be adopted without a substitute protocol owner.
 * @param local Exact schema origin.
 * @param receiver Consumer retained independently by the test.
 * @param applied Optional caller-owned initial command receipt.
 * @return Producer-safe handle, independent of recovery facade lifetime.
 */
m::MuxHandle subscribe(m::MuxQueue &queue, const c::LocalSchema &local, const std::shared_ptr<Receiver> &receiver,
                       m::MuxCommandTicket *applied = nullptr) {
    auto work = r::queueWork();
    auto opened =
        queue.open({32, 512 * 1024, 8, 128 * 1024, 1, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, receiver, work);
    REQUIRE(opened);
    const std::array<c::SubscriptionRequest, 1> desired{{{{local.recordContext(0), u"A"}, {}}}};
    m::QueueSubmitWork submission{10000, 1000000, 1000000, 1000000};
    auto ticket = opened->replace(desired, submission);
    REQUIRE(ticket);

    if (applied)
        *applied = std::move(*ticket);

    return std::move(*opened);
}

/** Encode one complete four-record source event independently of the production subscription writer.
 * @return A single DATA admission boundary, not four independently admitted frames.
 */
std::vector<std::byte> records() {
    std::vector<std::byte> payload;

    for (const auto value : {10, 20, 30, 40}) {
        payload.push_back(std::byte{0xfd});
        r::number(payload, 1);
        payload.push_back(std::byte{'A'});
        r::number(payload, 41);
        r::number(payload, 0);
        r::number(payload, 1);
        r::number(payload, value);
    }

    return r::frame(15, payload);
}

/** Supply a real descriptor and payload after the native client sends its first output.
 * @param reply Complete or deliberately incomplete native input.
 * @return Owned finite peer script.
 */
root::test::TcpPeerConfig peerConfig(std::vector<std::byte> reply = records()) {
    root::test::TcpPeerScript script;
    script.initialBytes = r::ready();
    r::append(script.initialBytes, r::description(41));
    script.clientBytesToRead = 1;
    script.replyBytes = std::move(reply);
    script.waitForControllerClose = true;
    root::test::TcpPeerConfig result;
    result.sessions.push_back(std::move(script));

    return result;
}

/** Advance a real owner until a public observation, bounded even on a regression.
 * @tparam Predicate Detached readiness condition.
 * @param owner Stable actual public owner.
 * @param predicate Exact public boundary.
 * @param callbacks Permit consumer entry; zero credits still allow native and admission work.
 * @return Last detached status.
 */
template <class Predicate> t::RecoveryStatus until(t::TcpRecovery &owner, Predicate predicate, bool callbacks = true) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    auto last = owner.status();

    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        auto work = r::turn();

        if (!callbacks)
            work.queues[static_cast<std::size_t>(t::RecoveryFamily::STREAM)].callbacks = 0;

        auto progressed = owner.poll(work);
        REQUIRE(progressed);
        last = *progressed;
        std::this_thread::sleep_for(1ms);
    }

    INFO("phase=", static_cast<int>(last.phase), " attempts=", last.attempts,
         " first=", static_cast<int>(last.first.error), " stage=", last.first.stage);
    REQUIRE(predicate());

    return last;
}

/** Prepare an unstarted actual owner from explicit queue/endpoint inputs.
 * @param bootstrap Independent process/native lifetime root.
 * @param local Exact immutable schema.
 * @param port Owned native peer or unused configured-only port.
 * @param queues Whole source owners, adopted by the real create transaction.
 * @return Real recovery, with no hidden test transport.
 */
t::TcpRecovery create(t::RuntimeBootstrap &bootstrap, const c::LocalSchema &local, std::uint16_t port,
                      t::RecoveryQueues queues) {
    auto input = r::settings(port, 1);
    input.timeouts.read = 5s;
    auto endpoint = r::endpoint(local, input);
    auto work = r::preparation();
    auto options = r::options();
    options.protocolInactivity = 4s;
    auto owner = t::TcpRecovery::create(bootstrap, endpoint, r::blueprint(), std::move(queues), options, work);
    REQUIRE(owner);

    return std::move(*owner);
}

/** Read detached retained records after actual native and public-owner retirement.
 * @param receiver Caller-owned batches; no borrowed callback span survives.
 * @param expected Complete selected values in callback order.
 */
void retainedValues(const Receiver &receiver, std::span<const std::int64_t> expected) {
    REQUIRE(receiver.shapeValid);
    REQUIRE(receiver.retained.size() == expected.size());

    for (std::size_t index = 0; index != expected.size(); ++index) {
        const auto records = receiver.retained[index].records();
        REQUIRE(records.size() == 1);
        REQUIRE(records[0].typedValues.size() == 3);
        CHECK(records[0].typedValues[2].raw == expected[index]);
    }
}
} // namespace

TEST_CASE("subscription shutdown configured owners preserve earliest drain and immutable terminal") {
    for (const bool withQueue : {false, true}) {
        for (const bool abort : {false, true}) {
            auto bootstrap = t::RuntimeBootstrap::create();
            REQUIRE(bootstrap);
            auto local = r::schema();
            t::RecoveryQueues queues;

            if (withQueue)
                queues.stream.emplace(stream(local));

            auto owner = create(*bootstrap, local, 4321, std::move(queues));
            const auto first = std::chrono::steady_clock::now() + 5s;
            REQUIRE(owner.requestDrain(first));
            REQUIRE(owner.requestDrain(first + 10s));
            REQUIRE(owner.nextWake());
            CHECK(*owner.nextWake() <= first);
            CHECK(owner.status().stopped);
            CHECK_FALSE(owner.status().terminal);

            if (abort)
                REQUIRE(owner.requestStop());

            const auto stopped = until(owner, [&] {
                return owner.status().terminal.has_value();
            });
            REQUIRE(stopped.terminal);
            const auto before = *stopped.terminal;
            CHECK(stopped.phase == t::RecoveryPhase::CLOSED);
            CHECK(stopped.attempts == 0);
            CHECK(before.outcome == (abort ? m::TerminalOutcome::ABORTED : m::TerminalOutcome::DRAINED));
            CHECK_FALSE(before.deadlineExpired);
            CHECK_FALSE(before.heldDataDiscarded);
            CHECK(before.queues[1].has_value() == withQueue);

            for (unsigned repeat = 0; repeat != 3; ++repeat) {
                REQUIRE(owner.requestStop());
                REQUIRE(owner.requestDrain(std::chrono::steady_clock::time_point::min()));
                auto work = r::turn();
                const auto after = owner.poll(work);
                REQUIRE(after);
                REQUIRE(after->terminal);
                CHECK(after->terminal->outcome == before.outcome);
                CHECK(after->terminal->deadlineExpired == before.deadlineExpired);
                CHECK(after->terminal->heldDataDiscarded == before.heldDataDiscarded);

                if (withQueue) {
                    REQUIRE(after->terminal->queues[1]);
                    CHECK(after->terminal->queues[1]->epoch.matches(before.queues[1]->epoch));
                }
            }
        }
    }
}

TEST_CASE("subscription shutdown expired configured drain cannot report successful completion") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto local = r::schema();
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto owner = create(*bootstrap, local, 4321, std::move(queues));
    REQUIRE(owner.requestDrain(std::chrono::steady_clock::time_point::min()));
    const auto stopped = until(owner, [&] {
        return owner.status().terminal.has_value();
    });
    REQUIRE(stopped.terminal);
    CHECK(stopped.terminal->outcome == m::TerminalOutcome::ABORTED);
    CHECK(stopped.terminal->deadlineExpired);
    CHECK(stopped.attempts == 0);
}

TEST_CASE("subscription shutdown drains real admitted payload after native retirement independently per client") {
    for (const unsigned peerBehavior : {0U, 1U, 2U}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        auto config = peerConfig();
        config.sessions[0].waitForControllerReply = true;
        root::test::OwnedTcpPeer peer(std::move(config));
        REQUIRE(peer.port() != 0);
        auto local = r::schema();
        t::RecoveryQueues queues;
        queues.stream.emplace(stream(local));
        auto healthy = std::make_shared<Receiver>();
        auto other = std::make_shared<Receiver>();
        other->pause = peerBehavior == 1;
        other->throws = peerBehavior == 2;
        m::MuxCommandTicket firstApplied, secondApplied;
        auto first = subscribe(*queues.stream, local, healthy, &firstApplied);
        auto second = subscribe(*queues.stream, local, other, &secondApplied);
        std::optional<t::TcpRecovery> owner;
        owner.emplace(create(*bootstrap, local, peer.port(), std::move(queues)));
        bool accepted = false;
        std::vector<t::RecoveryPhase> phases;
        healthy->observe = [&] {
            phases.push_back(owner->status().phase);
        };
        healthy->action = [&] {
            accepted = owner->requestDrain(std::chrono::steady_clock::now() + 3s).has_value();
        };
        REQUIRE(owner->start());
        until(*owner, [&] {
            return firstApplied.result() && secondApplied.result() &&
                   firstApplied.result()->status == m::CommandStatus::APPLIED &&
                   secondApplied.result()->status == m::CommandStatus::APPLIED &&
                   first.state() == m::QueueClientState::ACTIVE && second.state() == m::QueueClientState::ACTIVE &&
                   owner->status().phase == t::RecoveryPhase::ACTIVE;
        });
        peer.allowReply(0);
        const auto stopped = until(*owner, [&] {
            return owner->status().terminal.has_value();
        });
        CHECK(accepted);
        REQUIRE(stopped.terminal);
        REQUIRE(stopped.terminal->queues[1]);
        CHECK(stopped.phase == t::RecoveryPhase::CLOSED);
        CHECK(stopped.attempts == 1);
        const std::array<std::int64_t, 4> expected{10, 20, 30, 40};
        CHECK(healthy->values == std::vector<std::int64_t>(expected.begin(), expected.end()));
        REQUIRE(first.terminal());
        CHECK(first.terminal()->outcome == m::TerminalOutcome::DRAINED);
        CHECK(first.terminal()->counts.callbackCompletedRecords == 4);
        CHECK(stopped.terminal->queues[1]->counts.acceptedRecords == 4);
        CHECK_FALSE(stopped.terminal->deadlineExpired);
        CHECK_FALSE(stopped.terminal->heldDataDiscarded);
        REQUIRE(phases.size() == 4);

        // STOPPING is a phase observation; the Core's native-disposal barrier precedes its drain callbacks.
        for (std::size_t index = 1; index != phases.size(); ++index)
            CHECK(phases[index] == t::RecoveryPhase::STOPPING);

        const auto outcome = peerBehavior == 0   ? m::TerminalOutcome::DRAINED
                             : peerBehavior == 1 ? m::TerminalOutcome::ABORTED
                                                 : m::TerminalOutcome::FAILED;
        CHECK(stopped.terminal->outcome == outcome);

        if (peerBehavior == 0) {
            REQUIRE(stopped.terminal->wire);
            const auto &wire = *stopped.terminal->wire;

            if (wire.primary.error != t::TcpError::NONE) {
                CHECK(stopped.first.transport.error == wire.primary.error);
                CHECK(stopped.first.transport.nativeCode == wire.primary.nativeCode);
                CHECK(stopped.first.transport.stage == wire.primary.stage);
            }

            std::cout << "SUBSCRIPTION_DRAIN_WIRE\t" << static_cast<int>(wire.primary.error) << '\t'
                      << wire.primary.nativeCode << '\t' << wire.primary.stage << '\n';
        }

        if (peerBehavior == 2) {
            REQUIRE(second.terminal());
            REQUIRE(second.terminal()->primary);
            CHECK(second.terminal()->primary->error == m::QueueError::CALLBACK_FAILURE);
        }

        CHECK_FALSE(healthy->retained.empty());
        const auto detached = *stopped.terminal;
        owner.reset();
        retainedValues(*healthy, expected);
        REQUIRE(first.terminal());
        REQUIRE(detached.queues[1]);
        CHECK(detached.queues[1]->epoch.matches(first.terminal()->epoch));
        healthy->retained.clear();
        other->retained.clear();
        peer.stop();
    }
}

TEST_CASE("subscription shutdown callback drain permits only the admitted prefix and expiry fences the next callback") {
    constexpr auto CALLBACK_DELAY = 20ms;
    constexpr auto CALLBACK_DEADLINE = 5ms;

    for (const unsigned mode : {0U, 1U, 2U}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        root::test::OwnedTcpPeer peer(peerConfig());
        REQUIRE(peer.port() != 0);
        auto local = r::schema();
        t::RecoveryQueues queues;
        queues.stream.emplace(stream(local));
        auto receiver = std::make_shared<Receiver>();
        auto handle = subscribe(*queues.stream, local, receiver);
        auto owner = create(*bootstrap, local, peer.port(), std::move(queues));
        bool accepted = false;
        receiver->action = [&] {
            const auto deadline = mode == 1   ? std::chrono::steady_clock::time_point::min()
                                  : mode == 2 ? std::chrono::steady_clock::now() + CALLBACK_DEADLINE
                                              : std::chrono::steady_clock::now() + 3s;
            accepted = owner.requestDrain(deadline).has_value();

            if (mode == 2)
                std::this_thread::sleep_for(CALLBACK_DELAY);
        };
        REQUIRE(owner.start());
        const auto stopped = until(owner, [&] {
            return owner.status().terminal.has_value();
        });
        CHECK(accepted);
        REQUIRE(stopped.terminal);
        CHECK(receiver->values.size() == (mode ? 1U : 4U));
        CHECK(stopped.terminal->outcome == (mode ? m::TerminalOutcome::ABORTED : m::TerminalOutcome::DRAINED));
        CHECK(stopped.terminal->deadlineExpired == (mode != 0));
        REQUIRE(handle.terminal());
        CHECK(handle.terminal()->counts.callbackCompletedRecords == (mode ? 1U : 4U));
        peer.stop();
    }
}

TEST_CASE("subscription shutdown distinguishes a held complete DATA event from queue-admitted delivery") {
    constexpr unsigned HELD_EVENT_TURNS = 100;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::OwnedTcpPeer peer(peerConfig());
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto receiver = std::make_shared<Receiver>();
    auto handle = subscribe(*queues.stream, local, receiver);
    auto owner = create(*bootstrap, local, peer.port(), std::move(queues));
    REQUIRE(owner.start());
    const auto deadline = std::chrono::steady_clock::now() + 4s;
    bool supplied = false;

    while (!supplied && std::chrono::steady_clock::now() < deadline) {
        auto work = r::turn();
        work.queues[1].operations = 0;
        REQUIRE(owner.poll(work));
        const auto snapshot = peer.snapshot();
        supplied = snapshot.sessions[0].replyBytesWritten == records().size();
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(supplied);

    for (unsigned turn = 0; turn != HELD_EVENT_TURNS; ++turn) {
        auto work = r::turn();
        work.queues[1].operations = 0;
        REQUIRE(owner.poll(work));
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(owner.requestDrain(std::chrono::steady_clock::now() + 3s));
    const auto stopped = until(owner, [&] {
        return owner.status().terminal.has_value();
    });
    REQUIRE(stopped.terminal);
    REQUIRE(stopped.terminal->queues[1]);
    CHECK(stopped.terminal->heldDataDiscarded);
    CHECK(stopped.terminal->queues[1]->counts.acceptedRecords == 0);
    CHECK(receiver->values.empty());
    REQUIRE(handle.terminal());
    peer.stop();
}

TEST_CASE("subscription shutdown incomplete native frame is outside the admitted delivery prefix") {
    constexpr unsigned BUFFERED_INPUT_TURNS = 100;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto truncated = records();
    truncated.resize(3);
    root::test::OwnedTcpPeer peer(peerConfig(std::move(truncated)));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto receiver = std::make_shared<Receiver>();
    auto handle = subscribe(*queues.stream, local, receiver);
    auto owner = create(*bootstrap, local, peer.port(), std::move(queues));
    REQUIRE(owner.start());
    until(owner, [&] {
        const auto snapshot = peer.snapshot();

        return snapshot.sessions[0].replyBytesWritten == 3 && owner.status().phase == t::RecoveryPhase::ACTIVE;
    });

    for (unsigned turn = 0; turn != BUFFERED_INPUT_TURNS; ++turn) {
        auto work = r::turn();
        REQUIRE(owner.poll(work));
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(owner.requestDrain(std::chrono::steady_clock::now() + 3s));
    const auto stopped = until(owner, [&] {
        return owner.status().terminal.has_value();
    });
    REQUIRE(stopped.terminal);
    REQUIRE(stopped.terminal->queues[1]);
    REQUIRE(stopped.terminal->wire);
    CHECK(stopped.terminal->wire->discardedInputBytes == 3);
    CHECK(stopped.terminal->queues[1]->counts.acceptedRecords == 0);
    CHECK(stopped.terminal->outcome == m::TerminalOutcome::DRAINED);
    CHECK_FALSE(stopped.terminal->heldDataDiscarded);
    CHECK(receiver->values.empty());
    REQUIRE(handle.terminal());
    peer.stop();
}

TEST_CASE("subscription shutdown preserves a recovered diagnostic without inventing a shutdown failure") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto config = peerConfig({});
    config.sessions[0].waitForControllerClose = false;
    auto second = config.sessions[0];
    second.waitForControllerClose = true;
    config.sessions.push_back(std::move(second));
    root::test::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto input = r::settings(peer.port(), 2, 20ms);
    input.timeouts.read = 5s;
    auto endpoint = r::endpoint(local, input);
    auto preparation = r::preparation();
    auto options = r::options();
    options.protocolInactivity = 4s;
    t::RecoveryQueues queues;
    auto owner = t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), options, preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    const auto recovered = until(*owner, [&] {
        return owner->status().attempts == 2 && owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    REQUIRE(recovered.first.error != t::RecoveryError::NONE);
    REQUIRE(owner->requestDrain(std::chrono::steady_clock::now() + 3s));
    const auto stopped = until(*owner, [&] {
        return owner->status().terminal.has_value();
    });
    REQUIRE(stopped.terminal);
    CHECK(stopped.terminal->outcome == m::TerminalOutcome::DRAINED);
    CHECK(stopped.first.error == recovered.first.error);
    CHECK(stopped.first.stage == recovered.first.stage);
    CHECK(stopped.attempts == 2);
    peer.stop();
}

TEST_CASE("subscription shutdown cannot turn a current fatal protocol failure into a successful drain") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::TcpPeerScript script;
    const std::array<std::byte, 1> malformed{std::byte{0}};
    script.initialBytes = r::frame(1, malformed);
    script.waitForControllerClose = true;
    root::test::TcpPeerConfig config;
    config.sessions.push_back(std::move(script));
    root::test::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto owner = create(*bootstrap, local, peer.port(), {});
    REQUIRE(owner.start());
    const auto failed = until(owner, [&] {
        return owner.status().first.error != t::RecoveryError::NONE;
    });
    CHECK(failed.first.error == t::RecoveryError::PROTOCOL_FAILURE);
    CHECK_FALSE(failed.terminal);
    REQUIRE(owner.requestDrain(std::chrono::steady_clock::now() + 3s));
    const auto stopped = until(owner, [&] {
        return owner.status().terminal.has_value();
    });
    REQUIRE(stopped.terminal);
    CHECK(stopped.terminal->outcome == m::TerminalOutcome::FAILED);
    CHECK(stopped.first.error == failed.first.error);
    CHECK(stopped.first.stage == failed.first.stage);
    peer.stop();
}

TEST_CASE("subscription shutdown supersedes an unfunded pending source reset without fabricating reset failure") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto config = peerConfig({});
    config.sessions[0].waitForControllerClose = false;
    root::test::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto receiver = std::make_shared<Receiver>();
    auto handle = subscribe(*queues.stream, local, receiver);
    auto owner = create(*bootstrap, local, peer.port(), std::move(queues));
    REQUIRE(owner.start());
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (owner.status().phase != t::RecoveryPhase::RESETTING && std::chrono::steady_clock::now() < deadline) {
        auto work = r::turn();
        work.queues[1].operations = 0;
        REQUIRE(owner.poll(work));
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(owner.status().phase == t::RecoveryPhase::RESETTING);
    REQUIRE(owner.requestDrain(std::chrono::steady_clock::now() + 3s));
    const auto stopped = until(owner, [&] {
        return owner.status().terminal.has_value();
    });
    REQUIRE(stopped.terminal);
    CHECK(stopped.terminal->outcome == m::TerminalOutcome::ABORTED);
    CHECK(stopped.first.error != t::RecoveryError::RESET_FAILURE);
    CHECK(stopped.secondary.error != t::RecoveryError::RESET_FAILURE);
    CHECK(stopped.attempts == 1);
    CHECK(receiver->values.empty());
    REQUIRE(handle.terminal());
    peer.stop();
}

TEST_CASE("subscription shutdown retires actual pending read and write readiness before terminal publication") {
    constexpr unsigned HELD_HANDLER_TURNS = 2;
    constexpr std::size_t WRITE_QUANTUM = 16;
    constexpr auto STREAM_BIT = static_cast<std::uint8_t>(1U << static_cast<unsigned>(t::RecoveryFamily::STREAM));
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::TcpPeerScript script;
    script.initialBytes = r::ready();
    script.waitForControllerClose = true;
    root::test::TcpPeerConfig config;
    config.sessions.push_back(std::move(script));
    root::test::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto input = r::settings(peer.port(), 1);
    input.budgets.tcp.writeBytes = WRITE_QUANTUM;
    input.timeouts.read = 5s;
    input.timeouts.write = 5s;
    auto endpoint = r::endpoint(local, input);
    auto preparation = r::preparation();
    auto options = r::options();
    options.protocolInactivity = 4s;
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto receiver = std::make_shared<Receiver>();
    auto queueWork = r::queueWork();
    auto handle = queues.stream->open({32, 512 * 1024, 8, 128 * 1024, 1, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096},
                                      receiver, queueWork);
    REQUIRE(handle);
    auto owner = t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), options, preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    until(*owner, [&] {
        return owner->status().phase == t::RecoveryPhase::ACTIVE;
    });

    const std::array<c::SubscriptionRequest, 1> desired{
        {{{local.recordContext(0), u"LONG-SYMBOL-FOR-PENDING-WRITE"}, {}}}};
    m::QueueSubmitWork submission{10000, 1000000, 1000000, 1000000};
    auto accepted = handle->add(desired, submission);
    REQUIRE(accepted);
    const auto replayDeadline = std::chrono::steady_clock::now() + 5s;
    bool replayed = false;

    while (!replayed && std::chrono::steady_clock::now() < replayDeadline) {
        auto work = r::turn();
        work.runtime.handlers = 0;
        const auto progressed = owner->poll(work);
        REQUIRE(progressed);
        const auto applied = accepted->result();
        replayed = applied && applied->status == m::CommandStatus::APPLIED &&
                   handle->state() == m::QueueClientState::ACTIVE && (progressed->convergedFamilies & STREAM_BIT) != 0;
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(replayed);

    for (unsigned turn = 0; turn != HELD_HANDLER_TURNS; ++turn) {
        auto work = r::turn();
        work.runtime.handlers = 0;
        REQUIRE(owner->poll(work));
    }

    REQUIRE(owner->requestDrain(std::chrono::steady_clock::now() + 3s));
    auto withheld = r::turn();
    withheld.runtime.handlers = 0;
    const auto pending = owner->poll(withheld);
    REQUIRE(pending);
    CHECK_FALSE(pending->terminal);
    const auto stopped = until(*owner, [&] {
        return owner->status().terminal.has_value();
    });
    REQUIRE(stopped.terminal);
    REQUIRE(stopped.terminal->nativeOperationsAtCutoff);
    CHECK(*stopped.terminal->nativeOperationsAtCutoff == 2);
    REQUIRE(stopped.terminal->wire);
    CHECK(stopped.terminal->wire->pendingOutputBytes != 0);
    CHECK(stopped.terminal->outcome == m::TerminalOutcome::DRAINED);
    const auto before = *stopped.terminal->nativeOperationsAtCutoff;
    REQUIRE(owner->requestStop());
    REQUIRE(owner->status().terminal);
    CHECK(owner->status().terminal->nativeOperationsAtCutoff == before);
    // The two owned operations prove read/write completion retirement, not OS backpressure or peer acceptance.
    peer.stop();
}

TEST_CASE("subscription shutdown owner destruction escalates an active callback drain without a second callback") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::OwnedTcpPeer peer(peerConfig());
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto receiver = std::make_shared<Receiver>();
    auto handle = subscribe(*queues.stream, local, receiver);
    std::optional<t::TcpRecovery> owner;
    owner.emplace(create(*bootstrap, local, peer.port(), std::move(queues)));
    bool accepted = false;
    receiver->action = [&] {
        accepted = owner->requestDrain(std::chrono::steady_clock::now() + 3s).has_value();
        owner.reset();
    };
    REQUIRE(owner->start());
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    t::RecoveryStatus returned;

    while (owner && std::chrono::steady_clock::now() < deadline) {
        auto work = r::turn();
        const auto progressed = owner->poll(work);
        REQUIRE(progressed);
        returned = *progressed;
        std::this_thread::sleep_for(1ms);
    }

    CHECK(accepted);
    REQUIRE_FALSE(owner);
    CHECK(returned.stopped);
    CHECK(returned.attempts == 1);
    CHECK(receiver->values == std::vector<std::int64_t>{10});
    REQUIRE(handle.terminal());
    CHECK(handle.terminal()->outcome == m::TerminalOutcome::ABORTED);
    CHECK(handle.terminal()->counts.callbackCompletedRecords == 1);
    const std::array<std::int64_t, 1> expected{10};
    retainedValues(*receiver, expected);
    peer.stop();
}
