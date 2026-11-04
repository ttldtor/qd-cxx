// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/ipf/instrument_profile_collector.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <limits>
#include <map>
#include <mutex>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::ipf {
namespace collector_detail {
/** Allocation-free inspection of existing value backing for collector admission. */
struct CollectorProfileAccess {
    /** Read retained UTF-16 units without converting numeric fields.
     * @param profile Stable source value.
     * @return Combined actual retained text/name lengths.
     */
    static std::size_t units(const InstrumentProfile &profile) noexcept {
        return profile.totalUnits_;
    }

    /** Read nonempty custom-field count without allocating a name list.
     * @param profile Stable source value.
     * @return Number of custom map entries.
     */
    static std::size_t fields(const InstrumentProfile &profile) noexcept {
        return profile.custom_.size();
    }
};
} // namespace collector_detail

namespace {
/** Admission interruption, kept distinct from allocator exceptions. */
struct Admission {
    ProfileCollectorError error;
};

/** Finite retained counters; revisions/batches own this ledger, never the collector State. */
struct Ledger {
    enum Counter : std::size_t { REVISIONS, UNITS, BATCHES, REFERENCES, COUNT };

    std::array<std::atomic<std::size_t>, COUNT> used{};
    std::array<std::size_t, COUNT> caps;

    /** Set immutable caps.
     * @param limits Explicit collector allowances.
     */
    explicit Ledger(const ProfileCollectorLimits &limits)
        : caps{limits.liveRevisions, limits.retainedUnits, limits.liveBatches, limits.batchReferences} {
    }

    /** Reserve without overflow or partial publication.
     * @param counter Counter being reserved.
     * @param amount Requested units/count.
     */
    void take(std::size_t counter, std::size_t amount) {
        auto value = used[counter].load(std::memory_order_relaxed);

        do {
            if (amount > caps[counter] || value > caps[counter] - amount) {
                throw Admission{ProfileCollectorError::LIMIT_EXCEEDED};
            }
        } while (!used[counter].compare_exchange_weak(value, value + amount, std::memory_order_relaxed));
    }
};

/** Reservation releases after its owned profile/vector backing has been destroyed. */
struct Reservation {
    std::shared_ptr<Ledger> ledger;
    std::array<std::size_t, Ledger::COUNT> amounts{};

    /** Reserve a pair of independent allowances with rollback.
     * @param owner Independent retained ledger.
     * @param first First counter.
     * @param firstAmount First requested count.
     * @param second Second counter.
     * @param secondAmount Second requested count.
     */
    Reservation(std::shared_ptr<Ledger> owner, std::size_t first, std::size_t firstAmount, std::size_t second,
                std::size_t secondAmount)
        : ledger(std::move(owner)) {
        ledger->take(first, firstAmount);
        amounts[first] = firstAmount;

        try {
            ledger->take(second, secondAmount);
            amounts[second] = secondAmount;
        } catch (...) {
            ledger->used[first].fetch_sub(firstAmount, std::memory_order_relaxed);
            throw;
        }
    }

    Reservation(const Reservation &) = delete;
    Reservation &operator=(const Reservation &) = delete;

    /** Transfer an already acquired stack reservation without allocation.
     * @param other Reservation whose counters move to this owner.
     */
    Reservation(Reservation &&other) noexcept : ledger(std::move(other.ledger)), amounts(other.amounts) {
        other.amounts.fill(0);
    }

    ~Reservation() {
        if (!ledger) {
            return;
        }

        for (std::size_t i = 0; i < amounts.size(); ++i) {
            ledger->used[i].fetch_sub(amounts[i], std::memory_order_relaxed);
        }
    }
};

/** Read wall time at the owning boundary.
 * @return Milliseconds since the system-clock epoch.
 */
std::int64_t wallMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/** Debit conservative finite algorithm work.
 * @param remaining Remaining per-call work allowance.
 * @param amount Required comparison/copy/index units.
 */
void spend(std::size_t &remaining, std::size_t amount) {
    if (amount > remaining) {
        throw Admission{ProfileCollectorError::LIMIT_EXCEEDED};
    }

    remaining -= amount;
}
} // namespace

ProfileRevision::ProfileRevision(const InstrumentProfile &value, std::uint64_t identity,
                                 std::shared_ptr<void> retention)
    : retention_(std::move(retention)), profile(value), revision(identity) {
}

ProfileBatch::ProfileBatch(std::vector<std::shared_ptr<const ProfileRevision>> profiles,
                           std::shared_ptr<void> retention)
    : retention_(std::move(retention)), profiles_(std::move(profiles)) {
}

std::span<const std::shared_ptr<const ProfileRevision>> ProfileBatch::profiles() const noexcept {
    return profiles_;
}

namespace collector_detail {
/** Stable state: serialized publication, request-only close and one finite outside-lock dispatcher. */
struct ProfileCollectorState {
    using Revision = std::shared_ptr<const ProfileRevision>;

    /** Current immutable value and independently replaceable generation. */
    struct Entry {
        Revision revision;
        std::uint64_t generation;
    };

    using Model = std::map<std::u16string_view, Entry, std::less<>>;

    /** Owning registration; removal is serialized with callback selection. */
    struct Listener {
        std::uint64_t token;
        InstrumentProfileCollector::Listener callback;
        bool removed = false;
    };

    /** One admitted callback selection with private, not-yet-published batch. */
    struct Task {
        std::shared_ptr<Listener> listener;
        std::shared_ptr<ProfileBatch> batch;
    };

    const ProfileCollectorLimits limits;
    std::shared_ptr<Ledger> ledger;
    mutable std::mutex mutex;
    Model model;
    std::vector<std::shared_ptr<Listener>> listeners;
    std::vector<Task> tasks;
    std::size_t taskHead = 0;
    bool closed = false;
    bool dispatching = false;
    std::size_t cycleReserved = 0;
    std::uint64_t revision = 0;
    std::uint64_t listenerIdentity = 0;
    std::uint64_t failures = 0;
    std::int64_t updated = wallMillis();
    std::int64_t reported = 0;

    /** Initialize finite state without SDK/transport dependencies.
     * @param value Explicit per-instance caps.
     */
    explicit ProfileCollectorState(ProfileCollectorLimits value)
        : limits(value), ledger(std::make_shared<Ledger>(value)) {
    }

    /** Copy one admitted revision; source is never retained.
     * @param source Stable borrowed profile.
     * @param identity Proposed monotonic identity.
     * @return Owning immutable revision with an independent ledger lease.
     */
    Revision copy(const InstrumentProfile &source, std::uint64_t identity) {
        if (CollectorProfileAccess::fields(source) > limits.customFields) {
            throw Admission{ProfileCollectorError::LIMIT_EXCEEDED};
        }

        auto reservation = std::make_shared<Reservation>(
            Reservation(ledger, Ledger::REVISIONS, 1, Ledger::UNITS, CollectorProfileAccess::units(source)));

        return Revision(new ProfileRevision(source, identity, std::move(reservation)));
    }

    /** Reserve counters before any batch vector/control-block allocation or copy.
     * @param source Stable staged revision references, borrowed under the state mutex.
     * @return Admitted mutable-private batch, immutable once delivered.
     */
    std::shared_ptr<ProfileBatch> batch(std::span<const Revision> source) {
        auto reservation =
            std::make_shared<Reservation>(Reservation(ledger, Ledger::BATCHES, 1, Ledger::REFERENCES, source.size()));
        std::vector<Revision> storage(source.begin(), source.end());

        return std::shared_ptr<ProfileBatch>(new ProfileBatch(std::move(storage), std::move(reservation)));
    }

    /** Reserve counters before gathering any current-model references.
     * @param source Current model, borrowed under the state mutex.
     * @return Admitted mutable-private snapshot batch.
     */
    std::shared_ptr<ProfileBatch> batch(const Model &source) {
        auto reservation =
            std::make_shared<Reservation>(Reservation(ledger, Ledger::BATCHES, 1, Ledger::REFERENCES, source.size()));
        auto storage = values(source);

        return std::shared_ptr<ProfileBatch>(new ProfileBatch(std::move(storage), std::move(reservation)));
    }

    /** Capture references to the current model under its mutex.
     * @param source Model whose keys borrow only its own retained revisions.
     * @return Owning reference vector, bounded by modelProfiles.
     */
    static std::vector<Revision> values(const Model &source) {
        std::vector<Revision> result;
        result.reserve(source.size());

        for (const auto &[symbol, entry] : source) {
            result.push_back(entry.revision);
        }

        return result;
    }

    /** Admit a task count into the current finite dispatch cycle.
     * @param amount Number of new callback tasks.
     */
    void checkTasks(std::size_t amount) const {
        const auto already = dispatching ? cycleReserved : 0;

        if (amount > limits.deliveryCalls || already > limits.deliveryCalls - amount) {
            throw Admission{ProfileCollectorError::LIMIT_EXCEEDED};
        }
    }

    /** Drain only admitted work; all callback invocation/capture destruction is outside locks. */
    void dispatch() noexcept {
        for (;;) {
            Task task;

            {
                std::lock_guard lock(mutex);

                if (taskHead == tasks.size()) {
                    tasks.clear();
                    taskHead = 0;
                    dispatching = false;
                    cycleReserved = 0;

                    return;
                }

                task = std::move(tasks[taskHead]);
                ++taskHead;

                if (!closed && !task.listener->removed) {
                    auto &profiles = task.batch->profiles_;
                    std::erase_if(profiles, [](const Revision &value) {
                        return value->superseded_;
                    });
                } else {
                    task.batch->profiles_.clear();
                }
            }

            if (!task.batch->profiles_.empty()) {
                try {
                    task.listener->callback(std::move(task.batch));
                } catch (...) {
                    std::lock_guard lock(mutex);

                    if (failures != std::numeric_limits<std::uint64_t>::max()) {
                        ++failures;
                    }
                }
            }
        }
    }

    /** Apply updates or selected generation removals atomically.
     * @param input Stable caller-owned profile span.
     * @param generation Replacement generation, zero meaning none.
     * @param tags Stable generation tags when removing.
     * @param removing Select generation-removal mode.
     * @return Published counts, or an unchanged-state admission failure.
     */
    std::expected<ProfileCollectorChange, ProfileCollectorError> apply(std::span<const InstrumentProfile> input,
                                                                       std::uint64_t generation,
                                                                       std::span<const std::uint64_t> tags,
                                                                       bool removing) {
        Model staged;
        std::vector<Revision> changed;
        std::vector<Revision> superseded;
        std::vector<Task> stagedTasks;
        bool run = false;
        ProfileCollectorChange result;

        try {
            std::unique_lock lock(mutex);

            if (closed) {
                return std::unexpected(ProfileCollectorError::CLOSED);
            }

            if ((!removing && input.empty()) || (removing && tags.empty())) {
                return ProfileCollectorChange{revision, 0};
            }

            auto work = limits.workUnits;
            const auto count = removing ? model.size() : input.size();

            if (count > limits.stagingProfiles || tags.size() > limits.stagingProfiles) {
                throw Admission{ProfileCollectorError::LIMIT_EXCEEDED};
            }

            spend(work, model.size());
            spend(work, count);
            staged = model;
            changed.reserve(count);
            superseded.reserve(count);

            const auto append = [&](const InstrumentProfile &profile, std::uint64_t tag) {
                spend(work, CollectorProfileAccess::units(profile));
                spend(work, CollectorProfileAccess::fields(profile));
                // Linear bound also covers the logarithmic index and exact symbol comparisons.
                spend(work, profile.getSymbol().size());
                spend(work, staged.size());

                if (staged.size() != 0 && profile.getSymbol().size() > work / staged.size()) {
                    throw Admission{ProfileCollectorError::LIMIT_EXCEEDED};
                }

                spend(work, staged.size() * profile.getSymbol().size());

                const auto old = staged.find(profile.getSymbol());

                if (old != staged.end()) {
                    spend(work, CollectorProfileAccess::units(old->second.revision->profile));
                }

                if (old != staged.end() && old->second.revision->profile == profile) {
                    old->second.generation = tag;

                    return;
                }

                const bool removed = profile.getType() == u"REMOVED";

                if (removed && old == staged.end()) {
                    return;
                }

                if (changed.size() >= std::numeric_limits<std::uint64_t>::max() - revision) {
                    throw Admission{ProfileCollectorError::IDENTITY_EXHAUSTED};
                }

                auto value = copy(profile, revision + changed.size() + 1);

                if (old != staged.end()) {
                    superseded.push_back(old->second.revision);
                    staged.erase(old);
                }

                if (!removed) {
                    if (staged.size() >= limits.modelProfiles) {
                        throw Admission{ProfileCollectorError::LIMIT_EXCEEDED};
                    }

                    staged.emplace(value->profile.getSymbol(), Entry{value, tag});
                }

                changed.push_back(std::move(value));
            };

            if (removing) {
                for (const auto &[symbol, entry] : model) {
                    spend(work, tags.size());

                    if (entry.generation != 0 && std::find(tags.begin(), tags.end(), entry.generation) != tags.end()) {
                        if (symbol.size() > std::numeric_limits<std::size_t>::max() - 7) {
                            throw Admission{ProfileCollectorError::LIMIT_EXCEEDED};
                        }

                        InstrumentProfile removed(
                            ProfileLimits{0, 6, std::max<std::size_t>(7, symbol.size()), symbol.size() + 7});
                        (void)removed.setSymbol(symbol);
                        (void)removed.setType(u"REMOVED");
                        append(removed, 0);
                    }
                }
            } else {
                for (const auto &profile : input) {
                    append(profile, generation);
                }
            }

            if (!changed.empty()) {
                if (reported > std::numeric_limits<std::int64_t>::max() - 1000) {
                    throw Admission{ProfileCollectorError::IDENTITY_EXHAUSTED};
                }

                checkTasks(listeners.size());
                spend(work, tasks.size());
                stagedTasks.reserve(tasks.size() + listeners.size());
                stagedTasks.insert(stagedTasks.end(), tasks.begin(), tasks.end());

                for (const auto &listener : listeners) {
                    spend(work, changed.size());
                    stagedTasks.push_back(Task{listener, batch(changed)});
                }
            }

            model.swap(staged);

            for (const auto &old : superseded) {
                old->superseded_ = true;
            }

            if (!changed.empty()) {
                revision += changed.size();
                updated = std::max(wallMillis(), reported + 1000);
                tasks.swap(stagedTasks);
                cycleReserved = (dispatching ? cycleReserved : 0) + listeners.size();

                if (!dispatching && !tasks.empty()) {
                    dispatching = true;
                    run = true;
                }
            }

            result = {revision, changed.size()};
        } catch (const Admission &failure) {
            return std::unexpected(failure.error);
        }

        if (run) {
            dispatch();
        }

        return result;
    }
};
} // namespace collector_detail

InstrumentProfileCollector::InstrumentProfileCollector(std::shared_ptr<collector_detail::ProfileCollectorState> state)
    : state_(std::move(state)) {
}

std::unique_ptr<InstrumentProfileCollector> InstrumentProfileCollector::create(ProfileCollectorLimits limits) {
    auto state = std::make_shared<collector_detail::ProfileCollectorState>(limits);

    return std::unique_ptr<InstrumentProfileCollector>(new InstrumentProfileCollector(std::move(state)));
}

InstrumentProfileCollector::~InstrumentProfileCollector() {
    close();
}

std::expected<ProfileCollectorChange, ProfileCollectorError>
InstrumentProfileCollector::updateInstrumentProfiles(std::span<const InstrumentProfile> profiles,
                                                     std::uint64_t generation) {
    auto state = state_;

    return state->apply(profiles, generation, {}, false);
}

std::expected<ProfileCollectorChange, ProfileCollectorError>
InstrumentProfileCollector::removeGenerations(std::span<const std::uint64_t> generations) {
    auto state = state_;

    return state->apply({}, 0, generations, true);
}

std::expected<std::uint64_t, ProfileCollectorError> InstrumentProfileCollector::addUpdateListener(Listener callback) {
    auto state = state_;

    if (!callback) {
        return std::unexpected(ProfileCollectorError::INVALID_LISTENER);
    }

    auto listener = std::make_shared<collector_detail::ProfileCollectorState::Listener>();
    listener->callback = std::move(callback);
    std::vector<collector_detail::ProfileCollectorState::Task> stagedTasks;
    bool run = false;
    std::uint64_t token = 0;

    try {
        std::unique_lock lock(state->mutex);

        if (state->closed) {
            return std::unexpected(ProfileCollectorError::CLOSED);
        }

        if (state->listeners.size() >= state->limits.listeners) {
            return std::unexpected(ProfileCollectorError::LIMIT_EXCEEDED);
        }

        if (state->listenerIdentity == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(ProfileCollectorError::IDENTITY_EXHAUSTED);
        }

        auto work = state->limits.workUnits;
        spend(work, state->model.size());
        spend(work, state->tasks.size());
        token = state->listenerIdentity + 1;
        listener->token = token;

        if (!state->model.empty()) {
            state->checkTasks(1);
            stagedTasks.reserve(state->tasks.size() + 1);
            stagedTasks.insert(stagedTasks.end(), state->tasks.begin(), state->tasks.end());
            stagedTasks.push_back({listener, state->batch(state->model)});
        }

        state->listeners.push_back(listener);
        state->listenerIdentity = token;

        if (!state->model.empty()) {
            state->tasks.swap(stagedTasks);
            state->cycleReserved = (state->dispatching ? state->cycleReserved : 0) + 1;

            if (!state->dispatching) {
                state->dispatching = true;
                run = true;
            }
        }
    } catch (const Admission &failure) {
        return std::unexpected(failure.error);
    }

    if (run) {
        state->dispatch();
    }

    return token;
}

bool InstrumentProfileCollector::removeUpdateListener(std::uint64_t token) {
    auto state = state_;
    std::shared_ptr<collector_detail::ProfileCollectorState::Listener> removed;

    {
        std::lock_guard lock(state->mutex);

        const auto position =
            std::find_if(state->listeners.begin(), state->listeners.end(), [token](const auto &listener) {
                return listener->token == token;
            });

        if (position == state->listeners.end()) {
            return false;
        }

        removed = *position;
        removed->removed = true;
        state->listeners.erase(position);
    }

    return true;
}

std::expected<std::shared_ptr<const ProfileBatch>, ProfileCollectorError> InstrumentProfileCollector::snapshot() const {
    auto state = state_;

    try {
        std::lock_guard lock(state->mutex);

        if (state->closed) {
            return std::unexpected(ProfileCollectorError::CLOSED);
        }

        auto work = state->limits.workUnits;
        spend(work, state->model.size());

        return state->batch(state->model);
    } catch (const Admission &failure) {
        return std::unexpected(failure.error);
    }
}

std::int64_t InstrumentProfileCollector::lastUpdateTime() const {
    auto state = state_;
    std::lock_guard lock(state->mutex);
    state->reported = state->updated;

    return state->updated;
}

std::uint64_t InstrumentProfileCollector::callbackFailures() const {
    auto state = state_;
    std::lock_guard lock(state->mutex);

    return state->failures;
}

void InstrumentProfileCollector::close() {
    auto state = state_;
    std::vector<std::shared_ptr<collector_detail::ProfileCollectorState::Listener>> removedListeners;
    std::vector<collector_detail::ProfileCollectorState::Task> removedTasks;
    static_assert(std::is_nothrow_default_constructible_v<decltype(removedListeners)>);
    static_assert(std::is_nothrow_default_constructible_v<decltype(removedTasks)>);

    {
        std::lock_guard lock(state->mutex);
        state->closed = true;
        // Profile/ledger destructors contain no user payload; captures retire through the vectors below.
        state->model.clear();
        state->taskHead = 0;
        state->listeners.swap(removedListeners);
        state->tasks.swap(removedTasks);
    }
}
} // namespace org::ttldtor::qdcxx::ipf
