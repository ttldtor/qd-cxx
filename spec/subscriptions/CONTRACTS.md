# Native subscription state and lifecycle contract

[Русская версия](CONTRACTS.ru.md). English is authoritative.

## Status and scope

This is the selected design contract for future native subscriptions. It defines local API/state requirements; it does not claim an implemented client, collector, transport or Java compatibility beyond the linked existing evidence. It adds no C++ declarations, runtime dependency or wire profile. The [architecture decision](../../docs/decisions/ADR-0005-live-subscriptions.md) records the rationale.

The selected goal is a read-side client over explicitly supported binary records and caller-supplied immutable schemas: STREAM, TICKER and HISTORY delivery, shared upstream demand, isolated local consumers, finite resources and explicit recovery/stop. [Current capabilities](../../docs/CURRENT-STATE.md) remain authoritative for implemented behavior. The [coverage matrix and scenarios](../../tests/conformance/subscriptions/CONTRACTS.md) distinguish current support, fixed design, pending evidence and exclusions. Collector/capability/wildcard compatibility must be measured before dependent implementation; runtime/TLS/backend selection is a separate prerequisite.

The pure [bounded desired-state API](STATE.md) and [outgoing composer](WRITER.md) are implemented. The remaining client, live-session and collector lifecycle described here is still a design contract. Selected Java observations are recorded in the [capture profile](../../tests/fixtures/subscriptions/CAPTURES.md).

## Ownership boundaries

| Boundary | Owns | Must not own or infer |
| --- | --- | --- |
| Pure core | Validated schema/record identity, exact selectors, desired maps, mutation revisions and finite operation/resident accounting | Connections, clocks, callback execution, runtime threads, I/O, JSON or process settings |
| Protocol session | Directional sender tables, immutable binding snapshots, negotiation state, partial input/output and one connection epoch | A socket library, callback scheduling or a claim of peer acknowledgement without evidence |
| Collectors/MUX | Selected delivery state, aggregate interests, membership eligibility and bounded per-client staging | Business conversions inferred from names or unbounded retained history |
| Client dispatcher | Registrations, executing callable guards, owning deliveries, command/control admission and completion barriers | Borrowed caller buffers retained after submission or nested delivery into user code |
| Runtime adapter | Execution host, socket/DNS/TLS operations, timers, progress and quiescence | A dependency pointing from core/codecs/events back to runtime |

The pure core has a nonconcurrent, non-reentrant caller contract. Its caller serializes access; it need not inspect an OS thread identifier. The upper client has one serialized owner domain. A separate bounded command port may be used from other threads and callbacks; direct concurrent mutation of the owner state is unsupported. This is a serialization contract, not selection of an event loop or OS threading library.

The application keeps the execution host alive until every run using it is quiescent. Destroying/joining the execution host from its own callback is unsupported; requesting stop there is supported. A caller-driven host requires continued pumping after a stop request. No background progress or real-time deadline is implied by the pure driver.

## Identity and requested state

A run uses one immutable local schema context. Each registration binds that context and one contract family. Its context and family do not change; another family needs a new registration, and another context needs a separate run. A desired entry is keyed by that context's record identity and a selector. Local record IDs are validated within their own context; equal numeric IDs or equal schema shapes do not make independently created contexts interchangeable. Schema copies preserve the existing [RecordContext identity](../../spec/events/EVENT-MAPPING.md).

Exact selectors own their UTF-16 code units. No case folding, trimming, normalization, currency conversion or sender-ID identity is introduced. Null is not an exact selector; an empty non-null string remains a distinct key. Well-formedness is not inferred for symbol text: the selected encoding preserves exact UTF-16 units, including embedded zero and unpaired surrogates, within its limits. Record names still obey existing schema validation.

Wildcard is a distinct explicit selector, never automatic string expansion. Exact strings starting with the reserved `*` prefix are rejected by this native profile rather than accidentally emitting a wildcard. No literal escape, glob or regex syntax is invented. Actual wildcard family/peer support is pending production evidence; unsupported combinations fail explicitly before their interest is emitted.

HISTORY entries additionally carry a signed 64-bit raw record-time lower bound. This is an attribute of the key, not an extra symbol or registration identity. It is not Unix milliseconds, MESSAGE/FIELD tape time or an inferred business timestamp. Its representation follows the explicitly validated [indexed layout](../schema/INDEXED-SCHEMAS.md): high/low words or one logical signed64 value, preserving all bits. Native state stores the raw bound; collector comparison/inclusive-range and wire packing require the selected compatibility evidence. This profile requires explicit indexed/time metadata for HISTORY and rejects a missing layout rather than guessing it.

The local schema remains immutable for a run. Replacing local configuration requires a new context/run; live local-schema replacement is outside this design. Peer description/binding changes are a separate protocol operation.

## Atomic commands and barriers

| Command | Successful local effect |
| --- | --- |
| Add | Upsert the requested keys; identical entries are a no-op. A changed HISTORY bound replaces that key's attribute and starts a new membership activation. |
| Remove | Remove present keys; absent keys are a no-op. |
| Replace | Publish exactly the validated input map; empty input clears it. Contradictory duplicate entries in one command are rejected; identical duplicates collapse. |
| Clear | Publish an empty map. |
| Close registration | Retire its memberships irreversibly and request its quiescence. This control path is not rejected because the ordinary command/data queue is full. |

Validate the complete candidate, reserve its peak resources and then publish it atomically. Invalid identities/selectors/layouts, arithmetic overflow, allocation or resource/work failure preserve the previous map, revision and membership activations. Temporary resource reservations are released; work already charged in the current turn is not refunded. Report a typed cause; a rejected local command does not itself terminate a healthy connection.

A successful effective change advances the registration revision exactly once; no-op or failed commands do not. Unchanged memberships retain their activation IDs. New keys, re-added keys and changed HISTORY bounds receive new non-reused IDs. A replaced bound invalidates the old subscription view and requires the selected collector's fresh/incomplete transition; the exact peer snapshot behavior is pending compatibility evidence.

Submission acceptance means only that the upper layer owns a bounded command copy. It is distinct from owner application and wire emission. An accepted command eventually reports applied, rejected or cancelled-closing with its identity and relevant revision; it is not silently lost. A full/closed command port refuses admission immediately. Direct owner commands outside callbacks can report their application result synchronously; callback commands queue for after return. Owned completion state must not retain the endpoint, socket operations or callable once complete.

Remove/clear/replace completion is the owner application barrier: after it, no new callback begins with data for retired memberships. Already executing user code may finish before that barrier. Old queued items carry eligibility tokens and are filtered/discarded as withdrawn by revision; they cannot become eligible merely because the same key is re-added. A command queued during a callback cannot retroactively revoke records already in that callback's borrowed batch. No nested dispatch runs to force a barrier.

## Admission, delivery and callable lifetime

Live data admission validates and constructs the whole current frame before publishing any of its records to collectors or user-visible queues. A failed frame publishes none of its completed decoder prefix. Previously admitted frames remain accounted separately; a later terminal error does not undo already executed callbacks. Partial socket reads are neither complete frames nor successful EOF. Control-frame changes likewise publish a complete valid candidate. Existing [decoder completed-prefix semantics](../protocol/STREAM-DATA.md) and offline readers/replay are unchanged.

Each queued delivery owns immutable values, exact symbol/flags and provenance, and identifies its connection epoch, binding snapshot and membership activation. Sender IDs are not delivery identity. Borrowed views are valid only during their callback. Retaining an explicitly owning delivery keeps its quota reservation until its last lease is released; caller-created independent deep copies are caller-owned and outside library resident accounting. Neither copying a schema handle nor a callback span manufactures compatible provenance.

The dispatcher invokes no user code while holding a mutation/container lock or a borrowed iterator into erasable registration storage. It protects the executing callable itself, its arguments and required backing state through return or exception. Self-remove, request-stop and release of the last public registration/endpoint handle are permitted; those handles initiate nonblocking retirement/abort, while the run and execution host preserve progress ownership until quiescence. Releasing a handle is not synchronous close completion.

Callback mutation takes effect after return; recursive delivery is forbidden. Exceptions retire the affected registration with an observable callback-failure cause and its pending counts; side effects are not retried. Other registrations may continue when shared state remains valid. Exception/diagnostic text is bounded and sanitized; secrets are not copied into public errors.

Callbacks must return promptly. A consumer that withholds queue credits/owning leases is isolated by quotas; arbitrary blocking user code in the serialized domain can stall that domain. The runtime cannot preempt such C++ code or truthfully publish quiescence while it still executes.

## Epochs, bindings and recovery

Use distinct monotonic identities for a connection epoch, immutable binding publication, registration lifetime, registration revision and membership activation. Arithmetic never wraps or reuses a live/stale identity: reject the operation or fail the owning run before exhaustion. Equal shapes do not imply equal contexts.

Desired subscriptions live outside connection-local parser/composer/negotiation/output state. A new connection creates fresh directional state, drops old raw input and unsent output, and authenticates/negotiates before emitting the current desired snapshot. Offline removals must not be resurrected by replaying stale queued deltas. A locally applied or emitted subscription is not a peer acknowledgement; the selected binary profile must not invent a per-key acknowledgement absent from the measured protocol.

Reset is an owner barrier. Finish any executing callback, discard old-epoch pending deliveries with visible gap/discard accounting, and reset collector freshness/completeness before new-epoch data. The reset notice precedes any new-epoch data callback. Old caller-held owning values remain valid values carrying their old epoch; they are never current collector state. A gap can have an unknown number of lost upstream records; do not fabricate a count, exactly-once delivery or resumable history.

Within one connection, a new peer binding snapshot does not mutate already owning normalized records. Such records can remain queued when their immutable local context and membership are still compatible; binding publication alone does not silently discard them. No queued record is reinterpreted through a newer sender table. Peer-ID reuse, description replacement and readiness/auth ordering require the selected production evidence before protocol implementation.

## Finite resources and work

Limits are explicit immutable configuration, with documented units and scope. No universal numerical defaults are selected here. Zero is a zero capacity, never “unlimited”. Validate arithmetic and required relationships before activation/I/O. Configuration needing initial schema or reserved control storage cannot activate with insufficient capacity.

| Dimension | Required charge |
| --- | --- |
| Desired keys/selectors | Distinct resident entries and all owned UTF-16 capacity; validate input duplicates/work even when they collapse |
| Queues/deliveries | Items, owned payload capacity and leases, including executing and externally retained library allocations |
| Schema/binding/staging | Old published, candidate and temporary owners together at peak; count shared allocations once within their accounting scope |
| Retired generations | Pending operations, outstanding leases and control/completion storage until actually released |
| Per-command/turn work | Visited keys, examined/copied UTF-16 units, commands, frames and callbacks; codec-specific counters retain their existing meanings |
| Diagnostics/controls | Finite causes/text/counts and reserved reset/terminal storage, never an unbounded error list |

Count backing capacity requested by library containers, not merely their nonempty logical contents. Item/key/generation limits cover metadata cardinality. These are logical resource bounds, not an allocator-overhead/process-RSS guarantee. Accounting counters must not overflow; only diagnostic cumulative counters may saturate with an explicit overflow flag.

Do not transfer a tape reader's cumulative file/frame/work ceiling to the lifetime of a continuing feed. Resident limits remain in force until owners release. Per-operation/turn work resets only at its documented boundary; failures/retries within that turn do not refund work. A bounded frame decode may finish within its own finite limit or reject before admission; it need not be made resumable by this contract. Scheduling checks its conservative bounded cost before starting another frame and yields between bounded operations. Optional explicit lifetime quotas are distinct settings and must never appear implicitly.

A registration reserves its configured data and control credits before becoming active. Aggregate reservations cannot exceed context capacity; held leases keep their reservation even after retirement. This prevents one slow client from consuming another active client's reserved credits. Shared parser/cache exhaustion remains an explicit shared-scope failure; isolation is not a promise of infinite progress after the application's total resource pool is exhausted.

STREAM overflow does not silently conflate/drop and continue as lossless. Fail/retire the affected consumer with visible gap/counts; healthy consumers keep their reserved capacity. HISTORY overflow/interruption invalidates completeness before further delivery. TICKER conflation is permitted only by its measured contract. Blocking a shared owner waiting for consumer space is not an overflow policy.

## Reserved control delivery

Reserve at least one pending-reset slot and one terminal slot per registration at creation, plus the bounded completion state needed for accepted commands. Retirement/abort requests have an allocation-free, idempotent control path that ordinary queue saturation cannot block. No ordinary data enqueue can consume this reservation.

Reset/gap notices may be coalesced only as a documented interval from the earliest affected epoch to the newest, with a skipped-reset count, bounded cause fields and an explicit unknown-loss marker where needed. They cannot disappear or be relabeled as one uninterrupted epoch. Before new-epoch data, the client can observe the pending interval. A terminal notice retains final cause/integrity/counts even when its data queue is full; querying terminal state must not require making space there. No requirement for an unbounded callback/log per intermediate transition is introduced.

## Lifecycle and truthful completion

These are logical states, not committed C++ enum spellings.

| State | Permitted progress / exit |
| --- | --- |
| Configured | Accept desired interests; start with validated limits/schema; no implied I/O |
| Starting / negotiating | Own bounded pending operations; accept desired changes; do not emit interests before selected readiness/auth |
| Active | Admit valid frames and deliver eligible memberships in source order |
| Resetting / retry-wait | Preserve desired state; publish reset before new data; retry only under the selected explicit policy |
| Quiescing | Stop new intake/ordinary commands, drain the defined admitted prefix or abort; allow idempotent/escalating stop |
| Terminal | Immutable outcome after quiescence; no new library callback/operation starts; no reopening this run |

The graceful cutoff is application of its stop command in owner order. Stop new read admission and ordinary mutations, cancel not-yet-applied commands with an explicit result, and drain eligible data admitted before the cutoff. An incomplete input frame is unadmitted, not successful EOF; count its discarded bytes separately. A callback may finish its current borrowed batch; request-abort/handle retirement remains available during drain. No extra peer unsubscribe/close message is invented.

Already-started outgoing work must finish within its configured stop policy or be aborted with an explicit partial-write/cause result. Required sink flush and runtime/TLS closure belong to the composing adapter/tool; their failure cannot become successful completion. Before activation, graceful stop cancels pending connect/negotiation rather than waiting for market data.

There is one terminal outcome per registration and per run. A drained outcome states only that the defined still-eligible admitted prefix and required flush/closure completed; it never certifies a gap-free upstream lifetime. Return prior gap/unknown-loss state, withdrawn/discarded counts, failed registrations and secondary cleanup errors. Forced drain discard/escalation is aborted or failed, not clean drained. User abort is distinct from successful drain.

The first observed non-cancellation failure remains the primary cause in serialized owner order; later stop requests never replace it. Expected cancellation caused by stop is not a new failure. If no earlier failure exists, a stop timeout is a failure; later close/flush faults are retained as bounded secondary causes. An already recorded failure is not erased by reaching an empty queue.

Quiescence means no executing library-owned callback, no pending operation able to access the retired state and no future delivery that can start. Caller-held immutable deliveries and detached completed result values may survive; they retain no callable or live operation. Publish terminal completion only after this boundary. A deadline escalates owned cancellation/discard but cannot force an arbitrary callback to return. If the caller stops pumping or violates execution-host/callback preconditions, report lack of completion rather than fabricate quiescence.

## Compatibility prerequisites

The [finite binary message contract](../protocol/BINARY-MESSAGES.md) currently rejects subscription messages. [P03/P05 captures](../protocol/CAPTURES.md) are limited subscription/remapping evidence; P05 suppresses nondeterministic heartbeat/protocol metadata and is not full handshake evidence. The [schema-exchange source study](../protocol/SCHEMA-EXCHANGE.md) and [offline replay contract](../protocol/OFFLINE-REPLAY.md) do not implement this live lifecycle.

Before native state/wire/collectors, capture the selected peer's add/remove families, HISTORY raw-bound handling, exact/empty/reserved selectors, descriptor/readiness/auth/heartbeat behavior and membership/initial/latest/snapshot/transaction/removal cases. Distinguish source inspection, production observations and native tests. Never classify a future collector row as supported because generic data decoding exists.

The design fixes local ownership, mutation, work and epoch boundaries. Peer-dependent protocol/collector details, literal escapes, broader field conversions, PART/compression requirements and exact authentication options remain blocking evidence rows for their implementation. The separate runtime decision must establish executable progress/cancellation/trust behavior on each supported platform. No additional framework, persistent journal, business-event catalog or speculative recovery mechanism is added here.
