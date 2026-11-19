// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/publisher.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/mux/queue.hpp"
#include "qd/protocol/live_session.hpp"

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
struct Delivery final : mux::MuxConsumer {
    mux::OwnedMuxBatch batch; ///< Detached owner, independent of the completed callback.
    std::size_t cursor = 0;   ///< First row not fully written or suppressed.
    DeliveryMask suppressed;  ///< Committed policy revocations within this batch.

    /** Retain the callback batch and pause further delivery until the worker releases it.
     * @param view Borrowed callback rows, retained before return.
     * @return PAUSE until the worker requests resume.
     */
    mux::BatchAction onBatch(const mux::BorrowedMuxBatch &view) override {
        batch = view.retain();
        cursor = 0;
        suppressed.reset();

        return mux::BatchAction::PAUSE;
    }

    /** Release retained rows and reset the cursor and revocation mask. */
    void clear() noexcept {
        batch = {};
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
 * @param revoked Initially empty candidate mask; discarded by the caller on any failure.
 * @throws Error for the existing filter query failures, before queue or policy commit.
 */
inline void collectDeliveryRevocations(const Delivery &delivery, const std::optional<FeedFilter> &policy,
                                       const core::LocalSchema &schema, DeliveryMask &revoked) {
    const auto held = delivery.batch.records();

    for (auto row = delivery.cursor; row < held.size(); ++row) {
        if (!delivery.suppressed[row] && !publisherRecordAllowed(policy, schema, held[row].context, held[row].symbol))
            revoked.set(row);
    }
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
    const auto records = delivery.batch.records();
    std::size_t count = 0;

    for (auto i = delivery.cursor; i < records.size(); ++i) {
        if (delivery.suppressed[i])
            continue;

        if (count == outgoing.size())
            break;

        const auto &record = records[i];
        const auto id = schema.recordId(record.context);

        if (!id)
            return std::unexpected(protocol::LiveError::INVALID_CONFIGURATION);

        const bool removed = (record.eventFlags & events::indexed_event_flags::REMOVE_EVENT) != 0;
        outgoing[count++] = {*id, record.symbol, removed ? std::span<const core::FieldValueView>{} : record.typedValues,
                             record.eventFlags, removed ? record.rawTime : std::nullopt};
    }

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
    const auto records = delivery.batch.records();

    while (delivery.cursor < records.size() && delivery.suppressed[delivery.cursor])
        ++delivery.cursor;

    if (!records.empty() && delivery.cursor == records.size()) {
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
