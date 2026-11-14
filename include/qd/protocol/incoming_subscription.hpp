// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/subscription_writer.hpp"

#include <expected>
#include <string>

namespace org::ttldtor::qdcxx::protocol {

/** Explicit record ID mapping for one peer-to-local direction. */
struct IncomingSubscriptionRecord {
    std::int32_t senderId;                     ///< Strictly ascending nonnegative peer record ID.
    std::optional<std::int32_t> localRecordId; ///< Local position, or absent for a described unknown record.
};

/** Finite logical limits, with zero providing no allowance. */
struct IncomingSubscriptionLimits {
    std::size_t payloadBytes;     ///< Maximum complete subscription payload bytes.
    std::size_t schemaRecords;    ///< Maximum local schema record count; declarations are borrowed.
    std::size_t senderRecords;    ///< Maximum number of incoming mapping rows.
    std::size_t entries;          ///< Maximum wire tuples, including duplicates and ignored records.
    std::size_t symbolUnits;      ///< Maximum decoded UTF-16 units per symbol, including ignored records.
    std::size_t totalSymbolUnits; ///< Maximum sum of decoded UTF-16 units of all wire tuples.
};

/** Caller-turn work credits, with no refunds after rejection. */
struct IncomingSubscriptionWork {
    std::size_t frames;     ///< Remaining decode attempts.
    std::size_t inputBytes; ///< Remaining complete payload bytes admitted for primitive examination.
    std::size_t visits;     ///< Remaining mapping validation, tuple and searched mapping row visits.
    std::size_t textUnits;  ///< Remaining logical UTF-16 units admitted for symbol reads and copies.
};

/** A complete incoming command for one known local record. */
struct IncomingSubscriptionEntry {
    std::int32_t senderId;                    ///< Original peer record ID.
    core::RecordContext context;              ///< Owning local provenance independent of schema declarations.
    std::u16string symbol;                    ///< Owned exact symbol, including a valid empty symbol.
    std::optional<std::int64_t> historyBound; ///< Raw signed64 value only for HISTORY_ADD.
};

/** Whole-frame publication in original wire order, retaining duplicates. */
struct IncomingSubscriptionBatch {
    SubscriptionMessage message;                    ///< Action and family from the outer message type.
    std::vector<IncomingSubscriptionEntry> entries; ///< Known tuples, independent of parser and all input lifetimes.
};

/** A rejected frame publishes no tuples, including previously decoded valid tuples. */
enum class IncomingSubscriptionError {
    INVALID_MESSAGE,   ///< Envelope has no subscription message identity.
    INVALID_MAPPING,   ///< Missing schema provenance or invalid, duplicate or unsorted mapping row.
    UNKNOWN_RECORD,    ///< Tuple record ID has no explicit incoming mapping row.
    INVALID_SYMBOL,    ///< Malformed symbol or reserved leading asterisk.
    INVALID_HISTORY,   ///< Known HISTORY record has no explicit indexed time layout.
    TRUNCATED_PAYLOAD, ///< Complete frame payload ends within a tuple.
    RESOURCE_LIMIT,    ///< Explicit logical bound exceeded.
    WORK_LIMIT,        ///< Caller-turn credit exceeded; previously debited work stays charged.
    ALLOCATION_FAILURE ///< Candidate allocation failed before publication.
};

/** Decode one complete framed subscription for the exact-selector profile.
 * Use FrameReader for partial input and pass only FRAME_READY results. Symbol compression is
 * message-local. Event flag prefixes are consumed but the message identity determines the action.
 * A sender ID with an explicitly unknown local target is consumed and counted without publication.
 * @param frame Complete owning frame borrowed for this call.
 * @param schema Validated immutable local schema borrowed for this call.
 * @param sortedRecords Complete explicit peer mapping borrowed for this call, sorted by sender ID.
 * @param limits Finite logical bounds; independently retained caller results are outside this decode budget.
 * @param work Caller-turn credits, updated on success and rejection.
 * @return Complete owning batch or the first error, with no partial batch, callbacks or mutations to the mapping.
 */
[[nodiscard]] std::expected<IncomingSubscriptionBatch, IncomingSubscriptionError>
decodeIncomingSubscription(const Frame &frame, const core::LocalSchema &schema,
                           std::span<const IncomingSubscriptionRecord> sortedRecords, IncomingSubscriptionLimits limits,
                           IncomingSubscriptionWork &work);

} // namespace org::ttldtor::qdcxx::protocol
