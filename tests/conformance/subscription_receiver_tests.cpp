// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/client/subscription_receiver.hpp"
#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/protocol/live_session.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace q = org::ttldtor::qdcxx::events;
namespace p = org::ttldtor::qdcxx::protocol;
namespace m = org::ttldtor::qdcxx::mux;
namespace a = org::ttldtor::qdcxx::client;
constexpr c::SchemaLimits SCHEMA_LIMITS{8, 64, 128, 128, 8192}; ///< Finite fixture schema capacities.
constexpr q::MappingLimits MAPPING_LIMITS{64, 128, 32, 64, 128, 128, 8192, 100000}; ///< Per-route preparation bounds.
constexpr std::int32_t FIRST_SENDER = 41;  ///< Sparse directional identity, independent of local record IDs.
constexpr std::int32_t SNAPSHOT_BEGIN = 4; ///< Existing binary HISTORY source flag.
constexpr std::int32_t SNAPSHOT_END = 8;   ///< Existing binary HISTORY source flag.

static_assert(!std::is_copy_constructible_v<a::SubscriptionBatch>);
static_assert(!std::is_move_constructible_v<a::SubscriptionBatch>);

/** Supply one finite independent native parsing turn.
 * @return Actual LiveSession admission credits.
 */
p::LiveWork liveTurn() {
    return {10000, 10000000, 10000000, 10000000};
}

/** Supply generous finite queue work without hiding any automatic retry replenishment.
 * @return One independent caller-selected owner turn.
 */
m::QueueWork turn() {
    return {100000,
            1000000000,
            1000000000,
            1000000000,
            1000000000,
            10000,
            10000,
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000}};
}

/** Supply explicit producer-local command copy credits.
 * @return One bounded submission turn.
 */
m::QueueSubmitWork submitTurn() {
    return {10000, 1000000, 1000000, 1000000};
}

/** Supply bounded event projection credits chosen by the application callback.
 * @return One callback's caller-owned allowance.
 */
a::ReceiverProjectionWork projectionTurn() {
    return {100000, 100000, 1000000};
}

/** Reserve complete existing per-registration queue resources.
 * @return Logical maxima for small immutable batches and source payloads.
 */
m::ClientLimits clientLimits() {
    return {32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096};
}

/** Describe three different real event/custom shapes, optionally with explicit indexed identities.
 * @param indexed Whether every record has the explicit first-LONG time layout for HISTORY.
 * @return Fresh independent schema origin; Quote/Profile use their real required lookup names.
 */
c::LocalSchema schema(bool indexed = false) {
    std::vector<c::LocalRecord> records{
        {0,
         u"ConfiguredQuote",
         indexed,
         {{104}, {8}, {8}, {8}, {8}, {10}, {233}},
         {{u"Index", 0},
          {u"BidPrice", 1},
          {u"BidSize", 2},
          {u"AskPrice", 3},
          {u"AskSize", 4},
          {u"ExtraText", 5},
          {u"ExtraCustom", 6}}},
        {1,
         u"ConfiguredProfile",
         indexed,
         {{104}, {10}, {10}, {8}, {9}},
         {{u"Index", 0}, {u"Description", 1}, {u"StatusReason", 2}, {u"Flags", 3}, {u"ExtraBytes", 4}}},
        {2,
         u"Custom",
         indexed,
         {{104}, {10}, {9}, {233}},
         {{u"Index", 0}, {u"Units", 1}, {u"Bytes", 2}, {u"Custom", 3}}}};

    if (indexed) {
        for (auto &record : records) {
            record.indexedTimeLayout = c::IndexedTimeSingleInt64{0};
        }
    }

    auto result = c::LocalSchema::create(records, SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Supply complete source values, including payloads absent from typed event projections.
 * @param localId Exact local record selection.
 * @param index Raw signed index, not event milliseconds.
 * @return Independently owned fields matching the fixture record.
 */
std::vector<c::FieldValue> values(std::int32_t localId, std::int64_t index = -777) {
    using T = c::SerialFieldType;

    if (localId == 0) {
        return {{T::LONG, index},
                {T::COMPACT_INT, 12},
                {T::COMPACT_INT, 34},
                {T::COMPACT_INT, 56},
                {T::COMPACT_INT, 78},
                {T::UTF_CHAR_ARRAY, 0, u"extra"},
                {T::CUSTOM_OBJECT, 0, {}, std::vector{std::byte{0}, std::byte{0xff}}}};
    }

    if (localId == 1) {
        return {{T::LONG, index},
                {T::UTF_CHAR_ARRAY, 0, std::u16string{u'D', u'\0', char16_t{0xd800}}},
                {T::UTF_CHAR_ARRAY, 0, u""},
                {T::COMPACT_INT, 0x76},
                {T::BYTE_ARRAY, 0, {}, std::vector<std::byte>{}}};
    }

    return {{T::LONG, index},
            {T::UTF_CHAR_ARRAY, 0},
            {T::BYTE_ARRAY, 0, {}, std::vector<std::byte>{}},
            {T::CUSTOM_OBJECT, 0}};
}

/** Append one canonical compact number through the production codec.
 * @param out Test-owned destination.
 * @param value Signed scalar or bounded length.
 */
void number(std::vector<std::byte> &out, std::int64_t value) {
    std::array<std::byte, 9> encoded{};
    const auto result = e::writeCompactLong(value, encoded);
    REQUIRE(result.status == e::Status::OK);
    out.insert(out.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append exact nullable UTFString data using the production encoder.
 * @param out Test-owned destination.
 * @param value Nullable borrowed UTF-16 units.
 */
void text(std::vector<std::byte> &out, std::optional<std::u16string_view> value) {
    std::array<std::byte, 8192> encoded{};
    const auto result = e::writeUtfString(value, encoded, {8192, 1024});
    REQUIRE(result.status == e::Status::OK);
    out.insert(out.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Wrap one native QTP envelope, without any parser reimplementation.
 * @param type Exact message ID.
 * @param payload Complete test-owned message payload.
 * @return Canonical bounded frame bytes.
 */
std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> body;
    number(body, type);
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<std::byte> result;
    number(result, static_cast<std::int64_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

/** Supply the existing bounded plain-session profile, optionally enabling actual FIELD-prefix parsing.
 * @param fieldTime Whether descriptors may carry the real physical FIELD metadata pair.
 * @return Explicit complete admission limits.
 */
p::LiveLimits liveLimits(bool fieldTime) {
    p::LiveLimits result{};
    result.frame = {8192, 8201};
    result.protocol = {128, 64, {8192, 1024}, 16384};
    result.descriptions = {8192, 32, 32, 256, {8192, 1024}, 8192};
    result.context = {{16, 128, 256, 128, 16384}, 32, {512, 1024, 16384}};
    result.data = {8192, 64,   512,   64,    512,   8192, {8192, 128, 128}, {8192, 1024}, 8192, 8192, 8192,
                   8192, 8192, 16384, 16384, 16384, 64};
    result.data.processedRecords = 256;
    result.data.emittedRecords = 256;
    result.data.emittedValues = 2048;
    result.data.ownedFieldTimeRecords = 64;
    result.residentBytes = 4 * 1024 * 1024;
    result.owners = 128;
    result.snapshotBytes = 65536;
    result.scratchBytes = 65536;
    result.eventBytes = 262144;
    result.outputBytes = 8192;
    result.credentials = p::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    result.fieldTime = fieldTime ? p::FieldTimePolicy::READ : p::FieldTimePolicy::DISABLED;
    result.events = p::DataEventPolicy::BINARY_CATALOG_V1;
    result.historySnapshot = true;

    return result;
}

/** Admit a complete frame through the actual production session parser.
 * @param session Actual live-session owner.
 * @param bytes Borrowed complete native frame.
 * @return Real immutable event owner, never fabricated queue record views.
 */
p::LiveEvent admit(p::LiveSession &session, std::span<const std::byte> bytes) {
    auto work = liveTurn();
    auto result = session.feed(bytes, work);
    INFO("live error=", static_cast<int>(session.diagnostic().error), " stage=", session.diagnostic().stage);
    REQUIRE(result.status == p::LiveStatus::EVENT);
    REQUIRE(result.accepted == bytes.size());
    REQUIRE(result.event);

    return std::move(*result.event);
}

/** Create native binding/session owners from the same immutable schema origin.
 * @param local Validated configured schema.
 * @param fieldTime Whether each incoming description adds the physical FIELD metadata prefix.
 * @return Ready session with actual peer capabilities and all three incoming record descriptions.
 */
p::LiveSession session(const c::LocalSchema &local, bool fieldTime) {
    std::vector<p::OutgoingRecord> declarations;

    for (const auto &record : local.records()) {
        p::OutgoingRecord declaration{FIRST_SENDER + record.id, record.id, record.name, {}};

        for (std::size_t field = 0; field < record.fields.size(); ++field) {
            REQUIRE(field < record.lookups.size());
            declaration.fields.push_back(
                {record.lookups[field].name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
        }

        declarations.push_back(std::move(declaration));
    }

    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(local, declarations, {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
    REQUIRE(writer);
    auto copy = local;
    auto work = liveTurn();
    auto result = p::LiveSession::create(std::move(copy), std::move(*writer), liveLimits(fieldTime), work);
    REQUIRE(result);
    std::vector<std::byte> ready{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(ready, 0);
    number(ready, 2);

    for (const auto &[id, name] : std::array{std::pair{15, u"STREAM_DATA"}, std::pair{20, u"HISTORY_DATA"}}) {
        number(ready, id);
        text(ready, name);
        number(ready, 0);
    }

    number(ready, 0);
    (void)admit(*result, frame(1, ready));

    for (const auto &record : local.records()) {
        std::vector<std::byte> description;
        number(description, FIRST_SENDER + record.id);
        text(description, record.name);
        number(description, static_cast<std::int64_t>(record.fields.size() + (fieldTime ? 2 : 0)));

        if (fieldTime) {
            text(description, u"EventTime");
            number(description, 56);
            text(description, u"EventSequence");
            number(description, 72);
        }

        for (std::size_t field = 0; field < record.fields.size(); ++field) {
            text(description, record.lookups[field].name);
            number(description, record.fields[field].serialTypeId);
        }

        (void)admit(*result, frame(2, description));
    }

    return std::move(*result);
}

/** Keep the actual protocol/collector/queue graph in its owner destruction order. */
struct Fixture {
    c::LocalSchema local;         ///< Retained declarations for factory calls and exact request contexts.
    p::LiveSession live;          ///< Actual production parser/source owner.
    m::MuxQueue queue;            ///< Actual family collector and callback dispatch owner.
    c::SubscriptionFamily family; ///< Immutable subscription contract.
    bool fieldTime;               ///< Whether native incoming descriptors carry the physical metadata prefix.
};

/** Create one actual STREAM or HISTORY graph from caller-selected schema declarations.
 * @param local Independent validated local schema, transferred to the fixture.
 * @param history Select indexed HISTORY rather than ordinary STREAM.
 * @param fieldTime Enable actual physical FIELD metadata in incoming descriptions.
 * @return Independent composed graph.
 */
Fixture fixture(c::LocalSchema local, bool history, bool fieldTime) {
    auto live = session(local, fieldTime);
    auto work = turn();
    const c::SubscriptionLimits desired{16, 32, 128, 4096, 64, 8192, 64};
    const m::MuxLimits interests{16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024};
    const m::QueueLimits limits{4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024};
    auto owner = history ? m::MuxQueue::createHistory(local, interests,
                                                      {32, 256, 128, 64, 4 * 1024 * 1024, 4, 128, 64, 4 * 1024 * 1024,
                                                       32 * 1024 * 1024, desired},
                                                      {false, 100, 10000}, limits, work)
                         : m::MuxQueue::createStream(local, interests, desired,
                                                     {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, limits, work);
    REQUIRE(owner);

    return {std::move(local), std::move(live), std::move(*owner),
            history ? c::SubscriptionFamily::HISTORY : c::SubscriptionFamily::STREAM, fieldTime};
}

/** Create the combined real-event/custom graph with finite existing queue profiles.
 * @param history Select indexed HISTORY rather than ordinary STREAM.
 * @param fieldTime Enable actual physical FIELD metadata in incoming descriptions.
 * @return Independent composed graph.
 */
Fixture fixture(bool history = false, bool fieldTime = false) {
    return fixture(schema(history), history, fieldTime);
}

/** Pump only the assigned owner for a finite number of explicit fresh turns.
 * @param graph Actual graph to progress.
 * @param ready Observable completion condition, checked before each turn.
 */
void until(Fixture &graph, const std::function<bool()> &ready) {
    for (std::size_t attempt = 0; attempt < 20000; ++attempt) {
        if (ready()) {
            return;
        }

        auto work = turn();
        auto result = graph.queue.progress(work);
        INFO("queue error=", result ? -1 : static_cast<int>(result.error().error));
        REQUIRE(result);
    }

    FAIL("bounded queue progress did not reach the observable condition");
}

/** Run a finite quiet tail to expose accidental later callbacks without timing assumptions.
 * @param graph Actual owner, independent of other tests/processes.
 */
void quiet(Fixture &graph) {
    for (std::size_t attempt = 0; attempt < 128; ++attempt) {
        auto work = turn();
        REQUIRE(graph.queue.progress(work));
    }
}

/** Prepare a known complete delivery batch without permitting an application callback yet.
 * @param graph Actual owner, progressed with explicit zero callback credits.
 * @param handle Existing receiving registration.
 * @param records Required complete queued record count before dispatch is enabled.
 */
void stageBatch(Fixture &graph, const m::MuxHandle &handle, std::size_t records) {
    for (std::size_t attempt = 0; attempt < 20000; ++attempt) {
        auto resources = graph.queue.resources(handle);
        REQUIRE(resources);

        if (resources->queuedRecords == records) {
            return;
        }

        auto work = turn();
        work.callbacks = 0;
        REQUIRE(graph.queue.progress(work));
    }

    FAIL("bounded callback-gated progress did not prepare the complete batch");
}

/** Observe one facade callback, retaining only explicitly detached/owned application data. */
struct Observer final : a::SubscriptionConsumer {
    std::size_t callbacks = 0;                          ///< Actual application invocations, not processed-record count.
    std::vector<m::OwnedMuxBatch> batches;              ///< Explicit immutable generic leases.
    std::vector<a::SubscriptionProjection> projections; ///< Independently owned successful events.
    std::vector<a::ReceiverProjectionFailure> errors;   ///< Exact local projection refusals.
    std::function<m::BatchAction(const a::SubscriptionBatch &)> action; ///< Optional application callback behavior.

    /** Record a whole callback and optionally exercise application-selected behavior.
     * @param batch Callback-only plan and immutable records; never stored as a borrowed wrapper.
     * @return Existing whole-batch action.
     */
    m::BatchAction onBatch(const a::SubscriptionBatch &batch) override {
        ++callbacks;
        batches.push_back(batch.retain());

        if (action) {
            return action(batch);
        }

        auto work = projectionTurn();

        for (std::size_t index = 0; index < batch.records().size(); ++index) {
            auto projected = batch.project(index, work);

            if (projected) {
                projections.push_back(std::move(*projected));
            } else {
                errors.push_back(projected.error());
            }
        }

        return m::BatchAction::CONTINUE;
    }
};

/** Configure the two existing prepared adapters, leaving the third record fully generic.
 * @param policy Explicit metadata policy, never inferred from raw indexed identity.
 * @return Finite exact local record routes.
 */
std::array<a::PreparedSubscriptionBinding, 2>
bindings(a::ReceiverEventTimePolicy policy = a::ReceiverEventTimePolicy::UNSPECIFIED) {
    return {{{0, a::PreparedEventKind::QUOTE, MAPPING_LIMITS, {16, 128, 256}, policy},
             {1, a::PreparedEventKind::PROFILE, MAPPING_LIMITS, {16, 128, 256}, policy}}};
}

/** Open the actual facade's initially empty existing handle.
 * @param graph Exact source/queue schema origin.
 * @param consumer Application-owned callable.
 * @param routes Prepared route descriptions; empty is generic-only.
 * @return Actual bounded MuxHandle, before any desired membership submission.
 */
m::MuxHandle open(Fixture &graph, const std::shared_ptr<Observer> &consumer,
                  std::span<const a::PreparedSubscriptionBinding> routes = {}) {
    a::ReceiverWork receiverWork{100000, 1000000};
    auto queueWork = turn();
    auto result =
        a::openSubscriptionReceiver(graph.queue, graph.local, graph.family, c::SubscriptionSelectorPolicy::EXACT_ONLY,
                                    routes, consumer, {4, 64, 128}, clientLimits(), receiverWork, queueWork);
    REQUIRE(result);

    return std::move(*result);
}

/** Replace complete desired interests and observe command completion separately from activation.
 * @param graph Actual graph.
 * @param handle Logical application registration.
 * @param records Exact local IDs to subscribe under the same canonical symbol.
 */
void activate(Fixture &graph, m::MuxHandle &handle, std::initializer_list<std::int32_t> records) {
    std::vector<c::SubscriptionRequest> requests;

    for (const auto localId : records) {
        requests.push_back({{graph.local.recordContext(localId), u"SYM"},
                            graph.family == c::SubscriptionFamily::HISTORY
                                ? std::optional<std::int64_t>{std::numeric_limits<std::int64_t>::min()}
                                : std::nullopt});
    }

    auto work = submitTurn();
    auto ticket = handle.replace(requests, work);
    REQUIRE(ticket);
    CHECK_FALSE(ticket->result());
    until(graph, [&] {
        return ticket->result().has_value();
    });
    REQUIRE(ticket->result());
    CHECK(ticket->result()->status == m::CommandStatus::APPLIED);
    until(graph, [&] {
        return handle.state() == m::QueueClientState::ACTIVE;
    });
}

/** One complete synthetic source record encoded through native field codecs. */
struct Input {
    std::int32_t localId;          ///< Explicit schema record identity.
    std::int64_t index = -777;     ///< Raw signed indexed/business-extra value, never an event timestamp.
    std::int32_t flags = 0;        ///< Original source flags.
    p::FieldEventTime fieldTime{}; ///< Actual metadata words when the fixture explicitly enables them.
    std::optional<std::vector<c::FieldValue>>
        fields; ///< Explicit complete values for a caller-selected minimal schema.
};

/** Encode only the finite fixture field types using existing scalar/text codecs.
 * @param out Test-owned native payload.
 * @param value Canonical exact field from values().
 */
void field(std::vector<std::byte> &out, const c::FieldValue &value) {
    if (value.type == c::SerialFieldType::UTF_CHAR_ARRAY) {
        std::array<std::byte, 8192> encoded{};
        const auto result = e::writeCharArray(
            value.units ? std::optional<std::u16string_view>(*value.units) : std::nullopt, encoded, {8192, 1024});
        REQUIRE(result.status == e::Status::OK);
        out.insert(out.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
    } else if (value.type == c::SerialFieldType::BYTE_ARRAY || value.type == c::SerialFieldType::CUSTOM_OBJECT) {
        number(out, value.bytes ? static_cast<std::int64_t>(value.bytes->size()) : -1);

        if (value.bytes) {
            out.insert(out.end(), value.bytes->begin(), value.bytes->end());
        }
    } else {
        REQUIRE((value.type == c::SerialFieldType::LONG || value.type == c::SerialFieldType::COMPACT_INT));
        number(out, value.raw);
    }
}

/** Transfer actual parsed records into the existing queue, with bounded explicit busy progress.
 * @param graph Real source/queue graph.
 * @param inputs Complete ordered source records.
 */
void send(Fixture &graph, std::initializer_list<Input> inputs) {
    std::vector<std::byte> payload;

    for (const auto &input : inputs) {
        payload.push_back(std::byte{0xf9});
        number(payload, input.flags);
        payload.push_back(std::byte{0xfd});
        constexpr std::u16string_view SYMBOL = u"SYM";
        number(payload, static_cast<std::int64_t>(SYMBOL.size()));

        for (const auto unit : SYMBOL) {
            std::array<std::byte, 4> encoded{};
            const auto result = e::writeUtfChar(unit, encoded);
            REQUIRE(result.status == e::Status::OK);
            payload.insert(payload.end(), encoded.begin(),
                           encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
        }

        number(payload, FIRST_SENDER + input.localId);

        if (graph.fieldTime) {
            number(payload, input.fieldTime.rawSeconds);
            number(payload, input.fieldTime.rawSequence.value_or(0));
        }

        const auto sourceValues = input.fields ? *input.fields : values(input.localId, input.index);

        for (const auto &value : sourceValues) {
            field(payload, value);
        }
    }

    auto event = admit(graph.live, frame(graph.family == c::SubscriptionFamily::HISTORY ? 20 : 15, payload));

    for (std::size_t attempt = 0; attempt < 20000; ++attempt) {
        auto work = turn();
        auto accepted = graph.queue.admit(std::move(event), work);

        if (accepted) {
            return;
        }

        REQUIRE(accepted.error().error == m::QueueError::BUSY);
        REQUIRE(graph.queue.progress(work));
    }

    FAIL("bounded source admission did not progress");
}

/** Compare relevant public resources after a refused creation without claiming RSS equality.
 * @param actual Current immutable resource snapshot.
 * @param before Snapshot before the attempted factory call.
 */
void unchanged(const m::QueueResources &actual, const m::QueueResources &before) {
    CHECK(actual.generations == before.generations);
    CHECK(actual.reservedBytes == before.reservedBytes);
    CHECK(actual.residentBytes == before.residentBytes);
    CHECK(actual.reservedItems == before.reservedItems);
    CHECK(actual.reservedCommandSlots == before.reservedCommandSlots);
    CHECK(actual.reservedResultSlots == before.reservedResultSlots);
}
} // namespace

TEST_CASE("receiver factory refuses foreign provenance and invalid typed routes before opening") {
    auto graph = fixture();
    auto consumer = std::make_shared<Observer>();
    auto existing = open(graph, consumer);
    activate(graph, existing, {0});
    const auto consumerReferences = consumer.use_count();
    auto foreign = schema();
    const auto before = graph.queue.resources();
    const auto revision = graph.queue.revision();
    const auto routes = bindings();
    a::ReceiverWork receiverWork{100000, 1000000};
    auto queueWork = turn();
    auto failed =
        a::openSubscriptionReceiver(graph.queue, foreign, graph.family, c::SubscriptionSelectorPolicy::EXACT_ONLY,
                                    routes, consumer, {4, 64, 128}, clientLimits(), receiverWork, queueWork);
    REQUIRE_FALSE(failed);
    REQUIRE(std::holds_alternative<m::QueueFailure>(failed.error().cause));
    CHECK(std::get<m::QueueFailure>(failed.error().cause).error == m::QueueError::INVALID_CONTEXT);
    unchanged(graph.queue.resources(), before);

    for (const auto invalidKind : {a::PreparedEventKind::QUOTE, a::PreparedEventKind::PROFILE}) {
        auto invalid = routes;
        invalid[0].localId = invalidKind == a::PreparedEventKind::QUOTE ? 2 : 0;
        invalid[0].kind = invalidKind;
        receiverWork = {100000, 1000000};
        queueWork = turn();
        auto missing = a::openSubscriptionReceiver(graph.queue, graph.local, graph.family,
                                                   c::SubscriptionSelectorPolicy::EXACT_ONLY, invalid, consumer,
                                                   {4, 64, 128}, clientLimits(), receiverWork, queueWork);
        REQUIRE_FALSE(missing);
        REQUIRE(std::holds_alternative<q::MappingError>(missing.error().cause));
        CHECK(std::get<q::MappingError>(missing.error().cause) == q::MappingError::MISSING_REQUIRED);
        CHECK(missing.error().bindingIndex == 0);
        unchanged(graph.queue.resources(), before);
    }

    CHECK(graph.queue.revision() == revision);
    CHECK(graph.queue.aggregateEntries().size() == 1);
    CHECK(existing.state() == m::QueueClientState::ACTIVE);
    CHECK(consumer->callbacks == 0);
    CHECK(consumer.use_count() == consumerReferences);
}

TEST_CASE("receiver factory enforces finite preparation and aggregate limits without partial registration") {
    auto graph = fixture();
    auto consumer = std::make_shared<Observer>();
    auto routes = bindings();
    const auto before = graph.queue.resources();

    for (const bool exhausted : {false, true}) {
        a::ReceiverWork receiverWork = exhausted ? a::ReceiverWork{} : a::ReceiverWork{100000, 1000000};
        auto queueWork = turn();
        auto failed = a::openSubscriptionReceiver(
            graph.queue, graph.local, graph.family, c::SubscriptionSelectorPolicy::EXACT_ONLY, routes, consumer,
            exhausted ? a::ReceiverLimits{4, 64, 128} : a::ReceiverLimits{}, clientLimits(), receiverWork, queueWork);
        REQUIRE_FALSE(failed);
        REQUIRE(std::holds_alternative<a::ReceiverError>(failed.error().cause));
        CHECK(std::get<a::ReceiverError>(failed.error().cause) ==
              (exhausted ? a::ReceiverError::WORK_LIMIT : a::ReceiverError::RESOURCE_LIMIT));
        unchanged(graph.queue.resources(), before);
    }

    routes[1] = routes[0];
    a::ReceiverWork receiverWork{100000, 1000000};
    auto queueWork = turn();
    auto duplicate =
        a::openSubscriptionReceiver(graph.queue, graph.local, graph.family, c::SubscriptionSelectorPolicy::EXACT_ONLY,
                                    routes, consumer, {4, 64, 128}, clientLimits(), receiverWork, queueWork);
    REQUIRE_FALSE(duplicate);
    REQUIRE(std::holds_alternative<a::ReceiverError>(duplicate.error().cause));
    CHECK(std::get<a::ReceiverError>(duplicate.error().cause) == a::ReceiverError::INVALID_INPUT);
    unchanged(graph.queue.resources(), before);
}

TEST_CASE("receiver exposes combined prepared events and every generic extra without hidden activation") {
    auto graph = fixture();
    auto observer = std::make_shared<Observer>();
    const auto routes = bindings();
    const auto initialRevision = graph.queue.revision();
    auto handle = open(graph, observer, routes);
    CHECK(graph.queue.aggregateEntries().empty());
    CHECK(graph.queue.revision() == initialRevision);
    activate(graph, handle, {0, 1, 2});
    send(graph, {{0}, {1}, {2}});
    stageBatch(graph, handle, 3);
    until(graph, [&] {
        return observer->projections.size() == 3;
    });
    REQUIRE(observer->errors.empty());
    REQUIRE(observer->projections.size() == 3);
    REQUIRE(std::holds_alternative<q::Quote>(observer->projections[0].value));
    REQUIRE(std::holds_alternative<q::Profile>(observer->projections[1].value));
    CHECK(std::holds_alternative<std::monostate>(observer->projections[2].value));
    CHECK(std::get<q::Quote>(observer->projections[0].value).bidPrice.toDouble() == 12);
    const auto &profile = std::get<q::Profile>(observer->projections[1].value);
    REQUIRE(profile.description);
    CHECK(profile.description->size() == 3);
    REQUIRE(profile.statusReason);
    CHECK(profile.statusReason->empty());
    CHECK(profile.flags == 0x76);
    REQUIRE_FALSE(observer->batches.empty());
    const auto records = observer->batches[0].records();
    REQUIRE(records.size() == 3);
    REQUIRE(records[0].typedValues.size() == 7);
    CHECK(records[0].typedValues[5].units == u"extra");
    REQUIRE(records[0].typedValues[6].bytes);
    REQUIRE(records[0].typedValues[6].bytes->size() == 2);
    CHECK((*records[0].typedValues[6].bytes)[1] == std::byte{0xff});
    REQUIRE(records[2].typedValues.size() == 4);
    CHECK_FALSE(records[2].typedValues[1].units);
    REQUIRE(records[2].typedValues[2].bytes);
    CHECK(records[2].typedValues[2].bytes->empty());
    CHECK_FALSE(records[2].typedValues[3].bytes);
    REQUIRE(records[0].source);
    CHECK(records[0].source->senderId == FIRST_SENDER);
    CHECK(records[0].source->flags == 0);
    CHECK_FALSE(records[0].source->fieldTime);
    CHECK_FALSE(records[0].rawTime);
    CHECK_FALSE(observer->projections[0].eventTimeMs);
    CHECK(std::get<q::Quote>(observer->projections[0].value).eventTime == 0);
}

TEST_CASE("receiver projections use explicit FIELD metadata and preserve independent signed HISTORY identity") {
    constexpr std::int64_t RAW_INDEX = -777;
    const std::array metadataCases{p::FieldEventTime{0, 0, 0}, p::FieldEventTime{7, std::int64_t{5} << 22, 0}};

    for (const auto &metadata : metadataCases) {
        auto graph = fixture(true, true);
        auto observer = std::make_shared<Observer>();
        auto unspecified = std::make_shared<Observer>();
        const auto routes = bindings(a::ReceiverEventTimePolicy::FIELD_METADATA);
        const auto plainRoutes = bindings();
        auto handle = open(graph, observer, routes);
        auto plainHandle = open(graph, unspecified, plainRoutes);
        activate(graph, handle, {0, 2});
        activate(graph, plainHandle, {0, 2});
        send(graph, {{0, RAW_INDEX, SNAPSHOT_BEGIN | SNAPSHOT_END, metadata},
                     {2, RAW_INDEX, SNAPSHOT_BEGIN | SNAPSHOT_END, metadata}});
        until(graph, [&] {
            return observer->projections.size() >= 2 && unspecified->projections.size() >= 2;
        });
        REQUIRE(observer->errors.empty());
        REQUIRE(unspecified->errors.empty());
        bool quoteSeen = false;
        bool genericSeen = false;

        for (const auto &batch : observer->batches) {
            for (const auto &record : batch.records()) {
                REQUIRE(record.rawTime);
                CHECK(*record.rawTime == RAW_INDEX);
                REQUIRE(record.source);
                CHECK(record.source->flags == (SNAPSHOT_BEGIN | SNAPSHOT_END));
                REQUIRE(record.source->fieldTime);
                CHECK(record.source->fieldTime->eventTimeMs() == metadata.eventTimeMs());
                CHECK((record.context.matches(graph.local.recordContext(0)) ||
                       record.context.matches(graph.local.recordContext(2))));
            }
        }

        for (const auto &projection : observer->projections) {
            if (const auto *quote = std::get_if<q::Quote>(&projection.value)) {
                quoteSeen = true;
                CHECK(projection.eventTimeMs == metadata.eventTimeMs());
                CHECK(quote->eventTime == metadata.eventTimeMs());
                CHECK(quote->eventTime != RAW_INDEX);
            } else if (std::holds_alternative<std::monostate>(projection.value)) {
                genericSeen = true;
                CHECK_FALSE(projection.eventTimeMs);
            }
        }

        for (const auto &projection : unspecified->projections) {
            CHECK_FALSE(projection.eventTimeMs);

            if (const auto *quote = std::get_if<q::Quote>(&projection.value)) {
                CHECK(quote->eventTime == 0);
            }
        }

        CHECK(quoteSeen);
        CHECK(genericSeen);
    }
}

TEST_CASE("receiver lazy projection exposes invalid indexes zero work and repeated work exhaustion") {
    auto graph = fixture();
    auto observer = std::make_shared<Observer>();
    bool invalidIndex = false;
    bool zeroWork = false;
    bool repeatExhausted = false;
    bool firstSucceeded = false;
    bool genericStillReadable = false;
    observer->action = [&](const a::SubscriptionBatch &batch) {
        auto work = projectionTurn();
        auto invalid = batch.project(batch.records().size(), work);
        invalidIndex = !invalid && std::get<a::ReceiverError>(invalid.error()) == a::ReceiverError::INVALID_INPUT;
        a::ReceiverProjectionWork empty{};
        auto zero = batch.project(0, empty);
        zeroWork = !zero && std::get<a::ReceiverError>(zero.error()) == a::ReceiverError::WORK_LIMIT;
        work = projectionTurn();
        work.textUnits = 16;
        auto first = batch.project(0, work);
        firstSucceeded = first.has_value();
        auto repeated = batch.project(0, work);
        repeatExhausted = !repeated && std::get<a::ReceiverError>(repeated.error()) == a::ReceiverError::WORK_LIMIT;
        genericStillReadable = !batch.records().empty() && batch.records()[0].typedValues.size() == 7;

        return m::BatchAction::CONTINUE;
    };
    const auto routes = bindings();
    auto handle = open(graph, observer, routes);
    activate(graph, handle, {0});
    send(graph, {{0}});
    until(graph, [&] {
        return observer->callbacks == 1;
    });
    CHECK(invalidIndex);
    CHECK(zeroWork);
    CHECK(firstSucceeded);
    CHECK(repeatExhausted);
    CHECK(genericStillReadable);
}

TEST_CASE("minimal Profile projection requires all twenty visit credits and never replenishes them") {
    const c::LocalRecord record{0, u"MinimalProfile", false, {{10}}, {{u"Description", 0}}};
    auto local = c::LocalSchema::create(std::span(&record, 1), SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    auto graph = fixture(std::move(*local), false, false);
    auto observer = std::make_shared<Observer>();
    auto mappingLimits = MAPPING_LIMITS;
    mappingLimits.selections = 16;
    const std::array routes{
        a::PreparedSubscriptionBinding{0, a::PreparedEventKind::PROFILE, mappingLimits, {16, 16, 16}}};
    bool oneShortRefused = false;
    bool exactSucceeded = false;
    bool repeatedRefused = false;
    bool oneByteShortRefused = false;
    bool exactBytesSucceeded = false;
    std::size_t remainingVisits = 1;
    std::size_t remainingBytes = 1;
    observer->action = [&](const a::SubscriptionBatch &batch) {
        auto work = projectionTurn();
        // One index, one route, one field, sixteen selections and one metadata inspection.
        constexpr std::size_t EXACT_VISITS = 20;
        work.visits = EXACT_VISITS - 1;
        auto oneShort = batch.project(0, work);
        oneShortRefused = !oneShort && std::holds_alternative<a::ReceiverError>(oneShort.error()) &&
                          std::get<a::ReceiverError>(oneShort.error()) == a::ReceiverError::WORK_LIMIT;
        work = projectionTurn();
        work.visits = EXACT_VISITS;
        auto exact = batch.project(0, work);
        exactSucceeded = exact && std::holds_alternative<q::Profile>(exact->value);
        remainingVisits = work.visits;

        if (exact) {
            observer->projections.push_back(std::move(*exact));
        }

        auto repeated = batch.project(0, work);
        repeatedRefused = !repeated && std::holds_alternative<a::ReceiverError>(repeated.error()) &&
                          std::get<a::ReceiverError>(repeated.error()) == a::ReceiverError::WORK_LIMIT;

        // The whole owning projection includes inline business snapshots and three potential string terminators.
        constexpr auto EXACT_BYTES = sizeof(a::SubscriptionProjection) + (16 + 3) * sizeof(char16_t);
        work = projectionTurn();
        work.bytes = EXACT_BYTES - 1;
        auto oneByteShort = batch.project(0, work);
        oneByteShortRefused = !oneByteShort && std::holds_alternative<a::ReceiverError>(oneByteShort.error()) &&
                              std::get<a::ReceiverError>(oneByteShort.error()) == a::ReceiverError::WORK_LIMIT &&
                              work.bytes == EXACT_BYTES - 1;
        work = projectionTurn();
        work.bytes = EXACT_BYTES;
        auto exactBytes = batch.project(0, work);
        exactBytesSucceeded = exactBytes && std::holds_alternative<q::Profile>(exactBytes->value) &&
                              std::get<q::Profile>(exactBytes->value).businessNumericSnapshot.has_value();
        remainingBytes = work.bytes;

        return m::BatchAction::CONTINUE;
    };
    auto handle = open(graph, observer, routes);
    activate(graph, handle, {0});
    Input input{0};
    input.fields = std::vector<c::FieldValue>{{c::SerialFieldType::UTF_CHAR_ARRAY, 0, u"D"}};
    send(graph, {input});
    until(graph, [&] {
        return observer->callbacks == 1;
    });
    CHECK(oneShortRefused);
    CHECK(exactSucceeded);
    CHECK(remainingVisits == 0);
    CHECK(repeatedRefused);
    CHECK(oneByteShortRefused);
    CHECK(exactBytesSucceeded);
    CHECK(remainingBytes == 0);
}

TEST_CASE("independent receiver handles preserve same-symbol peer demand and allow opening an already live queue") {
    auto graph = fixture();
    auto first = std::make_shared<Observer>();
    auto second = std::make_shared<Observer>();
    auto firstHandle = open(graph, first);
    activate(graph, firstHandle, {0});
    send(graph, {{0}});
    until(graph, [&] {
        return first->callbacks == 1;
    });
    auto secondHandle = open(graph, second);
    activate(graph, secondHandle, {0});
    auto work = submitTurn();
    const c::SubscriptionKey key{graph.local.recordContext(0), u"SYM"};
    auto removed = firstHandle.remove(std::span(&key, 1), work);
    REQUIRE(removed);
    until(graph, [&] {
        return removed->result().has_value();
    });
    REQUIRE(removed->result());
    CHECK(removed->result()->status == m::CommandStatus::APPLIED);
    quiet(graph);
    REQUIRE(graph.queue.aggregateEntries().size() == 1);
    send(graph, {{0}});
    until(graph, [&] {
        return second->callbacks == 1;
    });
    quiet(graph);
    CHECK(first->callbacks == 1);
    CHECK(second->callbacks == 1);
    CHECK(secondHandle.state() == m::QueueClientState::ACTIVE);
}

TEST_CASE("owned receiver projections and retained generic batches survive adapter queue and source destruction") {
    auto observer = std::make_shared<Observer>();
    std::weak_ptr<Observer> weak = observer;
    m::OwnedMuxBatch retained;
    std::optional<a::SubscriptionProjection> projection;

    {
        auto graph = fixture();
        const auto routes = bindings();
        auto handle = open(graph, observer, routes);
        activate(graph, handle, {1});
        send(graph, {{1}});
        until(graph, [&] {
            return observer->projections.size() == 1;
        });
        REQUIRE(observer->batches.size() == 1);
        retained = observer->batches[0];
        projection = std::move(observer->projections[0]);
    }

    observer.reset();
    CHECK(weak.expired());
    REQUIRE(retained.records().size() == 1);
    REQUIRE(retained.records()[0].typedValues.size() == 5);
    REQUIRE(retained.records()[0].typedValues[1].units);
    CHECK(retained.records()[0].typedValues[1].units->size() == 3);
    REQUIRE(projection);
    REQUIRE(std::holds_alternative<q::Profile>(projection->value));
    const auto &profile = std::get<q::Profile>(projection->value);
    REQUIRE(profile.description);
    REQUIRE(profile.description->size() == 3);
    CHECK((*profile.description)[1] == u'\0');
    CHECK(profile.flags == 0x76);
    retained = {};
    CHECK(profile.description->size() == 3);
}

TEST_CASE("one multi-record receiver callback may stop or release its handle and still project its current batch") {
    for (const bool release : {false, true}) {
        auto graph = fixture();
        auto observer = std::make_shared<Observer>();
        m::MuxHandle handle;
        std::size_t recordsSeen = 0;
        bool projectedAfterControl = false;
        observer->action = [&](const a::SubscriptionBatch &batch) {
            recordsSeen = batch.records().size();

            if (release) {
                handle = {};
            } else {
                (void)handle.requestStop(m::StopMode::ABORT);
            }

            auto work = projectionTurn();
            auto projected = batch.project(0, work);
            projectedAfterControl = projected && std::holds_alternative<q::Quote>(projected->value);

            if (projected) {
                observer->projections.push_back(std::move(*projected));
            }

            return m::BatchAction::CONTINUE;
        };
        const auto routes = bindings();
        handle = open(graph, observer, routes);
        activate(graph, handle, {0});
        send(graph, {{0, 1}, {0, 2}, {0, 3}});
        stageBatch(graph, handle, 3);
        until(graph, [&] {
            return observer->callbacks == 1;
        });
        quiet(graph);
        CHECK(recordsSeen == 3);
        CHECK(projectedAfterControl);
        CHECK(observer->callbacks == 1);
        REQUIRE(observer->batches.size() == 1);
        CHECK(observer->batches[0].records().size() == 3);

        if (!release) {
            until(graph, [&] {
                return handle.terminal().has_value();
            });
            REQUIRE(handle.terminal());
            CHECK(handle.terminal()->counts.callbackStartedBatches == 1);
            CHECK(handle.terminal()->counts.callbackCompletedBatches == 1);
        }

        send(graph, {{0, 4}});
        quiet(graph);
        CHECK(observer->callbacks == 1);
    }
}

TEST_CASE("receiver callback failure remains isolated from a healthy same-symbol peer") {
    auto graph = fixture();
    auto failing = std::make_shared<Observer>();
    auto healthy = std::make_shared<Observer>();
    failing->action = [](const a::SubscriptionBatch &) -> m::BatchAction {
        throw std::runtime_error("controlled receiver callback failure");
    };
    auto failedHandle = open(graph, failing);
    auto healthyHandle = open(graph, healthy);
    activate(graph, failedHandle, {0});
    activate(graph, healthyHandle, {0});
    send(graph, {{0}});
    until(graph, [&] {
        return failedHandle.terminal().has_value() && healthy->callbacks == 1;
    });
    REQUIRE(failedHandle.terminal());
    REQUIRE(failedHandle.terminal()->primary);
    CHECK(failedHandle.terminal()->primary->error == m::QueueError::CALLBACK_FAILURE);
    CHECK(healthyHandle.state() == m::QueueClientState::ACTIVE);
    send(graph, {{0}});
    until(graph, [&] {
        return healthy->callbacks == 2;
    });
    quiet(graph);
    CHECK(failing->callbacks == 1);
    CHECK(healthy->callbacks == 2);
}
