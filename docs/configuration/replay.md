# Replay

[Русский](replay.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [ReplayLimits](replay.md#cfg-0632-owner) — `include/qd/replay/tape_replay.hpp`

## ReplayLimits {#cfg-0632-owner}

[include/qd/replay/tape_replay.hpp](../../include/qd/replay/tape_replay.hpp)

### `ReplayLimits::reader` — CFG-0632 {#cfg-0632}

- Type: `protocol::TapeLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Independent fresh-reader parse/storage budgets.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] ReplayResult replayTape(const protocol::AdmittedTape &amp;tape, ReplayObserver &amp;observer, ReplayClock &amp;clock, ReplayCancellation &amp;cancellation, ReplayLimits limits, ReplayPolicy policy)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: ClockError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `ClockError`.

### `ReplayLimits::deliveredRecords` — CFG-0633 {#cfg-0633}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: successfully delivered records
- Accepted values: Before the next record, deliveredRecords must be below the limit; 0 stops before the first onRecord.
- Read timing: ReplaySession copies ReplayLimits for a synchronous run; values are read immediately before a work/callback attempt.
- Mutability and lifetime: Immutable within a run; a new run receives a new profile.
- Failure: Reaching deliveredRecords before the next onRecord yields ReplayOutcome::RESOURCE_LIMIT, stage=delivered-records; the next record callback is not invoked.
- Consumer sources: [include/qd/replay/tape_replay.hpp](../../include/qd/replay/tape_replay.hpp), [src/replay/tape_replay.cpp](../../src/replay/tape_replay.cpp).

### `ReplayLimits::callbackCalls` — CFG-0634 {#cfg-0634}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: observer call attempts including control callbacks
- Accepted values: callbackCalls bounds all corresponding callCharge uses; 0 admits no first callback. An attempt is charged before user code.
- Read timing: ReplaySession copies ReplayLimits for a synchronous run; values are read immediately before a work/callback attempt.
- Mutability and lifetime: Immutable within a run; a new run receives a new profile.
- Failure: Exhausting callbackCalls before a record/completion callback yields ReplayOutcome::RESOURCE_LIMIT, stage=callback-calls; that invocation does not begin.
- Consumer sources: [include/qd/replay/tape_replay.hpp](../../include/qd/replay/tape_replay.hpp), [src/replay/tape_replay.cpp](../../src/replay/tape_replay.cpp).

### `ReplayLimits::workUnits` — CFG-0635 {#cfg-0635}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate replay work attempts
- Accepted values: Every charge needs remaining workUnits and does not refund user-code failure; 0 admits no first work attempt.
- Read timing: ReplaySession captures aggregate workUnits for the run. Every cancellation poll, clock sample, wait, outgoing-layout visit, record/frame attempt and callback passes its own stage to charge/callCharge.
- Mutability and lifetime: Immutable within a run; a new run receives a new profile.
- Failure: Exhausting workUnits returns ReplayOutcome::RESOURCE_LIMIT at the attempted operation stage: cancellation-work, clock-calls, wait-calls, layout-work, record-work, frame-work or callback-calls. charge/callCharge check credit before the corresponding call or visit; earlier work remains in counters.
- Consumer sources: [include/qd/replay/tape_replay.hpp](../../include/qd/replay/tape_replay.hpp), [src/replay/tape_replay.cpp](../../src/replay/tape_replay.cpp).

### `ReplayLimits::maxRelativeMs` — CFG-0636 {#cfg-0636}

- Type: `std::uint64_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Maximum positive elapsed milliseconds from the first timed nonempty frame.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Maximum positive elapsed milliseconds from the first timed nonempty frame.
- Read timing: Explicit argument of public operations: [[nodiscard]] ReplayResult replayTape(const protocol::AdmittedTape &amp;tape, ReplayObserver &amp;observer, ReplayClock &amp;clock, ReplayCancellation &amp;cancellation, ReplayLimits limits, ReplayPolicy policy)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: ClockError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `ClockError`.

### `ReplayLimits::maxWaitSliceNs` — CFG-0637 {#cfg-0637}

- Type: `std::int64_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive relative wait slice; ignored by IMMEDIATE.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive relative wait slice; ignored by IMMEDIATE.
- Read timing: Explicit argument of public operations: [[nodiscard]] ReplayResult replayTape(const protocol::AdmittedTape &amp;tape, ReplayObserver &amp;observer, ReplayClock &amp;clock, ReplayCancellation &amp;cancellation, ReplayLimits limits, ReplayPolicy policy)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: ClockError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `ClockError`.

### `ReplayLimits::maxWaitCalls` — CFG-0638 {#cfg-0638}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: relative-clock wait attempts
- Accepted values: Every waitUntilNs needs a new callCharge below maxWaitCalls. 0 forbids waiting when the target is still ahead; IMMEDIATE may not use it.
- Read timing: ReplaySession copies ReplayLimits for a synchronous run; values are read immediately before a work/callback attempt.
- Mutability and lifetime: Immutable within a run; a new run receives a new profile.
- Failure: Exhausting maxWaitCalls before waitUntilNs yields ReplayOutcome::RESOURCE_LIMIT, stage=wait-calls; the next wait does not begin.
- Consumer sources: [include/qd/replay/tape_replay.hpp](../../include/qd/replay/tape_replay.hpp), [src/replay/tape_replay.cpp](../../src/replay/tape_replay.cpp).

### `ReplayLimits::clockCalls` — CFG-0639 {#cfg-0639}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: monotonic-clock sampling attempts
- Accepted values: Every nowNs needs callCharge below clockCalls. 0 forbids the first required sample; samples separately must be nonnegative and nondecreasing.
- Read timing: ReplaySession copies ReplayLimits for a synchronous run; values are read immediately before a work/callback attempt.
- Mutability and lifetime: Immutable within a run; a new run receives a new profile.
- Failure: Exhausting clockCalls before nowNs yields ReplayOutcome::RESOURCE_LIMIT, stage=clock-calls; the next sample is not attempted. If admitted, the returned sample is independently validated and may yield CLOCK_FAILURE, stage=clock-sample.
- Consumer sources: [include/qd/replay/tape_replay.hpp](../../include/qd/replay/tape_replay.hpp), [src/replay/tape_replay.cpp](../../src/replay/tape_replay.cpp).
