// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: real TICKER/HISTORY recovery, detached batches and exact reset ACKs.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_history_status_support.hpp"
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

#include "qd/events/quote.hpp"

#include <algorithm>
#include <exception>
#include <iostream>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace p = root::protocol;
namespace m = root::mux;
namespace t = root::transport;
namespace q = root::events;
namespace r = root::test::recovery;
using namespace std::chrono_literals;
constexpr auto RETRY_DELAY = 2s;
constexpr auto COMMAND_DEADLINE = 1500ms;
constexpr auto OBSERVATION_DEADLINE = 6s;
static_assert(COMMAND_DEADLINE < RETRY_DELAY);

/** Retain only detached application data, never callback borrows or live protocol owners. */
struct Recorder final : m::MuxConsumer {
    std::vector<m::OwnedMuxBatch> batches; ///< Immutable results surviving source reset and runtime disposal.

    /** Retain the actual callback result.
     * @param batch Callback-only borrowed view.
     * @return Continue; reset ACK remains an explicit producer-handle operation.
     */
    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        batches.push_back(batch.retain());

        return m::BatchAction::CONTINUE;
    }
};

/** Build the complete standard eight-field Quote provenance, as in the native TCP runtime test.
 * @return Same exact 56/2/120 field identities, explicitly admitted by the binary wire profile.
 */
c::LocalSchema quoteSchema() {
    constexpr c::SchemaLimits BOUNDS{16, 128, 256, 128, 16384};
    auto defaults = q::defaultQuoteSchema(BOUNDS);
    REQUIRE(defaults);
    auto local = c::LocalSchema::create(defaults->records(), BOUNDS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);

    return std::move(*local);
}

/** Copy every real local Quote field into a complete independently validated outgoing blueprint.
 * @param local Exact standard schema.
 * @return One complete record with client sender ID71, independently reused for each fresh writer.
 */
std::vector<p::OutgoingRecord> quoteBlueprint(const c::LocalSchema &local) {
    p::OutgoingRecord record{71, 0, u"Quote", {}};
    const auto &source = local.records()[0];

    for (std::size_t field = 0; field != source.fields.size(); ++field) {
        const auto lookup = std::ranges::find_if(source.lookups, [field](const auto &entry) {
            return entry.fieldId == field;
        });
        REQUIRE(lookup != source.lookups.end());
        record.fields.push_back({lookup->name, source.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
    }

    return {std::move(record)};
}

/** Construct one actual compatible source with finite collector, desired-map and delivery budgets.
 * @param local Exact schema provenance.
 * @param family Selected immutable source family.
 * @return Pristine source owner; no test event has been admitted directly.
 */
m::MuxQueue sourceQueue(const c::LocalSchema &local, c::SubscriptionFamily family) {
    const m::MuxLimits interests{16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024};
    const c::SubscriptionLimits desired{16, 32, 128, 4096, 64, 8192, 64};
    const m::QueueLimits delivery{
        4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024};
    auto work = r::queueWork();
    auto created = [&] {
        switch (family) {
        case c::SubscriptionFamily::TICKER:
            return m::MuxQueue::createTicker(local, interests, {4, 32, 64, 4 * 1024 * 1024, 16 * 1024 * 1024, desired},
                                             {false, 100}, delivery, work);
        case c::SubscriptionFamily::STREAM:
            return m::MuxQueue::createStream(local, interests, desired, {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
                                             delivery, work);
        case c::SubscriptionFamily::HISTORY:
            return m::MuxQueue::createHistory(
                local, interests,
                {32, 256, 128, 64, 4 * 1024 * 1024, 4, 128, 64, 4 * 1024 * 1024, 32 * 1024 * 1024, desired},
                {false, 100, 10000}, delivery, work);
        }

        std::terminate();
    }();
    REQUIRE(created);

    return std::move(*created);
}

/** Reserve a producer-safe registration before recovery adopts the source owner.
 * @param queue Actual pristine queue.
 * @param recorder Application-owned callback.
 * @return Handle independent of the moved queue facade.
 */
m::MuxHandle open(m::MuxQueue &queue, const std::shared_ptr<Recorder> &recorder) {
    auto work = r::queueWork();
    auto handle =
        queue.open({32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, recorder, work);
    REQUIRE(handle);

    return std::move(*handle);
}

/** Submit exact current desired state without querying a queue moved into recovery.
 * @param handle Real producer-safe registration.
 * @param local Exact request provenance.
 * @param symbols Explicit synthetic keys in replay order.
 * @param bound Raw HISTORY lower bound; absent for TICKER.
 * @return Detached pending or completed command ticket.
 */
m::MuxCommandTicket replace(m::MuxHandle &handle, const c::LocalSchema &local,
                            std::initializer_list<std::u16string_view> symbols,
                            std::optional<std::int64_t> bound = {}) {
    std::vector<c::SubscriptionRequest> requests;

    for (const auto symbol : symbols)
        requests.push_back({{local.recordContext(0), symbol}, bound});

    m::QueueSubmitWork work{10000, 1000000, 1000000, 1000000};
    auto ticket = handle.replace(requests, work);
    REQUIRE(ticket);

    return std::move(*ticket);
}

/** Pump real public owner turns within an independent monotonic test deadline.
 * @tparam Predicate Test-side synchronized observation.
 * @param owner Actual native recovery, with no fake clock or private state.
 * @param peer Owned finite peer used only for failure diagnostics.
 * @param predicate Required observation.
 * @param timeout Generous finite observation limit, not a runtime latency promise.
 */
template <class Predicate>
void until(t::TcpRecovery &owner, const root::test::OwnedTcpPeer &peer, Predicate predicate,
           std::chrono::milliseconds timeout = OBSERVATION_DEADLINE) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        auto work = r::turn();
        const auto progressed = owner.poll(work);
        REQUIRE(progressed);

        if (progressed->phase == t::RecoveryPhase::FAILED || progressed->phase == t::RecoveryPhase::EXHAUSTED)
            break;

        std::this_thread::sleep_for(1ms);
    }

    const auto status = owner.status();
    const auto snapshot = peer.snapshot();
    INFO("phase=", static_cast<int>(status.phase), " attempts=", status.attempts,
         " recoveryError=", static_cast<int>(status.first.error), " stage=", status.first.stage,
         " transportError=", static_cast<int>(status.first.transport.error),
         " protocolError=", static_cast<int>(status.first.transport.protocolError),
         " peerError=", static_cast<int>(snapshot.error), " nativeCode=", snapshot.nativeCode,
         " first captured bytes=", snapshot.sessions.empty() ? 0 : snapshot.sessions[0].received.size(),
         " first matched=", !snapshot.sessions.empty() && snapshot.sessions[0].matched,
         " second accepted=", snapshot.sessions.size() > 1 && snapshot.sessions[1].accepted,
         " second matched=", snapshot.sessions.size() > 1 && snapshot.sessions[1].matched,
         " second reply bytes=", snapshot.sessions.size() > 1 ? snapshot.sessions[1].replyBytesWritten : 0);
    REQUIRE(predicate());
}

/** One actual QTP family in its numeric wire order. */
struct WireFamily {
    std::int32_t data;              ///< DATA wire identifier.
    std::u16string_view dataName;   ///< Exact advertised DATA name.
    std::u16string_view addName;    ///< Exact advertised ADD name; identifier is DATA + 1.
    std::u16string_view removeName; ///< Exact advertised REMOVE name; identifier is DATA + 2.
};

constexpr std::array<WireFamily, 3> WIRE_FAMILIES{{
    {10, u"TICKER_DATA", u"TICKER_ADD_SUBSCRIPTION", u"TICKER_REMOVE_SUBSCRIPTION"},
    {15, u"STREAM_DATA", u"STREAM_ADD_SUBSCRIPTION", u"STREAM_REMOVE_SUBSCRIPTION"},
    {20, u"HISTORY_DATA", u"HISTORY_ADD_SUBSCRIPTION", u"HISTORY_REMOVE_SUBSCRIPTION"},
}};

/** Encode the expected client declaration independently of recovery output.
 * @param families Exact ordered families negotiated by this owner.
 * @return DXP3 common controls and the selected families' add/remove/data identities.
 */
std::vector<std::byte> clientProtocol(std::span<const WireFamily> families) {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    constexpr std::array<std::u16string_view, 3> COMMON{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS"};
    const auto append = [&](std::int32_t id, std::u16string_view name) {
        r::number(payload, id);
        r::text(payload, name);
        r::number(payload, 0);
    };

    r::number(payload, 0);
    r::number(payload, COMMON.size() + 2 * families.size());

    for (std::size_t index = 0; index != COMMON.size(); ++index)
        append(static_cast<std::int32_t>(index), COMMON[index]);

    for (const auto &family : families) {
        append(family.data + 1, family.addName);
        append(family.data + 2, family.removeName);
    }

    r::number(payload, COMMON.size() + families.size());

    for (std::size_t index = 0; index != COMMON.size(); ++index)
        append(static_cast<std::int32_t>(index), COMMON[index]);

    for (const auto &family : families)
        append(family.data, family.dataName);

    return r::frame(1, payload);
}

/** Encode the supplied families as an ordinary peer descriptor or capability delta.
 * @param families Exact subset to add to the peer's previously advertised capabilities.
 * @return DXP3 peer SEND data and RECEIVE subscription identities.
 */
std::vector<std::byte> peerProtocol(std::span<const WireFamily> families) {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    const auto append = [&](std::int32_t id, std::u16string_view name) {
        r::number(payload, id);
        r::text(payload, name);
        r::number(payload, 0);
    };

    r::number(payload, 0);
    r::number(payload, families.size());

    for (const auto &family : families)
        append(family.data, family.dataName);

    r::number(payload, 2 * families.size());

    for (const auto &family : families) {
        append(family.data + 1, family.addName);
        append(family.data + 2, family.removeName);
    }

    return r::frame(1, payload);
}

/** Append a complete message-local raw symbol using the public primitive codec.
 * @param payload Owning envelope payload.
 * @param symbol Exact synthetic ASCII text.
 */
void symbol(std::vector<std::byte> &payload, std::u16string_view symbol) {
    payload.push_back(std::byte{0xfd});
    r::number(payload, static_cast<std::int64_t>(symbol.size()));

    for (const auto unit : symbol) {
        std::array<std::byte, 4> buffer{};
        const auto encoded = root::encoding::writeUtfChar(unit, buffer);
        REQUIRE(encoded.status == root::encoding::Status::OK);
        payload.insert(payload.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(encoded.written));
    }
}

/** Encode every real standard Quote field declaration with a directional sender identity.
 * @param sender Current peer or outgoing client identity.
 * @return Complete type2 record declaration, with no simplified field types.
 */
std::vector<std::byte> quoteDescription(std::int32_t sender) {
    std::vector<std::byte> payload;
    r::number(payload, sender);
    r::text(payload, u"Quote");
    r::number(payload, static_cast<std::int64_t>(q::QUOTE_FIELD_NAMES.size()));

    for (std::size_t index = 0; index != q::QUOTE_FIELD_NAMES.size(); ++index) {
        const auto name = q::QUOTE_FIELD_NAMES[index];
        r::text(payload, std::u16string(name.begin(), name.end()));
        r::number(payload, std::to_underlying(q::QUOTE_FIELD_TYPES[index]));
    }

    return r::frame(2, payload);
}

/** Encode a whole ordinary TICKER Quote with all eight standard fields.
 * The time-1.qtp/tsv standard Quote capture establishes seconds=1, zero exchange and raw-zero
 * wide-decimal NaN. This synthetic full-field fixture preserves those values and changes only
 * symbol, sender and the explicit seconds on the second connection; it is not a Java TICKER capture.
 * @param sender Current peer sender identity.
 * @param key Exact subscribed key.
 * @param seconds Exact Bid.Time and Ask.Time raw TIME_SECONDS.
 * @return Whole type10 envelope using UTF_CHAR for exchanges and compact scalars for 56/120.
 */
std::vector<std::byte> quoteData(std::int32_t sender, std::u16string_view key, std::int32_t seconds) {
    std::vector<std::byte> payload;
    symbol(payload, key);
    r::number(payload, sender);

    for (unsigned side = 0; side != 2; ++side) {
        r::number(payload, seconds);
        std::array<std::byte, 4> buffer{};
        const auto exchange = root::encoding::writeUtfChar(0, buffer);
        REQUIRE(exchange.status == root::encoding::Status::OK);
        payload.insert(payload.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(exchange.written));
        r::number(payload, 0);
        r::number(payload, 0);
    }

    return r::frame(10, payload);
}

/** Construct the independently expected complete first output for one new connection.
 * @param history Selected family.
 * @param symbols Exact desired keys in replay order.
 * @param bound Exact signed raw HISTORY bound; never business milliseconds.
 * @return Fresh protocol plus complete description plus independently framed add requests.
 */
std::vector<std::byte> outgoing(bool history, std::initializer_list<std::u16string_view> symbols,
                                std::optional<std::int64_t> bound = {}) {
    auto bytes = clientProtocol(std::span(WIRE_FAMILIES).subspan(history ? 2 : 0, 1));
    r::append(bytes, history ? r::description(71) : quoteDescription(71));

    for (const auto key : symbols) {
        std::vector<std::byte> payload;
        symbol(payload, key);
        r::number(payload, 71);

        if (history) {
            REQUIRE(bound);
            r::number(payload, *bound);
        }

        r::append(bytes, r::frame(history ? 21 : 11, payload));
    }

    return bytes;
}

/** Build one exact add frame independently of production replay.
 * @param family QTP DATA identifier, followed by the corresponding ADD identity.
 * @param key Requested symbol.
 * @param bound Optional raw HISTORY lower bound.
 * @return Complete raw-symbol ADD frame.
 */
std::vector<std::byte> addFrame(const WireFamily &family, std::u16string_view key,
                                std::optional<std::int64_t> bound = {}) {
    std::vector<std::byte> payload;
    symbol(payload, key);
    r::number(payload, 71);

    if (bound)
        r::number(payload, *bound);

    return r::frame(family.data + 1, payload);
}

/** Verify complete peer bytes while allowing the scheduler to choose the first lane.
 * @param received Actual prefix captured on one native connection.
 * @param prefix Exact shared descriptor and schema bytes.
 * @param adds Whole expected ADD frames, one per enabled contract.
 */
void checkMixedOutput(std::span<const std::byte> received, std::span<const std::byte> prefix,
                      const std::array<std::vector<std::byte>, 3> &adds) {
    REQUIRE(received.size() > prefix.size());
    REQUIRE(std::ranges::equal(received.first(prefix.size()), prefix));
    received = received.subspan(prefix.size());
    p::FrameReader reader({256, 265});
    std::array<bool, 3> seen{};

    while (!received.empty()) {
        const auto next = reader.feed(received);
        REQUIRE(next.status == p::FrameStatus::FRAME_READY);
        REQUIRE(next.accepted != 0);
        const auto whole = received.first(next.accepted);
        const auto expected = std::ranges::find_if(adds, [whole](const auto &add) {
            return std::ranges::equal(whole, add);
        });
        REQUIRE(expected != adds.end());
        const auto lane = static_cast<std::size_t>(expected - adds.begin());
        CHECK_FALSE(seen[lane]);
        seen[lane] = true;
        received = received.subspan(next.accepted);
    }

    CHECK(std::ranges::all_of(seen, [](bool found) {
        return found;
    }));
}

/** Read only detached actual callback records after source reset or retirement.
 * @param recorder Retained application results.
 * @param index Callback ordinal.
 * @param key Exact expected symbol.
 * @param sender Directional identity of this connection.
 * @return Borrow of immutable batch-owned record, valid while recorder retains the batch.
 */
const m::MuxRecordView &record(const Recorder &recorder, std::size_t index, std::u16string_view key,
                               std::int32_t sender) {
    REQUIRE(recorder.batches.size() > index);
    const auto records = recorder.batches[index].records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].symbol == key);
    REQUIRE(records[0].source);
    CHECK(records[0].source->senderId == sender);

    return records[0];
}

/** Check all exact standard Quote types and scalar values in a real retained queue projection.
 * @param recorder Actual callback output.
 * @param index Callback ordinal.
 * @param key Expected synthetic key.
 * @param sender Fresh directional source ID.
 * @param seconds Raw standard time value.
 */
void checkQuote(const Recorder &recorder, std::size_t index, std::u16string_view key, std::int32_t sender,
                std::int32_t seconds) {
    const auto &value = record(recorder, index, key, sender);
    CHECK(value.family == c::SubscriptionFamily::TICKER);
    REQUIRE(value.typedValues.size() == q::QUOTE_FIELD_TYPES.size());

    for (std::size_t field = 0; field != value.typedValues.size(); ++field) {
        CHECK(value.typedValues[field].type == q::QUOTE_FIELD_TYPES[field]);
        CHECK(value.typedValues[field].raw == (field == 0 || field == 4 ? seconds : 0));
    }
}

/** Exercise real funded turns while a reset notice blocks the application's fresh callbacks.
 * @param owner Actual recovery owner.
 * @param handle Live producer handle.
 * @param recorder Actual callback results.
 */
void checkBlocked(t::TcpRecovery &owner, const m::MuxHandle &handle, const Recorder &recorder) {
    for (unsigned turn = 0; turn != 64; ++turn) {
        auto work = r::turn();
        REQUIRE(owner.poll(work));
        CHECK(handle.state() == m::QueueClientState::RESET_PENDING);
        CHECK(recorder.batches.size() == 1);
        std::this_thread::sleep_for(1ms);
    }
}
} // namespace

TEST_CASE("TICKER recovery resets actual Quote cache and gates fresh sender data on exact client ACK") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto expected = outgoing(false, {u"IBM", u"MSFT"});
    root::test::TcpPeerConfig script;
    script.lifetime = 35s;
    script.sessions.resize(2);

    for (std::size_t connection = 0; connection != 2; ++connection) {
        const auto sender = connection == 0 ? 41 : 7;
        auto &session = script.sessions[connection];
        session.initialBytes = r::ready();
        r::append(session.initialBytes, quoteDescription(sender));
        session.expectedClientBytes = expected;
        session.replyBytes = quoteData(sender, connection == 0 ? u"IBM" : u"MSFT", connection == 0 ? 1 : 2);
        session.waitForControllerClose = true;
    }

    root::test::OwnedTcpPeer peer(std::move(script));
    REQUIRE(peer.port() != 0);
    auto local = quoteSchema();
    auto queue = sourceQueue(local, c::SubscriptionFamily::TICKER);
    auto recorder = std::make_shared<Recorder>();
    auto handle = open(queue, recorder);
    auto desired = replace(handle, local, {u"IBM", u"MSFT"});
    t::RecoveryQueues queues;
    queues.ticker = std::move(queue);
    auto endpoint = r::endpoint(local, r::settings(peer.port(), 2, RETRY_DELAY));
    auto preparation = r::preparation();
    auto owner = t::TcpRecovery::create(*bootstrap, endpoint, quoteBlueprint(local), std::move(queues), r::options(),
                                        preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    until(*owner, peer, [&] {
        return recorder->batches.size() == 1 && owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    REQUIRE(desired.result());
    CHECK(desired.result()->status == m::CommandStatus::APPLIED);
    checkQuote(*recorder, 0, u"IBM", 41, 1);
    const auto oldEpoch = recorder->batches[0].epoch();
    peer.allowClose(0);
    until(*owner, peer, [&] {
        return owner->status().phase == t::RecoveryPhase::BACKOFF;
    });
    REQUIRE(owner->status().attempts == 1);
    const auto notice = handle.resetNotice();
    REQUIRE(notice);
    CHECK(notice->sourceCommitted);
    CHECK_FALSE(notice->globalAckPending);
    CHECK(notice->unknownUpstreamLoss);
    CHECK(notice->fromEpoch.matches(oldEpoch));
    REQUIRE(notice->toEpoch);
    CHECK_FALSE(notice->toEpoch->matches(oldEpoch));
    CHECK(handle.acknowledgeReset(notice->sequence + 1) == m::ControlAdmission::STALE);
    until(*owner, peer, [&] {
        const auto status = peer.snapshot();

        return status.sessions[1].matched && status.sessions[1].replyBytesWritten != 0 &&
               owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    CHECK(owner->status().attempts == 2);
    checkBlocked(*owner, handle, *recorder);
    checkQuote(*recorder, 0, u"IBM", 41, 1);
    CHECK(handle.acknowledgeReset(notice->sequence) == m::ControlAdmission::RECORDED);
    until(*owner, peer, [&] {
        return recorder->batches.size() == 2;
    });
    CHECK_FALSE(handle.resetNotice());
    CHECK(handle.state() == m::QueueClientState::ACTIVE);
    checkQuote(*recorder, 1, u"MSFT", 7, 2);
    CHECK(recorder->batches[1].epoch().matches(*notice->toEpoch));

    // IBM has no new-connection update: a retained old cache entry must never become a fresh callback.
    for (unsigned turn = 0; turn != 64; ++turn) {
        auto work = r::turn();
        REQUIRE(owner->poll(work));
        CHECK(recorder->batches.size() == 2);
        std::this_thread::sleep_for(1ms);
    }

    CHECK(peer.snapshot().sessions[0].received == expected);
    CHECK(peer.snapshot().sessions[1].received == expected);
    CHECK(peer.snapshot().error == root::test::TcpPeerError::NONE);
    r::stop(*owner);
    peer.stop();
    checkQuote(*recorder, 0, u"IBM", 41, 1);
    checkQuote(*recorder, 1, u"MSFT", 7, 2);
}

TEST_CASE("HISTORY recovery exposes interruption and replays current signed raw bound as a fresh request") {
    constexpr std::int64_t FIRST_BOUND = -0x200000007LL;
    constexpr std::int64_t CURRENT_BOUND = -0x100000007LL;
    constexpr std::int64_t POINT_TIME = -3;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto firstExpected = outgoing(true, {u"A"}, FIRST_BOUND);
    const auto secondExpected = outgoing(true, {u"A"}, CURRENT_BOUND);
    root::test::TcpPeerConfig script;
    script.lifetime = 35s;
    script.sessions.resize(2);

    for (std::size_t connection = 0; connection != 2; ++connection) {
        const auto sender = connection == 0 ? 41 : 7;
        auto &session = script.sessions[connection];
        session.initialBytes = r::ready();
        r::append(session.initialBytes, r::description(sender));
        session.expectedClientBytes = connection == 0 ? firstExpected : secondExpected;
        session.replyBytes = r::data(sender, u"A", connection == 0 ? 101 : 202, 20, POINT_TIME);
        session.waitForControllerClose = true;
    }

    root::test::OwnedTcpPeer peer(std::move(script));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto queue = sourceQueue(local, c::SubscriptionFamily::HISTORY);
    auto recorder = std::make_shared<Recorder>();
    auto handle = open(queue, recorder);
    auto desired = replace(handle, local, {u"A"}, FIRST_BOUND);
    t::RecoveryQueues queues;
    queues.history = std::move(queue);
    auto endpoint = r::endpoint(local, r::settings(peer.port(), 2, RETRY_DELAY));
    auto preparation = r::preparation();
    auto owner =
        t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), r::options(), preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());
    until(*owner, peer, [&] {
        return recorder->batches.size() == 1 && owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    REQUIRE(desired.result());
    CHECK(desired.result()->status == m::CommandStatus::APPLIED);
    const auto &first = record(*recorder, 0, u"A", 41);
    CHECK(first.family == c::SubscriptionFamily::HISTORY);
    CHECK(first.rawTime == POINT_TIME);
    REQUIRE(first.typedValues.size() == 3);
    CHECK(first.typedValues[2].raw == 101);
    CHECK(first.eventFlags == 0);
    const auto oldEpoch = recorder->batches[0].epoch();
    peer.allowClose(0);
    until(*owner, peer, [&] {
        return owner->status().phase == t::RecoveryPhase::BACKOFF;
    });
    REQUIRE(owner->status().attempts == 1);
    auto changed = replace(handle, local, {u"A"}, CURRENT_BOUND);
    until(
        *owner, peer,
        [&] {
            return changed.result().has_value();
        },
        COMMAND_DEADLINE);
    REQUIRE(changed.result());
    REQUIRE(changed.result()->status == m::CommandStatus::APPLIED);
    REQUIRE(changed.result()->changed);
    REQUIRE(owner->status().phase == t::RecoveryPhase::BACKOFF);
    REQUIRE(owner->status().attempts == 1);
    const auto notice = handle.resetNotice();
    REQUIRE(notice);
    CHECK(notice->sourceCommitted);
    CHECK_FALSE(notice->globalAckPending);
    CHECK(notice->unknownUpstreamLoss);
    CHECK(notice->fromEpoch.matches(oldEpoch));
    REQUIRE(notice->toEpoch);
    CHECK_FALSE(notice->toEpoch->matches(oldEpoch));
    CHECK(handle.acknowledgeReset(notice->sequence + 1) == m::ControlAdmission::STALE);
    until(*owner, peer, [&] {
        const auto status = peer.snapshot();

        return status.sessions[1].matched && status.sessions[1].replyBytesWritten != 0 &&
               owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    CHECK(owner->status().attempts == 2);
    checkBlocked(*owner, handle, *recorder);
    CHECK(handle.acknowledgeReset(notice->sequence) == m::ControlAdmission::RECORDED);
    until(*owner, peer, [&] {
        return recorder->batches.size() == 2;
    });
    CHECK_FALSE(handle.resetNotice());
    CHECK(handle.state() == m::QueueClientState::ACTIVE);
    const auto &fresh = record(*recorder, 1, u"A", 7);
    CHECK(fresh.rawTime == POINT_TIME);
    REQUIRE(fresh.typedValues.size() == 3);
    CHECK(fresh.typedValues[2].raw == 202);
    CHECK(fresh.eventFlags == 0);
    CHECK(recorder->batches[1].epoch().matches(*notice->toEpoch));
    CHECK_FALSE(recorder->batches[1].epoch().matches(oldEpoch));
    CHECK(peer.snapshot().sessions[0].received == firstExpected);
    CHECK(peer.snapshot().sessions[1].received == secondExpected);
    CHECK(peer.snapshot().error == root::test::TcpPeerError::NONE);

    // The raw point overlaps the old request. Gap/epoch evidence does not promise resume or exactly-once delivery.
    // The adopted queue is observable, but unflagged points still do not establish snapshot completion.
    auto rangeWork = r::queueWork();
    const auto incomplete = owner->historyRangeStatus(handle, {local.recordContext(0), u"A"}, rangeWork);
    REQUIRE(incomplete);
    CHECK_FALSE(incomplete->completeDeliveredRange);
    r::stop(*owner);
    peer.stop();
    const auto &retained = record(*recorder, 0, u"A", 41);
    CHECK(retained.rawTime == POINT_TIME);
    CHECK(retained.typedValues[2].raw == 101);
    CHECK(recorder->batches[0].epoch().matches(oldEpoch));
    CHECK(record(*recorder, 1, u"A", 7).typedValues[2].raw == 202);
}

TEST_CASE("TCP recovery replays three delivery families over one socket per attempt") {
    constexpr std::int64_t FIRST_BOUND = -0x200000007LL;
    constexpr std::int64_t CURRENT_BOUND = -0x100000007LL;
    constexpr std::int64_t POINT_TIME = -3;
    constexpr std::array families{c::SubscriptionFamily::TICKER, c::SubscriptionFamily::STREAM,
                                  c::SubscriptionFamily::HISTORY};
    constexpr std::array<std::u16string_view, 3> keys{u"PRICE", u"SALE", u"BOOK"};
    constexpr std::array<std::int32_t, 2> senders{41, 7};
    constexpr std::uint8_t ALL_FAMILIES = 0b111;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    auto prefix = clientProtocol(WIRE_FAMILIES);
    r::append(prefix, r::description(71));
    std::array<std::array<std::vector<std::byte>, 3>, 2> adds;
    root::test::TcpPeerConfig script;
    script.lifetime = 35s;
    script.sessions.resize(senders.size());

    for (std::size_t attempt = 0; attempt != senders.size(); ++attempt) {
        auto &session = script.sessions[attempt];
        session.initialBytes = r::ready();
        r::append(session.initialBytes, r::description(senders[attempt]));
        session.clientBytesToRead = prefix.size();
        session.waitForControllerClose = true;

        for (std::size_t lane = 0; lane != families.size(); ++lane) {
            const auto bound = lane == 2 ? std::optional{attempt == 0 ? FIRST_BOUND : CURRENT_BOUND} : std::nullopt;
            adds[attempt][lane] = addFrame(WIRE_FAMILIES[lane], keys[lane], bound);
            session.clientBytesToRead += adds[attempt][lane].size();
            r::append(session.replyBytes,
                      r::data(senders[attempt], keys[lane], static_cast<std::int32_t>((attempt + 1) * 100 + lane),
                              WIRE_FAMILIES[lane].data, POINT_TIME));
        }
    }

    root::test::OwnedTcpPeer peer(std::move(script));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    t::RecoveryQueues queues;
    const std::array lanes{&queues.ticker, &queues.stream, &queues.history};
    std::array<std::shared_ptr<Recorder>, 3> recorders;
    std::array<m::MuxHandle, 3> handles;
    std::array<m::MuxCommandTicket, 3> desired;

    for (std::size_t lane = 0; lane != families.size(); ++lane) {
        auto &queue = lanes[lane]->emplace(sourceQueue(local, families[lane]));
        recorders[lane] = std::make_shared<Recorder>();
        handles[lane] = open(queue, recorders[lane]);
        desired[lane] =
            replace(handles[lane], local, {keys[lane]}, lane == 2 ? std::optional{FIRST_BOUND} : std::nullopt);
    }

    constexpr std::size_t WRITE_CHUNK_BYTES = 64;
    REQUIRE(prefix.size() > WRITE_CHUNK_BYTES);
    auto input = r::settings(peer.port(), 2, RETRY_DELAY);
    input.budgets.tcp.writeBytes = WRITE_CHUNK_BYTES;
    auto endpoint = r::endpoint(local, input);
    auto preparation = r::preparation();
    auto options = r::options();
    options.requireAllContracts = true;
    auto owner = t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), options, preparation);
    REQUIRE(owner);
    REQUIRE(owner->start());

    const auto haveBatches = [&](std::size_t count) {
        return std::ranges::all_of(recorders, [count](const auto &recorder) {
            return recorder->batches.size() == count;
        });
    };
    const auto checkAttempt = [&](std::size_t attempt) {
        const auto status = owner->status();
        CHECK(status.attempts == attempt + 1);
        CHECK(status.demandedFamilies == ALL_FAMILIES);
        CHECK(status.convergedFamilies == ALL_FAMILIES);
        const auto statistics = owner->statistics();
        CHECK(statistics.activeConnections == 1);
        CHECK(statistics.sourceEpoch == attempt + 1);
        CHECK(statistics.establishedConnections.value == attempt + 1);
        CHECK(statistics.writtenSubscriptionRecords.value == (attempt + 1) * families.size());
        const auto received = peer.snapshot();
        CHECK(received.error == root::test::TcpPeerError::NONE);
        CHECK(received.sessions[attempt].accepted);
        CHECK(received.sessions[attempt].matched);
        checkMixedOutput(received.sessions[attempt].received, prefix, adds[attempt]);

        for (std::size_t lane = 0; lane != families.size(); ++lane) {
            const auto &entry = record(*recorders[lane], attempt, keys[lane], senders[attempt]);
            CHECK(entry.family == families[lane]);
            REQUIRE(entry.typedValues.size() == 3);
            CHECK(entry.typedValues[2].raw == static_cast<std::int32_t>((attempt + 1) * 100 + lane));

            if (lane == 2)
                CHECK(entry.rawTime == POINT_TIME);
        }
    };

    until(*owner, peer, [&] {
        return haveBatches(1) && owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    checkAttempt(0);
    CHECK_FALSE(peer.snapshot().sessions[1].accepted);

    for (const auto &ticket : desired) {
        REQUIRE(ticket.result());
        CHECK(ticket.result()->status == m::CommandStatus::APPLIED);
    }

    peer.allowClose(0);
    until(*owner, peer, [&] {
        return owner->status().phase == t::RecoveryPhase::BACKOFF;
    });
    auto changed = replace(handles[2], local, {keys[2]}, CURRENT_BOUND);
    until(
        *owner, peer,
        [&] {
            return changed.result().has_value();
        },
        COMMAND_DEADLINE);
    REQUIRE(changed.result()->status == m::CommandStatus::APPLIED);
    CHECK(changed.result()->changed);

    std::array<m::QueueIdentity, 3> nextEpochs;

    for (std::size_t lane = 0; lane != families.size(); ++lane) {
        const auto notice = handles[lane].resetNotice();
        REQUIRE(notice);
        CHECK(notice->sourceCommitted);
        CHECK(notice->fromEpoch.matches(recorders[lane]->batches[0].epoch()));
        REQUIRE(notice->toEpoch);
        CHECK_FALSE(notice->toEpoch->matches(notice->fromEpoch));
        nextEpochs[lane] = *notice->toEpoch;
        CHECK(handles[lane].acknowledgeReset(notice->sequence) == m::ControlAdmission::RECORDED);
    }

    until(*owner, peer, [&] {
        return haveBatches(2) && owner->status().phase == t::RecoveryPhase::ACTIVE;
    });
    checkAttempt(1);

    for (std::size_t lane = 0; lane != families.size(); ++lane) {
        CHECK(recorders[lane]->batches[1].epoch().matches(nextEpochs[lane]));
        CHECK_FALSE(handles[lane].resetNotice());
    }

    r::stop(*owner);
    const auto closed = owner->status();
    REQUIRE(closed.terminal);
    CHECK(std::ranges::all_of(closed.terminal->queues, [](const auto &queue) {
        return queue.has_value();
    }));
    CHECK(owner->statistics().activeConnections == 0);
    CHECK(owner->statistics().closedConnections.value == senders.size());
    peer.stop();
    CHECK(record(*recorders[0], 0, keys[0], senders[0]).typedValues[2].raw == 100);
}

TEST_CASE("mixed recovery gates even TICKER demand until every peer contract is known") {
    constexpr std::u16string_view KEY = u"PRICE";
    constexpr std::int32_t SENDER = 41;
    enum class Completion { CAPABILITY_DELTA, TIMEOUT, PAUSE, EARLY_DATA };

    for (const auto completion :
         {Completion::CAPABILITY_DELTA, Completion::TIMEOUT, Completion::PAUSE, Completion::EARLY_DATA}) {
        CAPTURE(static_cast<int>(completion));
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        auto prefix = clientProtocol(WIRE_FAMILIES);
        r::append(prefix, r::description(71));
        root::test::TcpPeerConfig script;
        script.sessions.resize(1);
        auto &session = script.sessions.front();
        session.initialBytes = peerProtocol(std::span(WIRE_FAMILIES).first(1));
        session.expectedClientBytes = prefix;
        session.waitForClientClose = true;

        if (completion == Completion::CAPABILITY_DELTA)
            session.stagedBytes.push_back(peerProtocol(std::span(WIRE_FAMILIES).subspan(1)));

        if (completion == Completion::EARLY_DATA) {
            auto early = r::description(SENDER);
            r::append(early, r::data(SENDER, KEY, 101, WIRE_FAMILIES[0].data));
            session.stagedBytes.push_back(std::move(early));
        }

        root::test::OwnedTcpPeer peer(std::move(script));
        REQUIRE(peer.port() != 0);
        auto local = r::schema();
        t::RecoveryQueues queues;
        queues.ticker.emplace(sourceQueue(local, c::SubscriptionFamily::TICKER));
        queues.stream.emplace(sourceQueue(local, c::SubscriptionFamily::STREAM));
        queues.history.emplace(sourceQueue(local, c::SubscriptionFamily::HISTORY));
        auto recorder = std::make_shared<Recorder>();
        auto handle = open(*queues.ticker, recorder);
        auto desired = replace(handle, local, {KEY});
        auto endpoint = r::endpoint(local, r::settings(peer.port(), 1, 0ms));
        auto preparation = r::preparation();
        auto options = r::options();
        options.requireAllContracts = true;
        auto owner =
            t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), options, preparation);
        REQUIRE(owner);
        REQUIRE(owner->start());
        until(*owner, peer, [&] {
            return peer.snapshot().sessions[0].matched && desired.result().has_value() &&
                   owner->statistics().readMessages.value != 0;
        });
        REQUIRE(desired.result()->status == m::CommandStatus::APPLIED);
        const auto checkUntil = std::chrono::steady_clock::now() + 100ms;

        while (std::chrono::steady_clock::now() < checkUntil) {
            auto work = r::turn();
            REQUIRE(owner->poll(work));
            CHECK(owner->status().phase != t::RecoveryPhase::FAILED);
            CHECK(owner->statistics().writtenSubscriptionRecords.value == 0);
            std::this_thread::sleep_for(1ms);
        }

        if (completion == Completion::CAPABILITY_DELTA) {
            peer.allowStages(0, 1);
            until(*owner, peer, [&] {
                return owner->status().phase == t::RecoveryPhase::ACTIVE &&
                       owner->statistics().writtenSubscriptionRecords.value == 1;
            });
            r::stop(*owner);
            r::append(prefix, addFrame(WIRE_FAMILIES[0], KEY));
        } else if (completion == Completion::PAUSE) {
            REQUIRE(owner->requestDisconnect());
            until(*owner, peer, [&] {
                return owner->status().phase == t::RecoveryPhase::SUSPENDED;
            });
            CHECK(owner->status().first.error == t::RecoveryError::NONE);
            CHECK_FALSE(owner->nextWake());
            CHECK(owner->statistics().activeConnections == 0);
            r::stop(*owner);
        } else {
            const bool earlyData = completion == Completion::EARLY_DATA;

            if (earlyData)
                peer.allowStages(0, 1);

            until(*owner, peer, [&] {
                return owner->status().phase == t::RecoveryPhase::FAILED;
            });
            const auto status = owner->status();
            CHECK(status.first.error == t::RecoveryError::UNSUPPORTED_PROFILE);
            CHECK(status.first.stage == (earlyData ? "recovery-data-before-contracts" : "recovery-peer-contracts"));
            CHECK(recorder->batches.empty());
            CHECK(owner->statistics().writtenSubscriptionRecords.value == 0);
            CHECK(status.attempts == 1);
            REQUIRE(status.terminal);
            CHECK(std::ranges::all_of(status.terminal->queues, [](const auto &queue) {
                return queue.has_value();
            }));
        }

        const auto peerDeadline = std::chrono::steady_clock::now() + OBSERVATION_DEADLINE;

        while (!peer.snapshot().sessions[0].completed && std::chrono::steady_clock::now() < peerDeadline)
            std::this_thread::sleep_for(1ms);

        const auto captured = peer.snapshot();
        REQUIRE(captured.sessions[0].completed);
        CHECK(captured.error == root::test::TcpPeerError::NONE);
        CHECK(captured.sessions[0].received == prefix);
    }
}

TEST_CASE("TCP recovery public HISTORY observation preserves covered range reentry guards and detached lifetime") {
    namespace hs = root::test::history_status;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    root::test::TcpPeerConfig script;
    script.lifetime = 15s;
    script.sessions.resize(1);
    auto &session = script.sessions[0];
    session.initialBytes = r::ready();
    r::append(session.initialBytes, r::description(41));
    session.expectedClientBytes = hs::clientOutput();
    session.replyBytes = hs::data({{3, 30, hs::SNAPSHOT_BEGIN}, {2, 20, 0}});
    session.waitForControllerClose = true;
    root::test::OwnedTcpPeer peer(std::move(script));
    REQUIRE(peer.port() != 0);
    auto local = r::schema();
    auto queue = hs::queue(local);
    auto recorder = std::make_shared<hs::Recorder>();
    auto handle = hs::open(queue, recorder);
    auto desired = hs::replace(handle, local, 2);
    t::RecoveryQueues queues;
    queues.history = std::move(queue);
    auto input = r::settings(peer.port(), 1, 0ms);
    hs::enableHistory(input);
    auto endpoint = r::endpoint(local, input);
    auto preparation = r::preparation();
    std::optional<t::TcpRecovery> owner;
    auto created =
        t::TcpRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), r::options(), preparation);
    REQUIRE(created);
    owner.emplace(std::move(*created));
    const c::SubscriptionKey key{local.recordContext(0), u"A"};
    auto configuredWork = r::queueWork();
    const auto configuredBefore = hs::coordinates(configuredWork);
    const auto configured = owner->historyRangeStatus(handle, key, configuredWork);
    REQUIRE_FALSE(configured);
    CHECK(configured.error().error == t::RecoveryError::INVALID_STATE);
    CHECK(hs::coordinates(configuredWork) == configuredBefore);
    bool reentryObserved = false;
    recorder->action = [&] {
        auto work = r::queueWork();
        const auto before = hs::coordinates(work);
        const auto rejected = owner->historyRangeStatus(handle, key, work);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == t::RecoveryError::REENTRANT);
        CHECK(hs::coordinates(work) == before);
        reentryObserved = true;
    };
    REQUIRE(owner->start());
    std::optional<m::QueueRangeStatus> observed;
    std::optional<m::QueueRangeStatus> lastEligible;
    std::optional<std::expected<m::QueueRangeStatus, t::RecoveryDiagnostic>> lastQuery;
    const auto deadline = std::chrono::steady_clock::now() + OBSERVATION_DEADLINE;

    while (!observed && std::chrono::steady_clock::now() < deadline) {
        auto turn = r::turn();
        const auto progressed = owner->poll(turn);
        REQUIRE(progressed);
        auto work = r::queueWork();
        const auto status = owner->historyRangeStatus(handle, key, work);
        lastQuery = status;

        if (status) {
            lastEligible = *status;

            if (status->completeDeliveredRange) {
                observed = *status;
            }
        }

        if (progressed->phase == t::RecoveryPhase::FAILED || progressed->phase == t::RecoveryPhase::EXHAUSTED) {
            break;
        }

        std::this_thread::sleep_for(1ms);
    }

    if (!observed) {
        const auto peerStatus = peer.snapshot();
        std::cout << "TCP_HISTORY_TIMEOUT phase=" << static_cast<int>(owner->status().phase)
                  << " stage=" << owner->status().first.stage
                  << " matched=" << (!peerStatus.sessions.empty() && peerStatus.sessions[0].matched) << '\n';

        if (lastQuery) {
            hs::diagnose(*lastQuery);
        }

        if (lastEligible) {
            hs::diagnose(*lastEligible);
        }
    }

    REQUIRE(observed);
    REQUIRE(observed->source);
    CHECK(observed->source->snapshot == root::collectors::HistorySnapshotState::COMPLETE);
    CHECK(observed->source->sourceFrontier == 2);
    CHECK(observed->requestedFrom == 2);
    CHECK(observed->queuedForKey == 0);
    CHECK_FALSE(observed->acceptedUnstagedData);
    CHECK(reentryObserved);
    REQUIRE(desired.result());
    CHECK(desired.result()->status == m::CommandStatus::APPLIED);

    auto noWork = m::QueueWork{};
    const auto shortQuery = owner->historyRangeStatus(handle, key, noWork);
    REQUIRE_FALSE(shortQuery);
    CHECK(shortQuery.error().error == t::RecoveryError::WORK_LIMIT);
    REQUIRE(shortQuery.error().queue);
    CHECK(shortQuery.error().queue->error == m::QueueError::WORK_LIMIT);
    auto invalidWork = r::queueWork();
    const auto invalid = owner->historyRangeStatus(m::MuxHandle{}, key, invalidWork);
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.error().queue);
    CHECK(invalid.error().queue->error == m::QueueError::INVALID_CLIENT);

    auto moved = std::move(*owner);
    auto movedWork = r::queueWork();
    const auto movedBefore = hs::coordinates(movedWork);
    const auto movedFrom = owner->historyRangeStatus(handle, key, movedWork);
    REQUIRE_FALSE(movedFrom);
    CHECK(movedFrom.error().error == t::RecoveryError::INVALID_STATE);
    CHECK(hs::coordinates(movedWork) == movedBefore);
    owner.reset();
    owner.emplace(std::move(moved));
    auto restoredWork = r::queueWork();
    const auto restored = owner->historyRangeStatus(handle, key, restoredWork);
    REQUIRE(restored);
    CHECK(restored->completeDeliveredRange);
    REQUIRE(owner->requestStop());
    auto cutoffWork = r::queueWork();
    const auto cutoffBefore = hs::coordinates(cutoffWork);
    const auto cutoff = owner->historyRangeStatus(handle, key, cutoffWork);
    REQUIRE_FALSE(cutoff);
    CHECK(cutoff.error().stage == "recovery-history-ineligible");
    CHECK(hs::coordinates(cutoffWork) == cutoffBefore);
    r::stop(*owner);
    owner.reset();
    peer.stop();
    CHECK(observed->completeDeliveredRange);
    CHECK(observed->source->sourceFrontier == 2);
    CHECK(observed->epoch.matches(recorder->batches.front().epoch()));
    std::cout << "TCP history observation: covered=1 aggregateComplete=1 callbackReentry=1 detachedAfterRetirement=1\n";
}
