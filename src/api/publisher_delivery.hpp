// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../mux/publisher_stream_batch.hpp"
#include "qd/api/publisher.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/mux/queue.hpp"
#include "qd/protocol/live_session.hpp"

#include <algorithm>
#include <array>
#include <bitset>

namespace org::ttldtor::qdcxx::api::detail {

/// Complete publisher DATA envelope allowance in bytes.
inline constexpr std::size_t MAX_FRAME_BYTES = 65536;
/// A DATA row occupies at least a symbol marker and a compact record ID.
inline constexpr std::size_t MINIMUM_WIRE_ROW_BYTES = 2;
/// Largest row window representable within the existing envelope allowance.
inline constexpr std::size_t MAX_STREAM_WINDOW_ROWS = MAX_FRAME_BYTES / MINIMUM_WIRE_ROW_BYTES;
/// Position of STREAM in the existing TICKER/STREAM/HISTORY delivery arrays.
inline constexpr std::size_t STREAM_FAMILY = 1;
/// Wire DATA identities in publisher family order.
inline constexpr std::array DATA_MESSAGES{protocol::LiveDataMessage::TICKER_DATA,
                                          protocol::LiveDataMessage::STREAM_DATA,
                                          protocol::LiveDataMessage::HISTORY_DATA};

/// One revocation bit per row in the retained delivery window.
using DeliveryMask = std::bitset<MAX_STREAM_WINDOW_ROWS>;

/** The worker retains one detached callback batch until every eligible row has completed writing. */
struct Delivery final : mux::detail::PublisherStreamConsumer {
    mux::OwnedMuxBatch batch;                    ///< Detached owner, independent of the completed callback.
    mux::detail::PublisherStreamBatch stream;    ///< Alternative compact owner for eligible STREAM callbacks.
    std::size_t rangeIndex = 0, rangeOffset = 0; ///< Position corresponding to cursor in a compact batch.
    std::size_t cursor = 0;                      ///< First row not fully written or suppressed.
    DeliveryMask suppressed;                     ///< Committed policy revocations within this batch.

    /** Retain the callback batch and pause further delivery until the worker releases it.
     * @param view Borrowed callback rows, retained before return.
     * @return PAUSE until the worker requests resume.
     */
    mux::BatchAction onBatch(const mux::BorrowedMuxBatch &view) override {
        clear();
        batch = view.retain();

        return mux::BatchAction::PAUSE;
    }

    mux::BatchAction onStreamRanges(mux::detail::PublisherStreamBatch selected) override {
        clear();
        stream = std::move(selected);

        return mux::BatchAction::PAUSE;
    }

    /** @return All rows in the held callback, including completed and suppressed positions. */
    [[nodiscard]] std::size_t size() const noexcept {
        return stream.size() ? stream.size() : batch.records().size();
    }

    /** Advance after a complete frame or a suppressed prefix; retain the entire callback owner. */
    void advanceTo(std::size_t end) noexcept {
        if (stream.size()) {
            for (auto left = end - cursor; left;) {
                const auto count = stream.range(rangeIndex).rows.size();
                const auto moved = std::min(left, count - rangeOffset);
                left -= moved;
                rangeOffset += moved;

                if (rangeOffset == count) {
                    ++rangeIndex;
                    rangeOffset = 0;
                }
            }
        }

        cursor = end;
    }

    /** Visit unsuppressed rows from the current position without committing a cursor move.
     * @param visit Receives the absolute position and a borrowed row; false ends this scan.
     */
    template <class Visit> void visitPending(Visit &&visit) const {
        if (!stream.size()) {
            const auto records = batch.records();

            for (auto i = cursor; i < records.size(); ++i) {
                if (!suppressed[i] && !visit(i, records[i]))
                    break;
            }

            return;
        }

        struct Row {
            const core::RecordContext &context;
            std::u16string_view symbol;
            std::span<const core::FieldValueView> typedValues;
            std::int32_t eventFlags;
            std::optional<std::int64_t> rawTime;
        };

        auto i = cursor;
        auto offset = rangeOffset;

        for (auto index = rangeIndex; i < stream.size(); ++index) {
            const auto range = stream.range(index);

            for (const auto &row : range.rows.subspan(offset)) {
                if (!suppressed[i] && !visit(i, Row{range.common.context, row.payload->symbol(), row.payload->fields(),
                                                    row.source.flags, range.common.rawTime}))
                    return;

                ++i;
            }

            offset = 0;
        }
    }

    /** Release retained rows and reset the cursor and revocation mask. */
    void clear() noexcept {
        batch = {};
        stream = {};
        rangeIndex = 0;
        rangeOffset = 0;
        cursor = 0;
        suppressed.reset();
    }
};

/** Apply the publisher policy to physical record coordinates without event projection.
 * @param policy Immutable family filter; absence permits every key, as before.
 * @param schema Publisher schema whose provenance identifies the context.
 * @param context Owning row or requested-key provenance, borrowed during the query.
 * @param symbol Exact original wire symbol, borrowed during the query.
 * @return Whether the existing filter admits this physical record and symbol.
 * @throws Error for the existing filter query failures; no caller state is changed.
 */
inline bool publisherRecordAllowed(const std::optional<FeedFilter> &policy, const core::LocalSchema &schema,
                                   const core::RecordContext &context, std::u16string_view symbol) {
    if (!policy)
        return true;

    const auto id = schema.recordId(context);

    return id && policy->acceptRecord(schema.records()[static_cast<std::size_t>(*id)].name, symbol);
}

/** Stage newly revoked held rows without changing their delivery or policy state.
 * @param delivery Retained batch with the current cursor and already suppressed row mask.
 * @param policy Proposed immutable replacement filter for this family.
 * @param schema Publisher schema whose provenance identifies the held contexts.
 * @param revoked Destination mask for additional revoked rows, including a staged policy candidate.
 * @throws Error for the existing filter query failures, before queue or policy commit.
 */
inline void collectDeliveryRevocations(const Delivery &delivery, const std::optional<FeedFilter> &policy,
                                       const core::LocalSchema &schema, DeliveryMask &revoked) {
    delivery.visitPending([&](std::size_t index, const auto &row) {
        if (!publisherRecordAllowed(policy, schema, row.context, row.symbol))
            revoked.set(index);

        return true;
    });
}

/** Front-loaded control output does not consume DATA queue reservations during negotiation. */
enum class OutputKind {
    PROTOCOL,    ///< Initial protocol capabilities.
    DESCRIPTION, ///< Local outgoing record definitions.
    HEARTBEAT,   ///< Periodic liveness frame.
    DATA         ///< Rows belonging to the retained family delivery.
};

/** Borrowed worker-owned output fields, used only during a serialized delivery operation. */
struct DeliveryOutputView {
    std::optional<protocol::LivePendingOutput> &pending; ///< Current owned frame, empty between writes.
    OutputKind &kind;                                    ///< Distinguishes DATA from independent control frames.
    std::size_t &family;                                 ///< Current DATA family array index.
    std::size_t &end;                                    ///< Batch cursor after the complete current frame.
    std::size_t &recordCount;                            ///< Unsuppressed rows represented by the current frame.
};

/** Compose only unsuppressed rows without advancing their delivery cursor.
 * @param delivery Detached batch and current committed revocations.
 * @param outputState Borrowed fields updated with the composed row count and ending cursor.
 * @param session Serialized protocol owner that retains the returned frame's backing.
 * @param family Existing publisher family index.
 * @param schema Immutable local schema matching the retained row contexts.
 * @param streamBytes Optional existing STREAM byte profile; absent preserves count-based output.
 * @param outgoing Caller-owned scratch views, valid only through composition.
 * @param work Finite protocol composition credits.
 * @return Owned unstarted frame, or the existing typed composition failure without cursor advancement.
 */
inline std::expected<protocol::LivePendingOutput, protocol::LiveError>
composeDelivery(Delivery &delivery, DeliveryOutputView outputState, protocol::LiveSession &session, std::size_t family,
                const core::LocalSchema &schema, const std::optional<PublisherStreamByteLimits> &streamBytes,
                std::span<protocol::LiveDataRecordView> outgoing, protocol::LiveWork &work) {
    std::size_t count = 0;
    bool valid = true;
    delivery.visitPending([&](std::size_t, const auto &record) {
        if (count == outgoing.size())
            return false;

        const auto id = schema.recordId(record.context);

        if (!id) {
            valid = false;

            return false;
        }

        const bool removed = (record.eventFlags & events::indexed_event_flags::REMOVE_EVENT) != 0;
        outgoing[count++] = {*id, record.symbol, removed ? std::span<const core::FieldValueView>{} : record.typedValues,
                             record.eventFlags, removed ? record.rawTime : std::nullopt};

        return true;
    });

    if (!valid)
        return std::unexpected(protocol::LiveError::INVALID_CONFIGURATION);

    const auto rememberRows = [&](std::size_t written) {
        outputState.recordCount = written;
        outputState.end = delivery.cursor + written;

        if (delivery.suppressed.any()) {
            outputState.end = delivery.cursor;

            while (written != 0) {
                if (!delivery.suppressed[outputState.end++])
                    --written;
            }
        }
    };

    if (family == STREAM_FAMILY && streamBytes) {
        std::size_t written = 0;
        auto output = session.publishStreamPrefix(outgoing.first(count), streamBytes->targetPayloadBytes,
                                                  streamBytes->maxFrameBytes, written, work);

        if (output)
            rememberRows(written);

        return output;
    }

    // Legacy count-based mode retries a shorter prefix when a frame exceeds its byte limit.
    while (count != 0) {
        auto output = session.publishData(DATA_MESSAGES[family], outgoing.first(count), work);

        if (output) {
            rememberRows(count);

            return output;
        }

        if (output.error() != protocol::LiveError::RESOURCE_LIMIT || count == 1)
            return output;

        count /= 2;
    }

    return std::unexpected(protocol::LiveError::INVALID_CONFIGURATION);
}

/** Check a pending DATA frame against newly revoked, previously unsuppressed rows.
 * @param outputState Borrowed current frame metadata.
 * @param delivery Batch whose current cursor begins the output interval.
 * @param family Family whose revocations are being applied.
 * @param revoked Newly forbidden batch row indexes.
 * @return Whether the DATA interval contains a newly revoked row.
 */
inline bool revokesOutput(DeliveryOutputView outputState, const Delivery &delivery, std::size_t family,
                          const DeliveryMask &revoked) noexcept {
    if (!outputState.pending || outputState.kind != OutputKind::DATA || outputState.family != family)
        return false;

    for (auto row = delivery.cursor; row < outputState.end; ++row) {
        if (revoked[row])
            return true;
    }

    return false;
}

/** Release only a consecutive suppressed prefix, resuming callbacks after the whole batch is done.
 * @param delivery Worker-owned retained rows and cursor.
 * @param handle Registration to resume once its retained batch empties.
 */
inline void releaseSuppressedPrefix(Delivery &delivery, mux::MuxHandle &handle) noexcept {
    const auto size = delivery.size();
    auto next = delivery.cursor;

    while (next < size && delivery.suppressed[next])
        ++next;

    delivery.advanceTo(next);

    if (size && delivery.cursor == size) {
        delivery.clear();
        (void)handle.requestResume();
    }
}

/** A revoked row in a partially written frame requires peer retirement before another write.
 * @param output Borrowed current frame metadata.
 * @param delivery Retained rows belonging to that output interval.
 * @param family Family whose revocations are being applied.
 * @param revoked Newly forbidden batch row indexes.
 * @return Whether the worker must retire the peer instead of applying revocation in place.
 */
inline bool revokesPartialOutput(DeliveryOutputView output, const Delivery &delivery, std::size_t family,
                                 const DeliveryMask &revoked) noexcept {
    return revokesOutput(output, delivery, family, revoked) &&
           output.pending->pendingBytes().size() != output.pending->bytes().size();
}

/** Discard an unstarted affected frame and suppress its revoked rows after the policy commits.
 * The worker must retire a peer with revoked partial output before reaching this step.
 * @param delivery Worker-owned retained rows, changed only after policy commit.
 * @param handle Registration resumed if all retained rows have been released.
 * @param output Borrowed frame metadata; control and unaffected frames are preserved.
 * @param family Family whose revocations are being applied.
 * @param revoked Newly forbidden batch row indexes.
 */
inline void applyDeliveryRevocation(Delivery &delivery, mux::MuxHandle &handle, DeliveryOutputView output,
                                    std::size_t family, const DeliveryMask &revoked) noexcept {
    if (!revoked.any())
        return;

    if (revokesOutput(output, delivery, family, revoked))
        output.pending.reset();

    delivery.suppressed |= revoked;
    releaseSuppressedPrefix(delivery, handle);
}

} // namespace org::ttldtor::qdcxx::api::detail
