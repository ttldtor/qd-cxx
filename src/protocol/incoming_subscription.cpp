// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/incoming_subscription.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/symbol_reader.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {
namespace {
using Error = IncomingSubscriptionError;

/// One pass produces the symbol, another copies it to or from the repeat dictionary.
/// The extra pass covers selector examination and temporary text from a failed primitive.
constexpr std::size_t SYMBOL_UNIT_WORK = 3;
/// The largest fixed-width symbol form contains at most seven five-bit groups.
constexpr std::size_t MAX_PENTA_UNITS = 7;

/** Charge one bounded operation before performing it.
 * @param amount Units or visits needed for the operation.
 * @param credit Caller credit to debit, with no underflow or refund.
 */
void debit(std::size_t amount, std::size_t &credit) {
    if (amount > credit) {
        throw Error::WORK_LIMIT;
    }

    credit -= amount;
}

/** Resolve the action and family without relying on enum ordering.
 * @param frame Complete envelope borrowed only for the check.
 * @return One supported action and family, or throws INVALID_MESSAGE.
 */
SubscriptionMessage messageType(const Frame &frame) {
    if (frame.kind != FrameKind::MESSAGE || !frame.type) {
        throw Error::INVALID_MESSAGE;
    }

    switch (const auto message = static_cast<SubscriptionMessage>(*frame.type)) {
    case SubscriptionMessage::TICKER_ADD:
    case SubscriptionMessage::TICKER_REMOVE:
    case SubscriptionMessage::STREAM_ADD:
    case SubscriptionMessage::STREAM_REMOVE:
    case SubscriptionMessage::HISTORY_ADD:
    case SubscriptionMessage::HISTORY_REMOVE:
        return message;
    }

    throw Error::INVALID_MESSAGE;
}

/** Validate the caller map once, before reading any tuples.
 * @param schema Borrowed immutable declarations.
 * @param records Borrowed complete sender map.
 * @param work Remaining caller visits debited for all validated rows.
 */
void checkMapping(const core::LocalSchema &schema, std::span<const IncomingSubscriptionRecord> records,
                  IncomingSubscriptionWork &work) {
    if (!schema.identity().valid()) {
        throw Error::INVALID_MAPPING;
    }

    const auto localRecords = schema.records().size();
    std::int32_t previous = -1;

    for (const auto &row : records) {
        debit(1, work.visits);

        if (row.senderId <= previous ||
            (row.localRecordId &&
             (*row.localRecordId < 0 || static_cast<std::size_t>(*row.localRecordId) >= localRecords))) {
            throw Error::INVALID_MAPPING;
        }

        previous = row.senderId;
    }
}

/** Find an explicitly declared sender identity; the peer ID never allocates a slot.
 * @param records Borrowed validated map.
 * @param senderId Wire ID to resolve.
 * @param work Remaining caller visits debited for each inspected row.
 * @return Target row from the caller map, or throws UNKNOWN_RECORD.
 */
const IncomingSubscriptionRecord &lookup(std::span<const IncomingSubscriptionRecord> records, std::int32_t senderId,
                                         IncomingSubscriptionWork &work) {
    auto first = records.begin();
    auto last = records.end();

    while (first != last) {
        debit(1, work.visits);

        const auto middle = first + (last - first) / 2;

        if (middle->senderId == senderId) {
            return *middle;
        }

        if (middle->senderId < senderId) {
            first = middle + 1;
        } else {
            last = middle;
        }
    }

    throw Error::UNKNOWN_RECORD;
}

/** Map a symbol primitive failure after charging its conservative work bound.
 * @param status Terminal symbol primitive result.
 * @param workLimited Whether caller credits narrowed the logical symbol limit.
 * @return Corresponding decode error.
 */
Error symbolError(encoding::SymbolStatus status, bool workLimited) noexcept {
    switch (status) {
    case encoding::SymbolStatus::TRUNCATED_INPUT:
        return Error::TRUNCATED_PAYLOAD;
    case encoding::SymbolStatus::RESOURCE_LIMIT:
        return workLimited ? Error::WORK_LIMIT : Error::RESOURCE_LIMIT;
    case encoding::SymbolStatus::ALLOCATION_FAILURE:
        return Error::ALLOCATION_FAILURE;
    default:
        return Error::INVALID_SYMBOL;
    }
}
} // namespace

std::expected<IncomingSubscriptionBatch, IncomingSubscriptionError>
decodeIncomingSubscription(const Frame &frame, const core::LocalSchema &schema,
                           std::span<const IncomingSubscriptionRecord> sortedRecords, IncomingSubscriptionLimits limits,
                           IncomingSubscriptionWork &work) {
    try {
        debit(1, work.frames);

        const auto message = messageType(frame);

        if (frame.payload.size() > limits.payloadBytes || schema.records().size() > limits.schemaRecords ||
            sortedRecords.size() > limits.senderRecords) {
            throw Error::RESOURCE_LIMIT;
        }

        debit(frame.payload.size(), work.inputBytes);
        checkMapping(schema, sortedRecords, work);

        std::span<const std::byte> remaining(frame.payload);
        encoding::SymbolReader symbols({limits.payloadBytes, limits.symbolUnits, limits.symbolUnits}, 0);
        IncomingSubscriptionBatch batch{message, {}};
        const bool hasBound = message == SubscriptionMessage::HISTORY_ADD;
        const bool history = hasBound || message == SubscriptionMessage::HISTORY_REMOVE;
        auto unitsLeft = limits.totalSymbolUnits;
        auto entriesLeft = limits.entries;
        std::size_t previousUnits = 0;

        while (!remaining.empty()) {
            debit(1, work.visits);

            if (entriesLeft == 0) {
                throw Error::RESOURCE_LIMIT;
            }

            --entriesLeft;

            // UTF text emits at most one unit per encoded byte. Fixed Penta and repeat have separate bounds.
            const auto possibleUnits = std::max({remaining.size(), MAX_PENTA_UNITS, previousUnits});
            const auto resourceUnits = std::min({limits.symbolUnits, unitsLeft, possibleUnits});
            const auto workUnits = work.textUnits / SYMBOL_UNIT_WORK;
            const auto permittedUnits = std::min(resourceUnits, workUnits);
            auto symbol = symbols.readBounded(remaining, permittedUnits);

            if (!symbol.value) {
                debit(permittedUnits * SYMBOL_UNIT_WORK, work.textUnits);

                throw symbolError(symbol.status, workUnits < resourceUnits);
            }

            previousUnits = symbol.value->units.size();
            unitsLeft -= previousUnits;
            debit(previousUnits * SYMBOL_UNIT_WORK, work.textUnits);

            if (!symbol.value->units.empty() && symbol.value->units.front() == u'*') {
                throw Error::INVALID_SYMBOL;
            }

            const auto sender = encoding::readCompactInt(remaining.subspan(symbol.consumed));

            if (sender.status != encoding::Status::OK) {
                throw Error::TRUNCATED_PAYLOAD;
            }

            remaining = remaining.subspan(symbol.consumed + sender.consumed);

            const auto &record = lookup(sortedRecords, sender.value, work);
            std::optional<std::int64_t> historyBound;

            if (hasBound) {
                const auto raw = encoding::readCompactLong(remaining);

                if (raw.status != encoding::Status::OK) {
                    throw Error::TRUNCATED_PAYLOAD;
                }

                historyBound = raw.value;
                remaining = remaining.subspan(raw.consumed);
            }

            if (!record.localRecordId) {
                continue;
            }

            const auto &local = schema.records()[static_cast<std::size_t>(*record.localRecordId)];

            if (history && (!local.hasTime || !local.indexedTimeLayout)) {
                throw Error::INVALID_HISTORY;
            }

            batch.entries.push_back({sender.value, schema.recordContext(*record.localRecordId),
                                     std::move(symbol.value->units), historyBound});
        }

        return batch;
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}
} // namespace org::ttldtor::qdcxx::protocol
