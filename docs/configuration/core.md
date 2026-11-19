# Schemas and desired subscriptions

[Русский](core.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [SchemaLimits](core.md#cfg-0289-owner) — `include/qd/core/local_schema.hpp`
- [SubscriptionLimits](core.md#cfg-0294-owner) — `include/qd/core/subscription.hpp`
- [SubscriptionWork](core.md#cfg-0301-owner) — `include/qd/core/subscription.hpp`

## SchemaLimits {#cfg-0289-owner}

[include/qd/core/local_schema.hpp](../../include/qd/core/local_schema.hpp)

### `SchemaLimits::records` — CFG-0289 {#cfg-0289}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: schema records
- Accepted values: Input record count ≤ records. 0 permits an empty schema; no positive minimum for the limit is imposed. Dense localId must also fit int32.
- Read timing: Each create/boundedCopy receives limits by value; aggregate budgets are locally debited while traversing the complete schema.
- Mutability and lifetime: Not live schema state: a new call gets its own limits; an existing schema declaration is immutable.
- Failure: Exceeding this budget is SchemaError::RESOURCE_LIMIT. Independent structure errors retain INVALID_SCHEMA/UNSUPPORTED_PROFILE; the limit itself does not cause those branches.
- Consumer sources: [include/qd/core/local_schema.hpp](../../include/qd/core/local_schema.hpp), [src/core/local_schema.cpp](../../src/core/local_schema.cpp).

### `SchemaLimits::fields` — CFG-0290 {#cfg-0290}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: fields across the schema
- Accepted values: The sum of fields.size() across records ≤ fields. 0 provides no field budget; remaining-budget subtraction prevents overflow.
- Read timing: Each create/boundedCopy receives limits by value; aggregate budgets are locally debited while traversing the complete schema.
- Mutability and lifetime: Not live schema state: a new call gets its own limits; an existing schema declaration is immutable.
- Failure: Exceeding this budget is SchemaError::RESOURCE_LIMIT. Independent structure errors retain INVALID_SCHEMA/UNSUPPORTED_PROFILE; the limit itself does not cause those branches.
- Consumer sources: [include/qd/core/local_schema.hpp](../../include/qd/core/local_schema.hpp), [src/core/local_schema.cpp](../../src/core/local_schema.cpp).

### `SchemaLimits::lookups` — CFG-0291 {#cfg-0291}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: lookup keys across the schema
- Accepted values: The sum of lookups.size() across records ≤ lookups. 0 provides no lookup-key budget.
- Read timing: Each create/boundedCopy receives limits by value; aggregate budgets are locally debited while traversing the complete schema.
- Mutability and lifetime: Not live schema state: a new call gets its own limits; an existing schema declaration is immutable.
- Failure: Exceeding this budget is SchemaError::RESOURCE_LIMIT. Independent structure errors retain INVALID_SCHEMA/UNSUPPORTED_PROFILE; the limit itself does not cause those branches.
- Consumer sources: [include/qd/core/local_schema.hpp](../../include/qd/core/local_schema.hpp), [src/core/local_schema.cpp](../../src/core/local_schema.cpp).

### `SchemaLimits::nameUnits` — CFG-0292 {#cfg-0292}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per record/key name
- Accepted values: Every record name and lookup.name ≤ nameUnits. 0 cannot hold a nonempty valid name; there is no separate configuration maximum.
- Read timing: Each create/boundedCopy receives limits by value; aggregate budgets are locally debited while traversing the complete schema.
- Mutability and lifetime: Not live schema state: a new call gets its own limits; an existing schema declaration is immutable.
- Failure: Exceeding this budget is SchemaError::RESOURCE_LIMIT. Independent structure errors retain INVALID_SCHEMA/UNSUPPORTED_PROFILE; the limit itself does not cause those branches.
- Consumer sources: [include/qd/core/local_schema.hpp](../../include/qd/core/local_schema.hpp), [src/core/local_schema.cpp](../../src/core/local_schema.cpp).

### `SchemaLimits::totalNameUnits` — CFG-0293 {#cfg-0293}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: total UTF-16 name code units
- Accepted values: Combined record and lookup-name usage ≤ totalNameUnits; 0 is an empty budget with no unlimited sentinel.
- Read timing: Each create/boundedCopy receives limits by value; aggregate budgets are locally debited while traversing the complete schema.
- Mutability and lifetime: Not live schema state: a new call gets its own limits; an existing schema declaration is immutable.
- Failure: Exceeding this budget is SchemaError::RESOURCE_LIMIT. Independent structure errors retain INVALID_SCHEMA/UNSUPPORTED_PROFILE; the limit itself does not cause those branches.
- Consumer sources: [include/qd/core/local_schema.hpp](../../include/qd/core/local_schema.hpp), [src/core/local_schema.cpp](../../src/core/local_schema.cpp).

## SubscriptionLimits {#cfg-0294-owner}

[include/qd/core/subscription.hpp](../../include/qd/core/subscription.hpp)

### `SubscriptionLimits::schemaRecords` — CFG-0294 {#cfg-0294}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: local schema records
- Accepted values: Schema records count ≤ schemaRecords; count×sizeof(Record) is checked separately. 0 permits only a schema with no records.
- Read timing: Copied by SubscriptionState::create; schemaRecords is checked at creation, other fields are read when preparing each add/remove/replace command.
- Mutability and lifetime: There is no limits_ replacement setter; add/remove/replace change desired state within the existing limits.
- Failure: Failure of the stated comparison or addCapacity overflow is SubscriptionError::RESOURCE_LIMIT; the candidate is not committed. Allocation failure separately returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/core/subscription.hpp](../../include/qd/core/subscription.hpp), [src/core/subscription.cpp](../../src/core/subscription.cpp).

### `SubscriptionLimits::entries` — CFG-0295 {#cfg-0295}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: published subscription entries
- Accepted values: Candidate entry count ≤ entries; 0 allows only empty desired state.
- Read timing: Copied by SubscriptionState::create; schemaRecords is checked at creation, other fields are read when preparing each add/remove/replace command.
- Mutability and lifetime: There is no limits_ replacement setter; add/remove/replace change desired state within the existing limits.
- Failure: Failure of the stated comparison or addCapacity overflow is SubscriptionError::RESOURCE_LIMIT; the candidate is not committed. Allocation failure separately returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/core/subscription.hpp](../../include/qd/core/subscription.hpp), [src/core/subscription.cpp](../../src/core/subscription.cpp).

### `SubscriptionLimits::symbolUnits` — CFG-0296 {#cfg-0296}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units of one symbol
- Accepted values: Each input key.symbol length ≤ symbolUnits; 0 is not unlimited. Empty-symbol validity additionally depends on selector policy.
- Read timing: Copied by SubscriptionState::create; schemaRecords is checked at creation, other fields are read when preparing each add/remove/replace command.
- Mutability and lifetime: There is no limits_ replacement setter; add/remove/replace change desired state within the existing limits.
- Failure: Failure of the stated comparison or addCapacity overflow is SubscriptionError::RESOURCE_LIMIT; the candidate is not committed. Allocation failure separately returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/core/subscription.hpp](../../include/qd/core/subscription.hpp), [src/core/subscription.cpp](../../src/core/subscription.cpp).

### `SubscriptionLimits::totalSymbolUnits` — CFG-0297 {#cfg-0297}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 symbol units of the candidate
- Accepted values: Sum of candidate entry symbol lengths ≤ totalSymbolUnits; accounting follows entries and their symbols. 0 is an empty budget.
- Read timing: Copied by SubscriptionState::create; schemaRecords is checked at creation, other fields are read when preparing each add/remove/replace command.
- Mutability and lifetime: There is no limits_ replacement setter; add/remove/replace change desired state within the existing limits.
- Failure: Failure of the stated comparison or addCapacity overflow is SubscriptionError::RESOURCE_LIMIT; the candidate is not committed. Allocation failure separately returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/core/subscription.hpp](../../include/qd/core/subscription.hpp), [src/core/subscription.cpp](../../src/core/subscription.cpp).

### `SubscriptionLimits::peakEntries` — CFG-0298 {#cfg-0298}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: coexisting old and candidate entries
- Accepted values: For a changing command, size_ + candidateSize ≤ peakEntries. An unchanged command returns Candidate before this check. 0 is literal zero.
- Read timing: Copied by SubscriptionState::create; schemaRecords is checked at creation, other fields are read when preparing each add/remove/replace command.
- Mutability and lifetime: There is no limits_ replacement setter; add/remove/replace change desired state within the existing limits.
- Failure: Failure of the stated comparison or addCapacity overflow is SubscriptionError::RESOURCE_LIMIT; the candidate is not committed. Allocation failure separately returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/core/subscription.hpp](../../include/qd/core/subscription.hpp), [src/core/subscription.cpp](../../src/core/subscription.cpp).

### `SubscriptionLimits::peakSymbolUnits` — CFG-0299 {#cfg-0299}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units of old and candidate state
- Accepted values: For a changing command, symbolUnits_ + candidateUnits ≤ peakSymbolUnits. An unchanged command needs no new peak reservation.
- Read timing: Copied by SubscriptionState::create; schemaRecords is checked at creation, other fields are read when preparing each add/remove/replace command.
- Mutability and lifetime: There is no limits_ replacement setter; add/remove/replace change desired state within the existing limits.
- Failure: Failure of the stated comparison or addCapacity overflow is SubscriptionError::RESOURCE_LIMIT; the candidate is not committed. Allocation failure separately returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/core/subscription.hpp](../../include/qd/core/subscription.hpp), [src/core/subscription.cpp](../../src/core/subscription.cpp).

### `SubscriptionLimits::commandEntries` — CFG-0300 {#cfg-0300}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input entries of one command
- Accepted values: REMOVE keys count or other command requests count ≤ commandEntries. 0 permits only empty input.
- Read timing: Copied by SubscriptionState::create; schemaRecords is checked at creation, other fields are read when preparing each add/remove/replace command.
- Mutability and lifetime: There is no limits_ replacement setter; add/remove/replace change desired state within the existing limits.
- Failure: Failure of the stated comparison or addCapacity overflow is SubscriptionError::RESOURCE_LIMIT; the candidate is not committed. Allocation failure separately returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/core/subscription.hpp](../../include/qd/core/subscription.hpp), [src/core/subscription.cpp](../../src/core/subscription.cpp).

## SubscriptionWork {#cfg-0301-owner}

[include/qd/core/subscription.hpp](../../include/qd/core/subscription.hpp)

### `SubscriptionWork::commands` — CFG-0301 {#cfg-0301}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining command attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionError`.

### `SubscriptionWork::keyVisits` — CFG-0302 {#cfg-0302}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining conservative input/record/entry/comparison and disposal visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionError`.

### `SubscriptionWork::symbolUnits` — CFG-0303 {#cfg-0303}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining conservative examined/copied UTF-16 units, including duplicates.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionError`.
