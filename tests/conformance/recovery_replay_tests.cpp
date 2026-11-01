// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: public recovery replay and epoch isolation over owned native loopback sockets.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace m = root::mux;
namespace t = root::transport;
namespace r = root::test::recovery;
using namespace std::chrono_literals;
constexpr std::uint8_t STREAM_BIT = 1U << 1;

/** Retain detached application batches, with no protocol event or borrowed callback span escaping. */
struct Recorder final : m::MuxConsumer {
    std::vector<m::OwnedMuxBatch> batches; ///< Owning immutable callback results, surviving source reset.

    /** Keep the actual delivered batch for later source/epoch assertions.
     * @param batch Callback-only borrowed batch.
     * @return Ordinary continue; reset ACK is controlled separately through the public handle.
     */
    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        batches.push_back(batch.retain());

        return m::BatchAction::CONTINUE;
    }
};

/** Create one compatible source owner with explicit finite desired, collector and detached-delivery limits.
 * @param local Exact schema provenance shared with recovery and producer commands.
 * @return Unbound STREAM owner; no native runtime or synthetic source event was admitted.
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

/** Reserve the callback's finite producer/delivery domain before recovery adopts the queue.
 * @param queue Actual unbound source owner.
 * @param recorder Owned application callback.
 * @return Producer-safe handle remaining valid after queue adoption.
 */
m::MuxHandle open(m::MuxQueue &queue, const std::shared_ptr<Recorder> &recorder) {
    auto work = r::queueWork();
    auto handle =
        queue.open({32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, recorder, work);
    REQUIRE(handle);

    return std::move(*handle);
}

/** Copy one explicit desired replacement without advancing the owner.
 * @param handle Actual registration.
 * @param local Immutable context retained by every input key.
 * @param symbols Finite exact synthetic keys.
 * @return Actual detached command completion, initially pending until an owner poll applies it.
 */
m::MuxCommandTicket replace(m::MuxHandle &handle, const c::LocalSchema &local,
                            std::initializer_list<std::u16string_view> symbols) {
    std::vector<c::SubscriptionRequest> requests;

    for (const auto symbol : symbols)
        requests.push_back({{local.recordContext(0), symbol}, {}});

    m::QueueSubmitWork work{10000, 1000000, 1000000, 1000000};
    auto ticket = handle.replace(requests, work);
    REQUIRE(ticket);

    return std::move(*ticket);
}

/** Advance only real public owner turns under a separate finite test deadline.
 * @tparam Predicate Nullary synchronized observation, evaluated on the creating thread.
 * @param owner Actual public recovery owner, never a fake clock or backend.
 * @param predicate Required actual observation.
 * @param timeout Maximum real monotonic wait, including funded poll turns.
 * @return Whether the observation became true before timeout or a terminal owner failure.
 */
template <class Predicate>
bool pumpUntil(t::TcpRecovery &owner, Predicate predicate, std::chrono::milliseconds timeout = 4s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        auto work = r::turn();
        auto progressed = owner.poll(work);
        REQUIRE(progressed);

        if (progressed->phase == t::RecoveryPhase::FAILED || progressed->phase == t::RecoveryPhase::EXHAUSTED) {
            INFO("phase=", static_cast<int>(progressed->phase), " stage=", progressed->first.stage,
                 " error=", static_cast<int>(progressed->first.error));

            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return predicate();
}

/** Build the independent selected client's complete DXP3 protocol declaration.
 * @return One PROTOCOL frame: common controls plus STREAM add/remove and received STREAM data.
 */
std::vector<std::byte> clientProtocol() {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    r::number(payload, 0);
    r::number(payload, 5);
    constexpr std::array<std::int32_t, 5> sendIds{0, 1, 2, 16, 17};
    constexpr std::array<std::u16string_view, 5> sendNames{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS",
                                                           u"STREAM_ADD_SUBSCRIPTION", u"STREAM_REMOVE_SUBSCRIPTION"};

    for (std::size_t index = 0; index != sendIds.size(); ++index) {
        r::number(payload, sendIds[index]);
        r::text(payload, sendNames[index]);
        r::number(payload, 0);
    }

    r::number(payload, 4);
    constexpr std::array<std::int32_t, 4> receiveIds{0, 1, 2, 15};
    constexpr std::array<std::u16string_view, 4> receiveNames{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS",
                                                              u"STREAM_DATA"};

    for (std::size_t index = 0; index != receiveIds.size(); ++index) {
        r::number(payload, receiveIds[index]);
        r::text(payload, receiveNames[index]);
        r::number(payload, 0);
    }

    return r::frame(1, payload);
}

/** Build an independent exact STREAM_ADD envelope in the selected raw-symbol profile.
 * @param symbol Exact ASCII synthetic symbol, sent with its own message-local symbol state.
 * @return Whole type16 frame using the client's fixed outgoing sender ID71.
 */
std::vector<std::byte> addFrame(std::u16string_view symbol) {
    std::vector<std::byte> payload{std::byte{0xfd}};
    r::number(payload, static_cast<std::int64_t>(symbol.size()));

    for (const auto unit : symbol) {
        std::array<std::byte, 4> buffer{};
        const auto encoded = root::encoding::writeUtfChar(unit, buffer);
        REQUIRE(encoded.status == root::encoding::Status::OK);
        payload.insert(payload.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(encoded.written));
    }

    r::number(payload, 71);

    return r::frame(16, payload);
}

/** Concatenate independently encoded complete expected outgoing envelopes.
 * @param symbols Desired entries in their explicit insertion order.
 * @return Fresh protocol, outgoing description and one independent ADD frame per desired entry.
 */
std::vector<std::byte> outgoing(std::initializer_list<std::u16string_view> symbols) {
    auto bytes = clientProtocol();
    r::append(bytes, r::description(71));

    for (const auto symbol : symbols)
        r::append(bytes, addFrame(symbol));

    return bytes;
}

/** Supply fresh peer protocol and incoming record identity without borrowing old-attempt state.
 * @param sender Current peer-selected sender ID.
 * @return Independently encoded complete initial frames.
 */
std::vector<std::byte> incoming(std::int32_t sender) {
    auto bytes = r::ready();
    r::append(bytes, r::description(sender));

    return bytes;
}

/** Inspect only owned detached records; never a protocol source event.
 * @param recorder Actual callback results.
 * @param index Callback ordinal.
 * @param sender Expected directional peer identity for this epoch.
 * @param value Expected raw third field.
 * @param symbol Exact expected synthetic key.
 */
void checkBatch(const Recorder &recorder, std::size_t index, std::int32_t sender, std::int64_t value,
                std::u16string_view symbol = u"A") {
    REQUIRE(recorder.batches.size() > index);
    const auto records = recorder.batches[index].records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].symbol == symbol);
    REQUIRE(records[0].typedValues.size() == 3);
    CHECK(records[0].typedValues[2].raw == value);
    REQUIRE(records[0].source);
    CHECK(records[0].source->senderId == sender);
}
} // namespace

TEST_CASE("recovery requires fresh protocol eligibility after an actual successful STREAM connection") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::TcpPeerConfig script;
    script.sessions.resize(2);
    script.sessions[0].initialBytes = incoming(41);
    script.sessions[0].expectedClientBytes = outgoing({u"A"});
    script.sessions[0].replyBytes = r::data(41, u"A", 101);
    script.sessions[0].waitForControllerClose = true;
    script.sessions[1].waitForControllerClose = true;
    root::test::OwnedTcpPeer peer(std::move(script));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto queue = stream(local);
    auto recorder = std::make_shared<Recorder>();
    auto handle = open(queue, recorder);
    auto desired = replace(handle, local, {u"A"});
    t::RecoveryQueues queues;
    queues.stream = std::move(queue);
    auto endpoint = r::endpoint(local, r::settings(peer.port(), 2, 100ms));
    auto options = r::options();
    auto preparation = r::preparation();
    auto owner = t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), options, preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    REQUIRE(pumpUntil(*owner, [&] {
        return recorder->batches.size() == 1 && owner->status().phase == t::RecoveryPhase::ACTIVE;
    }));
    REQUIRE(desired.result());
    CHECK(desired.result()->status == m::CommandStatus::APPLIED);
    CHECK(peer.snapshot().sessions[0].matched);
    checkBatch(*recorder, 0, 41, 101);
    peer.allowClose(0);
    REQUIRE(pumpUntil(*owner, [&] {
        return peer.snapshot().sessions[1].accepted && owner->status().phase == t::RecoveryPhase::NEGOTIATING;
    }));
    REQUIRE(owner->status().attempts == 2);

    // Multiple real native/protocol turns cannot borrow eligibility from the disposed first session.
    for (unsigned turn = 0; turn < 64; ++turn) {
        auto work = r::turn();
        auto status = owner->poll(work);
        REQUIRE(status);
        CHECK(status->phase == t::RecoveryPhase::NEGOTIATING);
        CHECK((status->demandedFamilies & STREAM_BIT) != 0);
        CHECK((status->convergedFamilies & STREAM_BIT) == 0);
        CHECK(recorder->batches.size() == 1);
        std::this_thread::sleep_for(1ms);
    }

    CHECK(peer.snapshot().error == root::test::TcpPeerError::NONE);
    r::stop(*owner);
    peer.stop();
    checkBatch(*recorder, 0, 41, 101);
}

TEST_CASE("recovery replays latest offline STREAM desired state and gates fresh sender data on exact reset ACK") {
    constexpr auto RETRY_DELAY = 2s;
    constexpr auto REMOVE_DEADLINE = 1500ms;
    static_assert(REMOVE_DEADLINE < RETRY_DELAY);
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto firstExpected = outgoing({u"A", u"B"});
    const auto secondExpected = outgoing({u"B"});
    root::test::TcpPeerConfig script;
    // Cover four 4s observation waits, the removal window, ACK-gated turns and the 3s stop drain.
    script.lifetime = 25s;
    script.sessions.resize(2);
    script.sessions[0].initialBytes = incoming(41);
    script.sessions[0].expectedClientBytes = firstExpected;
    script.sessions[0].replyBytes = r::data(41, u"A", 101);
    script.sessions[0].waitForControllerClose = true;
    script.sessions[1].initialBytes = incoming(7);
    script.sessions[1].expectedClientBytes = secondExpected;
    script.sessions[1].replyBytes = r::data(7, u"B", 202);
    script.sessions[1].waitForControllerClose = true;
    root::test::OwnedTcpPeer peer(std::move(script));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto queue = stream(local);
    auto recorder = std::make_shared<Recorder>();
    auto handle = open(queue, recorder);
    auto desired = replace(handle, local, {u"A", u"B"});
    t::RecoveryQueues queues;
    queues.stream = std::move(queue);
    auto endpoint = r::endpoint(local, r::settings(peer.port(), 2, RETRY_DELAY));
    auto options = r::options();
    auto preparation = r::preparation();
    auto owner = t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), options, preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    REQUIRE(pumpUntil(*owner, [&] {
        return recorder->batches.size() == 1 && owner->status().phase == t::RecoveryPhase::ACTIVE;
    }));
    REQUIRE(desired.result());
    CHECK(desired.result()->status == m::CommandStatus::APPLIED);
    checkBatch(*recorder, 0, 41, 101);
    const auto oldEpoch = recorder->batches[0].epoch();
    peer.allowClose(0);
    REQUIRE(pumpUntil(*owner, [&] {
        return owner->status().phase == t::RecoveryPhase::BACKOFF;
    }));
    REQUIRE(owner->status().attempts == 1);
    const c::SubscriptionKey removed{local.recordContext(0), u"A"};
    m::QueueSubmitWork submit{10000, 1000000, 1000000, 1000000};
    auto removal = handle.remove(std::span(&removed, 1), submit);
    REQUIRE(removal);
    const bool removalCompleted = pumpUntil(
        *owner,
        [&] {
            return removal->result().has_value();
        },
        REMOVE_DEADLINE);
    const auto removalStatus = owner->status();
    const auto removalResult = removal->result();
    const auto removalPeerStatus = peer.snapshot();
    INFO("offline removal: phase=", static_cast<int>(removalStatus.phase), " attempts=", removalStatus.attempts,
         " commandComplete=", removalResult.has_value(),
         " commandStatus=", removalResult ? static_cast<int>(removalResult->status) : -1,
         " recoveryError=", static_cast<int>(removalStatus.first.error), " recoveryStage=", removalStatus.first.stage,
         " peerError=", static_cast<int>(removalPeerStatus.error), " peerNativeCode=", removalPeerStatus.nativeCode,
         " peerFinished=", removalPeerStatus.finished, " secondAccepted=", removalPeerStatus.sessions[1].accepted);
    REQUIRE(removalCompleted);
    CHECK(removal->result()->status == m::CommandStatus::APPLIED);
    CHECK(removal->result()->changed);
    REQUIRE(owner->status().phase == t::RecoveryPhase::BACKOFF);
    REQUIRE(owner->status().attempts == 1);
    const auto notice = handle.resetNotice();
    REQUIRE(notice);
    CHECK(notice->sourceCommitted);
    CHECK(notice->unknownUpstreamLoss);
    CHECK(notice->fromEpoch.matches(oldEpoch));
    REQUIRE(notice->toEpoch);
    CHECK_FALSE(notice->toEpoch->matches(oldEpoch));
    CHECK(handle.acknowledgeReset(notice->sequence + 1) == m::ControlAdmission::STALE);
    REQUIRE(pumpUntil(*owner, [&] {
        const auto peerStatus = peer.snapshot();

        return peerStatus.sessions[1].matched && peerStatus.sessions[1].replyBytesWritten != 0 &&
               owner->status().phase == t::RecoveryPhase::ACTIVE;
    }));
    CHECK(owner->status().attempts == 2);
    CHECK(peer.snapshot().sessions[0].received == firstExpected);
    CHECK(peer.snapshot().sessions[1].received == secondExpected);

    for (unsigned turn = 0; turn < 64; ++turn) {
        auto work = r::turn();
        REQUIRE(owner->poll(work));
        CHECK(handle.state() == m::QueueClientState::RESET_PENDING);
        CHECK(recorder->batches.size() == 1);
        std::this_thread::sleep_for(1ms);
    }

    checkBatch(*recorder, 0, 41, 101);
    CHECK(handle.acknowledgeReset(notice->sequence) == m::ControlAdmission::RECORDED);
    REQUIRE(pumpUntil(*owner, [&] {
        return recorder->batches.size() == 2;
    }));
    CHECK_FALSE(handle.resetNotice());
    CHECK(handle.state() == m::QueueClientState::ACTIVE);
    checkBatch(*recorder, 1, 7, 202, u"B");
    CHECK_FALSE(recorder->batches[1].epoch().matches(oldEpoch));
    CHECK(recorder->batches[1].epoch().matches(*notice->toEpoch));
    CHECK(peer.snapshot().error == root::test::TcpPeerError::NONE);
    r::stop(*owner);
    peer.stop();
    checkBatch(*recorder, 0, 41, 101);
    checkBatch(*recorder, 1, 7, 202, u"B");
}
