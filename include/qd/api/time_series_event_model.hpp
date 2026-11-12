// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/indexed_event_model.hpp"
#include "qd/api/model_source_epoch.hpp"
#include "qd/api/time_series_subscription.hpp"

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::events {
struct Candle;
struct Greeks;
struct TheoPrice;
struct TimeAndSale;
struct Underlying;
} // namespace org::ttldtor::qdcxx::events

namespace org::ttldtor::qdcxx::api {

/** Refusal of a time-series view or mutation. Model state is preserved on failure. */
enum class TimeSeriesEventModelError {
    WRONG_KEY,             ///< Incoming symbol or source does not match the active range.
    RANGE_INACTIVE,        ///< The symbol is absent or fromTime is the disabled sentinel.
    RANGE_CHANGED,         ///< Input names an earlier or otherwise different range revision.
    RESOURCE_LIMIT,        ///< A finite input, symbol, pending, or retained allowance is exhausted.
    ALLOCATION_FAILURE,    ///< A required owning copy could not be allocated.
    REVISION_EXHAUSTED,    ///< A new range cannot receive an unused owner-local revision.
    SOURCE_EPOCH_REQUIRED, ///< Untagged input is disabled after an explicit source epoch starts.
    SOURCE_EPOCH_MISMATCH, ///< Input token does not name the active source epoch.
    SOURCE_INACTIVE        ///< The source is fenced or its cut is not current.
};

/** Detached cut of the selected time-series range.
 * @tparam Event Native TimeSeriesEvent business value.
 */
template <class Event> struct TimeSeriesEventSnapshot {
    std::uint64_t rangeRevision = 0;                        ///< Owner-local selection revision for this cut.
    std::optional<std::u16string> symbol;                   ///< Exact configured symbol, independently owned.
    std::int64_t fromTime = TIME_SERIES_FROM_TIME_DISABLED; ///< Inclusive business milliseconds or disabled sentinel.
    std::vector<Event> events; ///< Completed values in ascending business time, then full signed index order.
};

namespace detail {

template <class Event>
concept TimeSeriesRangeEvent =
    IndexedModelEvent<Event> && (std::same_as<Event, events::Candle> || std::same_as<Event, events::Greeks> ||
                                 std::same_as<Event, events::TheoPrice> || std::same_as<Event, events::TimeAndSale> ||
                                 std::same_as<Event, events::Underlying>);

/** Internal range mutation on the indexed owner, preserving its finite storage accounting. */
struct TimeSeriesEventModelStorage {
    /** Drop unfinished transactions and evict completed values below a new active lower bound.
     * @tparam Event Native TimeSeriesEvent business value.
     * @param model Private indexed owner of the range.
     * @param fromTime Inclusive signed business milliseconds.
     */
    template <TimeSeriesRangeEvent Event>
    static void updateRange(IndexedEventModel<Event> &model, std::int64_t fromTime) noexcept {
        for (auto entry = model.partitions_.begin(); entry != model.partitions_.end();) {
            auto &partition = entry->second;
            partition.pending.clear();
            partition.snapshot = IndexedEventModel<Event>::SnapshotProgress::NONE;
            std::erase_if(partition.active, [fromTime](const auto &value) {
                return value.second.getTime() < fromTime;
            });

            if (partition.active.empty()) {
                entry = model.partitions_.erase(entry);
            } else {
                ++entry;
            }
        }
    }
};

/** Preserve shared indexed model refusal categories at the time-series API boundary.
 * @param error Typed refusal from an indexed model operation.
 * @return Equivalent range-model refusal.
 */
[[nodiscard]] inline TimeSeriesEventModelError timeSeriesRangeError(IndexedEventModelError error) noexcept {
    switch (error) {
    case IndexedEventModelError::WRONG_KEY:
        return TimeSeriesEventModelError::WRONG_KEY;
    case IndexedEventModelError::RESOURCE_LIMIT:
        return TimeSeriesEventModelError::RESOURCE_LIMIT;
    case IndexedEventModelError::ALLOCATION_FAILURE:
        return TimeSeriesEventModelError::ALLOCATION_FAILURE;
    case IndexedEventModelError::SOURCE_EPOCH_REQUIRED:
        return TimeSeriesEventModelError::SOURCE_EPOCH_REQUIRED;
    case IndexedEventModelError::SOURCE_EPOCH_MISMATCH:
        return TimeSeriesEventModelError::SOURCE_EPOCH_MISMATCH;
    case IndexedEventModelError::SOURCE_INACTIVE:
        return TimeSeriesEventModelError::SOURCE_INACTIVE;
    case IndexedEventModelError::INVALID_OPTIONS:
        break;
    }

    return TimeSeriesEventModelError::RESOURCE_LIMIT;
}

} // namespace detail

/** Bounded model for one exact logical symbol and the DEFAULT indexed source.
 * Calls are serialized and nonreentrant. The caller owns subscription demand and forwards only
 * batches selected for this owner's current range revision. No Feed, listener or executor is retained.
 * Detached cuts remain usable after selection changes, moving, clearing or destruction.
 * @tparam Event Candle, Greeks, Underlying, TheoPrice or TimeAndSale.
 */
template <detail::TimeSeriesRangeEvent Event> class TimeSeriesEventModel final {
    static constexpr std::int32_t DEFAULT_SOURCE_ID = 0;

    IndexedEventModelLimits limits_;
    IndexedEventModel<Event> indexed_;
    std::optional<std::u16string> symbol_;
    std::int64_t fromTime_ = TIME_SERIES_FROM_TIME_DISABLED;
    std::uint64_t revision_ = 0;
    detail::ModelSourceFence source_;

    /** @return Whether the current selection admits event batches. */
    [[nodiscard]] bool active() const noexcept {
        return symbol_ && fromTime_ != TIME_SERIES_FROM_TIME_DISABLED;
    }

    /** @return Whether a distinct range revision can still be published. */
    [[nodiscard]] bool canAdvance() const noexcept {
        return revision_ != std::numeric_limits<std::uint64_t>::max();
    }

    public:
    /** Construct a model with no symbol and disabled demand.
     * @param limits Explicit finite allowances inherited from indexed transaction storage.
     * @throws std::bad_alloc Initial storage allocation fails.
     */
    explicit TimeSeriesEventModel(IndexedEventModelLimits limits = {}) : limits_(limits), indexed_(limits) {
    }

    TimeSeriesEventModel(const TimeSeriesEventModel &) = delete;
    TimeSeriesEventModel &operator=(const TimeSeriesEventModel &) = delete;

    /** Transfer state into a new owner. The source remains usable but inactive.
     * @param other Source, with no active concurrent or reentrant access.
     * @throws std::bad_alloc Initial storage allocation fails; the source is unchanged.
     */
    TimeSeriesEventModel(TimeSeriesEventModel &&other)
        : limits_(other.limits_), indexed_(std::move(other.indexed_)),
          symbol_(std::exchange(other.symbol_, std::nullopt)),
          fromTime_(std::exchange(other.fromTime_, TIME_SERIES_FROM_TIME_DISABLED)), revision_(other.revision_),
          source_(std::move(other.source_)) {
    }

    TimeSeriesEventModel &operator=(TimeSeriesEventModel &&) = delete;

    /** @return Borrowed exact configured symbol, valid until replacement, move, clear or destruction. */
    [[nodiscard]] std::optional<std::u16string_view> getSymbol() const noexcept {
        return symbol_ ? std::optional<std::u16string_view>(*symbol_) : std::nullopt;
    }

    /** @return Configured inclusive signed business milliseconds, or the disabled sentinel. */
    [[nodiscard]] std::int64_t getFromTime() const noexcept {
        return fromTime_;
    }

    /** @return Current selection revision, meaningful only for this owner. */
    [[nodiscard]] std::uint64_t getRangeRevision() const noexcept {
        return revision_;
    }

    /** @return Detached qualification of the selected range relative to the source. */
    [[nodiscard]] ModelSourceState getSourceState() const noexcept {
        return source_.state();
    }

    /** Begin a fresh source epoch and clear the data without changing the selected range.
     * @param ordinal Strictly increasing positive source ordinal known at the delivery boundary.
     * @return Token to capture with fresh deliveries, or a typed refusal.
     */
    [[nodiscard]] std::expected<ModelSourceEpoch, ModelSourceError> beginSourceEpoch(std::uint64_t ordinal) noexcept {
        auto epoch = source_.begin(ordinal);

        if (epoch) {
            indexed_.clear();
        }

        return epoch;
    }

    /** Fence matching delivery and retire incomplete transactions after a source gap.
     * @param epoch Token for the affected source epoch.
     * @param retention Whether the last completed range stays available as historical data.
     * @return Success or a typed refusal.
     */
    [[nodiscard]] std::expected<void, ModelSourceError> markSourceGap(const ModelSourceEpoch &epoch,
                                                                      ModelRetention retention) noexcept {
        const auto result = source_.stop(epoch, retention, false);

        if (result) {
            retireData(retention);
        }

        return result;
    }

    /** Qualify a fresh range after the caller observes its completed replay.
     * @param epoch Token for the fresh receiving source.
     * @param rangeRevision Selection revision whose completed replay was observed.
     * @return Success for the active selection with no unfinished transaction, or a typed refusal.
     */
    [[nodiscard]] std::expected<void, ModelSourceError> confirmSourceEpoch(const ModelSourceEpoch &epoch,
                                                                           std::uint64_t rangeRevision) noexcept {
        if (const auto error = source_.inputError(epoch)) {
            return std::unexpected(*error);
        }

        if (rangeRevision != revision_) {
            return std::unexpected(ModelSourceError::STALE_SELECTION);
        }

        return source_.confirm(epoch, !active() || indexed_.hasUnfinished());
    }

    /** Terminally fence matching delivery and retire incomplete transactions.
     * @param epoch Token for the affected source epoch.
     * @param retention Whether the last completed range stays available as historical data.
     * @return Success or a typed refusal. A closed model admits no later source epoch.
     */
    [[nodiscard]] std::expected<void, ModelSourceError> closeSource(const ModelSourceEpoch &epoch,
                                                                    ModelRetention retention) noexcept {
        const auto result = source_.stop(epoch, retention, true);

        if (result) {
            retireData(retention);
        }

        return result;
    }

    /** Consume a range batch with the token captured at its original delivery boundary.
     * @param epoch Token for the originating source activation.
     * @param rangeRevision Owner-local range revision captured with the batch.
     * @param input Borrowed owning events for the selected symbol and DEFAULT source.
     * @return Success or a typed refusal preserving visible and pending state.
     */
    [[nodiscard]] std::expected<void, TimeSeriesEventModelError>
    processSourceEvents(const ModelSourceEpoch &epoch, std::uint64_t rangeRevision, std::span<const Event> input) {
        if (const auto error = source_.inputError(epoch)) {
            return std::unexpected(*error == ModelSourceError::STALE_EPOCH
                                       ? TimeSeriesEventModelError::SOURCE_EPOCH_MISMATCH
                                       : TimeSeriesEventModelError::SOURCE_INACTIVE);
        }

        return applyEvents(rangeRevision, input);
    }

    /** @return Owning range and its source qualification, including provisional and historical cuts. */
    [[nodiscard]] std::expected<ModelSourceSnapshot<TimeSeriesEventSnapshot<Event>>, TimeSeriesEventModelError>
    getSourceSnapshot() const {
        auto snapshot = buildSnapshot();

        if (!snapshot) {
            return std::unexpected(snapshot.error());
        }

        return ModelSourceSnapshot<TimeSeriesEventSnapshot<Event>>{source_.state(), std::move(*snapshot)};
    }

    /** Change the exact configured symbol. A distinct identity clears all old events.
     * @param symbol Borrowed symbol copied before mutation, or null to remove the symbol.
     * @return Resulting range revision, unchanged for the same symbol, or a typed refusal.
     */
    [[nodiscard]] std::expected<std::uint64_t, TimeSeriesEventModelError>
    setSymbol(std::optional<std::u16string_view> symbol) {
        if (source_.closed()) {
            return std::unexpected(TimeSeriesEventModelError::SOURCE_INACTIVE);
        }

        if (symbol == getSymbol()) {
            return revision_;
        }

        if (symbol && symbol->size() > limits_.symbolUnits) {
            return std::unexpected(TimeSeriesEventModelError::RESOURCE_LIMIT);
        }

        if (!canAdvance()) {
            return std::unexpected(TimeSeriesEventModelError::REVISION_EXHAUSTED);
        }

        try {
            std::optional<std::u16string> next;

            if (symbol) {
                next.emplace(*symbol);
            }

            indexed_.clear();
            symbol_.swap(next);
            source_.invalidateSelection();

            return ++revision_;
        } catch (const std::length_error &) {
            return std::unexpected(TimeSeriesEventModelError::RESOURCE_LIMIT);
        } catch (const std::bad_alloc &) {
            return std::unexpected(TimeSeriesEventModelError::ALLOCATION_FAILURE);
        }
    }

    /** Change the local range and retire unfinished input from the previous range.
     * Completed overlapping events stay visible. Raising the lower bound evicts old values.
     * Widening does not restore evicted values without new input.
     * @param fromTime Inclusive signed business milliseconds; MAX disables and clears the current view.
     * @return Resulting range revision, unchanged for the same bound, or a typed refusal.
     */
    [[nodiscard]] std::expected<std::uint64_t, TimeSeriesEventModelError> setFromTime(std::int64_t fromTime) noexcept {
        if (source_.closed()) {
            return std::unexpected(TimeSeriesEventModelError::SOURCE_INACTIVE);
        }

        if (fromTime == fromTime_) {
            return revision_;
        }

        if (!canAdvance()) {
            return std::unexpected(TimeSeriesEventModelError::REVISION_EXHAUSTED);
        }

        if (fromTime == TIME_SERIES_FROM_TIME_DISABLED) {
            indexed_.clear();
        } else {
            detail::TimeSeriesEventModelStorage::updateRange(indexed_, fromTime);
        }

        fromTime_ = fromTime;
        source_.invalidateSelection();

        return ++revision_;
    }

    /** Consume a batch delivered for the current range.
     * Outside-range values keep transaction and snapshot boundary flags, but never occupy the retained view.
     * @param rangeRevision Selection revision originally associated with this batch, from this same owner.
     * @param input Borrowed owning events for the exact active symbol and DEFAULT source, in delivery order.
     * @return Success, including incomplete/no-op input, or a refusal preserving visible and pending state.
     */
    [[nodiscard]] std::expected<void, TimeSeriesEventModelError> processEvents(std::uint64_t rangeRevision,
                                                                               std::span<const Event> input) {
        if (!source_.untracked()) {
            return std::unexpected(TimeSeriesEventModelError::SOURCE_EPOCH_REQUIRED);
        }

        return applyEvents(rangeRevision, input);
    }

    private:
    /** Retire incomplete input and optionally drop the committed range.
     * @param retention Accepted historical cut policy.
     */
    void retireData(ModelRetention retention) noexcept {
        if (retention == ModelRetention::RETAIN_COMPLETED) {
            indexed_.discardUnfinished();
        } else {
            indexed_.clear();
        }
    }

    /** Apply already source-qualified input to the selected range.
     * @param rangeRevision The selection revision captured with the batch.
     * @param input Borrowed events for the current selection.
     * @return Atomic success or a typed refusal.
     */
    [[nodiscard]] std::expected<void, TimeSeriesEventModelError> applyEvents(std::uint64_t rangeRevision,
                                                                             std::span<const Event> input) {
        if (!active()) {
            return std::unexpected(TimeSeriesEventModelError::RANGE_INACTIVE);
        }

        if (rangeRevision != revision_) {
            return std::unexpected(TimeSeriesEventModelError::RANGE_CHANGED);
        }

        if (input.size() > limits_.inputEvents) {
            return std::unexpected(TimeSeriesEventModelError::RESOURCE_LIMIT);
        }

        try {
            const auto below = [this](const Event &event) {
                return event.getTime() < fromTime_ &&
                       (detail::indexedModelFlags(event) & events::indexed_event_flags::REMOVE_EVENT) == 0;
            };
            std::vector<Event> adjusted;

            if (std::ranges::any_of(input, below)) {
                adjusted.assign(input.begin(), input.end());

                for (auto &event : adjusted) {
                    if (below(event)) {
                        detail::indexedModelFlags(event) |= events::indexed_event_flags::REMOVE_EVENT;
                    }
                }

                input = adjusted;
            }

            auto applied = indexed_.processEvents(*symbol_, DEFAULT_SOURCE_ID, input);

            if (!applied) {
                return std::unexpected(detail::timeSeriesRangeError(applied.error()));
            }

            return {};
        } catch (const std::length_error &) {
            return std::unexpected(TimeSeriesEventModelError::RESOURCE_LIMIT);
        } catch (const std::bad_alloc &) {
            return std::unexpected(TimeSeriesEventModelError::ALLOCATION_FAILURE);
        }
    }

    /** @return An owning range cut independent of the source qualification policy, or a typed refusal. */
    [[nodiscard]] std::expected<TimeSeriesEventSnapshot<Event>, TimeSeriesEventModelError> buildSnapshot() const {
        try {
            TimeSeriesEventSnapshot<Event> snapshot{revision_, symbol_, fromTime_, {}};

            if (active()) {
                auto values = indexed_.getSnapshot(*symbol_, DEFAULT_SOURCE_ID);

                if (!values) {
                    return std::unexpected(detail::timeSeriesRangeError(values.error()));
                }

                snapshot.events = std::move(*values);
                std::ranges::sort(snapshot.events, {}, [](const Event &event) {
                    return std::pair{event.getTime(), event.getIndex()};
                });
            }

            return snapshot;
        } catch (const std::length_error &) {
            return std::unexpected(TimeSeriesEventModelError::RESOURCE_LIMIT);
        } catch (const std::bad_alloc &) {
            return std::unexpected(TimeSeriesEventModelError::ALLOCATION_FAILURE);
        }
    }

    public:
    /** @return An owning range cut with completed values, or a refusal for a tracked noncurrent cut. */
    [[nodiscard]] std::expected<TimeSeriesEventSnapshot<Event>, TimeSeriesEventModelError> getSnapshot() const {
        if (!source_.readable()) {
            return std::unexpected(TimeSeriesEventModelError::SOURCE_INACTIVE);
        }

        return buildSnapshot();
    }

    /** Reset to no symbol, disabled fromTime and no unfinished or retained events.
     * @return Resulting range revision, unchanged if already clear, or a typed refusal.
     */
    [[nodiscard]] std::expected<std::uint64_t, TimeSeriesEventModelError> clear() noexcept {
        if (!symbol_ && fromTime_ == TIME_SERIES_FROM_TIME_DISABLED) {
            return revision_;
        }

        if (!canAdvance()) {
            return std::unexpected(TimeSeriesEventModelError::REVISION_EXHAUSTED);
        }

        indexed_.clear();
        symbol_.reset();
        fromTime_ = TIME_SERIES_FROM_TIME_DISABLED;
        source_.invalidateSelection();

        return ++revision_;
    }
};

} // namespace org::ttldtor::qdcxx::api
