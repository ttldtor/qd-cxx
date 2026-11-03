// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/client/endpoint_options.hpp"
#include "qd/client/subscription_receiver.hpp"
#include "qd/mux/queue.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace org::ttldtor::qdcxx::transport {

/** Fixed family-array positions used by recovery limits, work and progress. */
enum class RecoveryFamily : std::size_t {
    TICKER = 0, ///< Latest-value delivery.
    STREAM = 1, ///< Ordered selected stream delivery.
    HISTORY = 2 ///< Explicit raw signed history bounds.
};

/** Detached recovery failure; nested fields preserve existing typed component evidence. */
enum class RecoveryError {
    NONE,                  ///< No failure.
    INVALID_CONFIGURATION, ///< Invalid immutable policy, schema or family binding.
    UNSUPPORTED_PROFILE,   ///< The compiled copy implementation has not been qualified.
    RESOURCE_LIMIT,        ///< A requested-capacity coordinate or representable size is insufficient.
    WORK_LIMIT,            ///< This caller turn cannot admit another indivisible action.
    ALLOCATION_FAILURE,    ///< Admitted storage could not be allocated.
    INVALID_STATE,         ///< Moved-from, stopped or otherwise ineligible owner.
    WRONG_THREAD,          ///< An owner operation ran on a different thread.
    REENTRANT,             ///< Recursive owner progress is forbidden.
    TRANSPORT_FAILURE,     ///< Actual native/DNS operation failed.
    PROTOCOL_FAILURE,      ///< Actual selected wire input or readiness failed.
    AUTHENTICATION,        ///< Credentials or authentication policy failed terminally.
    RESET_FAILURE,         ///< The old source barrier could not retire safely.
    IDENTITY_EXHAUSTED     ///< A local non-reused identity or clock range exhausted.
};

/** Static/numeric diagnostics without endpoint, token or peer text. */
struct RecoveryDiagnostic {
    RecoveryError error = RecoveryError::NONE;            ///< Recovery-level category.
    std::string_view stage = "none";                      ///< Static library-owned checkpoint.
    TcpDiagnostic transport{};                            ///< Existing native/protocol evidence when applicable.
    std::optional<mux::QueueFailure> queue{};             ///< Existing queue and nested source cause when applicable.
    std::optional<client::EndpointDiagnostic> endpoint{}; ///< Existing immutable-configuration refusal.
    std::optional<protocol::SubscriptionWriterError> writer{}; ///< Existing composition category when applicable.
};

/** Owner admission refusal or the complete original receiver preparation/registration failure. */
using RecoveryReceiverFailure = std::variant<RecoveryDiagnostic, client::ReceiverFailure>;

/** Simultaneous schema backing, in separate logical requested-capacity coordinates. */
struct RecoverySchemaCapacity {
    std::size_t records = 0;   ///< LocalRecord backing slots.
    std::size_t fields = 0;    ///< LocalField backing slots.
    std::size_t lookups = 0;   ///< FieldLookup backing slots.
    std::size_t nameUnits = 0; ///< UTF16 capacity including terminators and conservatively counted SSO.
};

/** Simultaneous outgoing declarations, independent of schema and encoded-output backing. */
struct RecoveryBlueprintCapacity {
    std::size_t records = 0;   ///< OutgoingRecord backing slots.
    std::size_t fields = 0;    ///< OutgoingField backing slots.
    std::size_t nameUnits = 0; ///< UTF16 capacity including terminators and conservatively counted SSO.
};

/** One family's independently reserved latest, emitted and candidate replay banks. */
struct RecoveryReplayLimits {
    std::size_t entries = 0;     ///< Slots in EACH of the three fixed banks.
    std::size_t symbolUnits = 0; ///< UTF16 units in EACH of the three fixed slabs.
};

/** Required finite capacities; zero means zero, never an implicit default or unlimited. */
struct RecoveryLimits {
    RecoverySchemaCapacity schemaCopies{};        ///< Simultaneous runtime-entry peak, including three schema copies.
    RecoveryBlueprintCapacity blueprintCopy{};    ///< One fresh writer's copied declaration backing.
    RecoveryBlueprintCapacity adoptedBlueprint{}; ///< Actual caller vector/string capacity retained on success.
    std::size_t endpointCopyBytes = 0;            ///< Simultaneous two-settings string capacity, including SSO/NUL.
    std::size_t writerConfigurationBytes = 0; ///< Postconstruction writer adoption ceiling; copies use tuple preflight.
    std::array<RecoveryReplayLimits, 3> replay{}; ///< TICKER, STREAM, HISTORY future capacities.
};

/** Explicit DNS route copied into fixed storage; numeric endpoints bypass its servers. */
struct RecoveryDns {
    std::array<DnsServer, 4> servers{}; ///< Explicit numeric IPv4 server slots, without OS discovery.
    std::size_t serverCount = 0;        ///< Number of supplied slots, in0..4.
    std::uint16_t port = 53;            ///< Shared UDP/TCP DNS port, required nonzero for a named route.
};

/** Immutable opt-in recovery policy; retry count/delay come from the original endpoint. */
struct RecoveryOptions {
    RecoveryLimits limits{};                                     ///< Independent new backing reservations.
    std::optional<std::chrono::milliseconds> heartbeatInterval;  ///< Local output-admission idle interval.
    std::optional<std::chrono::milliseconds> protocolInactivity; ///< Validated current-input inactivity.
    core::SubscriptionSelectorPolicy selectors = core::SubscriptionSelectorPolicy::EXACT_ONLY; ///< Exact profile.
    RecoveryDns dns{}; ///< Explicit finite route; reference identity remains the endpoint TLS identity.
};

/** Caller delivery owners, borrowed until a successful factory's final nonthrowing transfer. */
struct RecoveryQueues {
    std::optional<mux::MuxQueue> ticker;  ///< Optional stable TICKER source/registration domain.
    std::optional<mux::MuxQueue> stream;  ///< Optional stable STREAM source/registration domain.
    std::optional<mux::MuxQueue> history; ///< Optional stable HISTORY source/registration domain.
};

/** Owner-turn credits for the new bounded scans, copies and scheduler actions. */
struct RecoveryWork {
    std::size_t operations = 0; ///< Recovery operation/action attempts.
    std::size_t visits = 0;     ///< Metadata inspection, comparison, initialization and disposal visits.
    std::size_t textUnits = 0;  ///< Examined/copied UTF16 or ASCII units.
    std::size_t bytes = 0;      ///< Explicit new array/control initialization and disposal work.
};

/** Independent preparation credits; no field is replenished or retained by reference. */
struct RecoveryPreparationWork {
    RecoveryWork recovery{};                   ///< New configuration/admission work.
    protocol::SubscriptionWriterWork writer{}; ///< Actual first writer's preparation work.
    std::array<mux::QueueWork, 3> queues{};    ///< Actual queue binding-inspection work by family.
};

/** Mutable coordinates for one serialized recovery/native owner turn; no implicit replenishment. */
struct RecoveryTurnWork {
    RecoveryWork recovery{};                   ///< Scheduler observations/actions and bounded replay/control copying.
    protocol::SubscriptionWriterWork writer{}; ///< Actual fresh-writer creation on a later factory entry.
    TcpWork runtime{};                         ///< Actual native/DNS progress credits, unchanged in meaning.
    protocol::LiveWork session{};              ///< Actual session/factory/composition progress credits.
    std::array<mux::QueueWork, 3> queues{};    ///< Independent real queue progress/reset/callback credits.
};

/** Detached resource vector; byte scopes overlap independently counted declaration capacities. */
struct RecoveryCapacity {
    RecoverySchemaCapacity schemaCopies{};        ///< Enforced three-schema peak.
    RecoveryBlueprintCapacity blueprintCopy{};    ///< One independent writer's copy envelope.
    RecoveryBlueprintCapacity adoptedBlueprint{}; ///< Actual retained blueprint capacity.
    std::size_t endpointCopyBytes = 0;            ///< Enforced transient settings string capacity.
    std::size_t replayEntries = 0;                ///< Sum of all three fixed banks per enabled family.
    std::size_t replaySymbolUnits = 0;            ///< Sum of all fixed UTF16 slabs.
    std::size_t replayBytes = 0; ///< Known replay array requests; overlaps the two replay count coordinates.
    std::size_t queueBytes = 0;  ///< Sum of original queue/Mux/collector byte ceilings, excluding desired scopes.
    std::array<mux::QueueCapacity, 3> queues{}; ///< Original desired owner/count ceilings remain separate.
    std::size_t sourceSchemaBytes = 0;          ///< Observed borrowed input backing; not a new recovery allocation.
    std::size_t sourceEndpointTextBytes = 0;    ///< Actual borrowed settings string capacities.
    std::size_t writerConfigurationBytes = 0;   ///< Actual retained first-writer configuration after tuple preflight.
};

/** Serialized recovery lifecycle; ACTIVE is local convergence, never a peer acknowledgement. */
enum class RecoveryPhase {
    CONFIGURED,  ///< Explicit start has not been requested.
    PREPARING,   ///< A funded combined writer/runtime factory is pending.
    STARTING,    ///< The prepared runtime has not activated its route.
    NEGOTIATING, ///< Actual transport, protocol or authentication eligibility is absent.
    REPLAYING,   ///< Current desired state is not yet locally admitted.
    ACTIVE,      ///< Eligible desired state is locally converged.
    RETIRING,    ///< Old native/private owners remain.
    RESETTING,   ///< Stable delivery sources are committing their old-epoch barrier.
    BACKOFF,     ///< A finite fixed retry deadline is pending.
    STOPPING,    ///< Recovery-owned delivery/control work is retiring.
    CLOSED,      ///< Explicit stop and local retirement completed.
    EXHAUSTED,   ///< The finite lifetime attempt budget was consumed.
    FAILED       ///< Terminal failure and local retirement completed.
};

/** Detached local shutdown result, published only after owned native, event and queue retirement. */
struct RecoveryTerminal {
    mux::TerminalOutcome outcome = mux::TerminalOutcome::ABORTED; ///< Actual local result, never wire-flush proof.
    std::array<std::optional<mux::QueueTerminal>, 3> queues{};    ///< Exact TICKER/STREAM/HISTORY results, if adopted.
    bool deadlineExpired = false;      ///< An active graceful deadline expired before all retirement completed.
    bool heldDataDiscarded = false;    ///< A complete Core-held DATA event was released without queue admission.
    std::optional<TcpTerminal> wire{}; ///< Most recent actually retired attempt; encrypted-byte evidence for TLS.
    std::optional<std::size_t>
        nativeOperationsAtCutoff{}; ///< Owned native lifetimes observed at the first explicit fence.
};

/** Fixed owner status, without a per-attempt log or raw protocol/event owner. */
struct RecoveryStatus {
    RecoveryPhase phase = RecoveryPhase::CONFIGURED; ///< Actual current owner phase.
    std::uint32_t attempts = 0;                      ///< Actual entered combined factories; never reset after success.
    RecoveryDiagnostic first{};                      ///< First bounded failure evidence.
    RecoveryDiagnostic secondary{};                  ///< First distinct later failure evidence.
    std::uint8_t demandedFamilies = 0;               ///< Nonempty owner-applied demand bits.
    std::uint8_t convergedFamilies = 0;              ///< Exact locally admitted current-revision bits.
    bool stopped = false;                            ///< Monotonic public cutoff was requested.
    std::optional<RecoveryTerminal> terminal{};      ///< Frozen detached result after actual complete local quiescence.
};

} // namespace org::ttldtor::qdcxx::transport
