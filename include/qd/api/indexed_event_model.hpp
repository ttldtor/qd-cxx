// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/event_flags.hpp"

#include <concepts>
#include <cstddef>
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

namespace org::ttldtor::qdcxx::api {

/** Finite model refusal. A failed operation publishes no changes. */
enum class IndexedEventModelError {
    WRONG_KEY,          ///< The input symbol or source differs from the requested partition.
    RESOURCE_LIMIT,     ///< A finite count or symbol limit would be exceeded.
    ALLOCATION_FAILURE, ///< A required owning copy could not be allocated.
    INVALID_OPTIONS     ///< A derived model operation received unsupported options.
};

/** Independent finite allowances. Zero disables the corresponding capacity. */
struct IndexedEventModelLimits {
    std::size_t partitions = 256;      ///< Maximum retained exact symbol/source pairs.
    std::size_t symbolUnits = 1024;    ///< Maximum UTF-16 units in one partition symbol.
    std::size_t inputEvents = 4096;    ///< Maximum events inspected in one call.
    std::size_t pendingEvents = 4096;  ///< Maximum unfinished events retained across all partitions.
    std::size_t retainedEvents = 8192; ///< Maximum visible index entries across all partitions.
};

/** Completed publication category for one symbol and source. */
enum class IndexedEventChangeKind {
    UPDATES,         ///< Completed ordinary transactions in their input order.
    SNAPSHOT,        ///< Replacement delimited by SNAPSHOT_END.
    SNIPPED_SNAPSHOT ///< Replacement delimited by SNAPSHOT_SNIP.
};

/** Detached output for a single partition, identified by the enclosing call.
 * @tparam Event Owning indexed business event type.
 */
template <class Event> struct IndexedEventChange {
    IndexedEventChangeKind kind; ///< Completed transaction group or replacement.
    std::vector<Event> events;   ///< Raw updates, or distinct nonremoved snapshot entries with cleared event flags.
};

namespace detail {

struct TimeSeriesEventModelStorage;

template <class Event>
concept IndexedModelFlatFlags = requires(Event &event) {
    { event.eventFlags } -> std::same_as<std::int32_t &>;
};

template <class Event>
concept IndexedModelFlatSymbol = requires(const Event &event) {
    { event.symbol } -> std::same_as<const std::u16string &>;
};

template <class Event>
concept IndexedModelOrderWrapper = requires(Event &event, const Event &view) {
    { event.order.eventFlags } -> std::same_as<std::int32_t &>;
    { view.order.symbol } -> std::same_as<const std::u16string &>;
};

template <class Event>
concept IndexedModelNullableSymbol = requires(const Event &event) {
    { event.eventSymbol.has_value() } noexcept -> std::same_as<bool>;
    { event.eventSymbol->getSymbol() } noexcept -> std::same_as<std::optional<std::u16string_view>>;
};

template <class Event>
concept IndexedModelFlatSource = requires(const Event &event) {
    { event.getSourceId() } noexcept -> std::same_as<std::int32_t>;
};

template <class Event>
concept IndexedModelEvent =
    std::copyable<Event> &&
    requires(const Event &event) {
        { event.getIndex() } noexcept -> std::same_as<std::int64_t>;
    } &&
    (IndexedModelFlatSource<Event> ||
     requires(const Event &event) {
         { event.getSource().id() } noexcept -> std::same_as<std::int32_t>;
     }) &&
    (IndexedModelOrderWrapper<Event> ||
     (IndexedModelFlatFlags<Event> && (IndexedModelFlatSymbol<Event> || IndexedModelNullableSymbol<Event>)));

/** Borrow the exact event symbol through the caller's use of the returned view.
 * @tparam Event Supported indexed business event.
 * @param event Owning input value.
 * @return Exact logical symbol, or null for an absent Candle symbol.
 */
template <IndexedModelEvent Event> std::optional<std::u16string_view> indexedModelSymbol(const Event &event) noexcept {
    if constexpr (IndexedModelFlatSymbol<Event>) {
        return std::u16string_view(event.symbol);
    } else if constexpr (IndexedModelOrderWrapper<Event>) {
        return std::u16string_view(event.order.symbol);
    } else {
        return event.eventSymbol ? event.eventSymbol->getSymbol() : std::nullopt;
    }
}

/** @tparam Event Supported indexed business event.
 * @param event Owning input value.
 * @return Exact signed source identity, independent of source names.
 */
template <IndexedModelEvent Event> std::int32_t indexedModelSource(const Event &event) noexcept {
    if constexpr (IndexedModelFlatSource<Event>) {
        return event.getSourceId();
    } else {
        return event.getSource().id();
    }
}

/** @tparam Event Supported indexed business event, optionally const-qualified.
 * @param event Value whose flags are borrowed.
 * @return Reference to the raw indexed flags, preserving the input const qualification.
 */
template <class Event> decltype(auto) indexedModelFlags(Event &event) noexcept {
    if constexpr (requires { event.eventFlags; }) {
        return (event.eventFlags);
    } else {
        return (event.order.eventFlags);
    }
}

} // namespace detail

/** Bounded owner of indexed values and unfinished transactions for one business event type.
 * Calls must be serialized and nonreentrant. No callback, executor, Feed, or borrowed event is retained.
 * Detached results remain usable after later calls, move, clear, or destruction.
 * @tparam Event Copyable owning indexed event with an exact logical symbol, signed source and index, and indexed flags.
 */
template <detail::IndexedModelEvent Event> class IndexedEventModel final {
    friend struct detail::TimeSeriesEventModelStorage;

    enum class SnapshotProgress { NONE, RECEIVING, COMPLETE, SNIPPED };
    using Key = std::pair<std::u16string, std::int32_t>;
    using Values = std::map<std::int64_t, Event>;
    using Changes = std::vector<IndexedEventChange<Event>>;

    struct Partition {
        Values active;
        std::vector<Event> pending;
        SnapshotProgress snapshot = SnapshotProgress::NONE;
    };

    IndexedEventModelLimits limits_;
    std::map<Key, Partition> partitions_;

    /** Apply one event to the scratch retained view.
     * @param values Scratch values, ordered by signed index.
     * @param event Source event, copied only for an upsert.
     */
    static void applyValue(Values &values, const Event &event) {
        if ((detail::indexedModelFlags(event) & events::indexed_event_flags::REMOVE_EVENT) != 0) {
            values.erase(event.getIndex());
        } else {
            auto copy = event;
            detail::indexedModelFlags(copy) = 0;
            values.insert_or_assign(event.getIndex(), std::move(copy));
        }
    }

    /** Replace the scratch view and publish surviving entries in first-insertion order.
     * @param partition Scratch partition with a finished snapshot and transaction.
     * @param changes Detached output accumulated by the call.
     */
    static void completeSnapshot(Partition &partition, Changes &changes) {
        partition.active.clear();
        std::map<std::int64_t, std::size_t> positions;

        for (std::size_t i = 0; i < partition.pending.size(); ++i) {
            const auto &event = partition.pending[i];
            const auto index = event.getIndex();

            if ((detail::indexedModelFlags(event) & events::indexed_event_flags::REMOVE_EVENT) != 0) {
                positions.erase(index);
            } else {
                positions.try_emplace(index, i);
            }

            applyValue(partition.active, event);
        }

        auto &change = changes.emplace_back(partition.snapshot == SnapshotProgress::SNIPPED
                                                ? IndexedEventChangeKind::SNIPPED_SNAPSHOT
                                                : IndexedEventChangeKind::SNAPSHOT,
                                            std::vector<Event>{});
        change.events.reserve(positions.size());

        for (std::size_t i = 0; i < partition.pending.size(); ++i) {
            const auto index = partition.pending[i].getIndex();
            const auto position = positions.find(index);

            if (position != positions.end() && position->second == i) {
                change.events.push_back(partition.active.at(index));
            }
        }

        partition.snapshot = SnapshotProgress::NONE;
    }

    /** Apply one completed ordinary transaction and append its raw events.
     * @param partition Scratch partition with a finished ordinary transaction.
     * @param changes Detached output accumulated by the call.
     */
    static void completeUpdates(Partition &partition, Changes &changes) {
        if (changes.empty() || changes.back().kind != IndexedEventChangeKind::UPDATES) {
            changes.emplace_back(IndexedEventChangeKind::UPDATES, std::vector<Event>{});
        }

        auto &out = changes.back().events;

        for (auto &event : partition.pending) {
            applyValue(partition.active, event);
            out.push_back(std::move(event));
        }
    }

    public:
    /** Create an empty pure-data model.
     * @param limits Retained per-owner allowances. Text charges count UTF-16 symbol units; event limits count objects.
     * @throws std::bad_alloc Empty container storage could not be allocated.
     */
    explicit IndexedEventModel(IndexedEventModelLimits limits = {}) : limits_(limits) {
    }

    IndexedEventModel(const IndexedEventModel &) = delete;
    IndexedEventModel &operator=(const IndexedEventModel &) = delete;

    /** Transfer model data and leave the source empty with its original limits.
     * @param other Source model; detached results remain independent of both models.
     * @throws std::bad_alloc Empty container storage could not be allocated; source remains unchanged.
     */
    IndexedEventModel(IndexedEventModel &&other) : IndexedEventModel(other.limits_) {
        partitions_.swap(other.partitions_);
    }

    /** Transfer model data and leave the source empty with its original limits.
     * @param other Source model, or this model for a no-op.
     * @return This model with the transferred data and limits.
     */
    IndexedEventModel &operator=(IndexedEventModel &&other) noexcept {
        if (this != &other) {
            clear();
            partitions_.swap(other.partitions_);
            limits_ = other.limits_;
        }

        return *this;
    }

    /** Process a borrowed batch for one exact logical symbol and source.
     * All complete changes are returned together after a successful call. Failure preserves active values and
     * unfinished state even when earlier transactions in this input were complete. Payload copies of custom types
     * must own their data; exceptions other than allocation/length failures propagate with the same guarantee.
     * @param symbol Exact UTF-16 logical symbol, including exchange/Candle spelling when applicable.
     * @param sourceId Exact signed source ID.
     * @param input Borrowed owning events in arrival order. Every event must have the supplied key.
     * @return Owning completed changes in order, possibly empty, or a typed refusal.
     */
    [[nodiscard]] std::expected<Changes, IndexedEventModelError>
    processEvents(std::u16string_view symbol, std::int32_t sourceId, std::span<const Event> input) {
        return processEventsChecked(symbol, sourceId, input,
                                    [](const Changes &) -> std::expected<void, IndexedEventModelError> {
                                        return {};
                                    });
    }

    private:
    friend class OrderBookModel;

    /** Prepare indexed data and dependent state before publishing either.
     * @tparam Prepare Internal callable returning success or a typed refusal without publishing state.
     * @param symbol Exact logical symbol.
     * @param sourceId Exact signed source ID.
     * @param input Borrowed events in arrival order.
     * @param prepare Stages dependent data while completed changes remain valid.
     * @return Owning completed changes, or a refusal leaving the indexed model unchanged.
     */
    template <class Prepare>
    [[nodiscard]] std::expected<Changes, IndexedEventModelError>
    processEventsChecked(std::u16string_view symbol, std::int32_t sourceId, std::span<const Event> input,
                         Prepare &&prepare) {
        using namespace events::indexed_event_flags;

        if (symbol.size() > limits_.symbolUnits || input.size() > limits_.inputEvents) {
            return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
        }

        for (const auto &event : input) {
            if (detail::indexedModelSymbol(event) != symbol || detail::indexedModelSource(event) != sourceId) {
                return std::unexpected(IndexedEventModelError::WRONG_KEY);
            }
        }

        if (input.empty()) {
            return Changes{};
        }

        try {
            Key key{std::u16string(symbol), sourceId};
            const auto current = partitions_.find(key);
            auto activeCapacity = limits_.retainedEvents;
            auto pendingCapacity = limits_.pendingEvents;

            for (auto entry = partitions_.begin(); entry != partitions_.end(); ++entry) {
                if (entry != current) {
                    activeCapacity -= entry->second.active.size();
                    pendingCapacity -= entry->second.pending.size();
                }
            }

            Partition next = current == partitions_.end() ? Partition{} : current->second;
            Changes changes;

            for (const auto &event : input) {
                const auto flags = detail::indexedModelFlags(event);

                if ((flags & SNAPSHOT_BEGIN) != 0) {
                    next.pending.clear();
                    next.snapshot = SnapshotProgress::RECEIVING;
                }

                if (next.snapshot == SnapshotProgress::RECEIVING && (flags & (SNAPSHOT_END | SNAPSHOT_SNIP)) != 0) {
                    next.snapshot =
                        (flags & SNAPSHOT_SNIP) != 0 ? SnapshotProgress::SNIPPED : SnapshotProgress::COMPLETE;
                }

                const auto unfinished = next.snapshot == SnapshotProgress::RECEIVING || (flags & TX_PENDING) != 0;

                if (unfinished && next.pending.size() == pendingCapacity) {
                    return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
                }

                next.pending.push_back(event);

                if (unfinished) {
                    continue;
                }

                if (next.snapshot == SnapshotProgress::NONE) {
                    completeUpdates(next, changes);
                } else {
                    completeSnapshot(next, changes);
                }

                next.pending.clear();

                if (next.active.size() > activeCapacity) {
                    return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
                }
            }

            if (!changes.empty()) {
                if (const auto prepared = prepare(changes); !prepared) {
                    return std::unexpected(prepared.error());
                }
            }

            if (next.active.empty() && next.pending.empty()) {
                if (current != partitions_.end()) {
                    partitions_.erase(current);
                }
            } else if (current == partitions_.end()) {
                if (partitions_.size() == limits_.partitions) {
                    return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
                }

                partitions_.try_emplace(std::move(key), std::move(next));
            } else {
                current->second.active.swap(next.active);
                current->second.pending.swap(next.pending);
                current->second.snapshot = next.snapshot;
            }

            return std::expected<Changes, IndexedEventModelError>(std::move(changes));
        } catch (const std::length_error &) {
            return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
        } catch (const std::bad_alloc &) {
            return std::unexpected(IndexedEventModelError::ALLOCATION_FAILURE);
        }
    }

    public:
    /** Copy the currently retained view, independent of pending values.
     * @param symbol Exact UTF-16 logical symbol.
     * @param sourceId Exact signed source ID.
     * @return Detached values in ascending signed index order with cleared event flags, or a typed refusal.
     */
    [[nodiscard]] std::expected<std::vector<Event>, IndexedEventModelError> getSnapshot(std::u16string_view symbol,
                                                                                        std::int32_t sourceId) const {
        if (symbol.size() > limits_.symbolUnits) {
            return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
        }

        try {
            std::vector<Event> snapshot;
            const auto current = partitions_.find(Key{std::u16string(symbol), sourceId});

            if (current != partitions_.end()) {
                snapshot.reserve(current->second.active.size());

                for (const auto &entry : current->second.active) {
                    snapshot.push_back(entry.second);
                }
            }

            return snapshot;
        } catch (const std::length_error &) {
            return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
        } catch (const std::bad_alloc &) {
            return std::unexpected(IndexedEventModelError::ALLOCATION_FAILURE);
        }
    }

    /** Copy committed values of one symbol across all sources in a single serialized operation.
     * @param symbol Exact UTF-16 logical symbol.
     * @return Detached values ordered by signed source, then signed index, or a typed refusal.
     */
    [[nodiscard]] std::expected<std::vector<Event>, IndexedEventModelError>
    getSnapshotForSymbol(std::u16string_view symbol) const {
        if (symbol.size() > limits_.symbolUnits) {
            return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
        }

        try {
            std::vector<Event> snapshot;
            const Key first{std::u16string(symbol), std::numeric_limits<std::int32_t>::min()};

            for (auto entry = partitions_.lower_bound(first);
                 entry != partitions_.end() && entry->first.first == symbol; ++entry) {
                for (const auto &value : entry->second.active) {
                    snapshot.push_back(value.second);
                }
            }

            return snapshot;
        } catch (const std::length_error &) {
            return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
        } catch (const std::bad_alloc &) {
            return std::unexpected(IndexedEventModelError::ALLOCATION_FAILURE);
        }
    }

    /** Discard all retained and unfinished values. Previously returned owning copies remain usable. */
    void clear() noexcept {
        partitions_.clear();
    }
};

} // namespace org::ttldtor::qdcxx::api
