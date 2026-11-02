// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "recovery_test_support.hpp"

#include <functional>
#include <initializer_list>
#include <iostream>

namespace org::ttldtor::qdcxx::test::history_status {
namespace r = recovery;
namespace c = core;
namespace m = mux;
namespace p = protocol;
namespace t = transport;

inline constexpr std::int32_t TX_PENDING = 1;            ///< Actual QTP transaction continuation flag.
inline constexpr std::int32_t REMOVE_EVENT = 2;          ///< Actual QTP removal flag.
inline constexpr std::int32_t SNAPSHOT_BEGIN = 4;        ///< Actual descending snapshot start flag.
inline constexpr std::int32_t SNAPSHOT_END = 8;          ///< Actual complete snapshot end flag.
inline constexpr std::int32_t SNAPSHOT_SNIP = 16;        ///< Actual truncated snapshot boundary flag.
inline constexpr std::size_t HISTORY_PARTITIONS = 4;     ///< Finite key capacity for single-key observation scenarios.
inline constexpr std::size_t HISTORY_POINTS = 16;        ///< Covers actual snapshot, transaction and removal points.
inline constexpr std::size_t HISTORY_CACHE_FRAMES = 8;   ///< Shared frames, including retained old/incoming owners.
inline constexpr std::size_t HISTORY_VIEWS = 4;          ///< Independent registered observation views.
inline constexpr std::size_t HISTORY_PENDING_ITEMS = 16; ///< Finite per-view projection FIFO.
inline constexpr std::size_t HISTORY_PENDING_FRAMES = 8; ///< Finite per-view referenced source owners.

/** Retain actual detached callbacks; an optional action exercises public reentry without owner capture cycles. */
struct Recorder final : m::MuxConsumer {
    std::vector<m::OwnedMuxBatch> batches; ///< Detached records surviving native and collector retirement.
    std::function<void()> action;          ///< Test-only synchronous action; assigned after owner construction.
    bool pause = false;                    ///< Stop future callbacks after the current acknowledged batch.

    /** Retain one actual callback before invoking the optional observation action.
     * @param batch Callback-only existing queue projection.
     * @return Existing whole-batch pause or continue action.
     */
    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        batches.push_back(batch.retain());

        if (action) {
            action();
        }

        return pause ? m::BatchAction::PAUSE : m::BatchAction::CONTINUE;
    }
};

/** Create an actual indexed HISTORY queue without admitting any source data.
 * @param schema Exact caller provenance, shared with the later writer/session.
 * @return Pristine real collector/MUX/delivery owner under finite test capacities.
 */
inline m::MuxQueue queue(const c::LocalSchema &schema) {
    const m::MuxLimits interests{16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024};
    const c::SubscriptionLimits desired{16, 32, 128, 4096, 64, 8192, 64};
    const m::QueueLimits delivery{
        4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024};
    auto work = r::queueWork();
    auto created = m::MuxQueue::createHistory(schema, interests,
                                              {HISTORY_PARTITIONS, HISTORY_POINTS, 128, HISTORY_CACHE_FRAMES,
                                               4 * 1024 * 1024, HISTORY_VIEWS, HISTORY_PENDING_ITEMS,
                                               HISTORY_PENDING_FRAMES, 4 * 1024 * 1024, 32 * 1024 * 1024, desired},
                                              {false, 100, 10000}, delivery, work);
    REQUIRE(created);

    return std::move(*created);
}

/** Reserve a real logical HISTORY client before recovery adopts its queue.
 * @param owner Pristine source owner with the exact configured schema.
 * @param recorder Test callback kept alive through each actual invocation.
 * @return Existing producer-safe handle, independent of queue-facade moves.
 */
inline m::MuxHandle open(m::MuxQueue &owner, const std::shared_ptr<Recorder> &recorder) {
    auto work = r::queueWork();
    auto handle =
        owner.open({32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, recorder, work);
    REQUIRE(handle);

    return std::move(*handle);
}

/** Submit the one exact test key's current raw bound through an actual command ticket.
 * @param handle Existing registration.
 * @param schema Exact immutable provenance.
 * @param bound Signed raw HISTORY lower bound, unrelated to milliseconds.
 * @return Actual pending or applied replacement ticket.
 */
inline m::MuxCommandTicket replace(m::MuxHandle &handle, const c::LocalSchema &schema, std::int64_t bound) {
    const c::SubscriptionRequest request{{schema.recordContext(0), u"A"}, bound};
    m::QueueSubmitWork work{10000, 1000000, 1000000, 1000000};
    auto ticket = handle.replace(std::span(&request, 1), work);
    REQUIRE(ticket);

    return std::move(*ticket);
}

/** One exact wire point; flags drive the production HISTORY state machine. */
struct Point {
    std::int64_t time;      ///< Signed raw index, encoded through the schema's high/low pair.
    std::int32_t value;     ///< Ordinary R.V value, or zero for a removal.
    std::int32_t flags = 0; ///< Existing binary event flags, never a fabricated collector result.
};

/** Encode one complete actual HISTORY envelope with exact source flags.
 * @param points Ordered records for key A and configured compact R fields.
 * @param sender Actual incoming sender ID of the current connection.
 * @return Complete binary QTP type20 frame for LiveSession to admit.
 */
inline std::vector<std::byte> data(std::initializer_list<Point> points, std::int32_t sender = 41) {
    std::vector<std::byte> payload;

    for (const auto &point : points) {
        payload.push_back(std::byte{0xf9}); // Actual event-flag prefix, followed by an ordinary raw symbol.
        r::number(payload, point.flags);
        payload.push_back(std::byte{0xfd});
        r::number(payload, 1);
        payload.push_back(std::byte{'A'});
        r::number(payload, sender);

        if ((point.flags & REMOVE_EVENT) != 0) {
            REQUIRE(point.value == 0);
            r::number(payload, point.time);
        } else {
            const auto bits = std::bit_cast<std::uint64_t>(point.time);
            r::number(payload, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32)));
            r::number(payload, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits)));
            r::number(payload, point.value);
        }
    }

    return r::frame(20, payload);
}

/** Build an independently expected full native HISTORY subscription prefix before releasing test data.
 * @return Exact DESCRIBE_PROTOCOL, complete R declaration and HISTORY ADD A from raw bound 2.
 */
inline std::vector<std::byte> clientOutput() {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    r::number(payload, 1);
    r::text(payload, u"opt");
    r::text(payload, u"hs");
    r::number(payload, 5);
    constexpr std::array<std::int32_t, 5> send{0, 1, 2, 21, 22};
    constexpr std::array<std::u16string_view, 5> sendNames{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS",
                                                           u"HISTORY_ADD_SUBSCRIPTION", u"HISTORY_REMOVE_SUBSCRIPTION"};

    for (std::size_t index = 0; index < send.size(); ++index) {
        r::number(payload, send[index]);
        r::text(payload, sendNames[index]);
        r::number(payload, 0);
    }

    r::number(payload, 4);
    constexpr std::array<std::int32_t, 4> receive{0, 1, 2, 20};
    constexpr std::array<std::u16string_view, 4> receiveNames{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS",
                                                              u"HISTORY_DATA"};

    for (std::size_t index = 0; index < receive.size(); ++index) {
        r::number(payload, receive[index]);
        r::text(payload, receiveNames[index]);
        r::number(payload, 0);
    }

    auto result = r::frame(1, payload);
    r::append(result, r::description(71));
    payload = {std::byte{0xfd}};
    r::number(payload, 1);
    payload.push_back(std::byte{'A'});
    r::number(payload, 71);
    r::number(payload, 2);
    r::append(result, r::frame(21, payload));

    return result;
}

/** Publish fixed scalar diagnostics from an actual successful query before a bounded test timeout.
 * @param status Last actually eligible AS-OF observation, not a constructed expected outcome.
 */
inline void diagnose(const m::QueueRangeStatus &status) {
    std::cout << "HISTORY_RANGE complete=" << status.completeDeliveredRange
              << " mappingCurrent=" << status.mappingCurrent << " requested=" << status.requestedFrom
              << " unstaged=" << status.acceptedUnstagedData << " paused=" << status.paused
              << " reset=" << status.resetPending << " gap=" << status.hadGap << " queued=" << status.queuedForKey
              << " executing=" << status.executingForKey << " desired=" << status.clientDesiredRevision
              << " mapped=" << status.mappingDesiredRevision;

    if (status.source) {
        const auto &source = *status.source;
        std::cout << " sourceSnapshot=" << static_cast<int>(source.snapshot)
                  << " sourceRequested=" << source.requestedFrom << " frontier=" << source.sourceFrontier
                  << " knownFrontier=" << source.knownFrontier << " sourceTX=" << source.sourceTransactionPending
                  << " initialPending=" << source.initialPending << " viewTX=" << source.viewTransactionDirty
                  << " sourceComplete=" << source.completeRequestedRange;
    } else {
        std::cout << " source=null";
    }

    std::cout << '\n';
}

/** Publish the actual last query failure or its successful scalar source/delivery observation.
 * @param result Last actual query result before timeout or terminal failure.
 */
inline void diagnose(const std::expected<m::QueueRangeStatus, t::RecoveryDiagnostic> &result) {
    if (result) {
        diagnose(*result);
    } else {
        std::cout << "HISTORY_QUERY error=" << static_cast<int>(result.error().error)
                  << " stage=" << result.error().stage;

        if (result.error().queue) {
            std::cout << " queueError=" << static_cast<int>(result.error().queue->error);
        }

        std::cout << '\n';
    }
}

/** Enable the already supported binary flags and explicit HISTORY snapshot delivery.
 * @param settings Explicit unactivated endpoint input, changed only in the test caller.
 */
inline void enableHistory(client::EndpointSettings &settings) {
    settings.budgets.session.events = p::DataEventPolicy::BINARY_CATALOG_V1;
    settings.budgets.session.historySnapshot = true;
}

/** Flatten every existing independent queue work coordinate without comparing padding bytes.
 * @param work Actual caller-owned credits.
 * @return Exact numeric snapshot for debit/immutability checks.
 */
inline std::array<std::size_t, 26> coordinates(const m::QueueWork &work) {
    return {work.operations,
            work.visits,
            work.fields,
            work.textUnits,
            work.bytes,
            work.commands,
            work.callbacks,
            work.interest.operations,
            work.interest.visits,
            work.interest.symbolUnits,
            work.interest.bytes,
            work.desired.commands,
            work.desired.keyVisits,
            work.desired.symbolUnits,
            work.stream.operations,
            work.stream.visits,
            work.stream.textUnits,
            work.stream.bytes,
            work.ticker.operations,
            work.ticker.visits,
            work.ticker.textUnits,
            work.ticker.bytes,
            work.history.operations,
            work.history.visits,
            work.history.textUnits,
            work.history.bytes};
}

/** Compare detached source/delivery facts without inventing access to opaque token internals.
 * @param actual Fresh public recovery result.
 * @param expected Independent direct existing-queue query under the same state.
 */
inline void sameStatus(const m::QueueRangeStatus &actual, const m::QueueRangeStatus &expected) {
    CHECK(actual.epoch.matches(expected.epoch));
    CHECK(actual.authoritativeRevision == expected.authoritativeRevision);
    CHECK(actual.clientDesiredRevision == expected.clientDesiredRevision);
    CHECK(actual.mappingDesiredRevision == expected.mappingDesiredRevision);
    CHECK(actual.requestedFrom == expected.requestedFrom);
    CHECK(actual.mappingCurrent == expected.mappingCurrent);
    CHECK(actual.acceptedUnstagedData == expected.acceptedUnstagedData);
    CHECK(actual.paused == expected.paused);
    CHECK(actual.resetPending == expected.resetPending);
    CHECK(actual.hadGap == expected.hadGap);
    CHECK(actual.queuedForKey == expected.queuedForKey);
    CHECK(actual.executingForKey == expected.executingForKey);
    CHECK(actual.completeDeliveredRange == expected.completeDeliveredRange);
    REQUIRE(actual.source.has_value() == expected.source.has_value());

    if (actual.source) {
        const auto &a = *actual.source;
        const auto &e = *expected.source;
        CHECK(a.requestedFrom == e.requestedFrom);
        CHECK(a.revision == e.revision);
        CHECK(a.observationSerial == e.observationSerial);
        CHECK(a.partitionActivation == e.partitionActivation);
        CHECK(a.sourceFrontier == e.sourceFrontier);
        CHECK(a.knownFrontier == e.knownFrontier);
        CHECK(a.snipFrontier == e.snipFrontier);
        CHECK(a.snapshot == e.snapshot);
        CHECK(a.sourceTransactionPending == e.sourceTransactionPending);
        CHECK(a.initialPending == e.initialPending);
        CHECK(a.viewTransactionDirty == e.viewTransactionDirty);
        CHECK(a.completeRequestedRange == e.completeRequestedRange);
    }
}
} // namespace org::ttldtor::qdcxx::test::history_status
