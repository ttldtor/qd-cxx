// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/mux/demand.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::mux {
namespace {

void debit(std::size_t &credit, std::size_t amount) {
    if (credit < amount)
        throw DemandError::WORK_LIMIT;

    credit -= amount;
}

/** One fixed metadata and packed-symbol bank; views only refer to text in this bank. */
struct Bank {
    std::unique_ptr<core::SubscriptionRequest[]> entries;
    std::unique_ptr<char16_t[]> text;
    std::size_t size = 0;
    std::size_t units = 0;

    explicit Bank(DemandLimits limits)
        : entries(std::make_unique<core::SubscriptionRequest[]>(limits.entries)),
          text(std::make_unique_for_overwrite<char16_t[]>(limits.symbolUnits)) {
    }

    [[nodiscard]] std::span<const core::SubscriptionRequest> view() const noexcept {
        return {entries.get(), size};
    }

    void clear() noexcept {
        std::fill_n(entries.get(), size, core::SubscriptionRequest{});
        size = 0;
        units = 0;
    }

    const core::SubscriptionRequest &append(core::SubscriptionRequest request) noexcept {
        if (request.key.symbol.empty()) {
            request.key.symbol = u"";
        } else {
            const auto length = request.key.symbol.size();
            auto *destination = text.get() + units;
            std::copy(request.key.symbol.begin(), request.key.symbol.end(), destination);
            request.key.symbol = {destination, length};
            units += length;
        }

        auto &entry = entries[size++];
        entry = std::move(request);

        return entry;
    }
};

/** Search the complete serialized exact-key map with explicit per-comparison work. */
const core::SubscriptionRequest *find(std::span<const core::SubscriptionRequest> entries,
                                      const core::SubscriptionKey &key, DemandWork &work) {
    for (const auto &entry : entries) {
        debit(work.visits, 1);

        if (!entry.key.context.matches(key.context) || entry.key.symbol.size() != key.symbol.size())
            continue;

        debit(work.symbolUnits, key.symbol.size());

        if (entry.key.symbol == key.symbol)
            return &entry;
    }

    return nullptr;
}

void checkDesired(std::span<const core::SubscriptionRequest> desired, bool history, DemandLimits limits,
                  DemandWork &work) {
    if (desired.size() > limits.entries)
        throw DemandError::RESOURCE_LIMIT;

    debit(work.visits, desired.size());
    auto unitsLeft = limits.symbolUnits;

    for (std::size_t index = 0; index < desired.size(); ++index) {
        const auto &entry = desired[index];
        const auto &key = entry.key;

        if (!key.context.valid() || key.selector != core::SubscriptionSelectorKind::EXACT || !key.symbol.data() ||
            entry.historyBound.has_value() != history) {
            throw DemandError::INVALID_DESIRED;
        }

        if (!key.symbol.empty()) {
            debit(work.symbolUnits, 1);

            if (key.symbol.front() == u'*')
                throw DemandError::INVALID_DESIRED;
        }

        if (key.symbol.size() > unitsLeft)
            throw DemandError::RESOURCE_LIMIT;

        unitsLeft -= key.symbol.size();

        if (find(desired.first(index), key, work))
            throw DemandError::INVALID_DESIRED;
    }
}

} // namespace

struct DemandTracker::State {
    DemandLimits limits;
    bool history;
    std::array<Bank, 2> banks;
    std::size_t acknowledged = 0;
    std::uint64_t sequence = 0;
    std::optional<DemandChange> pending;

    State(DemandLimits limits, bool history) : limits(limits), history(history), banks{Bank(limits), Bank(limits)} {
    }
};

DemandTracker::DemandTracker(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {
}

DemandTracker::~DemandTracker() = default;
DemandTracker::DemandTracker(DemandTracker &&other) noexcept = default;
DemandTracker &DemandTracker::operator=(DemandTracker &&other) noexcept = default;

std::expected<DemandTracker, DemandError> DemandTracker::create(core::SubscriptionFamily family, DemandLimits limits) {
    using enum core::SubscriptionFamily;

    if (family != TICKER && family != STREAM && family != HISTORY)
        return std::unexpected(DemandError::INVALID_CONFIGURATION);

    const auto maximumBankBytes = (std::numeric_limits<std::size_t>::max() - sizeof(State)) / 2;

    if (limits.entries > maximumBankBytes / sizeof(core::SubscriptionRequest) ||
        limits.symbolUnits >
            (maximumBankBytes - limits.entries * sizeof(core::SubscriptionRequest)) / sizeof(char16_t)) {
        return std::unexpected(DemandError::RESOURCE_LIMIT);
    }

    try {
        return DemandTracker(std::make_unique<State>(limits, family == HISTORY));
    } catch (const std::bad_array_new_length &) {
        return std::unexpected(DemandError::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(DemandError::ALLOCATION_FAILURE);
    }
}

std::expected<std::optional<DemandChange>, DemandError>
DemandTracker::next(std::span<const core::SubscriptionRequest> desired, DemandWork &work) {
    try {
        debit(work.operations, 1);

        if (!state_)
            return std::unexpected(DemandError::INVALID_STATE);

        auto &state = *state_;

        if (state.pending)
            return state.pending;

        checkDesired(desired, state.history, state.limits, work);
        const auto &current = state.banks[state.acknowledged];
        auto &candidate = state.banks[1 - state.acknowledged];
        const core::SubscriptionRequest *removed = nullptr;
        const core::SubscriptionRequest *added = nullptr;

        for (const auto &entry : current.view()) {
            debit(work.visits, 1);
            const auto *target = find(desired, entry.key, work);

            if (!target || target->historyBound != entry.historyBound) {
                removed = &entry;
                added = target;
                break;
            }
        }

        if (!removed) {
            for (const auto &entry : desired) {
                debit(work.visits, 1);

                if (!find(current.view(), entry.key, work)) {
                    added = &entry;
                    break;
                }
            }
        }

        if (!removed && !added)
            return std::nullopt;

        if (state.sequence == std::numeric_limits<std::uint64_t>::max())
            return std::unexpected(DemandError::IDENTITY_EXHAUSTED);

        const auto count = current.size - (removed != nullptr) + (added != nullptr);
        const auto oldUnits = current.units - (removed ? removed->key.symbol.size() : 0);
        const auto newUnits = added ? added->key.symbol.size() : 0;

        if (count > state.limits.entries || newUnits > state.limits.symbolUnits - oldUnits)
            return std::unexpected(DemandError::RESOURCE_LIMIT);

        debit(work.visits, current.size);
        debit(work.visits, candidate.size);
        debit(work.visits, count);
        debit(work.symbolUnits, oldUnits + newUnits);

        candidate.clear();

        for (const auto &entry : current.view()) {
            if (&entry != removed)
                candidate.append(entry);
        }

        core::SubscriptionRequest request;

        if (added) {
            request = candidate.append(*added);
        } else {
            request = *removed;
            request.historyBound.reset();
        }

        request.projectionMask = core::NATIVE_DELIVERY_PROJECTION;
        state.pending.emplace(++state.sequence, added ? DemandAction::ADD : DemandAction::REMOVE, std::move(request));

        return state.pending;
    } catch (DemandError error) {
        return std::unexpected(error);
    }
}

std::expected<void, DemandError> DemandTracker::acknowledge(std::uint64_t sequence) noexcept {
    if (!state_)
        return std::unexpected(DemandError::INVALID_STATE);

    if (!state_->pending || state_->pending->sequence != sequence)
        return std::unexpected(DemandError::INVALID_SEQUENCE);

    state_->pending.reset();
    state_->acknowledged = 1 - state_->acknowledged;

    return {};
}

std::expected<void, DemandError> DemandTracker::reset(DemandWork &work) {
    try {
        debit(work.operations, 1);

        if (!state_)
            return std::unexpected(DemandError::INVALID_STATE);

        auto &state = *state_;
        debit(work.visits, state.banks[0].size);
        debit(work.visits, state.banks[1].size);
        state.pending.reset();

        for (auto &bank : state.banks)
            bank.clear();

        return {};
    } catch (DemandError error) {
        return std::unexpected(error);
    }
}

} // namespace org::ttldtor::qdcxx::mux
