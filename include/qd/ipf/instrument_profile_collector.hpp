// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/ipf/instrument_profile.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace org::ttldtor::qdcxx::ipf {
namespace collector_detail {
struct ProfileCollectorState;
struct ConnectionCollectorAccess;
} // namespace collector_detail

/** Checked collector admission failure. Allocation failures throw std::bad_alloc before publication. */
enum class ProfileCollectorError {
    CLOSED,            ///< The collector has been closed.
    LIMIT_EXCEEDED,    ///< A finite retained, staging, model, listener or dispatch allowance was exceeded.
    INVALID_LISTENER,  ///< An empty callback was supplied.
    IDENTITY_EXHAUSTED ///< A listener/revision/time identity cannot advance without overflow.
};

/** Finite counts and UTF-16 units; these are not exact allocator-byte or RSS measurements. */
struct ProfileCollectorLimits {
    std::size_t modelProfiles = 100000; ///< Maximum current non-removed symbols.
    std::size_t liveRevisions = 200000; ///< Maximum owned revisions, including external and staged references.
    std::size_t retainedUnits = 16 * 1024 * 1024; ///< Combined live profile text/name UTF-16 units.
    std::size_t customFields = 256;               ///< Maximum nonempty custom fields in one accepted profile.
    std::size_t stagingProfiles = 100000;     ///< Maximum incoming profiles or generation-removal scan size per call.
    std::size_t listeners = 64;               ///< Maximum registered listeners.
    std::size_t liveBatches = 1024;           ///< Maximum batches, including externally retained snapshots/deliveries.
    std::size_t batchReferences = 1000000;    ///< Maximum combined revision references in live batches.
    std::size_t workUnits = 64 * 1024 * 1024; ///< Per-call conservative comparison/copy/index work units.
    std::size_t deliveryCalls = 4096;         ///< Maximum callback tasks admitted in one synchronous dispatch cycle.
};

/** Immutable owned profile revision. Retaining it keeps its finite allowance charged. */
class ProfileRevision {
    std::shared_ptr<void> retention_;
    mutable bool superseded_ = false;
    friend struct collector_detail::ProfileCollectorState;
    /** Construct a revision after its retention has been reserved.
     * @param profile Independent source value, borrowed during construction.
     * @param revision Monotonic publication identity.
     * @param retention Owning ledger reservation.
     */
    ProfileRevision(const InstrumentProfile &profile, std::uint64_t revision, std::shared_ptr<void> retention);

    public:
    ProfileRevision(const ProfileRevision &) = delete;
    ProfileRevision &operator=(const ProfileRevision &) = delete;
    const InstrumentProfile profile; ///< Complete independent standard/custom value.
    const std::uint64_t revision;    ///< Monotonic change identity; zero denotes no published changes.
};

/** Stable immutable batch. Its references remain valid independently of collector mutation/close. */
class ProfileBatch {
    std::shared_ptr<void> retention_;
    std::vector<std::shared_ptr<const ProfileRevision>> profiles_;
    friend struct collector_detail::ProfileCollectorState;
    /** Construct an admitted batch.
     * @param profiles Owned revision references.
     * @param retention Owning batch/reference reservation.
     */
    ProfileBatch(std::vector<std::shared_ptr<const ProfileRevision>> profiles, std::shared_ptr<void> retention);

    public:
    ProfileBatch(const ProfileBatch &) = delete;
    ProfileBatch &operator=(const ProfileBatch &) = delete;
    /** Inspect all revisions in this stable batch.
     * @return Borrowed span valid while this batch is retained; no collector lock is held.
     */
    [[nodiscard]] std::span<const std::shared_ptr<const ProfileRevision>> profiles() const noexcept;
};

/** Successful atomic mutation result. */
struct ProfileCollectorChange {
    std::uint64_t revision = 0; ///< Last published revision; unchanged calls preserve it.
    std::size_t updates = 0;    ///< Published changes, including removals, before superseded delivery filtering.
};

/**
 * Thread-safe independent live profile repository with synchronous owning notifications.
 * Each method retains stable internal state before invoking callbacks. A callback may mutate, close,
 * remove itself or delete the facade; callbacks never run under the state mutex. Nested/concurrent
 * updates join the current finite dispatch cycle. Concurrent facade destruction still requires the
 * caller to synchronize ordinary facade access. Inputs must remain alive and unmodified for the call.
 * Callback exceptions are contained and counted. No executor, worker, network or application pump is used.
 */
class InstrumentProfileCollector {
    std::shared_ptr<collector_detail::ProfileCollectorState> state_;
    friend struct collector_detail::ConnectionCollectorAccess;
    /** Bind a fresh stable state.
     * @param state Independently owned collector state.
     */
    explicit InstrumentProfileCollector(std::shared_ptr<collector_detail::ProfileCollectorState> state);

    public:
    using Listener = std::function<void(std::shared_ptr<const ProfileBatch>)>; ///< Owning delivery callback.
    /** Create an independent collector.
     * @param limits Explicit finite per-instance allowances; zero is an actual zero allowance.
     * @return Owned collector; allocation failure throws std::bad_alloc.
     */
    [[nodiscard]] static std::unique_ptr<InstrumentProfileCollector> create(ProfileCollectorLimits limits = {});
    /** Request close and release current model, registrations and unstarted deliveries. */
    ~InstrumentProfileCollector();
    InstrumentProfileCollector(const InstrumentProfileCollector &) = delete;
    InstrumentProfileCollector &operator=(const InstrumentProfileCollector &) = delete;

    /** Atomically apply profiles in order; equal values only replace generation.
     * @param profiles Borrowed immutable span; neither it nor its profiles may be concurrently mutated.
     * @param generation Explicit tag; zero means no generation.
     * @return Change counts, or an admission error with no model/generation/publication change.
     */
    [[nodiscard]] std::expected<ProfileCollectorChange, ProfileCollectorError>
    updateInstrumentProfiles(std::span<const InstrumentProfile> profiles, std::uint64_t generation = 0);
    /** Remove current profiles bearing any nonzero selected generation.
     * @param generations Borrowed immutable tag list; zero never matches.
     * @return Change counts, or an admission error with no changes.
     */
    [[nodiscard]] std::expected<ProfileCollectorChange, ProfileCollectorError>
    removeGenerations(std::span<const std::uint64_t> generations);
    /** Register a listener and deliver its current snapshot when nonempty.
     * @param listener Owning callback; captures must not require destruction under a collector lock.
     * @return Unique registration token, or an admission error. A callback can run before return.
     */
    [[nodiscard]] std::expected<std::uint64_t, ProfileCollectorError> addUpdateListener(Listener listener);
    /** Prevent later callback starts for a token; an already started callback may finish.
     * @param token Registration identity returned by addUpdateListener.
     * @return True if a current registration was removed.
     */
    [[nodiscard]] bool removeUpdateListener(std::uint64_t token);
    /** Capture current non-removed profiles in exact UTF-16 symbol order.
     * @return Stable owning snapshot, or an admission error; allocation failure throws before publication.
     */
    [[nodiscard]] std::expected<std::shared_ptr<const ProfileBatch>, ProfileCollectorError> snapshot() const;
    /** Observe modification time and establish its next-change floor.
     * @return Wall-clock milliseconds; a subsequent changed call advances at least 1000 ms past this value.
     */
    [[nodiscard]] std::int64_t lastUpdateTime() const;
    /** Observe contained callback exceptions.
     * @return Saturating number of failed callback invocations.
     */
    [[nodiscard]] std::uint64_t callbackFailures() const;
    /** Close from any thread; no waiting, callbacks or destruction of captures under the state lock.
     * Existing batches/revisions remain valid; a started callback may finish. Repeated close is harmless.
     */
    void close();
};
} // namespace org::ttldtor::qdcxx::ipf
