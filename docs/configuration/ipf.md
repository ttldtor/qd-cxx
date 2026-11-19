# Instrument profiles

[Русский](ipf.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [ProfileCollectorLimits](ipf.md#cfg-0345-owner) — `include/qd/ipf/instrument_profile_collector.hpp`
- [IpfConnectionLimits](ipf.md#cfg-0355-owner) — `include/qd/ipf/instrument_profile_connection.hpp`
- [IpfConnectionRuntimeOptions](ipf.md#cfg-0365-owner) — `include/qd/ipf/instrument_profile_connection.hpp`
- [IpfReadLimits](ipf.md#cfg-0368-owner) — `include/qd/ipf/instrument_profile_reader.hpp`
- [IpfReadOptions](ipf.md#cfg-0382-owner) — `include/qd/ipf/instrument_profile_reader.hpp`
- [IpfReadWork](ipf.md#cfg-0378-owner) — `include/qd/ipf/instrument_profile_reader.hpp`
- [IpfEnvelopeLimits](ipf.md#cfg-0385-owner) — `include/qd/ipf/instrument_profile_url_reader.hpp`
- [IpfTransferLimits](ipf.md#cfg-0392-owner) — `include/qd/ipf/instrument_profile_url_reader.hpp`
- [IpfTransferOptions](ipf.md#cfg-0415-owner) — `include/qd/ipf/instrument_profile_url_reader.hpp`
- [IpfWriteLimits](ipf.md#cfg-0432-owner) — `include/qd/ipf/instrument_profile_writer.hpp`
- [IpfWriteOptions](ipf.md#cfg-0439-owner) — `include/qd/ipf/instrument_profile_writer.hpp`
- [ProfileLimits](ipf.md#cfg-0440-owner) — `include/qd/ipf/instrument_profile.hpp`
- [OptionChainLimits](ipf.md#cfg-0445-owner) — `include/qd/ipf/option_chains.hpp`

## ProfileCollectorLimits {#cfg-0345-owner}

[include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp)

### `ProfileCollectorLimits::modelProfiles` — CFG-0345 {#cfg-0345}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: active profiles in current model
- Accepted values: create permits zero; adding a new active profile requires staged.size()&lt;modelProfiles. Replacement/removal need not increase size.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::liveRevisions` — CFG-0346 {#cfg-0346}

- Type: `std::size_t`. Declared default: `= 200000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneously retained profile revisions
- Accepted values: Any configured grant including 0. Every copied revision needs one REVISIONS credit; external snapshots/batches keep old revisions charged until release.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::retainedUnits` — CFG-0347 {#cfg-0347}

- Type: `std::size_t`. Declared default: `= 16 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of retained profiles
- Accepted values: Each new revision reserves Profile.retainedUnits() in a shared ledger. Zero permits zero usage only; old revisions and new candidates can consume the grant simultaneously.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::customFields` — CFG-0348 {#cfg-0348}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: custom fields per copied profile
- Accepted values: CollectorProfileAccess::fields(profile)&lt;=customFields before copying. Zero permits profiles without custom fields; standard fields have their own values/storage.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::stagingProfiles` — CFG-0349 {#cfg-0349}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input/staging profiles per atomic operation
- Accepted values: For update count=input.size(); for removal count=model.size(). Both count and tags.size() must be&lt;=stagingProfiles. Zero permits empty short-circuit operations but may reject removal from a nonempty model.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::listeners` — CFG-0350 {#cfg-0350}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: registered collector listeners
- Accepted values: listeners.size()&lt;listeners before registration. Zero forbids listeners; finite token IDs, snapshot work and deliveryCalls are separate constraints.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::liveBatches` — CFG-0351 {#cfg-0351}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneously live revision batches
- Accepted values: Creating a batch reserves one BATCHES credit independently of item count. Zero prevents even an empty batch object; external retention keeps the reservation.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::batchReferences` — CFG-0352 {#cfg-0352}

- Type: `std::size_t`. Declared default: `= 1000000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate revision references in all live batches
- Accepted values: Batch construction debits source.size() REFERENCES. Zero permits an empty batch only if BATCHES is available; references stay charged until batch destruction.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::workUnits` — CFG-0353 {#cfg-0353}

- Type: `std::size_t`. Declared default: `= 64 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: work units per collector operation
- Accepted values: apply/listen receive a fresh workUnits allowance per operation. Empty updates can return before debiting;0 rejects connection-domain acquisition and positive-cost operations.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::deliveryCalls` — CFG-0354 {#cfg-0354}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: admitted callback tasks per dispatch cycle
- Accepted values: Checks amount&lt;=deliveryCalls and already&lt;=deliveryCalls-amount. Zero permits operations with no callback tasks; reentrant updates share cycleReserved.
- Read timing: InstrumentProfileCollector::create captures values without generally forbidding 0. apply/listen/batch reserve resources before publishing a candidate; ledger charges retire with external owners.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileCollectorError::LIMIT_EXCEEDED for this budget excess. Allocation failures throw std::bad_alloc before publication; ProfileCollectorError has no ALLOCATION_FAILURE member.
- Consumer sources: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

## IpfConnectionLimits {#cfg-0355-owner}

[include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp)

### `IpfConnectionLimits::batchProfiles` — CFG-0355 {#cfg-0355}

- Type: `std::size_t`. Declared default: `= 1000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: profiles in a pending batch
- Accepted values: Strictly&gt;0 in Runtime::validate. pending.reserve(batchProfiles); reaching the size flushes the batch. Count participates in checked owner backing; the collector has independent limits.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / connection-profile returns directly from Runtime::create before publication for zero batchProfiles. Reaching positive batchProfiles triggers flush; collector failures during flush belong to sink processing and enter observed connection state.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::pendingUnits` — CFG-0356 {#cfg-0356}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of pending profiles
- Accepted values: Zero is permitted. Every whole profile retainedUnits must fit remaining pendingUnits; residentBytes and appendPendingProfile also constrain byte backing.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfConnectionDiagnostic.transfer: IpfTransferError::RESOURCE_LIMIT / connection-pending-units or connection-pending-backing. The worker/sink branch records the refusal in observed diagnostics through Port::fail.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::oldGenerations` — CFG-0357 {#cfg-0357}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retained source generations
- Accepted values: validate permits zero, but first flush of a new generation rejects old.size()&gt;=oldGenerations. oldGenerations*sizeof(uint64_t) participates in owner backing.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfConnectionDiagnostic.transfer: IpfTransferError::RESOURCE_LIMIT / connection-generations. The worker/sink branch records the refusal in observed diagnostics through Port::fail.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::workUnits` — CFG-0358 {#cfg-0358}

- Type: `std::size_t`. Declared default: `= 64 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative connection work
- Accepted values: Zero prevents domain start with connection-domain-work. Later charge checks amount&lt;=workUnits and spent&lt;=workUnits-amount; consumption persists across retries.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero workUnits: detached WORK_LIMIT / connection-domain-work from connection create before publication. Later copy-work exhaustion: WORK_LIMIT / connection-copy-control-work; the sink calls Port::fail and records observed diagnostics.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp).

### `IpfConnectionLimits::attempts` — CFG-0359 {#cfg-0359}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: attempts over connection lifetime
- Accepted values: Runtime::validate allows zero, but the worker stops before the first attempt when attempts reaches the limit. Retries share one lifetime counter.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfConnectionDiagnostic.transfer: IpfTransferError::WORK_LIMIT / connection-lifetime. The worker/sink branch records the refusal in observed diagnostics through Port::fail.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::listenerSlots` — CFG-0360 {#cfg-0360}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: state listeners including retiring captures
- Accepted values: validate permits zero and the first ObserverLease then rejects. A slot retires after capture destruction, not merely list removal. Array backing is included in residentBytes.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / connection-listener-slots returns to the listen caller as a detached unexpected. Registration failure does not change Port::observed.diagnostic. std::bad_alloc creating Observer can propagate before registration publication.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::residentBytes` — CFG-0361 {#cfg-0361}

- Type: `std::size_t`. Declared default: `= 16 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: connection-owned backing bytes
- Accepted values: Must cover runtime/port/control/sink/ledger, completionWaiters, batchProfiles, pendingUnits, oldGenerations, listenerSlots and trust/source/auth copies. Products/sums are checked before allocation;0 cannot fit the state.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / connection-owner-backing returns from Runtime::create validation before connection publication. It belongs to the call result; observed connection state does not yet exist.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp).

### `IpfConnectionLimits::lifetime` — CFG-0362 {#cfg-0362}

- Type: `std::chrono::milliseconds`. Declared default: `{86400000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: connection lifetime milliseconds
- Accepted values: &gt;0 and &lt;=duration_cast&lt;milliseconds&gt;(steady_clock::duration::max()/4). Origin is fixed at start; absolute deadline saturates at time_point::max. Expiry is checked between bounded turns.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Nonpositive/excessive lifetime: detached INVALID_OPTIONS / connection-profile before runtime publication. Expiry of an admitted lifetime in the worker loop: WORK_LIMIT / connection-lifetime in observed connection diagnostics.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::completionWaiters` — CFG-0363 {#cfg-0363}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneous completion waiters
- Accepted values: Zero rejects a blocking wait for an unfinished connection; already-complete results return before slot admission. Frames remain charged through deregistration/retirement.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: RESOURCE_LIMIT / connection-wait-capacity returns to blocking await as a detached IpfConnectionDiagnostic. Refusing a waiter does not write Port::observed.diagnostic or terminate the connection.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_wait.hpp](../../src/ipf/connection_wait.hpp).

### `IpfConnectionLimits::completionWaitTurns` — CFG-0364 {#cfg-0364}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: completion-wait turns per call
- Accepted values: Zero returns WORK_LIMIT if the result is not yet ready. Positive remaining turns decrement before each condition wait; deadline/cancellation/completion are checked separately.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: WORK_LIMIT / connection-wait-work returns to the waiter at zero or when its wait loop is exhausted. This is detached await diagnostics; Port::observed.diagnostic remains the connection state.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_wait.hpp](../../src/ipf/connection_wait.hpp).

## IpfConnectionRuntimeOptions {#cfg-0365-owner}

[include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp)

### `IpfConnectionRuntimeOptions::maximumConnections` — CFG-0365 {#cfg-0365}

- Type: `std::size_t`. Declared default: `= 1`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: connections per runtime
- Accepted values: Exactly 1:0 and values&gt;1 yield INVALID_OPTIONS. Implementation owns one lifetime slot; attempting to reuse an occupied slot yields connection-lifetime-slot.
- Read timing: IpfConnectionRuntime::create validates the owner envelope; Port captures limits. Sink, worker, listen and await consume them in the indicated phases.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: maximumConnections!=1: detached INVALID_OPTIONS / connection-profile from Runtime::create. Reusing an occupied/stopping runtime: detached INVALID_STATE / connection-lifetime-slot from connection create. These refusals return to the caller and do not write an existing Port::observed.diagnostic.
- Consumer sources: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp).

### `IpfConnectionRuntimeOptions::transfer` — CFG-0366 {#cfg-0366}

- Type: `IpfTransferOptions`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Existing finite source/native/TLS/HTTP/decoder grants, cumulative across retries.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Standalone connection runtime create; transfer grants cumulative across source attempts; own lifetime model/listener/waiter limits
- Mutability and lifetime: one lifetime connection slot; ordinary source/listener lifecycle APIs do not refill attempts/work
- Failure: Owner error vocabulary: IpfCompletionWaitStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `IpfCompletionWaitStatus`.

### `IpfConnectionRuntimeOptions::limits` — CFG-0367 {#cfg-0367}

- Type: `IpfConnectionLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Independent pending/model-control bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Standalone connection runtime create; transfer grants cumulative across source attempts; own lifetime model/listener/waiter limits
- Mutability and lifetime: one lifetime connection slot; ordinary source/listener lifecycle APIs do not refill attempts/work
- Failure: Owner error vocabulary: IpfCompletionWaitStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `IpfCompletionWaitStatus`.

## IpfReadLimits {#cfg-0368-owner}

[include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp)

### `IpfReadLimits::inputBytes` — CFG-0368 {#cfg-0368}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: accepted input bytes over parser lifetime
- Accepted values: Each byte requires progress.inputBytes&lt;inputBytes before admission. Zero rejects nonempty input but not construction/EOF itself. feed fragmentation never replenishes the grant.
- Read timing: Reader captures IpfReadOptions; the incremental parser checks remainders when accepting bytes, rows, formats or whole profiles.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Terminal IpfDiagnostic with IpfReadError::INPUT_LIMIT and source position; the previously emitted prefix remains valid. bad_alloc yields ALLOCATION_FAILURE.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::fieldUnits` — CFG-0369 {#cfg-0369}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 decoded-field units
- Accepted values: cell.size()&lt;fieldUnits before appending a unit;0 rejects nonempty field text. CSV delimiters/quotes have separate recordUnits accounting.
- Read timing: Reader captures IpfReadOptions; the incremental parser checks remainders when accepting bytes, rows, formats or whole profiles.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Terminal IpfDiagnostic with IpfReadError::FIELD_LIMIT and source position; the previously emitted prefix remains valid. bad_alloc yields ALLOCATION_FAILURE.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::recordUnits` — CFG-0370 {#cfg-0370}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: decoded syntax/text units per row
- Accepted values: recordUnits bounds the complete current row; stagedUnits is also checked against it. Zero permits no positive debit; the per-row counter resets for a new row.
- Read timing: Reader captures IpfReadOptions; the incremental parser checks remainders when accepting bytes, rows, formats or whole profiles.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Terminal IpfDiagnostic with IpfReadError::RECORD_LIMIT and source position; the previously emitted prefix remains valid. bad_alloc yields ALLOCATION_FAILURE.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::fieldsPerRecord` — CFG-0371 {#cfg-0371}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: fields per row
- Accepted values: Every new field index must be&lt;fieldsPerRecord. Zero rejects a field; exact declared-format field count is checked separately.
- Read timing: Reader captures IpfReadOptions; the incremental parser checks remainders when accepting bytes, rows, formats or whole profiles.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Terminal IpfDiagnostic with IpfReadError::RECORD_LIMIT and source position; the previously emitted prefix remains valid. bad_alloc yields ALLOCATION_FAILURE.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::records` — CFG-0372 {#cfg-0372}

- Type: `std::size_t`. Declared default: `= 1000000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: accepted logical rows
- Accepted values: progress.records&lt;records before accepting a row;0 rejects the first row. Control/format rows follow row-dispatch accounting; profiles have a separate counter.
- Read timing: Reader captures IpfReadOptions; the incremental parser checks remainders when accepting bytes, rows, formats or whole profiles.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Terminal IpfDiagnostic with IpfReadError::RECORD_LIMIT and source position; the previously emitted prefix remains valid. bad_alloc yields ALLOCATION_FAILURE.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::formats` — CFG-0373 {#cfg-0373}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneously retained formats
- Accepted values: A new format requires formats.size()&lt;formats. Redefining an existing key needs no new slot;0 rejects the first format.
- Read timing: Reader captures IpfReadOptions; the incremental parser checks remainders when accepting bytes, rows, formats or whole profiles.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Terminal IpfDiagnostic with IpfReadError::FORMAT_LIMIT and source position; the previously emitted prefix remains valid. bad_alloc yields ALLOCATION_FAILURE.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::headerUnits` — CFG-0374 {#cfg-0374}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 retained format/field-name units
- Accepted values: New stagedUnits must fit alongside retained headers in headerUnits; replacement accounts for the retired description. Zero permits no positive header text.
- Read timing: Reader captures IpfReadOptions; the incremental parser checks remainders when accepting bytes, rows, formats or whole profiles.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Terminal IpfDiagnostic with IpfReadError::HEADER_LIMIT and source position; the previously emitted prefix remains valid. bad_alloc yields ALLOCATION_FAILURE.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::profiles` — CFG-0375 {#cfg-0375}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: emitted profiles over parser lifetime
- Accepted values: Emission requires progress.profiles&lt;profiles. Zero permits control/format parsing but rejects the first emitted profile.
- Read timing: Reader captures IpfReadOptions; the incremental parser checks remainders when accepting bytes, rows, formats or whole profiles.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Terminal IpfDiagnostic with IpfReadError::OUTPUT_LIMIT and source position; the previously emitted prefix remains valid. bad_alloc yields ALLOCATION_FAILURE.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::profileUnits` — CFG-0376 {#cfg-0376}

- Type: `std::size_t`. Declared default: `= 16777216`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 emitted-profile units
- Accepted values: currentUnits must fit remaining profileUnits; aggregate progress.profileUnits persists across feed calls. Zero permits zero-retained-unit profiles only when count is separately available.
- Read timing: Reader captures IpfReadOptions; the incremental parser checks remainders when accepting bytes, rows, formats or whole profiles.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Terminal IpfDiagnostic with IpfReadError::OUTPUT_LIMIT and source position; the previously emitted prefix remains valid. bad_alloc yields ALLOCATION_FAILURE.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::profile` — CFG-0377 {#cfg-0377}

- Type: `ProfileLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Independent limits for each owned profile value.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: IpfReadOptions nested parser limits captured by reader creation; cumulative read contract
- Mutability and lifetime: nested explicit input follows owning options; changing original aggregate does not mutate existing load/parser; new operation needs explicit new options
- Failure: Owner error vocabulary: IpfReadError, IpfReadStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `IpfReadError, IpfReadStatus`.

## IpfReadOptions {#cfg-0382-owner}

[include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp)

### `IpfReadOptions::limits` — CFG-0382 {#cfg-0382}

- Type: `IpfReadLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Independent input/storage/output bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] IpfReadResult readInstrumentProfiles(std::istream &amp;input, IpfReadOptions options = {}) / [[nodiscard]] IpfReadResult readInstrumentProfilesFile(const std::filesystem::path &amp;path, IpfReadOptions options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: IpfReadError, IpfReadStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `IpfReadError, IpfReadStatus`.

### `IpfReadOptions::completion` — CFG-0383 {#cfg-0383}

- Type: `IpfCompletionPolicy`. Declared default: `= IpfCompletionPolicy::ALLOW`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: clean-EOF completion policy
- Accepted values: ALLOW accepts clean EOF without &#35;&#35;COMPLETE; REQUIRE returns MISSING_COMPLETE when no marker exists. Code checks equality with REQUIRE only: an unknown enum value currently follows ALLOW behavior; no separate INVALID_OPTIONS rejects it.
- Read timing: At explicit input completion after processing the last row.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfReadError::MISSING_COMPLETE only for REQUIRE without an admitted completion marker; an invalid enum value has no independent rejection.
- Consumer sources: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadOptions::turnWork` — CFG-0384 {#cfg-0384}

- Type: `IpfReadWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive per-turn quotas used only by synchronous convenience readers.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive per-turn quotas used only by synchronous convenience readers.
- Read timing: Explicit argument of public operations: [[nodiscard]] IpfReadResult readInstrumentProfiles(std::istream &amp;input, IpfReadOptions options = {}) / [[nodiscard]] IpfReadResult readInstrumentProfilesFile(const std::filesystem::path &amp;path, IpfReadOptions options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: IpfReadError, IpfReadStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `IpfReadError, IpfReadStatus`.

## IpfReadWork {#cfg-0378-owner}

[include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp)

### `IpfReadWork::inputBytes` — CFG-0378 {#cfg-0378}

- Type: `std::size_t`. Declared default: `= 4096`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Bytes accepted this call, including malformed UTF-8 bytes.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `IpfReadError, IpfReadStatus`.

### `IpfReadWork::decodedUnits` — CFG-0379 {#cfg-0379}

- Type: `std::size_t`. Declared default: `= 4096`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: UTF-16 units processed this call, including replacements and syntax.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `IpfReadError, IpfReadStatus`.

### `IpfReadWork::fields` — CFG-0380 {#cfg-0380}

- Type: `std::size_t`. Declared default: `= 256`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Completed field conversion/metadata operations this call.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `IpfReadError, IpfReadStatus`.

### `IpfReadWork::records` — CFG-0381 {#cfg-0381}

- Type: `std::size_t`. Declared default: `= 256`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete row admission operations this call.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `IpfReadError, IpfReadStatus`.

## IpfEnvelopeLimits {#cfg-0385-owner}

[include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp)

### `IpfEnvelopeLimits::inputBytes` — CFG-0385 {#cfg-0385}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: accepted encoded body/source bytes
- Accepted values: Zero is permitted by the envelope and rejects the first input byte. Includes ignored trailing bytes; fragmentation does not replenish it.
- Read timing: SourceEnvelope captures per-load limits. feed/inflater/ZIP parsing debit before admission; streaming runtime preserves consumption across continuations.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / envelope-input. The first failure is retained.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::outputBytes` — CFG-0386 {#cfg-0386}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: produced decoded bytes
- Accepted values: Zero forbids positive output. Skipped ZIP-directory payload also counts; inflater may receive a one-byte probe window to recognize the end, but produced&gt;remaining still rejects.
- Read timing: SourceEnvelope captures per-load limits. feed/inflater/ZIP parsing debit before admission; streaming runtime preserves consumption across continuations.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / envelope-output. The first failure is retained.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::calls` — CFG-0387 {#cfg-0387}

- Type: `std::size_t`. Declared default: `= 1000000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: decoder progress calls
- Accepted values: feed requires calls below the limit;0 rejects the first working call. No-progress and failing calls consume a credit, except the early failed/pending-output return.
- Read timing: SourceEnvelope captures per-load limits. feed/inflater/ZIP parsing debit before admission; streaming runtime preserves consumption across continuations.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferDiagnostic: IpfTransferError::WORK_LIMIT / envelope-calls. The first failure is retained.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::workBytes` — CFG-0388 {#cfg-0388}

- Type: `std::size_t`. Declared default: `= 1073741824`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input/output/zeroed-allocation byte visits
- Accepted values: Monotonic workBytes grant; each debit checks used&lt;=limit and amount&lt;=limit-used. Zero permits paths with no positive debit only. Temporary allocation work is also charged.
- Read timing: SourceEnvelope captures per-load limits. feed/inflater/ZIP parsing debit before admission; streaming runtime preserves consumption across continuations.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferDiagnostic: IpfTransferError::WORK_LIMIT / envelope-work. The first failure is retained.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::inflaterBytes` — CFG-0389 {#cfg-0389}

- Type: `std::size_t`. Declared default: `= 131072`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneously owned zlib bytes including headers
- Accepted values: Checks items*size+sizeof(Allocation) and allocated+bytes&lt;=inflaterBytes. free releases current owned storage, while work stays consumed. Zero permits uncompressed input but no inflater allocation.
- Read timing: SourceEnvelope captures per-load limits. feed/inflater/ZIP parsing debit before admission; streaming runtime preserves consumption across continuations.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / envelope-inflater-backing or envelope-allocation-overflow; ALLOCATION_FAILURE / envelope-allocation. The first failure is retained.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::metadataBytes` — CFG-0390 {#cfg-0390}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate ZIP local-header/name/extra bytes
- Accepted values: Sums header_.size()+nameSize+extraSize per inspected ZIP entry. Zero cannot fit a ZIP local header; gzip/uncompressed paths do not consume it.
- Read timing: SourceEnvelope captures per-load limits. feed/inflater/ZIP parsing debit before admission; streaming runtime preserves consumption across continuations.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / zip-metadata. The first failure is retained.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::entries` — CFG-0391 {#cfg-0391}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: gzip members or ZIP directories/selected file
- Accepted values: Each admitted member/entry requires entries below the limit. Zero rejects the first gzip member/ZIP entry but not uncompressed IPF. This check does not imply support for every later ZIP file.
- Read timing: SourceEnvelope captures per-load limits. feed/inflater/ZIP parsing debit before admission; streaming runtime preserves consumption across continuations.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / envelope-entries. The first failure is retained.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

## IpfTransferLimits {#cfg-0392-owner}

[include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp)

### `IpfTransferLimits::sourceBytes` — CFG-0392 {#cfg-0392}

- Type: `std::size_t`. Declared default: `= 8192`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Initial source cap, at most the fixed 8192-byte backing.
- Units: bytes; scope defined by field contract
- Declared constraints: Initial source cap, at most the fixed 8192-byte backing.
- Read timing: IpfTransferOptions nested whole-load finite framing/envelope/native preparation; redirects/attempts do not reset grants
- Mutability and lifetime: nested explicit input follows owning options; changing original aggregate does not mutate existing load/parser; new operation needs explicit new options
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferLimits::locationBytes` — CFG-0393 {#cfg-0393}

- Type: `std::size_t`. Declared default: `= 8192`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Each redirect field cap, at most fixed source backing.
- Units: bytes; scope defined by field contract
- Declared constraints: Each redirect field cap, at most fixed source backing.
- Read timing: IpfTransferOptions nested whole-load finite framing/envelope/native preparation; redirects/attempts do not reset grants
- Mutability and lifetime: nested explicit input follows owning options; changing original aggregate does not mutate existing load/parser; new operation needs explicit new options
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferLimits::authorizationBytes` — CFG-0394 {#cfg-0394}

- Type: `std::size_t`. Declared default: `= 8192`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete Authorization value bytes
- Accepted values: 1..8192 in validateOptions. Sizes the secret buffer; Basic/explicit/provider tokens must additionally satisfy syntax and fit. Whole requestBytes may be smaller.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::AUTHORIZATION_FAILURE / authorization-provider; INVALID_OPTIONS / transfer-options. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::headerBytes` — CFG-0395 {#cfg-0395}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: HTTP status/header section bytes
- Accepted values: 1..UINT32_MAX. Passed to Beast header_limit(uint32_t); actual status/fields must fit. Zero rejects before I/O.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options; Beast excess → HTTP_FAILURE / http-parse. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::headerFields` — CFG-0396 {#cfg-0396}

- Type: `std::size_t`. Declared default: `= 128`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: HTTP header fields per response
- Accepted values: Strictly&gt;0. on_field_impl increments fields_ and requires &lt;=headerFields; a new response resets this count, while work and informational responses have separate aggregate bounds.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / http-header-fields. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::trailerBytes` — CFG-0397 {#cfg-0397}

- Type: `std::size_t`. Declared default: `= 16384`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: trailer-section bytes including final CRLF
- Accepted values: 2..UINT32_MAX-2. After headers, Beast receives trailerBytes-2 because its accounting adds final-chunk CRLF. Minimum 2 allows only an empty trailer section under parser rules.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options; Beast excess → HTTP_FAILURE / http-parse. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::trailerFields` — CFG-0398 {#cfg-0398}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: trailer fields per response
- Accepted values: Zero permits no trailers. Every field checks ++trailers_&lt;=trailerFields; routing/authentication headers are separately forbidden in trailers.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / http-trailer-fields. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::bodyBytes` — CFG-0399 {#cfg-0399}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative HTTP payload bytes
- Accepted values: The setting permits zero. Beast body_limit and aggregate work.body bound payload;0 rejects nonempty bodies. Status 200 feeds IPF; other accepted bodies still consume the counter.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / http-payload-bytes; early Beast body_limit → HTTP_FAILURE / http-parse. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::wireBytes` — CFG-0400 {#cfg-0400}

- Type: `std::size_t`. Declared default: `= 83886080`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: accepted transport/file input bytes
- Accepted values: Zero permits no positive input. HTTP/local-file paths charge result.wireBytes; redirects share the cumulative counter.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / http-input-capacity for HTTP; RESOURCE_LIMIT / source-input-limit for a local file. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::framingBytes` — CFG-0401 {#cfg-0401}

- Type: `std::size_t`. Declared default: `= 8388608`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: HTTP framing byte visits
- Accepted values: Each put debits consumed-body;0 rejects the first positive framing charge. Payload is independently bounded; consumed&lt;body is separately rejected.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::WORK_LIMIT / http-framing-work. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::chunkExtensionBytes` — CFG-0402 {#cfg-0402}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: extension bytes per chunk header
- Accepted values: extensions.size()&lt;=chunkExtensionBytes. Zero permits chunks without extensions; totalChunkExtensionBytes is charged as well.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / http-chunk-extensions. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::totalChunkExtensionBytes` — CFG-0403 {#cfg-0403}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate chunk-extension bytes
- Accepted values: charge(work.extensions,extensions.size(),totalChunkExtensionBytes) precedes admission. Zero permits zero-length extensions only; repeated headers consume the shared remainder.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / http-chunk-extensions. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::redirects` — CFG-0404 {#cfg-0404}

- Type: `std::size_t`. Declared default: `= 5`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: followed HTTP redirects
- Accepted values: Zero forbids the first redirect but permits direct loads. Every transition charges one; target/origin/downgrade have separate checks. This layer performs no automatic connection retries.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::REDIRECT_REJECTED / redirect-count. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::informationalResponses` — CFG-0405 {#cfg-0405}

- Type: `std::size_t`. Declared default: `= 8`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: nonfinal HTTP responses
- Accepted values: Zero permits final responses without informationals. Each nonfinal response charges one and creates a fresh parser; the aggregate allowance is not replenished.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / http-informational-count. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::attempts` — CFG-0406 {#cfg-0406}

- Type: `std::size_t`. Declared default: `= 6`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: HTTP connections over a load
- Accepted values: Zero stops before the first connection. Each connection/redirect charges one attempt; redirects have an independent cap. Local files create no HTTP connection.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / http-attempts. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::parserCalls` — CFG-0407 {#cfg-0407}

- Type: `std::size_t`. Declared default: `= 1000000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: HTTP parser invocations including EOF
- Accepted values: Zero rejects the first put/EOF parser call. work.calls persists across redirects/informational responses; IPF/envelope calls have independent budgets.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::WORK_LIMIT / http-parser-work. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::presentedBytes` — CFG-0408 {#cfg-0408}

- Type: `std::size_t`. Declared default: `= 268435456`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative request/copy/parser/profile byte visits
- Accepted values: Zero permits no positive preparation/copying. One wire byte can be charged repeatedly for copying, put, retained suffixes and profile publication; this counts work, not network volume.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / http-request-capacity or http-input-capacity; RESOURCE_LIMIT / source-input-limit for local-file input; WORK_LIMIT / http-parser-work or http-framing-work for repeated presentation/data shifting. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::cleanupTurns` — CFG-0409 {#cfg-0409}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native retirement-reserve turns
- Accepted values: Zero is permitted; cleanupWork gets min(nativeWork.steps/handlers,cleanupTurns), partitioned from total nativeWork. If retirement requires unavailable progress, WORK_LIMIT is retained and fallback retirement runs.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::WORK_LIMIT / transfer-cleanup-work; an earlier cause may be TLS/TRANSPORT_FAILURE. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::turns` — CFG-0410 {#cfg-0410}

- Type: `std::size_t`. Declared default: `= 1000000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bounded operation turns
- Accepted values: Each checkpoint charges one turns credit;0 rejects the first checkpoint. Cancellation/deadline checks precede charging; calls/attempts are separate grants.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::WORK_LIMIT / transfer-turns. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::inputBytes` — CFG-0411 {#cfg-0411}

- Type: `std::size_t`. Declared default: `= 131072`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: fixed HTTP input-window bytes
- Accepted values: Strictly&gt;0; allocation participates in residentBytes. Copies must fit inputBytes-inputSize; an undersized window can reject indivisible header/parser input.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options; RESOURCE_LIMIT / http-input-capacity. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::requestBytes` — CFG-0412 {#cfg-0412}

- Type: `std::size_t`. Declared default: `= 32768`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete HTTP request bytes
- Accepted values: Strictly&gt;0; every GET/Host/date/auth/CRLF fragment must fit remaining requestBytes before sending. Fixed output-slot size participates in residentBytes.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options; RESOURCE_LIMIT / http-request-capacity. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::residentBytes` — CFG-0413 {#cfg-0413}

- Type: `std::size_t`. Declared default: `= 16 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: transfer-owned backing bytes
- Accepted values: Covers host/control/response/source/path, input/request/secret, trust copies, DNS, inflater and explicitly accounted grants. Checked twice: validateOptions and the full reserve formula.0 cannot fit required structures.
- Read timing: validateOptions runs before copying/SDK work. Operation and BasicTransferResponse use captured limits during one load; streaming continuations preserve consumption.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::RESOURCE_LIMIT / transfer-backing or transfer-resident; additional stream backing → connection-stream-backing. Invalid required configuration yields INVALID_OPTIONS / transfer-options before I/O.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::envelope` — CFG-0414 {#cfg-0414}

- Type: `IpfEnvelopeLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Independent encoded/decoded/work/backing ceilings for one load.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: IpfTransferOptions nested whole-load finite framing/envelope/native preparation; redirects/attempts do not reset grants
- Mutability and lifetime: nested explicit input follows owning options; changing original aggregate does not mutate existing load/parser; new operation needs explicit new options
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

## IpfTransferOptions {#cfg-0415-owner}

[include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp)

### `IpfTransferOptions::ipf` — CFG-0415 {#cfg-0415}

- Type: `IpfReadOptions`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Existing independent parser/output limits and positive turn quotas.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Existing independent parser/output limits and positive turn quotas.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::limits` — CFG-0416 {#cfg-0416}

- Type: `IpfTransferLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Source, framing, copy and transfer backing limits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::dnsServers` — CFG-0417 {#cfg-0417}

- Type: `std::vector<transport::DnsServer>`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit at most four numeric IPv4 DNS servers.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Explicit at most four numeric IPv4 DNS servers.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::dnsPort` — CFG-0418 {#cfg-0418}

- Type: `std::uint16_t`. Declared default: `= 53`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: DNS UDP/TCP port
- Accepted values: 1..65535 from uint16_t and the explicit nonzero check. Applies to explicit DNS servers; TLS.dns.servers must be empty to avoid a second server list.
- Read timing: validateOptions, then handoff to Raw/TLS byte settings when connecting.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options for 0.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::nativeLimits` — CFG-0419 {#cfg-0419}

- Type: `transport::TcpLimits`. Declared default: `{4096, 4096, 64, 16, 65536, 8192, 96 * 1024 * 1024}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Native backing.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::overallTimeout` — CFG-0420 {#cfg-0420}

- Type: `std::chrono::milliseconds`. Declared default: `{300000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: 1..INT_MAX inclusive. Whole-load positive duration. validateOptions requires a valid duration for every load, including routes that do not later use this phase.
- Read timing: Operation construction fixes Clock::now()+overallTimeout; checkpoints test this deadline. It covers HTTP and local-file loading.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options for an invalid duration; overall expiry yields DEADLINE / transfer-deadline, phase failures retain TRANSPORT_FAILURE/TLS_FAILURE and the lower cause.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::connectTimeout` — CFG-0421 {#cfg-0421}

- Type: `std::chrono::milliseconds`. Declared default: `{30000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: 1..INT_MAX inclusive. Positive duration separately for DNS and TCP connect. validateOptions requires a valid duration for every load, including routes that do not later use this phase.
- Read timing: connect passes one connectTimeout value into two transport::TcpOptions fields: dnsTimeout and connectTimeout. Transport starts them separately in RESOLVING and CONNECTING; overallTimeout bounds the whole load.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options for an invalid duration; overall expiry yields DEADLINE / transfer-deadline, phase failures retain TRANSPORT_FAILURE/TLS_FAILURE and the lower cause.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::readTimeout` — CFG-0422 {#cfg-0422}

- Type: `std::chrono::milliseconds`. Declared default: `{60000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: 1..INT_MAX inclusive. Positive transport-read inactivity duration. validateOptions requires a valid duration for every load, including routes that do not later use this phase.
- Read timing: connect passes readTimeout into transport::TcpOptions for native read inactivity. It does not time local-file reads; overallTimeout bounds the whole load.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options for an invalid duration; overall expiry yields DEADLINE / transfer-deadline, phase failures retain TRANSPORT_FAILURE/TLS_FAILURE and the lower cause.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::writeTimeout` — CFG-0423 {#cfg-0423}

- Type: `std::chrono::milliseconds`. Declared default: `{60000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: 1..INT_MAX inclusive. Positive pending-transport-write inactivity duration. validateOptions requires a valid duration for every load, including routes that do not later use this phase.
- Read timing: connect passes writeTimeout into transport::TcpOptions for native pending-write inactivity. Local-file input does not perform native sending; overallTimeout bounds the whole load.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options for an invalid duration; overall expiry yields DEADLINE / transfer-deadline, phase failures retain TRANSPORT_FAILURE/TLS_FAILURE and the lower cause.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::pollInterval` — CFG-0424 {#cfg-0424}

- Type: `std::chrono::milliseconds`. Declared default: `{1}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: 1..100 inclusive. Zero/negative reject; shared poll cadence also applies between cancellation/cleanup checks.
- Read timing: validateOptions before I/O; Operation construction fixes the overall deadline and passes phase durations to native settings.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options for an invalid duration; overall expiry yields DEADLINE / transfer-deadline, phase failures retain TRANSPORT_FAILURE/TLS_FAILURE and the lower cause.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::trustPem` — CFG-0425 {#cfg-0425}

- Type: `std::string`. Declared default: `{}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit ordinary PEM anchors for HTTPS, at most tls.limits.trustBytes.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Explicit ordinary PEM anchors for HTTPS, at most tls.limits.trustBytes.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::handshakeTimeout` — CFG-0426 {#cfg-0426}

- Type: `std::chrono::milliseconds`. Declared default: `{30000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: 1..INT_MAX inclusive. Positive TLS-handshake duration for a secured route. validateOptions requires a valid duration for every load, including routes that do not later use this phase.
- Read timing: validateOptions checks the value during configuration. connect copies it into EndpointTls only for a secure source; overallTimeout bounds the whole load.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options for an invalid duration; overall expiry yields DEADLINE / transfer-deadline, phase failures retain TRANSPORT_FAILURE/TLS_FAILURE and the lower cause.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::closeTimeout` — CFG-0427 {#cfg-0427}

- Type: `std::chrono::milliseconds`. Declared default: `{1000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: 1..INT_MAX inclusive. Positive TLS-protocol-close duration. validateOptions requires a valid duration for every load, including routes that do not later use this phase.
- Read timing: validateOptions checks the value during configuration. connect copies it into EndpointTls only for a secure source; cleanup and whole-load resources also bound retirement.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: IpfTransferError::INVALID_OPTIONS / transfer-options for an invalid duration; overall expiry yields DEADLINE / transfer-deadline, phase failures retain TRANSPORT_FAILURE/TLS_FAILURE and the lower cause.
- Consumer sources: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::tls` — CFG-0428 {#cfg-0428}

- Type: `transport::TlsOptions`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit qualified same-image host/shared/private reservations.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::tlsWork` — CFG-0429 {#cfg-0429}

- Type: `transport::TlsWorkVector`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Whole-reader TLS work, debited across every load/redirect, never reset.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::nativeWork` — CFG-0430 {#cfg-0430}

- Type: `transport::TcpWork`. Declared default: `{ 1000000, 1000000, 536870912, 1000000, 1000000, 134217728, (std::numeric_limits<std::uint64_t>::max)()}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Whole-load native work across operations; DNS units follow finite conservative profile, never unlimited sentinel.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::byteWork` — CFG-0431 {#cfg-0431}

- Type: `protocol::LiveWork`. Declared default: `{1000000, 1000000000, 1000000000, 64ULL * 1024 * 1024 * 1024}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Whole-load logical preparation/copy work including DNS setup; not allocated bytes; never refunded.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

## IpfWriteLimits {#cfg-0432-owner}

[include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp)

### `IpfWriteLimits::profiles` — CFG-0432 {#cfg-0432}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input profiles
- Accepted values: profiles.size()&lt;=profiles before inspecting content. Zero permits an empty list, which may still generate control output according to options.
- Read timing: composeInstrumentProfiles preflights and builds a complete document before writing a stream/file; options apply to one call.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected with IpfWriteDiagnostic{IpfWriteError::PROFILE_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Unrepresentable UTF-16 and numeric precision have separate causes.
- Consumer sources: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::types` — CFG-0433 {#cfg-0433}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: emitted file types/formats
- Accepted values: Creating a format requires formats_.size()&lt;types; existing formats are reused. Zero permits scenarios that create no format.
- Read timing: composeInstrumentProfiles preflights and builds a complete document before writing a stream/file; options apply to one call.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected with IpfWriteDiagnostic{IpfWriteError::TYPE_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Unrepresentable UTF-16 and numeric precision have separate causes.
- Consumer sources: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::fieldsPerType` — CFG-0434 {#cfg-0434}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: fields per format
- Accepted values: Nonempty profiles require&gt;=2 and customFieldCount&lt;=fieldsPerType-2; each new field checks format.count&lt;fieldsPerType. Values 0/1 can serve empty input only.
- Read timing: composeInstrumentProfiles preflights and builds a complete document before writing a stream/file; options apply to one call.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected with IpfWriteDiagnostic{IpfWriteError::FIELD_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Unrepresentable UTF-16 and numeric precision have separate causes.
- Consumer sources: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::headerUnits` — CFG-0435 {#cfg-0435}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 generated-header name units
- Accepted values: Aggregate headers_+units&lt;=headerUnits through checked add. Zero rejects positive header text; UTF-8 output and quoting separately consume outputBytes.
- Read timing: composeInstrumentProfiles preflights and builds a complete document before writing a stream/file; options apply to one call.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected with IpfWriteDiagnostic{IpfWriteError::HEADER_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Unrepresentable UTF-16 and numeric precision have separate causes.
- Consumer sources: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::inputUnits` — CFG-0436 {#cfg-0436}

- Type: `std::size_t`. Declared default: `= 16777216`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 source-profile units
- Accepted values: Sums retainedUnits() of input profiles; preflight checks the first before compose, then traversal checks the others. Zero permits zero usage only.
- Read timing: composeInstrumentProfiles preflights and builds a complete document before writing a stream/file; options apply to one call.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected with IpfWriteDiagnostic{IpfWriteError::INPUT_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Unrepresentable UTF-16 and numeric precision have separate causes.
- Consumer sources: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::outputBytes` — CFG-0437 {#cfg-0437}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete generated CSV/IPF document bytes
- Accepted values: Before each byte, bytes_.size()&lt;outputBytes is required. Zero rejects even the first marker/newline byte; the grant includes escaping and control text.
- Read timing: composeInstrumentProfiles preflights and builds a complete document before writing a stream/file; options apply to one call.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected with IpfWriteDiagnostic{IpfWriteError::OUTPUT_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Unrepresentable UTF-16 and numeric precision have separate causes.
- Consumer sources: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::work` — CFG-0438 {#cfg-0438}

- Type: `std::size_t`. Declared default: `= 33554432`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical visits/output bytes of composition work
- Accepted values: Preflight debits retained units+standardFields count+customFields count; compose additionally charges traversal and every output byte. Zero admits only paths with no positive work.
- Read timing: composeInstrumentProfiles preflights and builds a complete document before writing a stream/file; options apply to one call.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected with IpfWriteDiagnostic{IpfWriteError::WORK_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Unrepresentable UTF-16 and numeric precision have separate causes.
- Consumer sources: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

## IpfWriteOptions {#cfg-0439-owner}

[include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp)

### `IpfWriteOptions::limits` — CFG-0439 {#cfg-0439}

- Type: `IpfWriteLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Input, format, work and complete-output bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] std::expected&lt;std::string, IpfWriteDiagnostic&gt; composeInstrumentProfiles(std::span&lt;const InstrumentProfile&gt; profiles, IpfWriteOptions options = {}) / [[nodiscard]] IpfWriteResult writeInstrumentProfiles(std::ostream &amp;output, std::span&lt;const InstrumentProfile&gt; profiles, IpfWriteOptions options = {}) / [[nodiscard]] IpfWriteResult writeInstrumentProfilesFile(const std::filesystem::path &amp;path, std::span&lt;const InstrumentProfile&gt; profiles, IpfWriteOptions options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: IpfWriteError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `IpfWriteError`.

## ProfileLimits {#cfg-0440-owner}

[include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp)

### `ProfileLimits::customFields` — CFG-0440 {#cfg-0440}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: nonempty custom profile fields
- Accepted values: A new nonempty custom value requires custom_.size()&lt;customFields. Zero does not restrict standard fields; assigning an empty custom value removes it without requiring a new slot.
- Read timing: InstrumentProfile captures limits without numeric constructor validation. Setters/numeric getters check them before admitting a value/parsing.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileError::LIMIT_EXCEEDED. std::u16string/map allocation may separately throw std::bad_alloc; numeric/text kind errors are independent.
- Consumer sources: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

### `ProfileLimits::nameUnits` — CFG-0441 {#cfg-0441}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 field-name units
- Accepted values: name.size()&lt;=nameUnits in setters and numeric getters. Zero permits only an empty name before field-kind logic; ordinary standard names cannot fit.
- Read timing: InstrumentProfile captures limits without numeric constructor validation. Setters/numeric getters check them before admitting a value/parsing.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileError::LIMIT_EXCEEDED. std::u16string/map allocation may separately throw std::bad_alloc; numeric/text kind errors are independent.
- Consumer sources: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

### `ProfileLimits::valueUnits` — CFG-0442 {#cfg-0442}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 canonical field-value units
- Accepted values: Canonical text.size()&lt;=valueUnits, including numeric-setter formatting. Zero permits empty text; numeric input has separate inputUnits.
- Read timing: InstrumentProfile captures limits without numeric constructor validation. Setters/numeric getters check them before admitting a value/parsing.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileError::LIMIT_EXCEEDED. std::u16string/map allocation may separately throw std::bad_alloc; numeric/text kind errors are independent.
- Consumer sources: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

### `ProfileLimits::totalUnits` — CFG-0443 {#cfg-0443}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 retained names/values
- Accepted values: Standard fields charge text; nonempty custom fields charge name+text. Replacement subtracts old usage before admitting the new amount into totalUnits. Zero permits zero retention.
- Read timing: InstrumentProfile captures limits without numeric constructor validation. Setters/numeric getters check them before admitting a value/parsing.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileError::LIMIT_EXCEEDED. std::u16string/map allocation may separately throw std::bad_alloc; numeric/text kind errors are independent.
- Consumer sources: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

### `ProfileLimits::inputUnits` — CFG-0444 {#cfg-0444}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 parsing/conversion input units
- Accepted values: setField bounds raw value.size(); numeric getters bound text they will parse. Zero permits empty text; canonical output independently consumes valueUnits/totalUnits.
- Read timing: InstrumentProfile captures limits without numeric constructor validation. Setters/numeric getters check them before admitting a value/parsing.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: ProfileError::LIMIT_EXCEEDED. std::u16string/map allocation may separately throw std::bad_alloc; numeric/text kind errors are independent.
- Consumer sources: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

## OptionChainLimits {#cfg-0445-owner}

[include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp)

### `OptionChainLimits::profiles` — CFG-0445 {#cfg-0445}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input profiles considered over builder lifetime
- Accepted values: build checks its input span; every addProfile debits one profiles credit, including a nonoption accepted through ignore. Zero permits no add calls.
- Read timing: OptionChainsBuilder captures limits at construction; addProfile stages a private candidate and publishes with swap only after all checks.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: OptionChainError::LIMIT_EXCEEDED. Graph/string allocation can separately throw std::bad_alloc; prior graph remains until successful publication.
- Consumer sources: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::chains` — CFG-0446 {#cfg-0446}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: created symbol chains
- Accepted values: A new chain key debits one chains credit. Existing chains are reused; metadata can place an option into multiple chains. Zero prevents a new chain.
- Read timing: OptionChainsBuilder captures limits at construction; addProfile stages a private candidate and publishes with swap only after all checks.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: OptionChainError::LIMIT_EXCEEDED. Graph/string allocation can separately throw std::bad_alloc; prior graph remains until successful publication.
- Consumer sources: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::series` — CFG-0447 {#cfg-0447}

- Type: `std::size_t`. Declared default: `= 16384`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: series across all chains
- Accepted values: A new series debits one series credit; metadata comparison reuses existing series. Zero permits operations that create no series.
- Read timing: OptionChainsBuilder captures limits at construction; addProfile stages a private candidate and publishes with swap only after all checks.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: OptionChainError::LIMIT_EXCEEDED. Graph/string allocation can separately throw std::bad_alloc; prior graph remains until successful publication.
- Consumer sources: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::options` — CFG-0448 {#cfg-0448}

- Type: `std::size_t`. Declared default: `= 200000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: call/put option slots across all series
- Accepted values: A new slot debits one options credit; replacing an existing strike removes prior retainedUnits without increasing options. Zero forbids a new option slot.
- Read timing: OptionChainsBuilder captures limits at construction; addProfile stages a private candidate and publishes with swap only after all checks.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: OptionChainError::LIMIT_EXCEEDED. Graph/string allocation can separately throw std::bad_alloc; prior graph remains until successful publication.
- Consumer sources: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::retainedUnits` — CFG-0449 {#cfg-0449}

- Type: `std::size_t`. Declared default: `= 16777216`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 chain keys/metadata/profile units
- Accepted values: Charges chain keys, series metadata and profile copies per slot, using checked aggregate charge. Replacement releases the prior profile charge. Zero permits an empty graph.
- Read timing: OptionChainsBuilder captures limits at construction; addProfile stages a private candidate and publishes with swap only after all checks.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: OptionChainError::LIMIT_EXCEEDED. Graph/string allocation can separately throw std::bad_alloc; prior graph remains until successful publication.
- Consumer sources: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::stagingUnits` — CFG-0450 {#cfg-0450}

- Type: `std::size_t`. Declared default: `= 50331648`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: peak logical units while staging a new graph
- Accepted values: Before cloning, accounts for old/new graph, old/new symbol keys and three input-profile charges under the explicit formula. Zero permits zero staging only. This is separate from retainedUnits.
- Read timing: OptionChainsBuilder captures limits at construction; addProfile stages a private candidate and publishes with swap only after all checks.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: OptionChainError::LIMIT_EXCEEDED. Graph/string allocation can separately throw std::bad_alloc; prior graph remains until successful publication.
- Consumer sources: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::workUnits` — CFG-0451 {#cfg-0451}

- Type: `std::size_t`. Declared default: `= 1073741824`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative builder work
- Accepted values: Every profile debits one+retainedUnits, then old graph traversal, chains/series/options and further comparisons. workUnits_ persists across addProfile;0 rejects the first input.
- Read timing: OptionChainsBuilder captures limits at construction; addProfile stages a private candidate and publishes with swap only after all checks.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: OptionChainError::LIMIT_EXCEEDED. Graph/string allocation can separately throw std::bad_alloc; prior graph remains until successful publication.
- Consumer sources: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).
