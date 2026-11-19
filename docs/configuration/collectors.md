# Collectors

[Русский](collectors.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [HistoryLimits](collectors.md#cfg-0245-owner) — `include/qd/collectors/history.hpp`
- [HistoryOptions](collectors.md#cfg-0256-owner) — `include/qd/collectors/history.hpp`
- [HistoryViewOptions](collectors.md#cfg-0262-owner) — `include/qd/collectors/history.hpp`
- [HistoryWork](collectors.md#cfg-0263-owner) — `include/qd/collectors/history.hpp`
- [StreamLimits](collectors.md#cfg-0267-owner) — `include/qd/collectors/stream.hpp`
- [StreamWork](collectors.md#cfg-0271-owner) — `include/qd/collectors/stream.hpp`
- [TickerLimits](collectors.md#cfg-0275-owner) — `include/qd/collectors/ticker.hpp`
- [TickerOptions](collectors.md#cfg-0281-owner) — `include/qd/collectors/ticker.hpp`
- [TickerWork](collectors.md#cfg-0285-owner) — `include/qd/collectors/ticker.hpp`

## HistoryLimits {#cfg-0245-owner}

[include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp)

### `HistoryLimits::partitions` — CFG-0245 {#cfg-0245}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: record/symbol pair slots
- Accepted values: 0 means no available slots; no separate lower bound is imposed. Upper admissibility is constrained by checked sizing arithmetic and residentBytes, then allocation. partitions enters fixedVisits, including 16×partitions×points and 16×partitions×viewSlots×pendingItems, and the maintenance text estimate. An admitted capacity must also be serviceable by caller-supplied HistoryWork; successful creation does not guarantee enough work for a later operation.
- Read timing: HistoryCollector::create copies capacities into its owner, reserves two banks and checks requested and actual storage; partition subsequently reads the stored bound. During create, dimensions determine initial visits and text work; later fixedVisits/maintenance use stored dimensions to admit maintenance.
- Mutability and lifetime: There is no public HistoryLimits setter; changing it requires a new collector.
- Failure: Sizing overflow during create is INVALID_CONFIGURATION; insufficient residentBytes is RESOURCE_LIMIT; bad_alloc is ALLOCATION_FAILURE. Later, creating a partition without a free slot is RESOURCE_LIMIT. Separately, insufficient creation work yields HistoryError::WORK_LIMIT. After construction, overflow in fixedVisits/maintenance or insufficient work.visits/textUnits also yields WORK_LIMIT, even with storage capacity still available.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::points` — CFG-0246 {#cfg-0246}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: indexed points
- Accepted values: 0 creates an empty point table. Its size participates in checked arithmetic and residentBytes; no separate positive minimum exists. points enters fixedVisits linearly, in 16×partitions×points and in the (viewSlots+2) product over table dimensions. An admitted capacity must also be serviceable by caller-supplied HistoryWork; successful creation does not guarantee enough work for a later operation.
- Read timing: Copied by HistoryCollector::create; storage is reserved before the owner is published, then enforced by the indicated operation. During create, dimensions determine initial visits and text work; later fixedVisits/maintenance use stored dimensions to admit maintenance.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: No free element for a new point is HistoryError::RESOURCE_LIMIT. During create, sizing overflow is INVALID_CONFIGURATION, insufficient residentBytes is RESOURCE_LIMIT and bad_alloc is ALLOCATION_FAILURE. Separately, insufficient creation work yields HistoryError::WORK_LIMIT. After construction, overflow in fixedVisits/maintenance or insufficient work.visits/textUnits also yields WORK_LIMIT, even with storage capacity still available.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::symbolUnitsPerPartition` — CFG-0247 {#cfg-0247}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per partition
- Accepted values: 0 reserves no symbol storage. A newly stored symbol length must not exceed the field; partitions×symbolUnitsPerPartition must fit checked sizing and the overall resource budget. The maintenance text estimate is 8×(partitions+1)×(viewSlots+1)×(viewSubscriptions.totalSymbolUnits+partitions×symbolUnitsPerPartition). An admitted capacity must also be serviceable by caller-supplied HistoryWork; successful creation does not guarantee enough work for a later operation.
- Read timing: HistoryCollector::create copies capacities into its owner, reserves two banks and checks requested and actual storage; partition subsequently reads the stored bound. During create, dimensions determine initial visits and text work; later fixedVisits/maintenance use stored dimensions to admit maintenance.
- Mutability and lifetime: There is no public HistoryLimits setter; changing it requires a new collector.
- Failure: An overlong symbol in partition is RESOURCE_LIMIT. Product overflow during create is INVALID_CONFIGURATION; exceeding residentBytes is RESOURCE_LIMIT; bad_alloc is ALLOCATION_FAILURE. Separately, insufficient creation work yields HistoryError::WORK_LIMIT. After construction, overflow in fixedVisits/maintenance or insufficient work.visits/textUnits also yields WORK_LIMIT, even with storage capacity still available.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::cacheFrames` — CFG-0248 {#cfg-0248}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: unique cache frame owners
- Accepted values: 0 permits retaining 0 frames. Shared slot capacity includes cacheFrames + viewSlots×pendingFrames + 1; overflow is checked. cacheFrames determines frame slots cacheFrames+viewSlots×pendingFrames+1; fixedVisits uses frames.size(), including a viewSlots+2 multiplier. An admitted capacity must also be serviceable by caller-supplied HistoryWork; successful creation does not guarantee enough work for a later operation.
- Read timing: Copied by HistoryCollector::create; storage is reserved before the owner is published, then enforced by the indicated operation. During create, dimensions determine initial visits and text work; later fixedVisits/maintenance use stored dimensions to admit maintenance.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: Exceeding the old-plus-incoming owner peak in peak(NONE), checked by stage, is RESOURCE_LIMIT. During create, sizing overflow is INVALID_CONFIGURATION, insufficient residentBytes is RESOURCE_LIMIT and bad_alloc is ALLOCATION_FAILURE. Separately, insufficient creation work yields HistoryError::WORK_LIMIT. After construction, overflow in fixedVisits/maintenance or insufficient work.visits/textUnits also yields WORK_LIMIT, even with storage capacity still available.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::cacheBytes` — CFG-0249 {#cfg-0249}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: State+arena bytes of unique cache events
- Accepted values: 0 provides no retention budget; cacheBytes + viewSlots×pendingBytes is reserved within residentBytes. Peak is checked before old data are released.
- Read timing: Copied by HistoryCollector::create; storage is reserved before the owner is published, then enforced by the indicated operation.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: Exceeding the cache byte peak checked in stage is RESOURCE_LIMIT. During create, sizing overflow is INVALID_CONFIGURATION, insufficient residentBytes is RESOURCE_LIMIT and bad_alloc is ALLOCATION_FAILURE.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::viewSlots` — CFG-0250 {#cfg-0250}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: independent views
- Accepted values: 0 creates a collector with no available views. Products with pendingItems, pendingFrames and subscription limits are checked during creation. viewSlots enters viewSlots×pendingItems, viewSlots×viewSubscriptions.entries, frame-slot count and nonlinear fixedVisits products; the maintenance text estimate is multiplied by viewSlots+1. An admitted capacity must also be serviceable by caller-supplied HistoryWork; successful creation does not guarantee enough work for a later operation.
- Read timing: Copied by HistoryCollector::create; storage is reserved before the owner is published, then enforced by the indicated operation. During create, dimensions determine initial visits and text work; later fixedVisits/maintenance use stored dimensions to admit maintenance.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: openView without a free slot is RESOURCE_LIMIT. During create, sizing overflow is INVALID_CONFIGURATION, insufficient residentBytes is RESOURCE_LIMIT and bad_alloc is ALLOCATION_FAILURE. Separately, insufficient creation work yields HistoryError::WORK_LIMIT. After construction, overflow in fixedVisits/maintenance or insufficient work.visits/textUnits also yields WORK_LIMIT, even with storage capacity still available.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::pendingItems` — CFG-0251 {#cfg-0251}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: FIFO items per view
- Accepted values: 0 leaves no room for a new local item; replacing the last item with the same index uses its existing slot. Total viewSlots×pendingItems storage is checked in create. pendingItems enters vp=viewSlots×pendingItems, then fixedVisits through 16×partitions×vp and (viewSlots+2)×vp. An admitted capacity must also be serviceable by caller-supplied HistoryWork; successful creation does not guarantee enough work for a later operation.
- Read timing: Copied by HistoryCollector::create; storage is reserved before the owner is published, then enforced by the indicated operation. During create, dimensions determine initial visits and text work; later fixedVisits/maintenance use stored dimensions to admit maintenance.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: A full FIFO invokes failView: HistoryViewFailure records RESOURCE_LIMIT and ordinal; it is a failure of that view. During create, sizing overflow is INVALID_CONFIGURATION, insufficient residentBytes is RESOURCE_LIMIT and bad_alloc is ALLOCATION_FAILURE. Separately, insufficient creation work yields HistoryError::WORK_LIMIT. After construction, overflow in fixedVisits/maintenance or insufficient work.visits/textUnits also yields WORK_LIMIT, even with storage capacity still available.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::pendingFrames` — CFG-0252 {#cfg-0252}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: peak frame owners per view
- Accepted values: 0 means 0 retained owners. Each view checks its old-plus-incoming owner peak; the frame table includes viewSlots×pendingFrames. pendingFrames enters frame slots cacheFrames+viewSlots×pendingFrames+1; fixedVisits uses frames.size(), including a viewSlots+2 multiplier. An admitted capacity must also be serviceable by caller-supplied HistoryWork; successful creation does not guarantee enough work for a later operation.
- Read timing: Copied by HistoryCollector::create; storage is reserved before the owner is published, then enforced by the indicated operation. During create, dimensions determine initial visits and text work; later fixedVisits/maintenance use stored dimensions to admit maintenance.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: A failed peak(view) invokes failView with RESOURCE_LIMIT; creation sizing overflow is INVALID_CONFIGURATION. During create, sizing overflow is INVALID_CONFIGURATION, insufficient residentBytes is RESOURCE_LIMIT and bad_alloc is ALLOCATION_FAILURE. Separately, insufficient creation work yields HistoryError::WORK_LIMIT. After construction, overflow in fixedVisits/maintenance or insufficient work.visits/textUnits also yields WORK_LIMIT, even with storage capacity still available.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::pendingBytes` — CFG-0253 {#cfg-0253}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: whole-event bytes per view
- Accepted values: 0 means 0 retention bytes. Peak charges a whole event; viewSlots×pendingBytes participates in the initial residentBytes reservation.
- Read timing: Copied by HistoryCollector::create; storage is reserved before the owner is published, then enforced by the indicated operation.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: Exceeding a view byte peak invokes failView with RESOURCE_LIMIT. During create, sizing overflow is INVALID_CONFIGURATION, insufficient residentBytes is RESOURCE_LIMIT and bad_alloc is ALLOCATION_FAILURE.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::residentBytes` — CFG-0254 {#cfg-0254}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of known fixed backing and unique event peak
- Accepted values: 0 is not unlimited and cannot hold the mandatory Impl/identity. Fixed requested and actual capacities plus cacheBytes + viewSlots×pendingBytes must fit; arithmetic is checked.
- Read timing: Read in create before bank allocation and again afterward using actual capacities; copied into the owner.
- Mutability and lifetime: There is no public HistoryLimits setter; changing it requires a new collector.
- Failure: reserveBytes, requestedFixed+reserveBytes or fixed+reserveBytes above the bound is RESOURCE_LIMIT; helper arithmetic overflow is INVALID_CONFIGURATION.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryLimits::viewSubscriptions` — CFG-0255 {#cfg-0255}

- Type: `core::SubscriptionLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Independently enforced desired-state capacity per view.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Collector create copies explicit capacity/profile; individual stage/front/pop/read operations use existing owner limits
- Mutability and lifetime: No aggregate live setter; desired/view mutations and reset use explicit APIs within configured limits
- Failure: Owner error vocabulary: HistoryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

## HistoryOptions {#cfg-0256-owner}

[include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp)

### `HistoryOptions::storeEverything` — CFG-0256 {#cfg-0256}

- Type: `bool`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Retain unrequested points; false keeps only requested ranges plus bounded empty metadata.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: Collector create copies explicit capacity/profile; individual stage/front/pop/read operations use existing owner limits
- Mutability and lifetime: No aggregate live setter; desired/view mutations and reset use explicit APIs within configured limits
- Failure: Owner error vocabulary: HistoryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

### `HistoryOptions::retrievalQuantum` — CFG-0257 {#cfg-0257}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive global-snapshot/local-update balance, selected source default100.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive global-snapshot/local-update balance, selected source default100.
- Read timing: Collector create copies explicit capacity/profile; individual stage/front/pop/read operations use existing owner limits
- Mutability and lifetime: No aggregate live setter; desired/view mutations and reset use explicit APIs within configured limits
- Failure: Owner error vocabulary: HistoryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

### `HistoryOptions::partitionSnapshotQuantum` — CFG-0258 {#cfg-0258}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive per-partition snapshot rotation, source default10000.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive per-partition snapshot rotation, source default10000.
- Read timing: Collector create copies explicit capacity/profile; individual stage/front/pop/read operations use existing owner limits
- Mutability and lifetime: No aggregate live setter; desired/view mutations and reset use explicit APIs within configured limits
- Failure: Owner error vocabulary: HistoryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

### `HistoryOptions::selectorPolicy` — CFG-0259 {#cfg-0259}

- Type: `core::SubscriptionSelectorPolicy`. Declared default: `= core::SubscriptionSelectorPolicy::EXACT_ONLY`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: symbol matching policy
- Accepted values: Only EXACT_ONLY and CANONICAL_EXACT_V1 are accepted; All is not admitted by this boundary.
- Read timing: Checked during create and passed to view SubscriptionState creation.
- Mutability and lifetime: Immutable after create.
- Failure: Any other value is HistoryError::UNSUPPORTED_SELECTOR.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryOptions::retransmitSnapshotRecords` — CFG-0260 {#cfg-0260}

- Type: `std::span<const std::int32_t>`. Declared default: `= {}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: local indexed-record IDs
- Accepted values: An empty list disables special retransmission. Count must not exceed schema records; every ID is nonnegative, exists, has indexedTimeLayout and is unique.
- Read timing: Borrowed only in create and copied into an owned flag table. On BEGIN/MODE, stage reads the record flag.
- Mutability and lifetime: The table is immutable after create; the source span is no longer used.
- Failure: Invalid ID, duplicate, non-indexed record or excessive count is HistoryError::INVALID_CONFIGURATION; sizing overflow uses the same error.
- Consumer sources: [include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp), [src/collectors/history.cpp](../../src/collectors/history.cpp).

### `HistoryOptions::reclaimInactive` — CFG-0261 {#cfg-0261}

- Type: `bool`. Declared default: `= false`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Release empty partitions after the last view withdraws. Requires storeEverything=false.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: Collector create copies explicit capacity/profile; individual stage/front/pop/read operations use existing owner limits
- Mutability and lifetime: No aggregate live setter; desired/view mutations and reset use explicit APIs within configured limits
- Failure: Owner error vocabulary: HistoryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

## HistoryViewOptions {#cfg-0262-owner}

[include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp)

### `HistoryViewOptions::historySnapshot` — CFG-0262 {#cfg-0262}

- Type: `bool`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Use selected snapshot/transaction projection flags.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: Collector create copies explicit capacity/profile; individual stage/front/pop/read operations use existing owner limits
- Mutability and lifetime: No aggregate live setter; desired/view mutations and reset use explicit APIs within configured limits
- Failure: Owner error vocabulary: HistoryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

## HistoryWork {#cfg-0263-owner}

[include/qd/collectors/history.hpp](../../include/qd/collectors/history.hpp)

### `HistoryWork::operations` — CFG-0263 {#cfg-0263}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Owner operation attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

### `HistoryWork::visits` — CFG-0264 {#cfg-0264}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Metadata/field/matching/queue/init/release visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

### `HistoryWork::textUnits` — CFG-0265 {#cfg-0265}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservatively examined/copied UTF16 units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

### `HistoryWork::bytes` — CFG-0266 {#cfg-0266}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Examined/copied payload and conservative shared-disposal work allowance.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `HistoryError`.

## StreamLimits {#cfg-0267-owner}

[include/qd/collectors/stream.hpp](../../include/qd/collectors/stream.hpp)

### `StreamLimits::frames` — CFG-0267 {#cfg-0267}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: frame owners
- Accepted values: 0 admits no new retained frame. No separate maximum is declared; checked sizing arithmetic and residentBytes constrain creation, while stage constrains occupancy. After the residentBytes check, creation also requires work.visits for frames and records and work.bytes for the entire calculated backing.
- Read timing: Copied during StreamCollector::create; fixed storage is calculated before allocation and stage reads the stored bounds.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: Sizing overflow or fixed above residentBytes during create is StreamError::RESOURCE_LIMIT. In stage, exceeding the corresponding bound sets a RESOURCE_LIMIT diagnostic and rejects admission; bad_alloc during create is ALLOCATION_FAILURE. Even with sufficient residentBytes, insufficient work.visits or work.bytes before allocation yields StreamError::WORK_LIMIT; debits occur in frames, records, then bytes order.
- Consumer sources: [include/qd/collectors/stream.hpp](../../include/qd/collectors/stream.hpp), [src/collectors/stream.cpp](../../src/collectors/stream.cpp).

### `StreamLimits::records` — CFG-0268 {#cfg-0268}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: selected records
- Accepted values: 0 admits no selected record. No separate maximum is declared; checked sizing arithmetic and residentBytes constrain creation, while stage constrains occupancy. After the residentBytes check, creation also requires work.visits for frames and records and work.bytes for the entire calculated backing.
- Read timing: Copied during StreamCollector::create; fixed storage is calculated before allocation and stage reads the stored bounds.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: Sizing overflow or fixed above residentBytes during create is StreamError::RESOURCE_LIMIT. In stage, exceeding the corresponding bound sets a RESOURCE_LIMIT diagnostic and rejects admission; bad_alloc during create is ALLOCATION_FAILURE. Even with sufficient residentBytes, insufficient work.visits or work.bytes before allocation yields StreamError::WORK_LIMIT; debits occur in frames, records, then bytes order.
- Consumer sources: [include/qd/collectors/stream.hpp](../../include/qd/collectors/stream.hpp), [src/collectors/stream.cpp](../../src/collectors/stream.cpp).

### `StreamLimits::stagedBytes` — CFG-0269 {#cfg-0269}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of unique whole events
- Accepted values: create captures any stagedBytes value without a separate numeric predicate or preallocation proportional to that value. 0 grants no retained-event budget. When stage selects at least one record, event.residentBytes() must fit stagedBytes-currentStagedBytes and independently residentBytes-fixedBytes-currentStagedBytes. With no selected records, that charge is avoided.
- Read timing: create only copies stagedBytes into the profile. stage reads it after record selection and before retaining the whole event.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: During stage, event.residentBytes() exceeding the stagedBytes remainder sets the RESOURCE_LIMIT diagnostic, invalidates the offer and returns StreamError::RESOURCE_LIMIT before event admission. Construction has no refusal caused by the stagedBytes magnitude itself.
- Consumer sources: [include/qd/collectors/stream.hpp](../../include/qd/collectors/stream.hpp), [src/collectors/stream.cpp](../../src/collectors/stream.cpp).

### `StreamLimits::residentBytes` — CFG-0270 {#cfg-0270}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: fixed storage and retained events, bytes
- Accepted values: 0 cannot hold mandatory Impl storage. The frames/records arrays and the new stagedBytes peak must fit. No separate maximum is declared; checked sizing arithmetic and residentBytes constrain creation, while stage constrains occupancy.
- Read timing: Copied during StreamCollector::create; fixed storage is calculated before allocation and stage reads the stored bounds.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: Sizing overflow or fixed above residentBytes during create is StreamError::RESOURCE_LIMIT. In stage, exceeding the corresponding bound sets a RESOURCE_LIMIT diagnostic and rejects admission; bad_alloc during create is ALLOCATION_FAILURE.
- Consumer sources: [include/qd/collectors/stream.hpp](../../include/qd/collectors/stream.hpp), [src/collectors/stream.cpp](../../src/collectors/stream.cpp).

## StreamWork {#cfg-0271-owner}

[include/qd/collectors/stream.hpp](../../include/qd/collectors/stream.hpp)

### `StreamWork::operations` — CFG-0271 {#cfg-0271}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining factory/stage/front/pop/discard attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `StreamError`.

### `StreamWork::visits` — CFG-0272 {#cfg-0272}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining metadata initialization, matching, membership and disposal visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `StreamError`.

### `StreamWork::textUnits` — CFG-0273 {#cfg-0273}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining conservatively examined UTF-16 units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `StreamError`.

### `StreamWork::bytes` — CFG-0274 {#cfg-0274}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining fixed-backing initialization and conservative shared-owner release work.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `StreamError`.

## TickerLimits {#cfg-0275-owner}

[include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp)

### `TickerLimits::viewSlots` — CFG-0275 {#cfg-0275}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: views
- Accepted values: 0 is accepted; NONE is rejected. The viewSlots×cacheKeys cell count is checked.
- Read timing: Copied in create; key/frame/byte limits are checked in stage and view slots in openView.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: viewSlots==NONE yields TickerError::INVALID_CONFIGURATION during create. viewSlots×cacheKeys overflow is INVALID_CONFIGURATION; no free slot in openView is RESOURCE_LIMIT.
- Consumer sources: [include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp), [src/collectors/ticker.cpp](../../src/collectors/ticker.cpp).

### `TickerLimits::cacheKeys` — CFG-0276 {#cfg-0276}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: distinct cache keys and pending keys per view
- Accepted values: 0 creates empty key tables; no separate positive minimum exists.
- Read timing: Copied in create; key/frame/byte limits are checked in stage and view slots in openView.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: Fixed-table sizing overflow during create is TickerError::INVALID_CONFIGURATION. If stage finds no free cacheKeys slot, it marks overflow and returns RESOURCE_LIMIT.
- Consumer sources: [include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp), [src/collectors/ticker.cpp](../../src/collectors/ticker.cpp).

### `TickerLimits::cachedFrames` — CFG-0277 {#cfg-0277}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: unique frame owners at peak
- Accepted values: 0 admits no new retained frame; create imposes no separate positive minimum.
- Read timing: Copied in create; key/frame/byte limits are checked in stage and view slots in openView.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: Array sizing overflow during create is TickerError::INVALID_CONFIGURATION. If an incoming event needs a new owner when frameCount==cachedFrames, stage returns RESOURCE_LIMIT.
- Consumer sources: [include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp), [src/collectors/ticker.cpp](../../src/collectors/ticker.cpp).

### `TickerLimits::cacheBytes` — CFG-0278 {#cfg-0278}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: State+arena bytes of whole events
- Accepted values: 0 gives events no byte budget. The old-plus-incoming event peak is checked.
- Read timing: Copied in create; key/frame/byte limits are checked in stage and view slots in openView.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: For an incoming retained event, incomingBytes&gt;cacheBytes−min(currentBytes,cacheBytes) yields TickerError::RESOURCE_LIMIT; the operation gets no credit for retiring an old event.
- Consumer sources: [include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp), [src/collectors/ticker.cpp](../../src/collectors/ticker.cpp).

### `TickerLimits::residentBytes` — CFG-0279 {#cfg-0279}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of fixed storage, schema and events
- Accepted values: Must hold fixed storage during create and fixed+event peak during stage. 0 cannot hold mandatory structures.
- Read timing: Copied in create; key/frame/byte limits are checked in stage and view slots in openView.
- Mutability and lifetime: The live collector has no capacity setter; a new owner is created.
- Failure: fixed&gt;residentBytes during create is TickerError::RESOURCE_LIMIT. During stage, incomingBytes&gt;residentBytes−min(fixed+currentBytes,residentBytes) is also RESOURCE_LIMIT.
- Consumer sources: [include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp), [src/collectors/ticker.cpp](../../src/collectors/ticker.cpp).

### `TickerLimits::viewSubscriptions` — CFG-0280 {#cfg-0280}

- Type: `core::SubscriptionLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Independently enforced desired-state scope for every view.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Collector create copies explicit capacity/profile; individual stage/front/pop/read operations use existing owner limits
- Mutability and lifetime: No aggregate live setter; desired/view mutations and reset use explicit APIs within configured limits
- Failure: Owner error vocabulary: TickerError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TickerError`.

## TickerOptions {#cfg-0281-owner}

[include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp)

### `TickerOptions::storeEverything` — CFG-0281 {#cfg-0281}

- Type: `bool`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: False evicts after last interest; true retains unrequested source keys.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: Collector create copies explicit capacity/profile; individual stage/front/pop/read operations use existing owner limits
- Mutability and lifetime: No aggregate live setter; desired/view mutations and reset use explicit APIs within configured limits
- Failure: Owner error vocabulary: TickerError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TickerError`.

### `TickerOptions::retrievalQuantum` — CFG-0282 {#cfg-0282}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive initial/update credit quantum; locked Java default is 100.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive initial/update credit quantum; locked Java default is 100.
- Read timing: Collector create copies explicit capacity/profile; individual stage/front/pop/read operations use existing owner limits
- Mutability and lifetime: No aggregate live setter; desired/view mutations and reset use explicit APIs within configured limits
- Failure: Owner error vocabulary: TickerError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TickerError`.

### `TickerOptions::selectorPolicy` — CFG-0283 {#cfg-0283}

- Type: `core::SubscriptionSelectorPolicy`. Declared default: `= core::SubscriptionSelectorPolicy::EXACT_ONLY`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: symbol matching policy
- Accepted values: Only EXACT_ONLY and CANONICAL_EXACT_V1 are accepted; All is not admitted by this boundary.
- Read timing: Checked during create and passed to view SubscriptionState creation.
- Mutability and lifetime: Immutable after create.
- Failure: Any other value is TickerError::UNSUPPORTED_SELECTOR.
- Consumer sources: [include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp), [src/collectors/ticker.cpp](../../src/collectors/ticker.cpp).

### `TickerOptions::versionBindings` — CFG-0284 {#cfg-0284}

- Type: `std::span<const TickerVersionBinding>`. Declared default: `{}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: scalar version-field bindings to schema records
- Accepted values: An empty list disables version comparisons. Count is at most schema records; context must match localRecordId, fieldIndex must exist, serial type must pass versionType and contexts must be unique. Accepted scalar types: VOID_FIELD, BYTE, UTF_CHAR, SHORT, INT, COMPACT_INT, DECIMAL, SHORT_STRING, TIME_SECONDS, SEQUENCE, DATE, LONG, WIDE_DECIMAL, TIME_MILLIS; see the exact switch.
- Read timing: The span is read in create; indices are copied into an owned table and the original span is cleared; stage reads versionFields.
- Mutability and lifetime: Immutable after create.
- Failure: Invalid binding, duplicate or excessive count is TickerError::INVALID_CONFIGURATION. Owned table sizing also passes the residentBytes bound.
- Consumer sources: [include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp), [src/collectors/ticker.cpp](../../src/collectors/ticker.cpp).

## TickerWork {#cfg-0285-owner}

[include/qd/collectors/ticker.hpp](../../include/qd/collectors/ticker.hpp)

### `TickerWork::operations` — CFG-0285 {#cfg-0285}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining owner operation attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TickerError`.

### `TickerWork::visits` — CFG-0286 {#cfg-0286}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining metadata, matching, equality, queue and disposal visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TickerError`.

### `TickerWork::textUnits` — CFG-0287 {#cfg-0287}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining conservatively examined UTF-16 units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TickerError`.

### `TickerWork::bytes` — CFG-0288 {#cfg-0288}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining examined payload bytes and conservative event/snapshot release work.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TickerError`.
