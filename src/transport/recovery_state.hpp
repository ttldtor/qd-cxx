// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace org::ttldtor::qdcxx::transport::detail {

/** Clock domain shared with the real caller-driven native runtime; tests supply explicit observations. */
using RecoveryClock = std::chrono::steady_clock;

/** Three fixed subscription-family slots; no data or callback storage belongs to this model. */
inline constexpr std::uint8_t RECOVERY_FAMILIES = 0x07;

/** Finite immutable scheduler policy, separate from transport and SDK capacities. */
struct RecoveryConfig {
    std::uint32_t maximumAttempts = 1;      ///< Lifetime factory-entry limit, including failed creation.
    std::chrono::milliseconds retryDelay{}; ///< Zero for one attempt; otherwise positive, at most INT_MAX ms.
    std::optional<std::chrono::milliseconds> heartbeatInterval; ///< Local output-admission idle interval.
    std::optional<std::chrono::milliseconds>
        protocolInactivity;                    ///< Validated input inactivity after transport eligibility.
    std::uint8_t families = RECOVERY_FAMILIES; ///< Enabled TICKER/STREAM/HISTORY bits, or zero for no delivery owner.
};

/** Serialized owner phase; terminal phases imply completion of the model's requested disposal actions. */
enum class RecoveryPhase {
    CONFIGURED,  ///< Waiting for explicit start or stop.
    PREPARING,   ///< Preparing one fresh single-attempt owner.
    STARTING,    ///< Prepared owner has not completed start.
    NEGOTIATING, ///< Transport, protocol advertisement or authentication is not eligible.
    REPLAYING,   ///< Eligible current demand has not locally converged.
    ACTIVE,      ///< Eligible current demand has locally converged; never peer acknowledgement.
    RETIRING,    ///< Abort, terminal observation and actual private disposal are outstanding.
    RESETTING,   ///< Stable delivery sources must commit their old-epoch barrier.
    BACKOFF,     ///< Waiting for the one finite fixed retry deadline.
    STOPPING,    ///< Delivery/control retirement remains outstanding.
    CLOSED,      ///< Explicit stop completed; external immutable values may still exist.
    EXHAUSTED,   ///< Retryable failure consumed the lifetime attempt limit and cleanup completed.
    FAILED       ///< Terminal failure and cleanup completed.
};

/** One bounded integration action; the model never executes it. */
enum class RecoveryAction {
    NONE,            ///< No action ticket.
    PREPARE_ATTEMPT, ///< Build a fresh writer and single-attempt runtime after component admission.
    START_ATTEMPT,   ///< Begin the prepared runtime.
    POLL_ATTEMPT,    ///< Progress the same current runtime once.
    REQUEST_ABORT,   ///< Request-only cutoff using the runtime's prepaid reserve.
    DISPOSE_ATTEMPT, ///< Destroy the terminal runtime and complete its private SDK disposal.
    RESET_SOURCES,   ///< Progress the actual stable-queue source reset barrier.
    NEGOTIATE,       ///< Admit one initial protocol/description output action.
    REPLAY,          ///< Progress one bounded current-desired reconciliation action.
    HEARTBEAT,       ///< Admit one explicit type0 heartbeat, without inventing an ACK.
    DELIVER,         ///< Progress one bounded eligible delivery action outside locks.
    STOP_DELIVERY,   ///< Retire recovery-owned delivery/control work without waiting for external leases.
    RETIRE_ALL       ///< Continue one prepaid fatal retirement after ordinary ticket identities exhaust.
};

/** Fixed scheduler causes; integration separately retains the existing typed native/TLS/queue diagnostics. */
enum class RecoveryCause {
    NONE,                ///< No observed failure.
    TRANSPORT,           ///< A fully created attempt ended or suffered retryable transport failure.
    PROTOCOL_INACTIVITY, ///< No validated current input before the inclusive deadline.
    FACTORY_FAILURE,     ///< Entered factory failed; always terminal, even if classified retryable by the caller.
    FACTORY_WORK_LIMIT,  ///< Entered factory encountered work rejection, possibly after SDK preparation.
    AUTHENTICATION,      ///< Terminal credential/authentication policy failure.
    CONFIGURATION,       ///< Invalid or unsupported immutable execution profile.
    RESOURCE,            ///< Terminal resource/shape/allocation failure.
    PROTOCOL,            ///< Terminal corrupt/incompatible protocol input.
    RESET_FAILURE,       ///< Actual source reset could not complete safely.
    CLOCK_BACKWARDS,     ///< An admitted monotonic observation regressed.
    CLOCK_RANGE,         ///< A checked time-point addition was not representable.
    IDENTITY_EXHAUSTED   ///< A checked action identity cannot advance without reuse.
};

/** Input provenance supplied only by the current real poll's engaged owning event. */
enum class RecoveryInput {
    NONE,            ///< No validated decoder event; also the default for moved-from/untrusted values.
    EMPTY_HEARTBEAT, ///< Actual decoder-produced empty envelope.
    HEARTBEAT,       ///< Actual explicit type0 frame.
    PROTOCOL,        ///< Actual complete protocol descriptor publication.
    DESCRIPTIONS,    ///< Actual complete sender/binding publication.
    DATA             ///< Actual whole data frame, including a valid empty batch.
};

/** One owner-local action identity; origin is compared only and never dereferenced. */
struct RecoveryTicket {
    const void *origin = nullptr; ///< Stable outer-owner identity; its operation lease outlives every observation.
    std::uint64_t sequence = 0;   ///< Nonzero checked identity, never reset within that owner.
    std::uint32_t attempt = 0;    ///< Admitted or prospective factory ordinal, not a peer sender ID.
    RecoveryAction action = RecoveryAction::NONE; ///< Exact expected action.

    /** Compare all ownership/action coordinates without dereferencing origin.
     * @param other Observed or pending ticket.
     * @return True for the same owner, sequence, attempt and action.
     */
    bool operator==(const RecoveryTicket &other) const = default;
};

/** Current-attempt facts sampled by the integration on the serialized owner. */
struct RecoveryFacts {
    bool transportEligible = false;      ///< Plain connected or verified TLS, including TLS authentication phase.
    bool authenticationEligible = false; ///< Actual UNCHALLENGED/ACKNOWLEDGED plus applicable TLS state.
    bool negotiationPending = false;     ///< Initial owned protocol/description outputs remain.
    bool replayPending = false;          ///< Owned current-desired reconciliation work remains.
    bool deliveryPending = false;        ///< One bounded source/delivery operation is useful.
    bool terminal = false;               ///< Actual inner terminal/native quiescence is published.
    std::uint8_t demandedFamilies = 0;   ///< Enabled families with owner-applied nonempty demand.
    std::uint8_t convergedFamilies = 0;  ///< Demand locally admitted at the exact current owner revision.
    std::uint64_t readinessVersion = 0;  ///< Checked wrapper revision for peer/auth/desired readiness changes.
};

/** One result observation for the outstanding action. */
enum class RecoveryCompletion {
    NONE,              ///< No action result in this call.
    FACTORY_ENTERED,   ///< Permission request immediately before the actual inner factory call.
    COMPLETED,         ///< Action completed at its documented real admission/disposal point.
    DEFERRED,          ///< Bounded action did not finish; PREPARE specifically has not entered its factory.
    RETRYABLE_FAILURE, ///< Fully created attempt suffered a finite-retry eligible failure.
    TERMINAL_FAILURE,  ///< Action failed without automatic retry.
    OUTPUT_BLOCKED,    ///< Exact sole-output-slot occupancy; rearm after a real poll.
    READINESS_BLOCKED  ///< Exact pure NOT_READY/auth gate; rearm after readinessVersion changes.
};

/** Bounded facts, never borrowed buffers or callbacks; default construction proves no input or disposal. */
struct RecoveryObservation {
    RecoveryTicket ticket{}; ///< Must exactly match the outstanding action when completion is present.
    RecoveryCompletion completion = RecoveryCompletion::NONE; ///< Real result classification.
    RecoveryFacts facts{};                     ///< Current-attempt snapshot, used only for matching successful actions.
    RecoveryInput input = RecoveryInput::NONE; ///< Actual validated input from a completed current poll only.
    bool outputAdmitted = false;      ///< Actual successful local output admission, distinct from wire/SSL acceptance.
    bool privateDisposed = false;     ///< DISPOSE returned after the old runtime's private SDK owners retired.
    bool sourceResetComplete = false; ///< RESET committed the real owner barrier/global ACK, not every client ACK.
    bool deliveryRetired = false;     ///< STOP_DELIVERY actually retired owned callbacks/commands/control work.
    RecoveryCause cause = RecoveryCause::NONE; ///< Bounded control classification; no error strings.
};

/** Explicit per-turn state-machine work; no component credit is silently replenished. */
struct RecoveryStateWork {
    std::size_t observations = 0; ///< Remaining bounded clock/result samples.
    std::size_t transitions = 0;  ///< Remaining issued actions or factory-entry commits.
};

/** Result of one pure step; never more than one new action. */
enum class RecoveryStepStatus {
    IDLE,                ///< No action is currently useful.
    ACTION,              ///< Exactly one new ticket was issued.
    CONTINUE,            ///< The same reserved fatal-retirement ticket may make another bounded progress step.
    WAITING,             ///< An action still awaits its matching result.
    YIELD,               ///< Explicit work is insufficient; due state is retained.
    STALE,               ///< Foreign/old action result was ignored.
    INVALID_OBSERVATION, ///< Result lacks the required real admission/disposal proof; no successor is issued.
    TERMINAL             ///< All model-requested retirement completed.
};

/** Detached bounded scheduler status, not an SDK requested-capacity or peer-delivery report. */
struct RecoveryStateStatus {
    RecoveryPhase phase = RecoveryPhase::CONFIGURED; ///< Current owner phase.
    std::uint32_t attempts = 0; ///< Actual admitted inner factory entries, including terminal failed creation.
    RecoveryCause firstCause = RecoveryCause::NONE;     ///< First observed failure across this recovery run.
    RecoveryCause secondaryCause = RecoveryCause::NONE; ///< First distinct later cause.
    RecoveryCause currentCause = RecoveryCause::NONE;   ///< Current failed attempt/terminal control cause.
    bool stopRequested = false;    ///< Monotonic public cutoff; no later factory/replay is eligible.
    bool attemptPresent = false;   ///< Prepared/running/terminal owner has not completed private disposal.
    bool factoryEntered = false;   ///< Current PREPARE ticket was committed immediately before factory entry.
    bool protocolObserved = false; ///< Current attempt returned an actual validated PROTOCOL event.
    bool eligible = false;         ///< Independent transport/protocol/auth/negotiation gate, even for empty demand.
    bool heartbeatDue = false;     ///< One due heartbeat; never an accumulating backlog.
    bool terminal = false;         ///< Terminal phase after delivery/control retirement.
    RecoveryTicket pending{};      ///< Outstanding action, or NONE.
};

/** One-step result, including the explicit permission needed before entering a factory. */
struct RecoveryStep {
    RecoveryStepStatus status = RecoveryStepStatus::IDLE; ///< Work/action/observation outcome.
    RecoveryTicket action{};           ///< New ticket only for ACTION; never a replay of a still-pending action.
    bool observationAccepted = false;  ///< Matching result or factory-entry observation was consumed.
    bool factoryEntryAccepted = false; ///< Integration may now enter the exact pending factory once.
};

/** Construction-only errors; no owner, clock, SDK or queue is touched. */
enum class RecoveryConfigError { INVALID_CONFIGURATION }; ///< Invalid identity, mask or finite duration policy.

/** Nonallocating scheduler used by the recovery wrappers and deterministic clock tests.
 * Origin is an opaque stable outer Core identity, protected by its operation lease; it is never
 * dereferenced. Do not copy a live machine into another owner. No socket, SDK, callback, borrowed
 * clock or container belongs here. A successful action result is a fact supplied by the integration,
 * not proof manufactured by this model. Idle native/SDK disposal is outside this pure class.
 */
class RecoveryState {
    RecoveryConfig config_{};                            ///< Immutable finite policy.
    const void *origin_ = nullptr;                       ///< Stable owning Core identity, not application storage.
    std::uint64_t sequence_ = 0;                         ///< Last issued checked ticket sequence.
    std::uint32_t attempts_ = 0;                         ///< Lifetime actual factory entries.
    RecoveryPhase phase_ = RecoveryPhase::CONFIGURED;    ///< Current scheduler phase.
    RecoveryTicket pending_{};                           ///< At most one in-flight owner action.
    RecoveryFacts facts_{};                              ///< Last matching current-attempt facts.
    RecoveryCause firstCause_ = RecoveryCause::NONE;     ///< First run failure.
    RecoveryCause secondaryCause_ = RecoveryCause::NONE; ///< First distinct later failure.
    RecoveryCause currentCause_ = RecoveryCause::NONE;   ///< Current recovery reason.
    std::optional<RecoveryClock::time_point> lastNow_;   ///< Last funded sample; no global clock access.
    std::optional<RecoveryClock::time_point> receiveDeadline_;   ///< Validated-input deadline.
    std::optional<RecoveryClock::time_point> heartbeatDeadline_; ///< Local output-admission idle deadline.
    std::optional<RecoveryClock::time_point> retryDeadline_;     ///< Fixed delay after actual disposal/reset.
    std::optional<std::uint64_t> blockedReadiness_;              ///< Version that rejected the current replay.
    bool startRequested_ = false;                                ///< Explicit start was requested.
    bool stopRequested_ = false;                                 ///< Monotonic cutoff.
    bool attemptPresent_ = false;                                ///< Native/TLS owner remains held.
    bool attemptStarted_ = false;                                ///< The current owner completed start.
    bool factoryEntered_ = false;                                ///< Pending PREPARE has charged its actual attempt.
    bool protocolObserved_ = false; ///< Current actual decoder PROTOCOL event was observed.
    bool recovering_ = false;       ///< Failure/disposition/reset path is active.
    bool retryable_ = false;        ///< Current fully created attempt may retry.
    bool abortRequested_ = false;   ///< Current request-only abort completed.
    bool resetComplete_ = true;     ///< No old source barrier remains outstanding.
    bool resetFailed_ = false;     ///< Failed reset must retire delivery owners, never masquerade as a committed reset.
    bool deliveryStopped_ = false; ///< Actual owned delivery/control retirement completed.
    bool pollNeeded_ = true;       ///< Native progress separates ordinary composition/delivery actions.
    bool outputBlocked_ = false;   ///< One composition must await a bounded real poll.
    bool clockFailed_ = false;     ///< Regressing clock is recorded once and cannot repeatedly reopen reset work.
    bool preferDelivery_ = false;  ///< Eligible delivery gets a turn after each attempted ordinary output.
    bool preferHeartbeat_ = false; ///< Alternate replay/heartbeat opportunities when both output classes are ready.

    /** Adopt already validated scalar policy and stable origin.
     * @param config Valid immutable policy.
     * @param origin Stable owner identity.
     * @param sequence Last used identity in this owner, never reused.
     */
    RecoveryState(RecoveryConfig config, const void *origin, std::uint64_t sequence) noexcept;
    /** Record a failure and quarantine the current epoch without releasing any owner.
     * @param cause Actual bounded cause.
     * @param retryable Whether a fully created attempt may retry.
     */
    void fail(RecoveryCause cause, bool retryable) noexcept;
    /** Evaluate the independent eligibility gate.
     * @return Actual transport/protocol/auth eligibility plus completed initial negotiation output.
     */
    [[nodiscard]] bool eligible() const noexcept;
    /** Rotate only bounded ordinary action classes, including blocked/deferred attempts.
     * @param action Matching completed/deferred ordinary action.
     */
    void rotate(RecoveryAction action) noexcept;
    /** Consume one matching completion, never a foreign continuation.
     * @param now Funded monotonic sample.
     * @param observation Current action result.
     * @param work Explicit remaining state credits.
     * @return Acceptance/rejection; new actions are selected separately.
     */
    RecoveryStep observe(RecoveryClock::time_point now, const RecoveryObservation &observation,
                         RecoveryStateWork &work) noexcept;
    /** Select one bounded next integration action after admitted observations.
     * @param now Current funded sample.
     * @return Next action or NONE, without issuing a ticket.
     */
    [[nodiscard]] RecoveryAction choose(RecoveryClock::time_point now) noexcept;
    /** Issue one checked ticket after transition admission.
     * @param action Non-NONE selected action.
     * @param work Remaining action credits.
     * @return New action, work yield or terminal identity failure.
     */
    RecoveryStep issue(RecoveryAction action, RecoveryStateWork &work) noexcept;

    public:
    /** Validate scalar policy without allocation or time sampling.
     * @param config Required finite policy; existing EndpointRetry supplies attempt count/delay.
     * @param origin Stable nonnull Core identity protected through all its action observations.
     * @param sequence Previously used owner sequence, or zero for a fresh owner; never reuse old identities.
     * @return New value or a configuration rejection, without touching caller policy.
     */
    [[nodiscard]] static std::expected<RecoveryState, RecoveryConfigError>
    create(RecoveryConfig config, const void *origin, std::uint64_t sequence = 0) noexcept;
    /** Mutable scheduler copies are unavailable.
     * @param other Source that must be moved instead.
     */
    RecoveryState(const RecoveryState &other) = delete;
    /** Mutable scheduler copy assignment is unavailable.
     * @param other Source that must be moved instead.
     * @return No result; copying is unavailable.
     */
    RecoveryState &operator=(const RecoveryState &other) = delete;
    /** Move only during preparation, before start or any action ticket is issued.
     * @param other Value whose old operations must no longer be accessed; active schedulers never move.
     */
    RecoveryState(RecoveryState &&other) noexcept = default;
    /** Mutable scheduler replacement is unavailable; the outer stable Core owns its lifetime.
     * @param other Source that must instead be moved during construction.
     * @return No result; replacement is unavailable.
     */
    RecoveryState &operator=(RecoveryState &&other) = delete;
    /** Request start without entering a factory; idempotent before cutoff. */
    void requestStart() noexcept;
    /** Record allocation-free monotonic cutoff; integration requests current native abort separately. */
    void requestStop() noexcept;
    /** Admit one clock/result sample and return at most one integration action.
     * @param now Explicit monotonic sample from the selected runtime clock or the deterministic test.
     * @param observation Optional bounded result of the outstanding action.
     * @param work Explicit mutable credits, never stored, reset or refunded here.
     * @return One action/acceptance or a truthful work/stale/terminal result.
     */
    [[nodiscard]] RecoveryStep step(RecoveryClock::time_point now, const RecoveryObservation &observation,
                                    RecoveryStateWork &work) noexcept;
    /** Copy current detached scalar status without sampling a clock or SDK.
     * @return Fixed owner/attempt/action/deadline evidence.
     */
    [[nodiscard]] RecoveryStateStatus status() const noexcept;
    /** Inspect the scheduler's earliest useful deadline, without a notifier or timer.
     * @return Minimum liveness/backoff deadline, or empty when inactive/terminal.
     */
    [[nodiscard]] std::optional<RecoveryClock::time_point> nextWake() const noexcept;
};

} // namespace org::ttldtor::qdcxx::transport::detail
