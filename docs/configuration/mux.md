# Multiplexing and demand

[Русский](mux.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [DemandLimits](mux.md#cfg-0452-owner) — `include/qd/mux/demand.hpp`
- [DemandWork](mux.md#cfg-0454-owner) — `include/qd/mux/demand.hpp`
- [MuxLimits](mux.md#cfg-0457-owner) — `include/qd/mux/interest.hpp`
- [MuxWork](mux.md#cfg-0465-owner) — `include/qd/mux/interest.hpp`
- [ClientLimits](mux.md#cfg-0482-owner) — `include/qd/mux/queue.hpp`
- [QueueCapacity](mux.md#cfg-0514-owner) — `include/qd/mux/queue.hpp`
- [QueueLimits](mux.md#cfg-0469-owner) — `include/qd/mux/queue.hpp`
- [QueueSubmitWork](mux.md#cfg-0498-owner) — `include/qd/mux/queue.hpp`
- [QueueWork](mux.md#cfg-0502-owner) — `include/qd/mux/queue.hpp`

## DemandLimits {#cfg-0452-owner}

[include/qd/mux/demand.hpp](../../include/qd/mux/demand.hpp)

### `DemandLimits::entries` — CFG-0452 {#cfg-0452}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: distinct exact keys
- Accepted values: desired.size() and the new in-flight demand set ≤ entries. 0 permits an empty set. Combined two-bank storage must be size_t-representable; create checks the (max-sizeof(State))/2 formula.
- Read timing: Copied in create and sizes two bank arrays; next checks desired and each ADD/REMOVE change.
- Mutability and lifetime: There is no limit setter; acknowledge changes the acknowledged set within the original bounds.
- Failure: Bank sizing overflow or exceeding desired/new-set bounds is DemandError::RESOURCE_LIMIT; bad_array_new_length during create is also RESOURCE_LIMIT. Other desired errors have separate branches.
- Consumer sources: [include/qd/mux/demand.hpp](../../include/qd/mux/demand.hpp), [src/mux/demand.cpp](../../src/mux/demand.cpp).

### `DemandLimits::symbolUnits` — CFG-0453 {#cfg-0453}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: total symbol UTF-16 units
- Accepted values: Sum of symbol lengths in desired and the new set ≤ symbolUnits. 0 permits only empty symbols with nonnull data(); null data is rejected separately. Combined two-bank storage must be size_t-representable; create checks the (max-sizeof(State))/2 formula.
- Read timing: Copied in create and sizes two bank arrays; next checks desired and each ADD/REMOVE change.
- Mutability and lifetime: There is no limit setter; acknowledge changes the acknowledged set within the original bounds.
- Failure: Bank sizing overflow or exceeding desired/new-set bounds is DemandError::RESOURCE_LIMIT; bad_array_new_length during create is also RESOURCE_LIMIT. Other desired errors have separate branches.
- Consumer sources: [include/qd/mux/demand.hpp](../../include/qd/mux/demand.hpp), [src/mux/demand.cpp](../../src/mux/demand.cpp).

## DemandWork {#cfg-0454-owner}

[include/qd/mux/demand.hpp](../../include/qd/mux/demand.hpp)

### `DemandWork::operations` — CFG-0454 {#cfg-0454}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining polls and resets.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `DemandError`.

### `DemandWork::visits` — CFG-0455 {#cfg-0455}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining entry inspections, comparisons and fixed metadata writes.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `DemandError`.

### `DemandWork::symbolUnits` — CFG-0456 {#cfg-0456}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining examined/copied UTF-16 units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `DemandError`.

## MuxLimits {#cfg-0457-owner}

[include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp)

### `MuxLimits::schemaRecords` — CFG-0457 {#cfg-0457}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: schema record metadata
- Accepted values: Schema record count must be ≤schemaRecords; 0 permits only an empty schema. Increasing schemaRecords itself does not resize storage or participate in its arithmetic; the record table uses actual records.size().
- Read timing: MuxState::create checks record count once before table sizing. Record storage uses records.size(); later commands do not reread the original grant.
- Mutability and lifetime: No public limits setter; subscription mutations use the initial profile.
- Failure: records.size()&gt;schemaRecords yields MuxError::RESOURCE_LIMIT before storage sizing. Later sizing INVALID_CONFIGURATION concerns actual schema data and other capacities; increasing schemaRecords alone does not cause it.
- Consumer sources: [include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp), [src/mux/interest.cpp](../../src/mux/interest.cpp).

### `MuxLimits::clients` — CFG-0458 {#cfg-0458}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: clients including empty ones
- Accepted values: 0 creates a Mux with no available client slots. No separate positive minimum exists. Creation sizes additionally use checked addition/multiplication; no separate fixed field maximum is imposed.
- Read timing: Copied during MuxState::create; create reserves tables and commands/registration read the stored bound.
- Mutability and lifetime: No public limits setter; subscription mutations use the initial profile.
- Failure: Exceeding the corresponding capacity is MuxError::RESOURCE_LIMIT. Two-bank sizing overflow during create is INVALID_CONFIGURATION; insufficient residentBytes is RESOURCE_LIMIT.
- Consumer sources: [include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp), [src/mux/interest.cpp](../../src/mux/interest.cpp).

### `MuxLimits::entries` — CFG-0459 {#cfg-0459}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: total client keys per bank
- Accepted values: 0 permits only empty client sets; bank.entryCount must fit entries. Creation sizes additionally use checked addition/multiplication; no separate fixed field maximum is imposed.
- Read timing: Copied during MuxState::create; create reserves tables and commands/registration read the stored bound.
- Mutability and lifetime: No public limits setter; subscription mutations use the initial profile.
- Failure: Exceeding the corresponding capacity is MuxError::RESOURCE_LIMIT. Two-bank sizing overflow during create is INVALID_CONFIGURATION; insufficient residentBytes is RESOURCE_LIMIT.
- Consumer sources: [include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp), [src/mux/interest.cpp](../../src/mux/interest.cpp).

### `MuxLimits::aggregateEntries` — CFG-0460 {#cfg-0460}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: distinct aggregate subscription keys
- Accepted values: 0 permits only an empty union; a new distinct key requires a free aggregate slot. Creation sizes additionally use checked addition/multiplication; no separate fixed field maximum is imposed.
- Read timing: Copied during MuxState::create; create reserves tables and commands/registration read the stored bound.
- Mutability and lifetime: No public limits setter; subscription mutations use the initial profile.
- Failure: Exceeding the corresponding capacity is MuxError::RESOURCE_LIMIT. Two-bank sizing overflow during create is INVALID_CONFIGURATION; insufficient residentBytes is RESOURCE_LIMIT.
- Consumer sources: [include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp), [src/mux/interest.cpp](../../src/mux/interest.cpp).

### `MuxLimits::symbolUnits` — CFG-0461 {#cfg-0461}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units per exact symbol
- Accepted values: create captures symbolUnits without an independent numeric domain; its magnitude is not a bank-sizing factor. Key validation requires symbol length≤symbolUnits except for concrete data under CANONICAL_STREAM_ALL_V1. An operation also computes visits×symbolUnits with overflow checks and charges the result against work.symbolUnits. 0 means zero maximum length and a zero factor for this part of the work model.
- Read timing: The value is only captured at create. State::reserve later reads it for text-work preflight, and key validation reads it for the length comparison.
- Mutability and lifetime: No public limits setter; subscription mutations use the initial profile.
- Failure: An overlong checked key yields MuxError::RESOURCE_LIMIT. An unrepresentable visits×symbolUnits calculation in State::reserve also yields RESOURCE_LIMIT; insufficient computed work.symbolUnits yields WORK_LIMIT. create has no independent refusal depending on the numerical symbolUnits value.
- Consumer sources: [include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp), [src/mux/interest.cpp](../../src/mux/interest.cpp).

### `MuxLimits::totalSymbolUnits` — CFG-0462 {#cfg-0462}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of client symbols per bank
- Accepted values: Total client symbol storage ≤ totalSymbolUnits; 0 gives an empty arena. The aggregate set references client symbols. Creation sizes additionally use checked addition/multiplication; no separate fixed field maximum is imposed.
- Read timing: Copied during MuxState::create; create reserves tables and commands/registration read the stored bound.
- Mutability and lifetime: No public limits setter; subscription mutations use the initial profile.
- Failure: Exceeding the corresponding capacity is MuxError::RESOURCE_LIMIT. Two-bank sizing overflow during create is INVALID_CONFIGURATION; insufficient residentBytes is RESOURCE_LIMIT.
- Consumer sources: [include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp), [src/mux/interest.cpp](../../src/mux/interest.cpp).

### `MuxLimits::commandEntries` — CFG-0463 {#cfg-0463}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: command input entries including duplicates
- Accepted values: create captures commandEntries without an independent range check or a correspondingly sized bank allocation. For every command, inputCount is keys.size() for REMOVE or requests.size() otherwise and must be≤commandEntries. 0 permits only empty input; after that comparison, work is calculated from the actual inputCount.
- Read timing: create only captures the value. A command reads it before reserve(inputCount), input-key validation and copying.
- Mutability and lifetime: No public limits setter; subscription mutations use the initial profile.
- Failure: inputCount&gt;commandEntries returns MuxError::RESOURCE_LIMIT. reserve(inputCount) may subsequently return RESOURCE_LIMIT for unrepresentable actual-work sizing or WORK_LIMIT for insufficient credits. These later failures are not a create-time check of commandEntries itself.
- Consumer sources: [include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp), [src/mux/interest.cpp](../../src/mux/interest.cpp).

### `MuxLimits::residentBytes` — CFG-0464 {#cfg-0464}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of both banks, metadata and schema reserve
- Accepted values: Must hold calculated two-bank storage and schema.storageBytes(). 0 cannot hold mandatory structures. Creation sizes additionally use checked addition/multiplication; no separate fixed field maximum is imposed.
- Read timing: Copied during MuxState::create; create reserves tables and commands/registration read the stored bound.
- Mutability and lifetime: No public limits setter; subscription mutations use the initial profile.
- Failure: Exceeding the corresponding capacity is MuxError::RESOURCE_LIMIT. Two-bank sizing overflow during create is INVALID_CONFIGURATION; insufficient residentBytes is RESOURCE_LIMIT.
- Consumer sources: [include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp), [src/mux/interest.cpp](../../src/mux/interest.cpp).

## MuxWork {#cfg-0465-owner}

[include/qd/mux/interest.hpp](../../include/qd/mux/interest.hpp)

### `MuxWork::operations` — CFG-0465 {#cfg-0465}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining public command/query/factory attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `MuxError`.

### `MuxWork::visits` — CFG-0466 {#cfg-0466}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservative metadata, comparison, initialization and disposal visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `MuxError`.

### `MuxWork::symbolUnits` — CFG-0467 {#cfg-0467}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservative examined/copied UTF-16 units, including duplicate comparisons.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `MuxError`.

### `MuxWork::bytes` — CFG-0468 {#cfg-0468}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservative fixed initialization, rewrite and retirement byte work.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `MuxError`.

## ClientLimits {#cfg-0482-owner}

[include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp)

### `ClientLimits::dataItems` — CFG-0482 {#cfg-0482}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: distinct queued, executing and caller-retained records
- Accepted values: 0 cannot retain a new record; opening reserves the entire ClientLimits.dataItems.
- Read timing: Copied by open into ClientBudget; the full profile is reserved against QueueLimits, then the allocator checks current usage.
- Mutability and lifetime: Immutable for the open generation; retained objects extend its accounting.
- Failure: Insufficient aggregate reservation during open or current QueueAllocator capacity is QueueError::RESOURCE_LIMIT. Preflight sum overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::recordBytes` — CFG-0483 {#cfg-0483}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of record control/object and full arenas
- Accepted values: 0 admits no nonzero record allocation; the bound covers all retained records including detached objects.
- Read timing: Copied by open into ClientBudget; the full profile is reserved against QueueLimits, then the allocator checks current usage.
- Mutability and lifetime: Immutable for the open generation; retained objects extend its accounting.
- Failure: Insufficient aggregate reservation during open or current QueueAllocator capacity is QueueError::RESOURCE_LIMIT. Preflight sum overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::batchOwners` — CFG-0484 {#cfg-0484}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: composite batch owners
- Accepted values: 0 admits no new batch owner; candidate, executing and caller-retained batches are counted.
- Read timing: Copied by open into ClientBudget; the full profile is reserved against QueueLimits, then the allocator checks current usage.
- Mutability and lifetime: Immutable for the open generation; retained objects extend its accounting.
- Failure: Insufficient aggregate reservation during open or current QueueAllocator capacity is QueueError::RESOURCE_LIMIT. Preflight sum overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::batchBytes` — CFG-0485 {#cfg-0485}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of batch control/object and roots/view arrays
- Accepted values: 0 admits no nonzero batch allocation; retained composites are included.
- Read timing: Copied by open into ClientBudget; the full profile is reserved against QueueLimits, then the allocator checks current usage.
- Mutability and lifetime: Immutable for the open generation; retained objects extend its accounting.
- Failure: Insufficient aggregate reservation during open or current QueueAllocator capacity is QueueError::RESOURCE_LIMIT. Preflight sum overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::batchRecords` — CFG-0486 {#cfg-0486}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive per-callback record maximum.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive per-callback record maximum.
- Read timing: Queue create / registration activation: explicit aggregate/per-generation reservations, retained through retirement and caller-held owners
- Mutability and lifetime: Immutable registration maxima; new generation/registration required; commands mutate desired state within maxima
- Failure: QueueError via explicit public expected/diagnostic result; RESOURCE_LIMIT vs WORK_LIMIT distinct
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `ClientLimits::recordFields` — CFG-0487 {#cfg-0487}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: fields per record, legacy+tagged
- Accepted values: legacy.size()+fields.size() ≤ recordFields; 0 admits only records without these fields. An alternate checking path uses typedValues.size().
- Read timing: Copied by open; read by record/shared record validation before publishing to the client queue.
- Mutability and lifetime: Fixed for the client registration.
- Failure: Exceeding the specific bound is QueueError::RESOURCE_LIMIT. Size-sum arithmetic uses INVALID_CONFIGURATION on overflow.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::recordSymbolUnits` — CFG-0488 {#cfg-0488}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of one record symbol
- Accepted values: key.symbol.size() ≤ recordSymbolUnits; 0 gives no symbol budget and is not unlimited.
- Read timing: Copied by open; read by record/shared record validation before publishing to the client queue.
- Mutability and lifetime: Fixed for the client registration.
- Failure: An excessive symbol length directly yields QueueError::RESOURCE_LIMIT. This comparison neither sums lengths nor performs arithmetic on recordSymbolUnits. Field and arena dimensions are checked by separate branches.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::recordFieldUnits` — CFG-0489 {#cfg-0489}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: total UTF-16 field-value units per record
- Accepted values: Sum of all present f.units lengths ≤ recordFieldUnits; null and empty values consume 0 text units.
- Read timing: Copied by open; read by record/shared record validation before publishing to the client queue.
- Mutability and lifetime: Fixed for the client registration.
- Failure: Exceeding the specific bound is QueueError::RESOURCE_LIMIT. Size-sum arithmetic uses INVALID_CONFIGURATION on overflow.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::recordByteUnits` — CFG-0490 {#cfg-0490}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: total opaque field bytes per record
- Accepted values: Sum of all present f.bytes lengths ≤ recordByteUnits; null and empty values consume 0 payload bytes.
- Read timing: Copied by open; read by record/shared record validation before publishing to the client queue.
- Mutability and lifetime: Fixed for the client registration.
- Failure: Exceeding the specific bound is QueueError::RESOURCE_LIMIT. Size-sum arithmetic uses INVALID_CONFIGURATION on overflow.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::recordArenaBytes` — CFG-0491 {#cfg-0491}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of the complete requested immutable record arena
- Accepted values: recordArenaBytes(symbol,legacy,fields) ≤ recordArenaBytes. compactRecordArena allocates required, otherwise the configured full arena; shared storage uses payloadBytes.
- Read timing: Copied by open; read by record/shared record validation before publishing to the client queue.
- Mutability and lifetime: Fixed for the client registration.
- Failure: Exceeding the specific bound is QueueError::RESOURCE_LIMIT. Size-sum arithmetic uses INVALID_CONFIGURATION on overflow.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::commandSlots` — CFG-0492 {#cfg-0492}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: ordinary command slots including the executing command
- Accepted values: 0 permits registration without ordinary commands; resultSlots must be ≥ commandSlots.
- Read timing: open reserves fixed commands/results and text for every command slot; submitRequests reads the bound before accepting a command.
- Mutability and lifetime: The command-port profile remains fixed, including retained completions after facade closure.
- Failure: A full commands ring is QueueError::COMMAND_LIMIT; violating resultSlots≥commandSlots during open is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::resultSlots` — CFG-0493 {#cfg-0493}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retained command-completion slots
- Accepted values: resultSlots ≥ commandSlots; 0 is valid only with commandSlots=0. Issued completions occupy slots until retention ends.
- Read timing: open reserves fixed commands/results and text for every command slot; submitRequests reads the bound before accepting a command.
- Mutability and lifetime: The command-port profile remains fixed, including retained completions after facade closure.
- Failure: No completion with use_count()==1 is QueueError::RESULT_LIMIT; insufficient slots during open is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::commandEntries` — CFG-0494 {#cfg-0494}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: command input entries including duplicates
- Accepted values: commandEntries ≤ MuxLimits.commandEntries during open; each input size ≤ commandEntries. 0 admits an empty command.
- Read timing: open reserves fixed commands/results and text for every command slot; submitRequests reads the bound before accepting a command.
- Mutability and lifetime: The command-port profile remains fixed, including retained completions after facade closure.
- Failure: An oversized profile during open is INVALID_CONFIGURATION; oversized submit is QueueError::RESOURCE_LIMIT.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::commandSymbolUnits` — CFG-0495 {#cfg-0495}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of one command text buffer including terminators
- Accepted values: Sum of key.symbol.size()+1 per input ≤ commandSymbolUnits. 0 permits no input keys; even an empty symbol needs 1 unit.
- Read timing: open reserves fixed commands/results and text for every command slot; submitRequests reads the bound before accepting a command.
- Mutability and lifetime: The command-port profile remains fixed, including retained completions after facade closure.
- Failure: Excess computed text is QueueError::RESOURCE_LIMIT before locking/copying; sum overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `ClientLimits::compactRecordArena` — CFG-0496 {#cfg-0496}

- Type: `bool`. Declared default: `= false`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Charge and allocate each record's measured arena instead of its maximum.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: Queue create / registration activation: explicit aggregate/per-generation reservations, retained through retirement and caller-held owners
- Mutability and lifetime: Immutable registration maxima; new generation/registration required; commands mutate desired state within maxima
- Failure: QueueError via explicit public expected/diagnostic result; RESOURCE_LIMIT vs WORK_LIMIT distinct
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `ClientLimits::batchRecordBytes` — CFG-0497 {#cfg-0497}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: selected-record bytes per batch
- Accepted values: 0 disables this soft byte target. A nonzero bound stops adding the next record after the first; an oversized first record is admitted for progress. batchRecords independently bounds record count.
- Read timing: Copied by open and read during ready-batch selection in STREAM and the generic path.
- Mutability and lifetime: Fixed for the registration; no setter exists.
- Failure: A next record exceeding the target is not an error: selection ends the current batch. The first record may exceed the value. sum overflow while sizing is QueueError::INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

## QueueCapacity {#cfg-0514-owner}

[include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp)

### `QueueCapacity::queueBytes` — CFG-0514 {#cfg-0514}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Queue ledger ceiling, including future caller-retained generations.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueCapacity::muxBytes` — CFG-0515 {#cfg-0515}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Independent interest-state byte ceiling.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueCapacity::collectorBytes` — CFG-0516 {#cfg-0516}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Collector byte ceiling, excluding its separate desired/session scopes.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueCapacity::desiredOwners` — CFG-0517 {#cfg-0517}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Maximum simultaneous desired owners: one STREAM owner or source view slots.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueCapacity::desiredPerOwner` — CFG-0518 {#cfg-0518}

- Type: `core::SubscriptionLimits`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Immutable count ceilings for each source desired owner.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

## QueueLimits {#cfg-0469-owner}

[include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp)

### `QueueLimits::clients` — CFG-0469 {#cfg-0469}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: active and retained retired client generations
- Accepted values: 0 creates a queue with no registrations. interestLimits.clients must be at least clients; for TICKER/HISTORY source.viewSlots must also be at least clients.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Insufficient ml.clients or source.viewSlots is INVALID_CONFIGURATION; no free/reservable generation is RESOURCE_LIMIT.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `QueueLimits::sourceRecords` — CFG-0470 {#cfg-0470}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: records of one whole input event
- Accepted values: routeRecords must be positive and ≤ sourceRecords, so 0 fails create. STREAM source.records must cover sourceRecords; nonzero sourceRecords requires source.frames.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Violating route/source or STREAM cross-field checks during create is INVALID_CONFIGURATION; an oversized whole event during stage is RESOURCE_LIMIT.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `QueueLimits::routeRecords` — CFG-0471 {#cfg-0471}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive maximum STREAM source chunk.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive maximum STREAM source chunk.
- Read timing: Queue create / registration activation: explicit aggregate/per-generation reservations, retained through retirement and caller-held owners
- Mutability and lifetime: Immutable registration maxima; new generation/registration required; commands mutate desired state within maxima
- Failure: QueueError via explicit public expected/diagnostic result; RESOURCE_LIMIT vs WORK_LIMIT distinct
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueLimits::batchRecords` — CFG-0472 {#cfg-0472}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive maximum callback batch.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive maximum callback batch.
- Read timing: Queue create / registration activation: explicit aggregate/per-generation reservations, retained through retirement and caller-held owners
- Mutability and lifetime: Immutable registration maxima; new generation/registration required; commands mutate desired state within maxima
- Failure: QueueError via explicit public expected/diagnostic result; RESOURCE_LIMIT vs WORK_LIMIT distinct
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueLimits::cleanupRecords` — CFG-0473 {#cfg-0473}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive unlink/ref-release quantum.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive unlink/ref-release quantum.
- Read timing: Queue create / registration activation: explicit aggregate/per-generation reservations, retained through retirement and caller-held owners
- Mutability and lifetime: Immutable registration maxima; new generation/registration required; commands mutate desired state within maxima
- Failure: QueueError via explicit public expected/diagnostic result; RESOURCE_LIMIT vs WORK_LIMIT distinct
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueLimits::dataItems` — CFG-0474 {#cfg-0474}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate reserved detached record slots
- Accepted values: The sum of ClientLimits.dataItems for active/retained generations ≤ dataItems. 0 permits only zero client reservations.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Capacity mismatch or insufficient aggregate reservation is QueueError::RESOURCE_LIMIT during open/allocation. Arithmetic overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `QueueLimits::recordBytes` — CFG-0475 {#cfg-0475}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate reserved record bytes
- Accepted values: The sum of client recordBytes ≤ recordBytes. Allocation additionally checks actual aggregate usage in this scope. 0 is an empty reservation.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Capacity mismatch or insufficient aggregate reservation is QueueError::RESOURCE_LIMIT during open/allocation. Arithmetic overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `QueueLimits::batchOwners` — CFG-0476 {#cfg-0476}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate reserved composite batch owners
- Accepted values: The sum of client batchOwners ≤ batchOwners; 0 reserves no new batch owner.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Capacity mismatch or insufficient aggregate reservation is QueueError::RESOURCE_LIMIT during open/allocation. Arithmetic overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `QueueLimits::batchBytes` — CFG-0477 {#cfg-0477}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate reserved composite batch bytes
- Accepted values: The sum of client batchBytes ≤ batchBytes; 0 provides no bytes in this scope.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Capacity mismatch or insufficient aggregate reservation is QueueError::RESOURCE_LIMIT during open/allocation. Arithmetic overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `QueueLimits::commandSlots` — CFG-0478 {#cfg-0478}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate reserved ordinary command slots
- Accepted values: The sum of ClientLimits.commandSlots ≤ commandSlots; no separate positive minimum exists.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Capacity mismatch or insufficient aggregate reservation is QueueError::RESOURCE_LIMIT during open/allocation. Arithmetic overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `QueueLimits::resultSlots` — CFG-0479 {#cfg-0479}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate reserved completion slots
- Accepted values: The sum of ClientLimits.resultSlots ≤ resultSlots; retained completions keep their generation reservation.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Capacity mismatch or insufficient aggregate reservation is QueueError::RESOURCE_LIMIT during open/allocation. Arithmetic overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `QueueLimits::controlSlots` — CFG-0480 {#cfg-0480}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: independent control slots
- Accepted values: Exactly 2 slots are reserved per activated generation; 0/1 cannot open a client. controlSlots=2×clients is not required up front.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Capacity mismatch or insufficient aggregate reservation is QueueError::RESOURCE_LIMIT during open/allocation. Arithmetic overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

### `QueueLimits::residentBytes` — CFG-0481 {#cfg-0481}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of all queue-owned requested blocks
- Accepted values: Must hold owner fixed storage and combined client reservations including recordBytes and batchBytes. 0 cannot hold mandatory fixed storage.
- Read timing: QueueLimits is copied by State::create; capacities are read during creation, open reservation and their corresponding consumption.
- Mutability and lifetime: No live queue profile setter; retained retired generations continue to occupy reservations.
- Failure: Capacity mismatch or insufficient aggregate reservation is QueueError::RESOURCE_LIMIT during open/allocation. Arithmetic overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp), [src/mux/queue.cpp](../../src/mux/queue.cpp).

## QueueSubmitWork {#cfg-0498-owner}

[include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp)

### `QueueSubmitWork::operations` — CFG-0498 {#cfg-0498}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Submit attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueSubmitWork::visits` — CFG-0499 {#cfg-0499}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Input, lookup, slot and metadata visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueSubmitWork::textUnits` — CFG-0500 {#cfg-0500}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Examined/copied exact UTF16.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueSubmitWork::bytes` — CFG-0501 {#cfg-0501}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Fixed metadata rewrite/copy/release allowance.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

## QueueWork {#cfg-0502-owner}

[include/qd/mux/queue.hpp](../../include/qd/mux/queue.hpp)

### `QueueWork::operations` — CFG-0502 {#cfg-0502}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Owner attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::visits` — CFG-0503 {#cfg-0503}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete metadata-slot/fixed-object visits, including initialization and prepaid disposal.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::fields` — CFG-0504 {#cfg-0504}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Legacy/generic field visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::textUnits` — CFG-0505 {#cfg-0505}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Examined/copied UTF16 units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::bytes` — CFG-0506 {#cfg-0506}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual fixed/payload request allowance and prepaid detached disposal; batch preparation
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::commands` — CFG-0507 {#cfg-0507}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Ordinary command application/cancellation actions.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::callbacks` — CFG-0508 {#cfg-0508}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Consumer invocations including throwing invocations.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::interest` — CFG-0509 {#cfg-0509}

- Type: `MuxWork`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual authoritative interest work.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::desired` — CFG-0510 {#cfg-0510}

- Type: `core::SubscriptionWork`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual derived desired-state work.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::stream` — CFG-0511 {#cfg-0511}

- Type: `collectors::StreamWork`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual STREAM work, never silently increased.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::ticker` — CFG-0512 {#cfg-0512}

- Type: `collectors::TickerWork`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual TICKER work including global reset ACK.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.

### `QueueWork::history` — CFG-0513 {#cfg-0513}

- Type: `collectors::HistoryWork`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual HISTORY work; its ACK has no work argument.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `CommandStatus, QueueError`.
