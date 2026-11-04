// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/ipf/instrument_profile_collector.hpp"
#include "qd/ipf/instrument_profile_url_reader.hpp"

#include <chrono>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace org::ttldtor::qdcxx::ipf {
namespace connection_detail {
struct Port;
struct Runtime;
} // namespace connection_detail

/** Initial connection phase; completion is sticky and does not describe current socket activity. */
enum class IpfConnectionState {
    NOT_CONNECTED, ///< start has not been admitted.
    CONNECTING,    ///< One-shot start was admitted.
    CONNECTED,     ///< A changed source was accepted.
    COMPLETED,     ///< Initial application snapshot was completed.
    CLOSED         ///< Request-only terminal cutoff; retirement can still be in progress.
};

/** Detached connection failure, without peer/source/credential text. */
struct IpfConnectionDiagnostic {
    IpfTransferDiagnostic transfer{};                 ///< Attempt/native first cause.
    std::optional<ProfileCollectorError> collector{}; ///< Independent collector admission failure.
};

/** Per-connection retained/staging/lifecycle bounds, with no unlimited sentinel. */
struct IpfConnectionLimits {
    std::size_t batchProfiles = 1000;   ///< Profiles per flush, including carried pending profiles.
    std::size_t pendingUnits = 1048576; ///< Maximum pending standard/custom UTF-16 units.
    std::size_t oldGenerations =
        64; ///< Maximum old generation tags, including one slot reserved for the current attempt.
    std::size_t workUnits =
        64 * 1024 * 1024; ///< Lifetime pending-copy units, profile/call/tag visits; lower-layer work remains separate.
    std::size_t attempts = 64;      ///< Maximum source checks, including unchanged responses and failures.
    std::size_t listenerSlots = 16; ///< Registered/selected/retiring listener nodes combined.
    std::size_t residentBytes =
        16 * 1024 * 1024; ///< Owner/port/options/batch requested backing; lower-layer bounds separate.
    std::chrono::milliseconds lifetime{86400000}; ///< Cooperative whole-connection lifetime.
};

/** Explicit standalone runtime profile, independent of ApiRuntime and process properties. */
struct IpfConnectionRuntimeOptions {
    std::size_t maximumConnections = 1; ///< Supported profile is exactly one lifetime connection slot.
    IpfTransferOptions
        transfer{};               ///< Existing finite source/native/TLS/HTTP/decoder grants, cumulative across retries.
    IpfConnectionLimits limits{}; ///< Independent pending/model-control bounds.
};

/** Last retired attempt evidence, without retaining a response or expanded profile collection. */
struct IpfConnectionAttemptObservation {
    std::uint64_t attempt = 0;                            ///< Lifetime attempt identity.
    IpfTransferStatus status = IpfTransferStatus::FAILED; ///< Source outcome after retirement.
    unsigned httpStatus = 0;                              ///< Parsed final HTTP status, zero for local sources/unknown.
    bool httpFinished = false;                            ///< HTTP framing end, independent of a semantic marker.
    bool nativeRetired = false;                           ///< This attempt's native/local ownership has retired.
    bool ipfFinished = false;                             ///< Parser EOF observed, independently of the marker.
    bool completionSeen = false;                          ///< A validated semantic completion marker was observed.
    IpfEnvelopeProgress envelope{};               ///< Independent selected container integrity and finite progress.
    std::optional<IpfDiagnostic> ipfDiagnostic{}; ///< Detached parser location/cause, without source text.
};

/** Detached current observation; owning values remain valid independently of a facade. */
struct IpfConnectionObservation {
    IpfConnectionState state = IpfConnectionState::NOT_CONNECTED; ///< Sticky initial phase/terminal cutoff.
    IpfLiveSupport capability = IpfLiveSupport::ABSENT; ///< Last accepted changed source's X-Live observation.
    bool activeAttempt = false;                         ///< Native/local attempt currently executing.
    bool initialSnapshotComplete = false; ///< Semantic COMPLETE/nonlive clean EOF, never HTTP/TLS authentication.
    bool retired = false;                 ///< Native work and final selected callbacks have retired.
    std::optional<IpfConnectionAttemptObservation>
        lastAttempt{};          ///< Last retired evidence remains visible during a later active attempt.
    std::size_t workUnits = 0;  ///< Irreversible lifetime connection copy/control work charges.
    bool unchangedDate = false; ///< Last check skipped an equal nonzero Last-Modified.
    std::optional<std::chrono::sys_time<std::chrono::milliseconds>>
        lastModified{};                                  ///< Last successful processed attempt metadata.
    std::optional<IpfConnectionDiagnostic> diagnostic{}; ///< Last attempt first cause or terminal admission failure.
    std::uint64_t attempts = 0;                          ///< Admitted lifetime checks, without reset.
    std::uint64_t callbackFailures = 0;                  ///< Saturating contained advisory callback exceptions.
};

/** Producer-safe detached connection facade; owns no worker or SDK cleanup.
 * A callback may close/delete this facade. Stable shared control/operation leases survive through drain.
 * Ordinary concurrent destruction of the same facade object still requires caller synchronization.
 * State callbacks are serialized on the explicit runtime worker and run outside owner/native locks.
 * If never started, final CLOSED dispatch/retirement occurs on the creator during runtime join.
 */
class InstrumentProfileConnection {
    std::shared_ptr<connection_detail::Port> port_;
    friend struct connection_detail::Runtime;
    /** Adopt one admitted stable port.
     * @param port Independently retained connection state.
     */
    explicit InstrumentProfileConnection(std::shared_ptr<connection_detail::Port> port) noexcept;

    public:
    using StateChangeListener =
        std::function<void(IpfConnectionState, IpfConnectionState)>; ///< Owning old/new callback.
    /** Request close without joining or invoking SDK work. */
    ~InstrumentProfileConnection();
    InstrumentProfileConnection(const InstrumentProfileConnection &) = delete;
    InstrumentProfileConnection &operator=(const InstrumentProfileConnection &) = delete;
    /** Admit one background start, after complete configuration and worker preparation.
     * @return Acceptance, or detached state/resource failure without partial start.
     */
    [[nodiscard]] std::expected<void, IpfConnectionDiagnostic> start();
    /** Request terminal cutoff from any thread, including a callback; no allocation or join. */
    void close() noexcept;
    /** Observe sticky initial phase.
     * @return Current phase; COMPLETED can coexist with a later retry or failed attempt.
     */
    [[nodiscard]] IpfConnectionState getState() const;
    /** Observe committed Last-Modified.
     * @return UTC milliseconds (HTTP source granularity remains seconds), absent when unknown; distinct from collector
     * local update time.
     */
    [[nodiscard]] std::optional<std::chrono::sys_time<std::chrono::milliseconds>> getLastModified() const;
    /** Read the configured next-check delay.
     * @return Nonnegative milliseconds, initially60000.
     */
    [[nodiscard]] std::chrono::milliseconds getUpdatePeriod() const;
    /** Set the delay used by a subsequent scheduled wait; does not shorten a latched current wait.
     * @param period Nonnegative delay; zero schedules another finite admitted check immediately.
     * @return Acceptance or detached argument/closed failure.
     */
    [[nodiscard]] std::expected<void, IpfConnectionDiagnostic> setUpdatePeriod(std::chrono::milliseconds period);
    /** Capture current independent phase/capability/attempt evidence.
     * @return Detached owning observation with no SDK or source borrow.
     */
    [[nodiscard]] IpfConnectionObservation observation() const;
    /** Register an advisory observer, without an initial callback or inline invocation.
     * @param listener Nonempty callback; captures and arbitrary callback work remain caller-owned.
     * @return Non-reused token or detached finite-capacity/state refusal.
     * @throws std::bad_alloc Callback-node allocation fails before publication.
     */
    [[nodiscard]] std::expected<std::uint64_t, IpfConnectionDiagnostic>
    addStateChangeListener(StateChangeListener listener);
    /** Stop future starts; a callback already selected may finish.
     * @param token Registration identity; unknown tokens are harmless.
     * @return True if a live registration was removed.
     */
    [[nodiscard]] bool removeStateChangeListener(std::uint64_t token);
};

/** Main/creator-thread SDK lifetime and joined single-worker owner.
 * Owned mode is created on main before application threads. Borrowed mode retains an explicit pre-thread
 * bootstrap identity and may be created later on that same creator. The moved-to bootstrap owner must
 * outlive this runtime and drain. All native attempts execute on the one lazy SDK worker.
 * Connection facades may outlive runtime destruction as detached closed/retired handles.
 * Runtime destruction/join is creator-only, never permitted from a worker/application callback.
 */
class InstrumentProfileConnectionRuntime {
    std::unique_ptr<connection_detail::Runtime> runtime_;
    /** Adopt an independently prepared main-thread owner.
     * @param runtime Stable runtime implementation with bounded explicit SDK lifetime.
     */
    explicit InstrumentProfileConnectionRuntime(std::unique_ptr<connection_detail::Runtime> runtime) noexcept;

    public:
    /** Close/join on the creating thread before SDK lease/owned bootstrap cleanup. */
    ~InstrumentProfileConnectionRuntime();
    InstrumentProfileConnectionRuntime(const InstrumentProfileConnectionRuntime &) = delete;
    InstrumentProfileConnectionRuntime &operator=(const InstrumentProfileConnectionRuntime &) = delete;
    /** Prepare one standalone single-connection profile without opening a source or starting a worker.
     * @param options Explicit finite grants; maximumConnections must be1.
     * @return Owned main/pre-thread runtime, or detached preparation failure.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<InstrumentProfileConnectionRuntime>, IpfConnectionDiagnostic>
    create(const IpfConnectionRuntimeOptions &options = {});
    /** Borrow stable host process lifetime without hidden ApiRuntime sharing or SDK initialization.
     * @param bootstrap Same-linked-instance pre-thread owner, retained through creator join/destruction.
     * @param options Explicit one-slot profile and truthful same-image TLS/native funding.
     * @return Owned runtime facade, or detached thread/provenance/preparation failure.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<InstrumentProfileConnectionRuntime>, IpfConnectionDiagnostic>
    create(transport::RuntimeBootstrap &bootstrap, const IpfConnectionRuntimeOptions &options = {});
    /** Create the one lifetime connection slot; no worker/source activation occurs.
     * @param source Borrowed address, immutable through return; copied/sanitized before return. Optional
     * [update=period] uses bounded QD day/hour/minute/second syntax.
     * @param collector Explicit shared collector owner, retained until final connection/callback retirement.
     * @param request Owned authorization/provider configuration; conditional metadata is managed by this connection.
     * @return Producer-safe connection handle or detached admission failure; failed preparation does not consume the
     * slot.
     */
    [[nodiscard]] std::expected<std::shared_ptr<InstrumentProfileConnection>, IpfConnectionDiagnostic>
    createConnection(std::string_view source, std::shared_ptr<InstrumentProfileCollector> collector,
                     const IpfUrlRequest &request = {});
    /** Request every cutoff, drain/join worker, then detach ports. Not a snapshot-completion wait.
     * @return Acceptance or wrong-thread/self/reentrant-join refusal before waiting.
     */
    [[nodiscard]] std::expected<void, IpfConnectionDiagnostic> closeAndAwaitTermination();
};
} // namespace org::ttldtor::qdcxx::ipf
