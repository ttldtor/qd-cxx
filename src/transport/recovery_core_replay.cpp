// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_core_support.hpp"

#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

/** Compare one exact local key under funded metadata/text work.
 * @param left Stable owned-bank entry.
 * @param right Stable owned-bank entry.
 * @param work Actual remaining new-owner work.
 * @return Exact context/selector/symbol equality; HISTORY bound is a separate attribute.
 */
bool sameKey(const core::SubscriptionRequest &left, const core::SubscriptionRequest &right, RecoveryWork &work) {
    recoveryDebit(1, work.visits);

    if (!left.key.context.matches(right.key.context) || left.key.selector != right.key.selector)
        return false;

    recoveryDebit(recoverySum(left.key.symbol.size(), right.key.symbol.size()), work.textUnits);

    return left.key.symbol == right.key.symbol;
}

/** Search only the fixed initialized prefix of one bank.
 * @param bank Stable owned map.
 * @param request Exact key to find.
 * @param work Actual comparison work.
 * @return Matching bounded index, or no key.
 */
std::optional<std::size_t> find(const RecoveryReplayBank &bank, const core::SubscriptionRequest &request,
                                RecoveryWork &work) {
    for (std::size_t index = 0; index != bank.size; ++index) {
        if (sameKey(bank.entries[index], request, work))
            return index;
    }

    return std::nullopt;
}

/** Copy one entry into the already admitted fixed bank; no fallible phase remains here.
 * @param bank Stable destination arrays.
 * @param request Borrowed source, unchanged during this synchronous copy.
 */
void append(RecoveryReplayBank &bank, const core::SubscriptionRequest &request) noexcept {
    auto &entry = bank.entries[bank.size++];
    entry = request;
    const auto length = request.key.symbol.size();

    if (length == 0) {
        // Exact non-null empty and All remain valid without manufacturing a null empty view.
        entry.key.symbol = u"";
    } else {
        auto *destination = bank.symbols.get() + bank.units;
        std::copy(request.key.symbol.begin(), request.key.symbol.end(), destination);
        entry.key.symbol = {destination, length};
        bank.units += length;
    }
}

/** Fund a complete bank rewrite and old-reference disposal before changing any slot.
 * @param destination Previously initialized bounded owner.
 * @param entries Complete candidate count.
 * @param units Complete candidate exact text units.
 * @param limits Immutable per-bank capacities.
 * @param work Actual remaining copy/disposal credits.
 */
void prepareBank(RecoveryReplayBank &destination, std::size_t entries, std::size_t units, RecoveryReplayLimits limits,
                 RecoveryWork &work) {
    if (entries > limits.entries || units > limits.symbolUnits) {
        recoveryRefuse(RecoveryError::RESOURCE_LIMIT, "recovery-replay-shape");
    }

    const auto metadata = recoverySum(destination.size, entries);
    const auto bytes = recoverySum(recoveryProduct(metadata, sizeof(core::SubscriptionRequest)),
                                   recoveryProduct(units, sizeof(char16_t)));
    recoveryDebit(metadata, work.visits);
    recoveryDebit(units, work.textUnits);
    recoveryDebit(bytes, work.bytes);
    static_assert(std::is_nothrow_copy_assignable_v<core::SubscriptionRequest>);

    for (std::size_t index = 0; index != destination.size; ++index)
        destination.entries[index] = {};

    destination.size = 0;
    destination.units = 0;
}

/** Capture owner-applied demand without retaining its borrowed aggregate span across another action.
 * @param queue Stable serialized actual source.
 * @param replay Fixed banks and immutable limits.
 * @param work Actual caller copy/inspection credits.
 */
void snapshot(const mux::MuxQueue &queue, RecoveryReplay &replay, RecoveryWork &work) {
    auto &latest = replay.banks[0];
    const auto revision = queue.revision();

    if (latest.revision == revision && latest.size == queue.aggregateEntries().size())
        return;

    const auto source = queue.aggregateEntries();
    recoveryDebit(source.size(), work.visits);
    std::size_t units = 0;

    for (const auto &request : source)
        units = recoverySum(units, request.key.symbol.size());

    prepareBank(latest, source.size(), units, replay.limits, work);

    for (const auto &request : source)
        append(latest, request);

    latest.revision = revision;
}

/** Prepare the whole emitted-map candidate before composing its one changed whole frame.
 * @param replay Three actual fixed banks.
 * @param removed Optional old emitted index to remove.
 * @param added Optional latest desired entry to add.
 * @param work Actual candidate copy/cleanup credits.
 */
void candidate(RecoveryReplay &replay, std::optional<std::size_t> removed, const core::SubscriptionRequest *added,
               RecoveryWork &work) {
    const auto &emitted = replay.banks[1];
    auto count = emitted.size;
    auto units = emitted.units;

    if (removed) {
        --count;
        units -= emitted.entries[*removed].key.symbol.size();
    }

    if (added) {
        count = recoverySum(count, 1);
        units = recoverySum(units, added->key.symbol.size());
    }

    auto &next = replay.banks[2];
    prepareBank(next, count, units, replay.limits, work);

    for (std::size_t index = 0; index != emitted.size; ++index) {
        if (!removed || *removed != index)
            append(next, emitted.entries[index]);
    }

    if (added)
        append(next, *added);

    next.revision = replay.banks[0].revision;
}

} // namespace

RecoveryObservation RecoveryCore::replay(RecoveryTicket ticket, RecoveryTurnWork &work) {
    if (configuration_.options.requireAllContracts && !allContractsReady_) {
        return recoveryResult(ticket, RecoveryCompletion::READINESS_BLOCKED);
    }

    auto result = recoveryResult(ticket);

    for (std::size_t visited = 0; visited != 3; ++visited) {
        const auto family = family_;
        family_ = (family_ + 1) % 3;
        auto &queue = configuration_.queue(family);

        if (!queue)
            continue;

        auto &maps = configuration_.replay[family];
        snapshot(*queue, maps, work.recovery);
        const auto &latest = maps.banks[0];
        const auto &emitted = maps.banks[1];
        std::optional<std::size_t> removed;
        std::optional<std::size_t> replaced;
        const core::SubscriptionRequest *added = nullptr;

        for (std::size_t index = 0; index != emitted.size; ++index) {
            const auto target = find(latest, emitted.entries[index], work.recovery);

            if (!target) {
                removed = index;
                break;
            }

            if (latest.entries[*target].historyBound != emitted.entries[index].historyBound) {
                // HISTORY ADD replaces this key's lower bound; do not withdraw healthy overlapping views.
                replaced = index;
                added = &latest.entries[*target];
                break;
            }
        }

        if (!removed && !replaced) {
            for (std::size_t index = 0; index != latest.size; ++index) {
                if (!find(emitted, latest.entries[index], work.recovery)) {
                    added = &latest.entries[index];
                    break;
                }
            }
        }

        if (!removed && !added) {
            converged_[family] = true;
            continue;
        }

        converged_[family] = false;
        candidate(maps, removed ? removed : replaced, added, work.recovery);
        auto request = removed ? emitted.entries[*removed] : *added;

        if (removed)
            request.historyBound.reset();

        constexpr std::array adds{protocol::SubscriptionMessage::TICKER_ADD, protocol::SubscriptionMessage::STREAM_ADD,
                                  protocol::SubscriptionMessage::HISTORY_ADD};
        constexpr std::array removes{protocol::SubscriptionMessage::TICKER_REMOVE,
                                     protocol::SubscriptionMessage::STREAM_REMOVE,
                                     protocol::SubscriptionMessage::HISTORY_REMOVE};
        // A single-entry chunk is a whole frame. An individually unrepresentable legal entry fails terminally.
        const auto admitted = backend_.subscribe(removed ? removes[family] : adds[family], {&request, 1}, work.session);

        if (cutoff_)
            return recoveryResult(ticket, RecoveryCompletion::DEFERRED);

        if (!admitted) {
            if (recoveryOutputBlocked(admitted.error()))
                return recoveryResult(ticket, RecoveryCompletion::OUTPUT_BLOCKED);

            if (admitted.error().transport.protocolError == protocol::LiveError::NOT_READY ||
                admitted.error().transport.protocolError == protocol::LiveError::AUTHENTICATION_REQUIRED) {
                return recoveryResult(ticket, RecoveryCompletion::READINESS_BLOCKED);
            }

            if (admitted.error().error == RecoveryError::WORK_LIMIT)
                return recoveryResult(ticket, RecoveryCompletion::DEFERRED);

            record(admitted.error());
            result.completion = RecoveryCompletion::TERMINAL_FAILURE;
            result.cause = RecoveryCause::RESOURCE;

            return result;
        }

        std::swap(maps.banks[1], maps.banks[2]);
        result.outputAdmitted = true;

        return result;
    }

    return result;
}

} // namespace org::ttldtor::qdcxx::transport::detail
