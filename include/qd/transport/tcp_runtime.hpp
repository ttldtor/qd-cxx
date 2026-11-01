// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/live_session.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace org::ttldtor::qdcxx::transport {

namespace detail {
struct BootstrapFactory;
class BootstrapLease;
struct RuntimeCore;
struct TcpRuntimeFactory;
struct TlsRuntimeFactory;
} // namespace detail

/** Static transport failures; peer text, hostnames and credentials are never diagnostics. */
enum class TcpError {
    NONE,                  ///< No transport failure.
    INVALID_STATE,         ///< Operation is unavailable in this run state, or the object was moved from.
    WRONG_THREAD,          ///< A checked operation was attempted outside its recorded owner thread.
    INVALID_CONFIGURATION, ///< Explicit endpoint, security, SDK or finite limits are incompatible.
    RESOURCE_LIMIT,        ///< A fixed-capacity queue, address list or backing reservation is exhausted.
    WORK_LIMIT,            ///< Caller-turn credits cannot admit another bounded operation.
    ALLOCATION_FAILURE,    ///< Admitted requested storage could not be allocated.
    DNS_FAILURE,           ///< The explicit DNS-only lookup failed or returned no eligible address.
    UNSUPPORTED_DNS_NAME,  ///< A relevant DNS name is outside the selected ASCII/A-label profile.
    CONNECT_FAILURE,       ///< Every selected address failed to connect.
    READ_FAILURE,          ///< Native input failed before a clean peer boundary.
    WRITE_FAILURE,         ///< Native output failed; accepted bytes are not peer acknowledgement.
    CLOSE_FAILURE,         ///< An owned native descriptor reported a retirement failure.
    DEADLINE,              ///< A configured monotonic operation deadline expired.
    PROTOCOL_FAILURE,      ///< The pure session rejected input or its end boundary.
    BOOTSTRAP_FAILURE      ///< Explicit process bootstrap failed.
};

/** Static/numeric error evidence, safe to retain after all runtime owners are destroyed. */
struct TcpDiagnostic {
    TcpError error = TcpError::NONE; ///< Selected typed category.
    std::string_view stage = "none"; ///< Static library-owned checkpoint label.
    int nativeCode = 0;              ///< Native or DNS SDK error code; zero when unavailable.
    protocol::LiveError protocolError = protocol::LiveError::NONE; ///< Nested pure-session rejection.
};

/** One numeric IPv4 DNS server; explicit servers may resolve both endpoint address families. */
struct DnsServer {
    std::array<std::uint8_t, 4> address; ///< Network-order address octets; the all-zero address is rejected.
};

/** Explicit finite requested-capacity limits for one run, separate from protocol limits. */
struct TcpLimits {
    std::size_t inputBytes;    ///< Owned socket-read buffer; its whole capacity remains charged.
    std::size_t writeBytes;    ///< Maximum bytes submitted by one socket-write operation.
    std::size_t addresses;     ///< Eligible deduplicated endpoint capacity, in 1..64.
    std::size_t cnameDepth;    ///< Maximum followed in-response CNAME links, in 1..16.
    std::size_t contextBytes;  ///< Fixed Asio service/descriptor arena, with no fallback allocator.
    std::size_t handlerBytes;  ///< Fixed reusable Asio operation arena, with no fallback allocator.
    std::size_t residentBytes; ///< Total conservative run allowance, including the configured session bound.
};

/** Borrowed factory settings; all retained endpoint/server values are copied into fixed owned storage. */
struct TcpOptions {
    std::string_view endpoint;             ///< Numeric IPv4/IPv6 or explicit ASCII/A-label DNS name; no URI or zone ID.
    std::uint16_t port;                    ///< Numeric remote TCP port, in 1..65535; no service-name lookup.
    std::span<const DnsServer> dnsServers; ///< Explicit 1..4 IPv4 servers for names; numeric endpoints bypass DNS.
    std::uint16_t dnsPort; ///< Shared numeric UDP/TCP DNS port; normally 53, never discovered from the OS.
    std::chrono::milliseconds dnsTimeout;     ///< Whole two-family lookup deadline, positive and representable.
    std::chrono::milliseconds connectTimeout; ///< Whole serial connection-attempt deadline, positive.
    std::chrono::milliseconds readTimeout;    ///< Connected input inactivity deadline, positive.
    std::chrono::milliseconds writeTimeout;   ///< Pending-output inactivity deadline, positive.
    std::chrono::milliseconds pollInterval;   ///< Positive caller-owned DNS/socket progress cadence; no I/O notifier.
    TcpLimits limits;                         ///< Required finite socket, SDK and aggregate resource ceilings.
    std::size_t socketSendBufferBytes = 0;    ///< SO_SNDBUF request, in0..INT_MAX; zero keeps the OS default.
};

/** Caller-owned credits for one explicitly chosen turn; repeated polls never reset or refund them. */
struct TcpWork {
    std::size_t steps;       ///< Remaining bounded public progress-step attempts.
    std::size_t handlers;    ///< Remaining internal Asio handler dispatches.
    std::size_t socketBytes; ///< Conservative peer read/write buffer bytes admitted before each native operation.
    std::size_t dnsCalls;    ///< Remaining native DNS socket callback attempts, shared across all descriptors.
    std::size_t dnsPackets;  ///< Remaining successful DNS receive operations.
    std::size_t dnsBytes;    ///< Remaining DNS receive/send bytes; complete UDP datagrams are preserved.
    std::uint64_t dnsUnits;  ///< Remaining conservative complete-response parser/copy work units.
};

/** One immutable run state; CLOSED is observable only after internal callback quiescence. */
enum class TcpState {
    CONFIGURED, ///< Owned configuration/session prepared; no network request submitted.
    RESOLVING,  ///< One bounded DNS wire query at a time is owned.
    CONNECTING, ///< One serial address attempt is owned.
    CONNECTED,  ///< Independent bounded input/output operations may be pending.
    CLOSING,    ///< Close requested; owned completions still need caller progress.
    CLOSED      ///< No callback or operation can subsequently access retired state.
};

/** Progress observation; one call never exposes more than one complete protocol event. */
enum class TcpPollStatus {
    IDLE,     ///< No immediately available work; continue the caller-owned progress cadence.
    PROGRESS, ///< One bounded action or internal handler completed.
    EVENT,    ///< Exactly one whole admitted immutable event is returned.
    YIELD,    ///< Supplied turn credits do not admit another bounded action.
    CLOSED    ///< Retirement is complete; no later internal callback can start.
};

/** Owning poll result; event storage remains under the protocol epoch's retained-owner budget. */
struct TcpPollResult {
    TcpPollStatus status = TcpPollStatus::IDLE; ///< Current progress result.
    std::optional<protocol::LiveEvent> event;   ///< One complete owning event, present only for EVENT.
};

/** Final abort/failure/peer-end evidence; socket acceptance never proves peer acknowledgement.
 * Causes follow serialized owner observation order. Only operation_aborted from our cancellation is
 * ignored. Windows may instead report ERROR_CONNECTION_ABORTED (1236) after owned close: its source is
 * ambiguous, so the non-cancellation cause remains visible even when aborted is true.
 */
struct TcpTerminal {
    TcpDiagnostic primary;   ///< First observed non-cancellation failure, or NONE when none was observed.
    TcpDiagnostic secondary; ///< First later secondary/cleanup failure, or NONE; duplicate causes are coalesced.
    bool aborted = false;    ///< True for an explicit local abort request.
    bool peerEnded = false;  ///< True when the peer ended input at a clean negotiated protocol boundary.
    bool writeAcceptanceUnknown = false;  ///< An interrupted Windows write may have an unreported accepted prefix.
    std::uint64_t reportedWriteBytes = 0; ///< Completion/native-result local acceptance; a lower bound when unknown.
    std::size_t discardedInputBytes = 0;  ///< Unadmitted owned socket/framing suffix discarded at retirement.
    std::size_t pendingOutputBytes = 0;   ///< Uncredited local suffix; may overlap an unreported accepted prefix.
};

/** Requested backing and source-derived SDK allowance; neither is allocator overhead or process RSS. */
struct TcpResources {
    std::size_t adapterBytes;      ///< Full retained adapter buffers, metadata and fixed arenas.
    std::size_t dnsEnvelopeBytes;  ///< Conservative pinned-SDK retained/candidate/temporary requested-payload bound.
    std::size_t sessionBytes;      ///< Current protocol epoch reservation, including externally retained owners.
    std::size_t totalBytes;        ///< Conservative simultaneous total; checked against the run allowance.
    std::size_t peakBytes;         ///< Highest admitted conservative total observed by this run.
    std::size_t pendingOperations; ///< Socket/DNS operations still awaiting native progress or owned callback return.
};

/** Application-owned process bootstrap; no client/epoch destructor performs global SDK cleanup.
 * Create on the main thread before other application threads, using the default c-ares allocators.
 * Coordinate with every other c-ares user. Destroy on that thread after all runtimes and application
 * threads retire. Violating the borrowed-bootstrap lifetime precondition terminates instead of
 * cleaning global state under a live runtime. This type does not mutate process settings.
 */
class RuntimeBootstrap {
    struct State;
    std::unique_ptr<State> state_; ///< Stable process-lifetime state borrowed by runtimes.
    friend class TcpRuntime;
    friend class TlsRuntime;
    friend class detail::BootstrapLease;
    friend struct detail::RuntimeCore;
    friend struct detail::BootstrapFactory;
    friend struct detail::TcpRuntimeFactory;
    friend struct detail::TlsRuntimeFactory;
    /** Adopt successfully initialized SDK state.
     * @param state Complete process-lifetime owner.
     */
    explicit RuntimeBootstrap(std::unique_ptr<State> state) noexcept;

    public:
    /** Transfer the stable bootstrap owner; active runtime borrows remain valid.
     * @param other Source bootstrap, made unusable.
     */
    RuntimeBootstrap(RuntimeBootstrap &&other) noexcept;
    /** Transfer after releasing an unused destination bootstrap.
     * @param other Source bootstrap, made unusable.
     * @return This owner; both process/main-thread lifetime preconditions still apply.
     */
    RuntimeBootstrap &operator=(RuntimeBootstrap &&other) noexcept;
    /** Release process SDK references only after all borrowers and application threads retire. */
    ~RuntimeBootstrap();
    /** Sample native entropy readiness once, then acquire explicit process SDK lifetime.
     * Windows FALSE is a typed failure without a native error code; Linux has no recoverable status.
     * The sample promises neither future availability nor hard OS latency and tests no random quality.
     * Ambient resolver configuration is not read.
     * @return Owned bootstrap or static failure; main-thread/pre-thread ordering is a caller precondition.
     */
    [[nodiscard]] static std::expected<RuntimeBootstrap, TcpDiagnostic> create();
};

/** One explicit credential-free TCP run over an owned pure session and serialized caller-driven host.
 * No method invokes application callbacks. Every operation, query, move, destruction and related
 * protocol-owner release is serialized and non-reentrant on the creating thread. Methods returning
 * expected diagnose WRONG_THREAD; inspection/move/destruction have an owner-thread precondition.
 * A run never reopens or reconnects after terminal. Keep bootstrap alive through destruction.
 */
class TcpRuntime {
    std::unique_ptr<detail::RuntimeCore> impl_; ///< Stable executor, operation state, retained buffers and session.
    friend struct detail::TcpRuntimeFactory;
    /** Adopt fully prepared state without allocating.
     * @param impl Complete unactivated run.
     */
    explicit TcpRuntime(std::unique_ptr<detail::RuntimeCore> impl) noexcept;

    public:
    /** Transfer stable runtime ownership on its recorded owner thread.
     * @param other Source run, made unusable.
     */
    TcpRuntime(TcpRuntime &&other) noexcept;
    /** Retire the old destination and transfer stable runtime ownership on the owner thread.
     * @param other Source run, made unusable.
     * @return This runtime; destination fallback teardown has the destructor's limitations.
     */
    TcpRuntime &operator=(TcpRuntime &&other) noexcept;
    /** Request abort and safely drain owned operations before release when needed.
     * Prefer explicit requestClose and poll to terminal. Fallback drain and external context teardown
     * are not hard-deadline operations; native completion/host progress is required. No timer/helper runs.
     */
    ~TcpRuntime();
    /** Prepare runtime/SDK storage before consuming protocol configuration; sends no network request.
     * @param bootstrap Borrowed explicit process owner, which must outlive the runtime.
     * @param schema Validated immutable schema, transferred only on success.
     * @param composer Matching usable composer without outstanding outputs, transferred only on success.
     * @param sessionLimits Immutable pure-session limits; credentials must be CREDENTIAL_FREE_PLAIN.
     * @param options Borrowed endpoint and explicit finite settings, copied into owned storage.
     * @param work Caller-turn wrapper/session preparation credits, consumed conservatively without refunds.
     * @return Fresh unactivated run or typed rejection with caller configuration unconsumed.
     */
    [[nodiscard]] static std::expected<TcpRuntime, TcpDiagnostic>
    create(RuntimeBootstrap &bootstrap, core::LocalSchema &&schema, protocol::SubscriptionWriter &&composer,
           protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &work);
    /** Inspect opaque DNS channel-preparation credits before create; no SDK/bootstrap is touched.
     * Add these conservative credits to independent wrapper/session creation credits. They are a
     * source-work allowance, not allocated memory, measured CPU work or elapsed time.
     * @param serverCount Explicit IPv4 server cardinality in1..4.
     * @return DNS-only LiveWork bytes/visits, or INVALID_CONFIGURATION for an unsupported count.
     */
    [[nodiscard]] static std::expected<protocol::LiveWork, TcpDiagnostic>
    dnsPreparationWorkRequirement(std::size_t serverCount) noexcept;
    /** Begin this sole connection attempt sequence on the recorded owner.
     * @return Success, WRONG_THREAD or a typed state/configuration failure; not peer readiness.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> start();
    /** Perform at most one bounded action/handler and expose at most one whole admitted event.
     * @param work Mutable caller-turn runtime credits, never automatically replenished.
     * @param sessionWork Mutable caller-turn protocol credits, including failed operations.
     * @return Owning result, or WRONG_THREAD/INVALID_STATE; call repeatedly with the same turn credits.
     */
    [[nodiscard]] std::expected<TcpPollResult, TcpDiagnostic> poll(TcpWork &work, protocol::LiveWork &sessionWork);
    /** Compose into the sole owned output slot; queue saturation rejects before protocol mutation.
     * @param descriptor Borrowed explicit selected descriptor delta.
     * @param work Mutable caller-turn protocol credits.
     * @return Local queue admission or typed rejection; not network/peer acknowledgement.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> composeProtocol(protocol::LiveProtocolView descriptor,
                                                                     protocol::LiveWork &work);
    /** Compose an explicit heartbeat without an automatic heartbeat scheduler.
     * @param heartbeat Exact field presence and raw values.
     * @param work Mutable caller-turn protocol credits.
     * @return Local output-slot admission or typed rejection.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> composeHeartbeat(protocol::LiveHeartbeat heartbeat,
                                                                      protocol::LiveWork &work);
    /** Compose requested outgoing descriptions into the sole owned output slot.
     * @param senderIds Borrowed exact outgoing IDs.
     * @param work Mutable caller-turn protocol credits.
     * @return Local output-slot admission or typed rejection.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> describe(std::span<const std::int32_t> senderIds,
                                                              protocol::LiveWork &work);
    /** Compose one supported subscription family only after pure-session readiness permits it.
     * @param message Exact selected subscription operation.
     * @param requests Borrowed exact selectors/provenance, retained only by owned composed bytes.
     * @param work Mutable caller-turn protocol credits.
     * @return Local output-slot admission or typed rejection, never peer acknowledgement.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> subscribe(protocol::SubscriptionMessage message,
                                                               std::span<const core::SubscriptionRequest> requests,
                                                               protocol::LiveWork &work);
    /** Request allocation-free idempotent abort; continue poll until terminal is observable.
     * @return Request acceptance, WRONG_THREAD or INVALID_STATE for a moved-from object.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> requestClose() noexcept;
    /** Inspect run state on the owner thread.
     * @return Current state; moved-from objects report CLOSED without an implied terminal result.
     */
    [[nodiscard]] TcpState state() const noexcept;
    /** Inspect truthful completion only after all internal callbacks return.
     * @return Immutable detached evidence, or empty while retirement is incomplete/moved from.
     */
    [[nodiscard]] std::optional<TcpTerminal> terminal() const noexcept;
    /** Retain the pure session's effective immutable peer generation on the owner thread.
     * @return Owning snapshot, empty when moved from; its release must remain serialized.
     */
    [[nodiscard]] protocol::LiveSnapshot peer() const noexcept;
    /** Inspect source-derived authentication state without invoking user code.
     * @return Pure-session state; moved-from objects report UNCHALLENGED.
     */
    [[nodiscard]] protocol::LiveAuthentication authentication() const noexcept;
    /** Inspect current requested-capacity reservations and SDK allowance on the owner thread.
     * @return Counted resources; moved-from objects report zeros. Close never refunds retained owners.
     */
    [[nodiscard]] TcpResources resources() const noexcept;
    /** Inspect sufficient conservative credits for one complete pinned DNS progress step.
     * This is an SDK work allowance, not a minimum for all actions, a session budget or a latency bound.
     * @return Runtime/DNS-only credits for this immutable configuration; numeric endpoints return zeros.
     */
    [[nodiscard]] TcpWork dnsProgressWorkRequirement() const noexcept;
    /** Inspect a deadline/poll-cadence hint without waiting or registering an I/O wake notification.
     * Keep pumping while operations are pending; this does not replace the caller-owned execution host.
     * @return Earliest deadline/cadence target, or empty when inactive/closed.
     */
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> nextWake() const noexcept;
};

} // namespace org::ttldtor::qdcxx::transport
