// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/client/endpoint_options.hpp"
#include "qd/transport/tcp_runtime.hpp"
#include "qd/transport/tls_resources.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace org::ttldtor::qdcxx::transport {

/** Detached TLS failures; neither peer strings nor credential bytes enter diagnostics. */
enum class TlsError {
    NONE,                  ///< No TLS failure was observed.
    INVALID_STATE,         ///< Moved-from, retired, busy or incompatible lifecycle state.
    WRONG_THREAD,          ///< Operation attempted outside the creating serialized owner.
    INVALID_CONFIGURATION, ///< Explicit snapshot, limits or supported-host declaration is incompatible.
    RESOURCE_LIMIT,        ///< Requested backing or finite capacity cannot admit the operation.
    WORK_LIMIT,            ///< The exact qualified caller-turn vector cannot admit an indivisible phase.
    ALLOCATION_FAILURE,    ///< Admitted dynamic backing could not be allocated.
    TRANSPORT_FAILURE,     ///< DNS, connection, native I/O or completion retirement failed.
    TLS_POLICY_FAILURE,    ///< The qualified SDK, process state or selected crypto policy is unavailable.
    TRUST_FAILURE,         ///< Explicit trust input or the complete peer chain failed validation.
    IDENTITY_FAILURE,      ///< The configured SAN reference identity was not verified.
    HANDSHAKE_FAILURE,     ///< TLS negotiation failed without completed verification.
    CREDENTIAL_FAILURE,    ///< The verified attempt could not obtain or validate its selected Basic token.
    AUTHENTICATION_DENIED, ///< The peer still requires authentication after the sole authorization.
    PROTOCOL_FAILURE,      ///< The bounded pure session rejected input or output.
    DEADLINE,              ///< A cooperative monotonic operation deadline expired.
    CLOSE_FAILURE          ///< TLS close or native retirement failed.
};

/** Static/numeric failure evidence, independent of all runtime, snapshot and provider lifetimes. */
struct TlsDiagnostic {
    TlsError error = TlsError::NONE; ///< Typed primary category.
    std::string_view stage = "none"; ///< Static library-owned checkpoint.
    std::uint64_t sdkCode = 0;       ///< Numeric SDK evidence; no formatted error text.
    int verifyCode = 0;              ///< Numeric certificate-verification evidence when available.
    TcpDiagnostic transport{};       ///< Detached native/DNS/session cause when applicable.
};

/** Explicit DNS handoff; numeric route hosts bypass these servers. */
struct TlsDnsOptions {
    std::span<const DnsServer> servers{}; ///< Borrowed 1..4 numeric IPv4 servers for a named route host.
    std::uint16_t port = 0;               ///< Explicit DNS port; required for named routes, never read from the OS.
};

/** Required TLS-only qualification and allowances, copied by create before any activation.
 * Host maxima include every same-qualified-SDK user and failed creation contribution through final
 * retirement. They are embedding obligations, not maxima the library can discover or enforce in
 * foreign contexts. Every required capacity is finite; zero never means unlimited. SDK allowances
 * are requested dynamic backing, including embedded heap arrays and variable native scratch.
 * Ordinary fixed native call frames, allocator overhead, RSS and hard wall time are outside that metric.
 */
struct TlsOptions {
    TlsResourceLimits limits{};       ///< Explicit proof/input ceilings; actual immutable snapshot must fit.
    TlsHostLimits host{};             ///< Persistent truthful same-SDK state, history and concurrency maxima.
    TlsDnsOptions dns{};              ///< Explicit route resolver configuration, copied into the common core.
    std::size_t sdkInstanceBytes = 0; ///< Positive private SDK allowance, at least the computed requirement.
    std::size_t sdkSharedBytes = 0;   ///< Positive shared SDK allowance, including preexisting and failed prefixes.
    std::size_t residentBytes = 0;    ///< Whole run allowance: adapter, retained snapshot, TCP/session and SDK bounds.
};

/** One serialized TLS attempt; CONNECTED never denotes an unverified native connection. */
enum class TlsState {
    CONFIGURED,     ///< Prepared without network activation.
    RESOLVING,      ///< The common bounded DNS owner is resolving the route host.
    CONNECTING,     ///< The common TCP owner is trying its finite address list.
    HANDSHAKING,    ///< Native TCP connected; TLS chain and reference identity are not yet both verified.
    AUTHENTICATING, ///< Verified TLS is waiting for selected protocol authentication completion.
    CONNECTED,      ///< Verified TLS; selected protocol authentication does not currently block the run.
    CLOSING,        ///< Admission is cut off; TLS/native progress or owned completions remain.
    CLOSED          ///< All native operations and internal callbacks have retired.
};

/** At most one complete protocol event per admitted poll. */
struct TlsPollResult {
    TcpPollStatus status = TcpPollStatus::IDLE; ///< Bounded progress, yield, event or truthful closure.
    std::optional<protocol::LiveEvent> event{}; ///< Owning event, present only for EVENT.
};

/** Detached completion; local SSL or socket acceptance never acknowledges peer receipt. */
struct TlsTerminal {
    TlsDiagnostic primary{};                  ///< First observed non-cancellation cause in serialized owner order.
    TlsDiagnostic secondary{};                ///< First distinct later cleanup cause.
    TcpTerminal wire{};                       ///< Native encrypted-byte acceptance and completion evidence.
    bool aborted = false;                     ///< Explicit local abort or public-owner retirement requested cutoff.
    bool closeNotifyReceived = false;         ///< The verified peer sent TLS close_notify.
    bool orderly = false;                     ///< Both TLS closure and all ciphertext/native completion drain finished.
    bool tlsDataPending = false;              ///< Additional record/protocol data may remain inside the SDK.
    std::uint64_t acceptedPlaintextBytes = 0; ///< Bytes accepted by SSL_write, independently of socket acceptance.
    std::size_t pendingPlaintextBytes = 0;    ///< Unaccepted application suffix, including an exact WANT retry tuple.
    std::size_t pendingCiphertextBytes = 0; ///< Known queued wire suffix; tlsDataPending also covers private SDK data.
};

/** Conservative requested-capacity reservations, including retained owners after close. */
struct TlsResources {
    TcpResources transport{};      ///< Existing common native/DNS/session requested backing.
    TlsSdkRequirements sdk{};      ///< Private/shared qualified SDK and failed-prefix requirements.
    std::size_t adapterBytes = 0;  ///< TLS wrapper, engine, finite plaintext/ciphertext and token backing.
    std::size_t snapshotBytes = 0; ///< Retained immutable settings/schema capacity; provider state is external.
    std::size_t totalBytes = 0;    ///< Checked conservative simultaneous sum, compared with residentBytes.
    std::size_t peakBytes = 0;     ///< Highest conservative reservation admitted by this attempt.
};

/** Move-only explicit snapshot TLS/authentication handoff over the common caller-driven TCP owner.
 * No reconnect, automatic scheduler, system trust or default resolver is created. Keep bootstrap alive
 * until all runtime/in-flight owners retire. All methods, inspection, moves, destruction and protocol
 * owner release are serialized on the creating thread. Checked operations report WRONG_THREAD;
 * inspection/move/destruction have that owner-thread precondition. Normal operations are nonreentrant.
 *
 * A provider callback may requestClose, destroy this handle or replace it by move assignment. These
 * operations only request reserved allocation-free cutoff, without inline SDK cleanup. Each active
 * operation retains a private Impl lease; the old public-owner marker is independent of that lease.
 * Provider return cannot authorize a replaced/retired attempt. Callback/capture/destructor work and
 * native scheduling have no library duration bound. Continue explicit polling to observe quiescence.
 */
class TlsRuntime {
    friend struct detail::TlsRuntimeFactory;
    struct Impl;
    std::shared_ptr<Impl> impl_; ///< Stable private operation lease; public ownership is still unique.
    /** Adopt complete unactivated state without allocation.
     * @param impl Prepared stable owner, including its copied disposal reserve.
     */
    explicit TlsRuntime(std::shared_ptr<Impl> impl) noexcept;

    public:
    /** Public ownership cannot be copied.
     * @param other Unused source; copying is unavailable.
     */
    TlsRuntime(const TlsRuntime &other) = delete;
    /** Public ownership cannot be copied.
     * @param other Unused source; copying is unavailable.
     * @return No result; copying is unavailable.
     */
    TlsRuntime &operator=(const TlsRuntime &other) = delete;
    /** Transfer public ownership without cutting off the source attempt.
     * @param other Source handle, made moved from on the same owner thread.
     */
    TlsRuntime(TlsRuntime &&other) noexcept;
    /** Retire the old destination before transferring; self-move is a no-op.
     * The old owner cutoff is immediate even if an in-flight provider retains its Impl.
     * @param other Source handle on the creating owner thread.
     * @return This handle, owning the transferred attempt.
     */
    TlsRuntime &operator=(TlsRuntime &&other) noexcept;
    /** Cut off public ownership using the owned disposal reserve; never clean up inline in a provider.
     * Outside callbacks fallback native drain can require host progress and has no hard time bound.
     */
    ~TlsRuntime();
    /** Prepare explicit TLS and a new pure epoch; never activate DNS/network or invoke credentials.
     * Scan all immutable trust framing/counts before any certificate decode. Full chain and identity
     * verification occurs only during the later active attempt. Failure leaves composer unconsumed.
     * @param bootstrap Explicit process bootstrap, borrowed through final native retirement.
     * @param endpoint Non-moved-from owned TLS snapshot, retained independently of this handle.
     * @param composer Matching usable subscription composer, transferred only on successful preparation.
     * @param options Explicit immutable qualification/allowances and borrowed DNS values, copied by the call.
     * @param tlsWork Mutable qualified factory-plus-disposal credits, never retained by reference.
     * @param sessionWork Independent wrapper/schema/session preparation credits, with no failure refunds.
     * @return One prepared move-only attempt or detached typed rejection; no secret diagnostic text.
     */
    [[nodiscard]] static std::expected<TlsRuntime, TlsDiagnostic> create(RuntimeBootstrap &bootstrap,
                                                                         const client::EndpointOptions &endpoint,
                                                                         protocol::SubscriptionWriter &&composer,
                                                                         TlsOptions options, TlsWork &tlsWork,
                                                                         protocol::LiveWork &sessionWork);
    /** Activate the sole route connection sequence; this does not verify TLS or invoke credentials.
     * @return Acceptance or a detached wrong-thread/state/transport rejection.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> start();
    /** Admit bounded native/TLS/session progress and expose at most one complete protocol event.
     * Each call is one nonreentrant turn. Fund H once before its first possible SDK step and E before
     * every later entry; WANT/failure never refunds the current turn. Caller budget pointers never escape.
     * @param runtimeWork Mutable common native/DNS credits, not automatically reset.
     * @param tlsWork Mutable exact qualified TLS entry/initial/close credits, not automatically reset.
     * @param sessionWork Independent mutable protocol/authentication scanning/composition credits.
     * @return Owning progress/event result or static rejection before an unfunded indivisible phase.
     */
    [[nodiscard]] std::expected<TlsPollResult, TlsDiagnostic> poll(TcpWork &runtimeWork, TlsWork &tlsWork,
                                                                   protocol::LiveWork &sessionWork);
    /** Compose an explicit local descriptor on verified TLS; credentials use only the internal handoff.
     * @param descriptor Borrowed selected descriptor, valid during this call only.
     * @param work Mutable pure-session composition credits.
     * @return Local whole-output admission or detached rejection, never peer acknowledgement.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> composeProtocol(protocol::LiveProtocolView descriptor,
                                                                     protocol::LiveWork &work);
    /** Compose an explicit heartbeat on verified TLS, without an automatic timer.
     * @param heartbeat Exact field presence and raw values.
     * @param work Mutable pure-session composition credits.
     * @return Local output admission or detached rejection.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> composeHeartbeat(protocol::LiveHeartbeat heartbeat,
                                                                      protocol::LiveWork &work);
    /** Compose outgoing selected descriptions on verified TLS.
     * @param senderIds Exact sender IDs, borrowed during composition only.
     * @param work Mutable pure-session composition credits.
     * @return Whole local output admission or detached rejection.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> describe(std::span<const std::int32_t> senderIds,
                                                              protocol::LiveWork &work);
    /** Compose a selected subscription after existing session/authentication readiness permits it.
     * @param message One supported subscription operation.
     * @param requests Exact borrowed selectors/provenance, retained only in owned composed bytes.
     * @param work Mutable pure-session composition credits.
     * @return Local output admission or detached rejection; no peer acknowledgement is implied.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> subscribe(protocol::SubscriptionMessage message,
                                                               std::span<const core::SubscriptionRequest> requests,
                                                               protocol::LiveWork &work);
    /** Request idempotent allocation-free abort, including from the credential provider.
     * This uses owned R without a caller budget or a normal-entry/busy guard. It performs no inline
     * SSL/BIO/native cleanup; continue polling to drain completions and observe a terminal result.
     * @return Acceptance, WRONG_THREAD or INVALID_STATE for a moved-from handle.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> requestClose() noexcept;
    /** Request bounded TLS close on verified TLS; subsequent poll entries pay E plus close control.
     * This is not protocol graceful drain and rejects reentry from a credential provider.
     * @return Request acceptance or a detached owner/state rejection.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> requestTlsClose() noexcept;
    /** Inspect current state on the owner thread without entering the SDK.
     * @return Current state; a moved-from handle reports CLOSED without terminal evidence.
     */
    [[nodiscard]] TlsState state() const noexcept;
    /** Inspect detached completion only after native callbacks and operations retire.
     * @return Complete evidence or empty for an active/moved-from attempt.
     */
    [[nodiscard]] std::optional<TlsTerminal> terminal() const noexcept;
    /** Retain the pure session's effective immutable peer generation on its owner thread.
     * @return Owning snapshot, empty when moved from; release remains serialized.
     */
    [[nodiscard]] protocol::LiveSnapshot peer() const noexcept;
    /** Inspect pure authentication state without invoking the provider.
     * @return Effective state; moved-from handles report UNCHALLENGED.
     */
    [[nodiscard]] protocol::LiveAuthentication authentication() const noexcept;
    /** Inspect current conservative backing reservations without entering the SDK.
     * @return Requested capacities; moved-from handles report zeros, never an RSS measurement.
     */
    [[nodiscard]] TlsResources resources() const noexcept;
    /** Inspect the exact copied resource/work requirements for this immutable qualification.
     * @return Value copy; moved-from handles return an unqualified zero value.
     */
    [[nodiscard]] TlsRequirements requirements() const noexcept;
    /** Inspect the common runtime's sufficient bounded DNS progress credits.
     * @return Native/DNS credits, or zeros for a numeric route/moved-from handle.
     */
    [[nodiscard]] TcpWork dnsProgressWorkRequirement() const noexcept;
    /** Inspect the earliest useful native/TLS/authentication deadline or progress-cadence hint.
     * @return Monotonic hint, or empty for an inactive/closed attempt; no notifier is registered.
     */
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> nextWake() const noexcept;
};

} // namespace org::ttldtor::qdcxx::transport
