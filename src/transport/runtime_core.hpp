// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/tcp_runtime.hpp"

#include "bootstrap_lease.hpp"
#include "dns_resolver.hpp"
#include "fixed_arena.hpp"
#include "runtime_bootstrap.hpp"
#include "runtime_session_slot.hpp"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <thread>

namespace org::ttldtor::qdcxx::transport::detail {

using Clock = std::chrono::steady_clock;           ///< Monotonic owner-driven runtime clock.
using Socket = asio::ip::tcp::socket;              ///< Sole native TCP descriptor owner.
inline constexpr std::size_t MAX_ADDRESSES = 64;   ///< Fixed DNS result and serial attempt capacity.
inline constexpr std::size_t MAX_CNAME_DEPTH = 16; ///< Fixed in-response alias depth.
inline constexpr std::size_t MAX_HOST_BYTES = 253; ///< Maximum ASCII DNS name without final root dot.

/** Result of one privately admitted byte-engine step; counts refer to the supplied immutable spans. */
struct ByteEngineStep {
    std::size_t inputConsumed = 0;          ///< Encrypted input accepted into finite owned backing.
    std::size_t outputConsumed = 0;         ///< Application bytes accepted by the engine, not the native socket.
    bool progressed = false;                ///< One bounded internal action completed, including a WANT result.
    std::optional<TcpDiagnostic> failure{}; ///< Detached static engine failure, if any.
};

/** Private finite byte codec seam; no SDK or client-config type crosses into qdcxx_transport.
 * Only the selected internal adapter supplies an engine. Calls are serialized on the core owner;
 * advance is entered only after that adapter funds its exact phase/entry permit. Spans remain owned
 * and stable until the matching consume or abort/retirement. No method invokes application callbacks.
 */
class ByteEngine {
    public:
    /** Destroy only after native operations quiesce, using the adapter's owned disposal reserve. */
    virtual ~ByteEngine() = default;
    /** Inspect stable ciphertext currently available to the common native writer.
     * @return Borrowed immutable suffix; valid until consumeCiphertext or engine retirement.
     */
    [[nodiscard]] virtual std::span<const std::byte> ciphertext() const noexcept = 0;
    /** Account native ciphertext acceptance without entering an SDK.
     * @param bytes Accepted prefix, at most ciphertext().size().
     * @return False only for an inconsistent completion count.
     */
    [[nodiscard]] virtual bool consumeCiphertext(std::size_t bytes) noexcept = 0;
    /** Inspect verified plaintext awaiting bounded pure-session admission.
     * @return Borrowed immutable suffix; valid until consumePlaintext or engine retirement.
     */
    [[nodiscard]] virtual std::span<const std::byte> plaintext() const noexcept = 0;
    /** Account pure-session input admission without entering an SDK.
     * @param bytes Accepted prefix, at most plaintext().size().
     */
    virtual void consumePlaintext(std::size_t bytes) noexcept = 0;
    /** Check cached state for a useful bounded step, without entering an SDK.
     * @param hasInput The common core holds new encrypted input.
     * @param hasOutput An immutable application output owner is available.
     * @return True when advance can perform bounded useful work under its prepaid entry permit.
     */
    [[nodiscard]] virtual bool needsStep(bool hasInput, bool hasOutput) const noexcept = 0;
    /** Perform one admitted bounded codec action; WANT retains its exact immutable write tuple.
     * @param input Borrowed encrypted suffix; accepted backing is copied before return.
     * @param output Borrowed application suffix, retained by the core through an exact WANT retry.
     * @return Accepted prefixes, bounded progress and detached failure evidence.
     */
    [[nodiscard]] virtual ByteEngineStep advance(std::span<const std::byte> input,
                                                 std::span<const std::byte> output) noexcept = 0;
    /** Inspect cached verified chain/reference-identity eligibility.
     * @return True only after both checks succeed on this active attempt.
     */
    [[nodiscard]] virtual bool verified() const noexcept = 0;
    /** Inspect cached complete TLS closure, including every pending ciphertext/protocol suffix.
     * @return True when the common core may finish plaintext input and retire its native descriptor.
     */
    [[nodiscard]] virtual bool finished() const noexcept = 0;
    /** Notify native EOF without SDK calls or native cleanup.
     * @return Detached failure when the codec has no valid peer-end boundary.
     */
    [[nodiscard]] virtual std::optional<TcpDiagnostic> nativeEnd() noexcept = 0;
    /** Request allocation-free admission cutoff; never enter SDK/native cleanup inline. */
    virtual void abort() noexcept = 0;
};

/** Stable plain run owner; no completion retains the movable public handle or application callbacks.
 * Fields and operations retain the caller-driven TCP contract. This owner is never moved after allocation.
 */
struct RuntimeCore {
    RuntimeBootstrap::State *bootstrap;                 ///< Borrowed explicit application owner.
    BootstrapLease bootstrapBorrow;                     ///< Independent borrow, released after all SDK state.
    std::thread::id owner = std::this_thread::get_id(); ///< Immutable serialized execution affinity.
    TcpOptions options; ///< Views are rebound to owned fixed arrays immediately after construction.
    std::array<char, MAX_HOST_BYTES + 2> host{}; ///< NUL-terminated endpoint/root-name storage.
    std::array<DnsServer, 4> servers{};          ///< Bounded explicit server copy.
    detail::FixedArena contextArena; ///< Retained service and descriptor storage, before the context in lifetime order.
    detail::FixedArena handlerArena; ///< Retained reusable associated-handler storage.
    asio::io_context context;        ///< Caller-pumped IOCP/epoll context; no resolver or timer service.
    Socket socket;                   ///< Sole owner of the TCP native descriptor.
    detail::DnsResolver dns;         ///< Stable independent channel/callback domain, retired before bootstrap release.
    std::unique_ptr<ByteEngine> engine; ///< Optional finite internal codec; null preserves the plain byte path.
    std::unique_ptr<std::byte[]> input; ///< Whole counted input backing, never borrowed from the caller.
    RuntimeSessionSlot session; ///< Exclusive QTP epoch or stable raw-state borrow, installed after preparation.
    std::array<asio::ip::tcp::endpoint, MAX_ADDRESSES> addresses{}; ///< Bounded serial attempt list.
    std::size_t addressCount = 0;                                   ///< Number of initialized eligible endpoints.
    std::size_t addressIndex = 0;                                   ///< Next serial connection candidate.
    std::size_t inputBegin = 0;                ///< Beginning of the exact unconsumed socket suffix.
    std::size_t inputEnd = 0;                  ///< End of the exact unconsumed socket suffix.
    std::size_t frameBytes = 0;                ///< Accepted but not yet admitted pure-session frame bytes.
    std::size_t pendingApplicationAtClose = 0; ///< Exact unaccepted application suffix before terminal output release.
    std::optional<protocol::LivePendingOutput>
        output;                               ///< Sole whole-envelope output owner, including consumed backing.
    TcpState runState = TcpState::CONFIGURED; ///< Serialized run lifecycle.
    TcpTerminal outcome;            ///< Reserved primary/cleanup/terminal metadata, independent of data saturation.
    bool closeRequested = false;    ///< Immediate ordinary-admission cutoff.
    bool pauseCancellation = false; ///< Only explicit plain pause may classify its later local read abort as expected.
    bool closeApplied = false;      ///< Owned descriptor retirement has been applied once.
    bool terminalPublished = false; ///< Completion is observable only after all handlers return.
    bool connecting = false;        ///< Native nonblocking connect awaits bounded readiness inspection.
    bool namedEndpoint = false;     ///< Whether this immutable endpoint requires the selected DNS-only lookup.
    bool readPending = false;       ///< One submitted read/readiness operation owns its buffer.
    bool writePending = false;      ///< One submitted write/readiness operation owns its output suffix.
    bool readReady = false;         ///< One completed read/readiness result awaits owner processing.
    bool writeReady = false;        ///< One completed write/readiness result awaits owner processing.
    bool readErrorPending = false;  ///< A Windows error accompanying a completed prefix waits for prefix admission.
    asio::error_code readError;     ///< Static numeric completed input result.
    asio::error_code writeError;    ///< Static numeric completed output result.
    std::size_t readCount = 0;      ///< Windows completed input bytes, including raced retirement success.
    std::size_t writeCount = 0;     ///< Windows completed output bytes, including raced retirement success.
    std::size_t pending = 0;        ///< Submitted handlers that have not returned.
    std::size_t callbackDepth = 0;  ///< Executing internal handler guard.
    std::size_t adapterBytes = 0;   ///< Full retained adapter/arena/input reservations.
    std::size_t peakBytes = 0;      ///< Highest conservative simultaneous requested backing.
    Clock::time_point connectDeadline{}; ///< Whole serial connect sequence deadline.
    Clock::time_point dnsDeadline{};     ///< Whole serial two-family lookup deadline.
    Clock::time_point dnsTick{};         ///< Earliest next useful DNS readiness poll.
    Clock::time_point readDeadline{};    ///< Input inactivity deadline.
    Clock::time_point writeDeadline{};   ///< Pending output inactivity deadline.
    int lastConnectError = 0;            ///< Last serial native connect failure, retained without peer text.

    /** Own all fallible runtime storage before transferring pure-session configuration.
     * @param process Borrowed process owner.
     * @param borrow Independent counted borrow, retained before fallible native members.
     * @param config Validated explicit settings.
     */
    RuntimeCore(RuntimeBootstrap::State &process, BootstrapLease &&borrow, TcpOptions config);

    /** Release the bootstrap borrow after every owned descriptor/handler is retired. */
    ~RuntimeCore();

    /** Check immutable affinity before a checked public operation touches mutable state.
     * @return Empty on the owner, otherwise static WRONG_THREAD evidence.
     */
    [[nodiscard]] std::optional<TcpDiagnostic> wrongThread() const noexcept;

    /** Preserve the first failure in owner order without discarding an accompanying read prefix.
     * @param failure Static/numeric cause.
     */
    void recordFailure(TcpDiagnostic failure) noexcept;

    /** Record a failure and apply the ordinary-admission cutoff without reentering SDK calls.
     * @param failure Static/numeric cause.
     */
    void fail(TcpDiagnostic failure) noexcept;

    /** Set a positive checked deadline; range exhaustion retires rather than wrapping time.
     * @param target Owned deadline to replace only on success.
     * @param interval Explicit positive configured duration.
     * @return True when monotonic addition is representable.
     */
    bool setDeadline(Clock::time_point &target, std::chrono::milliseconds interval) noexcept;

    /** Preserve simultaneous SDK/session admission peaks before either owner scope releases storage. */
    void observePeak() noexcept;

    /** Apply an idempotent allocation-free control cutoff before further ordinary progress. */
    void applyClose() noexcept;

    /** Account a raced successful write before releasing its immutable original backing. */
    void accountWrite() noexcept;

    /** Publish completion only outside callbacks after all submitted handlers return. */
    void finishClose() noexcept;

    /** Record one complete internal handler without rearming I/O or invoking application code.
     * @param reading True for input, false for output.
     * @param error Native completion evidence.
     * @param bytes Completed Windows bytes; Linux readiness handlers supply zero.
     */
    void completed(bool reading, asio::error_code error, std::size_t bytes) noexcept;

    /** Inspect the exact immutable native-output suffix for either selected byte path.
     * @return Engine ciphertext or unchanged plain application output.
     */
    [[nodiscard]] std::span<const std::byte> wireBytes() const noexcept;
    /** Inspect application output independently of its selected codec.
     * @return Immutable raw/QTP output suffix, retained through matching acceptance.
     */
    [[nodiscard]] std::span<const std::byte> applicationBytes() const noexcept;
    /** Account a checked application prefix on the selected raw/QTP path.
     * @param bytes Accepted application prefix.
     * @return True unless the selected output rejects the count.
     */
    [[nodiscard]] bool consumeApplication(std::size_t bytes) noexcept;
    /** Account native acceptance without advancing an application span on a codec WANT result.
     * @param bytes Native accepted prefix of wireBytes().
     * @return True for an eligible prefix; otherwise records the detached consistency failure.
     */
    [[nodiscard]] bool consumeWire(std::size_t bytes) noexcept;
    /** Perform one connected codec/session/native action, keeping common socket ownership unique.
     * @param work Mutable native caller-turn credits.
     * @param sessionWork Mutable pure-session caller-turn credits.
     * @return One bounded action/event/yield observation; no application callback is invoked.
     */
    [[nodiscard]] TcpPollResult pollEngine(TcpWork &work, protocol::LiveWork &sessionWork);
    /** Begin a single owned read/readiness operation; its handler never rearms itself. */
    void beginRead();

    /** Begin one owned short write/readiness operation after checking cumulative offset headroom. */
    void beginWrite();

    /** Retire a failed candidate immediately; the last failure cannot disappear behind a later user abort.
     * @param code Native error observed for this candidate.
     */
    void rejectCandidate(int code) noexcept;

    /** Inspect or start one serial native nonblocking connect, without Asio ConnectEx/reactor fallback. */
    void connectStep();

    /** Process one completed input action; Linux executes exactly one native receive attempt. */
    void readStep() noexcept;

    /** End the selected input path; raw EOF is byte evidence, while QTP still requires complete frames. */
    void peerEnd() noexcept;

    /** Process one completed output action; Linux executes exactly one native send attempt. */
    void writeStep() noexcept;

    /** Inspect the earliest useful caller wake; no Asio timer service or helper thread is created.
     * @return Monotonic hint, empty for inactive/terminal runs.
     */
    [[nodiscard]] std::optional<Clock::time_point> wake() const noexcept;

    /** Reject new output before any pure-session mutation when admission or the sole slot is unavailable.
     * @return Empty on admission, otherwise static rejection evidence.
     */
    [[nodiscard]] std::optional<TcpDiagnostic> outputRejection() const noexcept;

    /** Adopt a completely composed immutable output without another allocation.
     * @param value Whole pure-session result.
     * @return Queue admission or detached pure-session failure.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic>
    adoptOutput(std::expected<protocol::LivePendingOutput, protocol::LiveError> value);

    /** Prepare the unchanged plain runtime before consuming pure-session configuration.
     * @param bootstrap Borrowed main-thread process owner, retained until core destruction.
     * @param schema Validated immutable schema, transferred only on success.
     * @param composer Matching usable composer, transferred only on success.
     * @param sessionLimits Required explicit credential-free session capacities.
     * @param options Borrowed endpoint and finite settings, copied into fixed owned storage.
     * @param work Mutable caller-turn preparation credits, conservatively consumed without refunds.
     * @param engine Optional prepared codec; only its verified-TLS adapter may select TLS eligibility.
     * @return One stable unactivated core or the unchanged detached typed rejection.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<RuntimeCore>, TcpDiagnostic>
    create(RuntimeBootstrap::State *bootstrap, core::LocalSchema &&schema, protocol::SubscriptionWriter &&composer,
           protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &work,
           std::unique_ptr<ByteEngine> engine = {});
    /** Prepare the same native spine with an explicit raw-state borrow and no QTP configuration.
     * @param bootstrap Stable process state retained by the enclosing outer lease.
     * @param raw Stable byte backing, retained beyond this core's destruction.
     * @param externalBytes Whole separately prepaid raw owner/backing reservation.
     * @param options Explicit copied finite native settings.
     * @param work Mutable native preparation credits, never refunded.
     * @param engine Optional prepared codec; its owner funds every entry separately.
     * @return Stable unactivated core or detached rejection.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<RuntimeCore>, TcpDiagnostic>
    createRaw(RuntimeBootstrap::State *bootstrap, RawByteState &raw, std::size_t externalBytes, TcpOptions options,
              protocol::LiveWork &work, std::unique_ptr<ByteEngine> engine = {});
    /** Inspect conservative DNS-only preparation credits without SDK/bootstrap access.
     * @param serverCount Explicit server cardinality in1..4.
     * @return Sufficient unchanged DNS preparation credits, or invalid configuration.
     */
    [[nodiscard]] static std::expected<protocol::LiveWork, TcpDiagnostic>
    dnsPreparationWorkRequirement(std::size_t serverCount) noexcept;
    /** Activate the sole connection sequence on its creating owner.
     * @return Acceptance or the unchanged owner/state/configuration rejection.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> start();
    /** Perform one bounded action/handler on its creating owner.
     * @param work Mutable native/runtime caller-turn credits, never replenished here.
     * @param sessionWork Mutable pure-session caller-turn credits.
     * @return At most one whole event or unchanged typed owner rejection.
     */
    [[nodiscard]] std::expected<TcpPollResult, TcpDiagnostic> poll(TcpWork &work, protocol::LiveWork &sessionWork);
    /** Compose into the sole output slot before any protocol mutation.
     * @param descriptor Borrowed explicit descriptor delta.
     * @param work Mutable caller-turn protocol credits.
     * @return Local queue admission or unchanged typed rejection.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> composeProtocol(protocol::LiveProtocolView descriptor,
                                                                     protocol::LiveWork &work);
    /** Compose one explicit heartbeat into the sole output slot.
     * @param heartbeat Exact raw presence/value selection.
     * @param work Mutable caller-turn protocol credits.
     * @return Local queue admission or unchanged typed rejection.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> composeHeartbeat(protocol::LiveHeartbeat heartbeat,
                                                                      protocol::LiveWork &work);
    /** Compose selected descriptions into the sole output slot.
     * @param senderIds Borrowed exact outgoing IDs.
     * @param work Mutable caller-turn protocol credits.
     * @return Local queue admission or unchanged typed rejection.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> describe(std::span<const std::int32_t> senderIds,
                                                              protocol::LiveWork &work);
    /** Compose a selected subscription only when the pure session permits it.
     * @param message Exact supported subscription family.
     * @param requests Borrowed explicit selectors/provenance.
     * @param work Mutable caller-turn protocol credits.
     * @return Local queue admission, never peer acknowledgement, or unchanged typed rejection.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> subscribe(protocol::SubscriptionMessage message,
                                                               std::span<const core::SubscriptionRequest> requests,
                                                               protocol::LiveWork &work);
    /** Request allocation-free idempotent abort on its creating owner.
     * @return Acceptance or unchanged wrong-thread rejection; poll still drives completion.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> requestClose() noexcept;
    /** Request reusable plain retirement with exact subsequent owned read-cancellation classification.
     * @return Existing owner-thread rejection or accepted cutoff; earlier diagnostics remain unchanged.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> requestPause() noexcept;
    /** Inspect lifecycle on the creating owner.
     * @return Current state; CLOSED requires callback quiescence.
     */
    [[nodiscard]] TcpState state() const noexcept;
    /** Inspect final detached evidence on the creating owner.
     * @return Terminal only after every owned callback has returned.
     */
    [[nodiscard]] std::optional<TcpTerminal> terminal() const noexcept;
    /** Retain the immutable protocol peer generation on the creating owner.
     * @return Owning peer snapshot, whose release stays serialized.
     */
    [[nodiscard]] protocol::LiveSnapshot peer() const noexcept;
    /** Inspect source-derived protocol authentication without user code.
     * @return Current pure-session authentication state.
     */
    [[nodiscard]] protocol::LiveAuthentication authentication() const noexcept;
    /** Inspect unchanged conservative requested-capacity accounting on the owner.
     * @return Current/peak backing and pending operations, not allocator overhead or RSS.
     */
    [[nodiscard]] TcpResources resources() noexcept;
    /** Inspect the next caller-owned progress hint, without an I/O wake notification.
     * @return Earliest useful monotonic deadline/cadence, or empty when inactive/closed.
     */
    [[nodiscard]] std::optional<Clock::time_point> nextWake() const noexcept;
    /** Inspect conservative DNS progress credits without resetting caller credits.
     * @return Sufficient pinned DNS step allowance, or zero for numeric endpoints.
     */
    [[nodiscard]] TcpWork dnsProgressWorkRequirement() const noexcept;
};

} // namespace org::ttldtor::qdcxx::transport::detail
