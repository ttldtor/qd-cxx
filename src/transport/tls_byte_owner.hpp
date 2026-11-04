// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/tls_runtime.hpp"

// Parse client enums before the native byte owner introduces the Windows CALLBACK macro.
#include "raw_byte_owner.hpp"
#include "tls_engine.hpp"

namespace org::ttldtor::qdcxx::transport::detail {

/** Private verified-TLS byte operation over the existing native spine and resource profile.
 * This owner participates in the same-image host maxima supplied to tlsRequirements; it neither
 * owns nor resets shared SDK history. Configuration outlives raw/core/engine/native retirement.
 * No credential provider, application callback, QTP session or new executor is installed here.
 */
class TlsByteOwner {
    client::EndpointTls settings_;        ///< Owned bounded trust, identity and positive phase deadlines.
    TlsRequirements requirements_;        ///< Copied immutable same-image requirement shape and permits.
    TlsWorkVector disposal_;              ///< Factory-prepaid retirement reserve, spent once after native drain.
    TlsResources resources_;              ///< Conservative simultaneous wrapper/native/SDK reservation.
    std::unique_ptr<RawByteOwner> bytes_; ///< Retired before configuration and disposal reserve.
    TlsEngine *engine_ = nullptr;         ///< Stable borrow from the native child, valid through bytes_ retirement.
    std::thread::id owner_ = std::this_thread::get_id(); ///< Immutable serialized owner affinity.
    std::optional<Clock::time_point> handshakeDeadline_; ///< Starts at observed native connection.
    std::optional<Clock::time_point> closeDeadline_;     ///< Starts at actual local/peer close control.
    bool initialFunded_ = false;                         ///< Once-only copied H, never reset or refunded.
    bool busy_ = false;                    ///< Reentry guard; requestClose is allocation-free and remains legal.
    bool abortRequested_ = false;          ///< Request-only ordinary-admission cutoff.
    std::optional<TlsDiagnostic> failure_; ///< First adapter phase failure, without peer/caller text.

    /** Retain bounded checked configuration and already admitted immutable permits.
     * @param settings Explicit owned copy source; no provider is permitted.
     * @param requirements Same-image requirement shape and F+R/H/E vectors.
     * @param resources Conservative complete requested reservation.
     */
    TlsByteOwner(const client::EndpointTls &settings, TlsRequirements requirements, TlsResources resources);
    /** Check affinity/reentry without entering SDK/native state.
     * @param allowBusy True only for request-only close.
     * @return Detached refusal, or empty for an eligible operation.
     */
    [[nodiscard]] std::optional<TlsDiagnostic> rejection(bool allowBusy = false) const noexcept;
    /** Preserve the first observed engine/native cause before newer phase failures. */
    void observeFailure() noexcept;
    /** Install a positive cooperative deadline or retain a typed monotonic-range failure.
     * @param target Owned phase deadline.
     * @param duration Validated positive configured duration.
     * @return True when the absolute deadline is representable.
     */
    [[nodiscard]] bool installDeadline(std::optional<Clock::time_point> &target,
                                       std::chrono::milliseconds duration) noexcept;
    /** Observe positive cooperative handshake/close deadlines; request retirement on expiry. */
    void observePhase() noexcept;

    /** Preserve the original TLS factory admission order for either process-identity source.
     * Exactly one source is supplied; no source borrow is acquired before the original raw checkpoint.
     * @param facade Movable application facade, borrowed only by this call, or null.
     * @param lease Stable outer process identity, or null for facade acquisition.
     * @param native Explicit finite native settings.
     * @param limits Whole owned byte windows.
     * @param settings No-provider verified-TLS configuration.
     * @param options Existing same-image SDK/host ceilings.
     * @param byteWork Mutable cumulative byte/preparation credits.
     * @param tlsWork Mutable exact-shape F+R/H/E credits.
     * @return Prepared unactivated owner or original ordered refusal.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<TlsByteOwner>, TlsDiagnostic>
    createImpl(RuntimeBootstrap *facade, const BootstrapLease *lease, TcpOptions native, RawByteLimits limits,
               const client::EndpointTls &settings, TlsOptions options, protocol::LiveWork &byteWork, TlsWork &tlsWork);

    public:
    /** Drain native borrowers and retire the engine before settings or disposal credits retire. */
    ~TlsByteOwner();
    TlsByteOwner(const TlsByteOwner &) = delete;
    TlsByteOwner &operator=(const TlsByteOwner &) = delete;
    /** Prepare one no-provider TLS attempt with existing same-image F+R admission.
     * @param bootstrap Explicit application-owned process lifetime, borrowed through child drain.
     * @param native Copied bounded endpoint/native settings; residentBytes bounds raw/native backing.
     * @param limits Whole application input/output windows.
     * @param settings Explicit trust/reference identity and handshake/close deadlines; no credentials.
     * @param options Existing SDK/host/resident ceilings; shared allowance is funded once at host scope.
     * @param byteWork Mutable native/copy preparation credits, never refunded.
     * @param tlsWork Mutable same-shape phase credits; factory precharges F+R before SDK allocation.
     * @return Stable unactivated byte operation or detached typed preparation refusal.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<TlsByteOwner>, TlsDiagnostic>
    create(RuntimeBootstrap &bootstrap, TcpOptions native, RawByteLimits limits, const client::EndpointTls &settings,
           TlsOptions options, protocol::LiveWork &byteWork, TlsWork &tlsWork);
    /** Prepare from an already retained stable process identity; no bootstrap facade is retained.
     * @param bootstrap Stable outer borrow; moved-to application owner must survive final child drain.
     * @param native Copied finite native settings.
     * @param limits Whole application byte windows.
     * @param settings Explicit no-provider trust/reference/phase settings.
     * @param options Existing SDK/host/resident ceilings.
     * @param byteWork Mutable cumulative preparation credits.
     * @param tlsWork Existing reader/host-lifetime same-shape credits, never renewed here.
     * @return Prepared owner or original ordered refusal; no global initialization or I/O.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<TlsByteOwner>, TlsDiagnostic>
    create(const BootstrapLease &bootstrap, TcpOptions native, RawByteLimits limits,
           const client::EndpointTls &settings, TlsOptions options, protocol::LiveWork &byteWork, TlsWork &tlsWork);
    /** Activate the sole native connection sequence.
     * @return Acceptance or detached affinity/state refusal.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> start();
    /** Drive at most one bounded raw/native/engine action under copied H/E/close permits.
     * @param nativeWork Mutable native turn credits.
     * @param byteWork Mutable application copy credits.
     * @param tlsWork Mutable exact-shape TLS credits; no work pointer is retained.
     * @return Bounded progress/retirement, or typed work/owner refusal; never HTTP completion.
     */
    [[nodiscard]] std::expected<TcpPollResult, TlsDiagnostic> poll(TcpWork &nativeWork, protocol::LiveWork &byteWork,
                                                                   TlsWork &tlsWork);
    /** Copy one complete immutable application output block.
     * @param bytes Borrowed complete block, never retained.
     * @param work Mutable copy credits.
     * @return Whole local admission or detached refusal.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> write(std::span<const std::byte> bytes, protocol::LiveWork &work);
    /** Inspect verified owned plaintext, including a retained terminal prefix.
     * @return Immutable suffix until explicit consume or owner retirement, or owner refusal.
     */
    [[nodiscard]] std::expected<std::span<const std::byte>, TlsDiagnostic> input() const noexcept;
    /** Release an explicitly processed plaintext prefix.
     * @param bytes Consumed prefix of input().
     * @return Acceptance or detached owner/count refusal.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> consumeInput(std::size_t bytes) noexcept;
    /** Request allocation-free abort; SDK/native cancellation occurs only at a subsequent safe poll.
     * Published terminal diagnostics and native evidence remain unchanged.
     * @return Acceptance, including during an owner turn, or wrong-thread refusal.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> requestClose() noexcept;
    /** Request orderly TLS close only after all application output/WANT borrowers are accepted.
     * An already closing attempt refuses repetition without resetting WANT or the original deadline.
     * @return Acceptance or detached state/owner refusal; no SDK entry occurs inline.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> requestTlsClose() noexcept;
    /** Inspect the stable underlying operation on its serialized owner.
     * @return Stable raw byte operation, valid through this owner; no facade borrow is retained.
     */
    [[nodiscard]] RawByteOwner &bytes() noexcept;
    /** Inspect first engine, phase or native failure without entering SDK state.
     * @return Detached failure, NONE when no failure was observed.
     */
    [[nodiscard]] TlsDiagnostic diagnostic() const noexcept;
    /** Inspect the exact admitted simultaneous backing reservation on its owner.
     * @return Wrapper, raw/native and same-image SDK capacity evidence; not RSS.
     */
    [[nodiscard]] TlsResources resources() noexcept;
};

} // namespace org::ttldtor::qdcxx::transport::detail
