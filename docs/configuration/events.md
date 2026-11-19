# Events and mappings

[Русский](events.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [CandleRouteLimits](events.md#cfg-0311-owner) — `include/qd/events/candle_routes.hpp`
- [EventValueLimits](events.md#cfg-0323-owner) — `include/qd/events/event_mapping.hpp`
- [MappingLimits](events.md#cfg-0315-owner) — `include/qd/events/event_mapping.hpp`
- [EventTextLimits](events.md#cfg-0326-owner) — `include/qd/events/event_text.hpp`
- [AttachmentEventLimits](events.md#cfg-0330-owner) — `include/qd/events/message.hpp`
- [AttachmentCodecRegistryLimits](events.md#cfg-0338-owner) — `include/qd/events/serialized_attachment.hpp`
- [AttachmentDecodeLimits](events.md#cfg-0333-owner) — `include/qd/events/serialized_attachment.hpp`

## CandleRouteLimits {#cfg-0311-owner}

[include/qd/events/candle_routes.hpp](../../include/qd/events/candle_routes.hpp)

### `CandleRouteLimits::records` — CFG-0311 {#cfg-0311}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: physical route record IDs
- Accepted values: recordIds.size()&lt;=records before inspecting IDs. Zero permits an empty ID input; validity and duplicates are checked separately.
- Read timing: PreparedCandleRoutes::prepare checks IDs/names; resolve checks the captured physical-symbol bound after conversion.
- Mutability and lifetime: The field is consumed during prepare and is not retained by PreparedCandleRoutes. Another prepare may use different bounds; the prepared set stores entries and the separate symbolUnits value.
- Failure: std::expected: CandleRouteFailure{CandleRouteError::RESOURCE_LIMIT,recordId?,...}; bad_alloc/length_error → ALLOCATION_FAILURE; grammar/route failures are separate.
- Consumer sources: [include/qd/events/candle_routes.hpp](../../include/qd/events/candle_routes.hpp), [src/events/candle_routes.cpp](../../src/events/candle_routes.cpp).

### `CandleRouteLimits::recordNameUnits` — CFG-0312 {#cfg-0312}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 physical-record-name units
- Accepted values: Each record.name.size()&lt;=recordNameUnits; zero excludes nonempty names from the route set. No additional upper ceiling applies.
- Read timing: PreparedCandleRoutes::prepare checks IDs/names; resolve checks the captured physical-symbol bound after conversion.
- Mutability and lifetime: The field is consumed during prepare and is not retained by PreparedCandleRoutes. Another prepare may use different bounds; the prepared set stores entries and the separate symbolUnits value.
- Failure: std::expected: CandleRouteFailure{CandleRouteError::RESOURCE_LIMIT,recordId?,...}; bad_alloc/length_error → ALLOCATION_FAILURE; grammar/route failures are separate.
- Consumer sources: [include/qd/events/candle_routes.hpp](../../include/qd/events/candle_routes.hpp), [src/events/candle_routes.cpp](../../src/events/candle_routes.cpp).

### `CandleRouteLimits::symbolUnits` — CFG-0313 {#cfg-0313}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 physical Candle-symbol units
- Accepted values: The same retained symbolUnits bound applies at four sites: resolve logical input, its physical output, restore physical input and restored logical output. Each corresponding UTF-16 length must be&lt;=symbolUnits. Zero permits only an empty length through this test; symbol grammar may separately reject it.
- Read timing: prepare retains symbolUnits alongside entries. resolve and restore check it before transforming input and again after constructing the output.
- Mutability and lifetime: PreparedCandleRoutes retains the bound for every subsequent resolve/restore. Another prepare creates an independent set with a new bound.
- Failure: std::expected: CandleRouteFailure{CandleRouteError::RESOURCE_LIMIT,recordId?,...}; bad_alloc/length_error → ALLOCATION_FAILURE; grammar/route failures are separate.
- Consumer sources: [include/qd/events/candle_routes.hpp](../../include/qd/events/candle_routes.hpp), [src/events/candle_routes.cpp](../../src/events/candle_routes.cpp).

### `CandleRouteLimits::mapping` — CFG-0314 {#cfg-0314}

- Type: `MappingLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Mapper preparation work and storage limits for each candidate.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;PreparedCandleRoutes, CandleRouteFailure&gt; prepare(const core::LocalSchema &amp;schema, std::span&lt;const std::int32_t&gt; recordIds, CandleRouteLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: CandleRouteError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `CandleRouteError`.

## EventValueLimits {#cfg-0323-owner}

[include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp)

### `EventValueLimits::symbolUnits` — CFG-0323 {#cfg-0323}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 copied-symbol units
- Accepted values: symbol.size()&lt;=symbolUnits and &lt;=totalUnits. Zero permits an empty symbol; context binding and record shape are still checked.
- Read timing: Owning event projection applies supplied EventValueLimits before copying selected values; Profile is a complete consumer example.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: MappingError::RESOURCE_LIMIT through std::expected; context/shape and allocation have separate causes.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/profile.cpp](../../src/events/profile.cpp).

### `EventValueLimits::stringUnits` — CFG-0324 {#cfg-0324}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units per nullable text field
- Accepted values: Each selected text field must be &lt;=stringUnits and remaining totalUnits. Null and empty text consume 0 but remain distinct values.
- Read timing: Owning event projection applies supplied EventValueLimits before copying selected values; Profile is a complete consumer example.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: MappingError::RESOURCE_LIMIT through std::expected; context/shape and allocation have separate causes.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/profile.cpp](../../src/events/profile.cpp).

### `EventValueLimits::totalUnits` — CFG-0325 {#cfg-0325}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 symbol and selected-string units
- Accepted values: Debits the symbol first, then every selected text field. Zero permits zero aggregate length only; per-symbol/per-field limits apply simultaneously.
- Read timing: Owning event projection applies supplied EventValueLimits before copying selected values; Profile is a complete consumer example.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: MappingError::RESOURCE_LIMIT through std::expected; context/shape and allocation have separate causes.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/profile.cpp](../../src/events/profile.cpp).

## MappingLimits {#cfg-0315-owner}

[include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp)

### `MappingLimits::fields` — CFG-0315 {#cfg-0315}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete local-record fields
- Accepted values: record.fields.size()&lt;=fields before preparation. Zero permits a fieldless record only; required selections/keys may independently make it unsuitable.
- Read timing: EventMapping::prepare takes limits by value, checks complete input and debits local remainders before publishing a prepared mapping.
- Mutability and lifetime: MappingLimits are by-value budgets for one EventMapping::prepare call. EventMapping retains context, exact type shape and selections; the budgets are absent from the result. Each prepare accepts an independent set.
- Failure: std::expected: MappingError::RESOURCE_LIMIT for the specific budget excess; ALLOCATION_FAILURE for bad_alloc and length_error. Unknown/missing/ambiguous keys have separate errors.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/event_mapping.cpp](../../src/events/event_mapping.cpp).

### `MappingLimits::lookups` — CFG-0316 {#cfg-0316}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete record lookup entries
- Accepted values: record.lookups.size()&lt;=lookups, including aliases. Zero permits no lookup entries; no separate upper ceiling applies.
- Read timing: EventMapping::prepare takes limits by value, checks complete input and debits local remainders before publishing a prepared mapping.
- Mutability and lifetime: MappingLimits are by-value budgets for one EventMapping::prepare call. EventMapping retains context, exact type shape and selections; the budgets are absent from the result. Each prepare accepts an independent set.
- Failure: std::expected: MappingError::RESOURCE_LIMIT for the specific budget excess; ALLOCATION_FAILURE for bad_alloc and length_error. Unknown/missing/ambiguous keys have separate errors.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/event_mapping.cpp](../../src/events/event_mapping.cpp).

### `MappingLimits::selections` — CFG-0317 {#cfg-0317}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: field-selection requests
- Accepted values: requests.size()&lt;=selections. Checks the complete input list before processing requests; zero permits an empty list.
- Read timing: EventMapping::prepare takes limits by value, checks complete input and debits local remainders before publishing a prepared mapping.
- Mutability and lifetime: MappingLimits are by-value budgets for one EventMapping::prepare call. EventMapping retains context, exact type shape and selections; the budgets are absent from the result. Each prepare accepts an independent set.
- Failure: std::expected: MappingError::RESOURCE_LIMIT for the specific budget excess; ALLOCATION_FAILURE for bad_alloc and length_error. Unknown/missing/ambiguous keys have separate errors.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/event_mapping.cpp](../../src/events/event_mapping.cpp).

### `MappingLimits::keys` — CFG-0318 {#cfg-0318}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate request keys
- Accepted values: Sum of request.keys.size()&lt;=keys, debiting the remainder per request. Zero permits keyless requests only; semantic validity is checked separately.
- Read timing: EventMapping::prepare takes limits by value, checks complete input and debits local remainders before publishing a prepared mapping.
- Mutability and lifetime: MappingLimits are by-value budgets for one EventMapping::prepare call. EventMapping retains context, exact type shape and selections; the budgets are absent from the result. Each prepare accepts an independent set.
- Failure: std::expected: MappingError::RESOURCE_LIMIT for the specific budget excess; ALLOCATION_FAILURE for bad_alloc and length_error. Unknown/missing/ambiguous keys have separate errors.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/event_mapping.cpp](../../src/events/event_mapping.cpp).

### `MappingLimits::typeAlternatives` — CFG-0319 {#cfg-0319}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate full-type alternatives
- Accepted values: Sum of request.types.size()&lt;=typeAlternatives. Zero permits no listed type; type lists must also satisfy request validity.
- Read timing: EventMapping::prepare takes limits by value, checks complete input and debits local remainders before publishing a prepared mapping.
- Mutability and lifetime: MappingLimits are by-value budgets for one EventMapping::prepare call. EventMapping retains context, exact type shape and selections; the budgets are absent from the result. Each prepare accepts an independent set.
- Failure: std::expected: MappingError::RESOURCE_LIMIT for the specific budget excess; ALLOCATION_FAILURE for bad_alloc and length_error. Unknown/missing/ambiguous keys have separate errors.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/event_mapping.cpp](../../src/events/event_mapping.cpp).

### `MappingLimits::nameUnits` — CFG-0320 {#cfg-0320}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per name/key
- Accepted values: Each lookup.name and request.key must be &lt;=nameUnits. Zero permits zero lengths only before separate key validation.
- Read timing: EventMapping::prepare takes limits by value, checks complete input and debits local remainders before publishing a prepared mapping.
- Mutability and lifetime: MappingLimits are by-value budgets for one EventMapping::prepare call. EventMapping retains context, exact type shape and selections; the budgets are absent from the result. Each prepare accepts an independent set.
- Failure: std::expected: MappingError::RESOURCE_LIMIT for the specific budget excess; ALLOCATION_FAILURE for bad_alloc and length_error. Unknown/missing/ambiguous keys have separate errors.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/event_mapping.cpp](../../src/events/event_mapping.cpp).

### `MappingLimits::totalNameUnits` — CFG-0321 {#cfg-0321}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 lookup-name and key units
- Accepted values: All lookup.name and request.key lengths debit the shared totalNameUnits. Repeated input keys/names are charged on each visit. Zero permits a zero sum.
- Read timing: EventMapping::prepare takes limits by value, checks complete input and debits local remainders before publishing a prepared mapping.
- Mutability and lifetime: MappingLimits are by-value budgets for one EventMapping::prepare call. EventMapping retains context, exact type shape and selections; the budgets are absent from the result. Each prepare accepts an independent set.
- Failure: std::expected: MappingError::RESOURCE_LIMIT for the specific budget excess; ALLOCATION_FAILURE for bad_alloc and length_error. Unknown/missing/ambiguous keys have separate errors.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/event_mapping.cpp](../../src/events/event_mapping.cpp).

### `MappingLimits::comparisonWork` — CFG-0322 {#cfg-0322}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: comparisons and compared-name code units
- Accepted values: Each key/lookup pair debits max(lengths)+1; type-alternative visits debit one more. Zero rejects the first operation requiring comparison. Exhaustion is reported as RESOURCE_LIMIT.
- Read timing: EventMapping::prepare takes limits by value, checks complete input and debits local remainders before publishing a prepared mapping.
- Mutability and lifetime: MappingLimits are by-value budgets for one EventMapping::prepare call. EventMapping retains context, exact type shape and selections; the budgets are absent from the result. Each prepare accepts an independent set.
- Failure: std::expected: MappingError::RESOURCE_LIMIT for the specific budget excess; ALLOCATION_FAILURE for bad_alloc and length_error. Unknown/missing/ambiguous keys have separate errors.
- Consumer sources: [include/qd/events/event_mapping.hpp](../../include/qd/events/event_mapping.hpp), [src/events/event_mapping.cpp](../../src/events/event_mapping.cpp).

## EventTextLimits {#cfg-0326-owner}

[include/qd/events/event_text.hpp](../../include/qd/events/event_text.hpp)

### `EventTextLimits::inputUnits` — CFG-0326 {#cfg-0326}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 input text units
- Accepted values: source/toUtf8 debit actual input units;0 permits empty source text, while generated literals may consume other budgets.
- Read timing: Each formatting call creates an EventTextWriter with a private limit copy; debits are monotonic over the complete result.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected: EventTextError::RESOURCE_LIMIT for consume exhaustion/length_error; ALLOCATION for bad_alloc. Context, UTF-16 and enum failures have separate values.
- Consumer sources: [include/qd/events/event_text.hpp](../../include/qd/events/event_text.hpp), [src/events/event_text.cpp](../../src/events/event_text.cpp).

### `EventTextLimits::outputUnits` — CFG-0327 {#cfg-0327}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: generated UTF-16 output units
- Accepted values: append debits every fragment length before result_.append. Zero permits empty output only; this is separate from later UTF-8 bytes.
- Read timing: Each formatting call creates an EventTextWriter with a private limit copy; debits are monotonic over the complete result.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected: EventTextError::RESOURCE_LIMIT for consume exhaustion/length_error; ALLOCATION for bad_alloc. Context, UTF-16 and enum failures have separate values.
- Consumer sources: [include/qd/events/event_text.hpp](../../include/qd/events/event_text.hpp), [src/events/event_text.cpp](../../src/events/event_text.cpp).

### `EventTextLimits::utf8Bytes` — CFG-0328 {#cfg-0328}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: generated UTF-8 bytes
- Accepted values: toUtf8 debits each code point byte count before appending;0 permits an empty result only. Malformed UTF-16 policy is independent.
- Read timing: Each formatting call creates an EventTextWriter with a private limit copy; debits are monotonic over the complete result.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected: EventTextError::RESOURCE_LIMIT for consume exhaustion/length_error; ALLOCATION for bad_alloc. Context, UTF-16 and enum failures have separate values.
- Consumer sources: [include/qd/events/event_text.hpp](../../include/qd/events/event_text.hpp), [src/events/event_text.cpp](../../src/events/event_text.cpp).

### `EventTextLimits::work` — CFG-0329 {#cfg-0329}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical formatting work units
- Accepted values: Debits SCALAR_WORK per scalar, input/output units and UTF-8 bytes. Text may be charged at several stages. Zero permits only paths that perform no debit.
- Read timing: Each formatting call creates an EventTextWriter with a private limit copy; debits are monotonic over the complete result.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected: EventTextError::RESOURCE_LIMIT for consume exhaustion/length_error; ALLOCATION for bad_alloc. Context, UTF-16 and enum failures have separate values.
- Consumer sources: [include/qd/events/event_text.hpp](../../include/qd/events/event_text.hpp), [src/events/event_text.cpp](../../src/events/event_text.cpp).

## AttachmentEventLimits {#cfg-0330-owner}

[include/qd/events/message.hpp](../../include/qd/events/message.hpp)

### `AttachmentEventLimits::symbolUnits` — CFG-0330 {#cfg-0330}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 symbol units
- Accepted values: symbol.size()&lt;=symbolUnits;0 permits an empty symbol. totalBytes also charges sizeof(char16_t) per symbol unit plus payload.
- Read timing: Message/Configuration projection checks the envelope before copying symbols and creating SerializedAttachment.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: MappingError::RESOURCE_LIMIT through std::expected on excess; SerializedAttachment failure maps to RESOURCE_LIMIT or ALLOCATION_FAILURE.
- Consumer sources: [include/qd/events/message.hpp](../../include/qd/events/message.hpp), [src/events/misc_events.cpp](../../src/events/misc_events.cpp).

### `AttachmentEventLimits::payloadBytes` — CFG-0331 {#cfg-0331}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: opaque payload bytes
- Accepted values: bodyBytes&lt;=payloadBytes and&lt;=totalBytes. Null and an empty body have 0 bytes while nullable engagement remains distinct.
- Read timing: Message/Configuration projection checks the envelope before copying symbols and creating SerializedAttachment.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: MappingError::RESOURCE_LIMIT through std::expected on excess; SerializedAttachment failure maps to RESOURCE_LIMIT or ALLOCATION_FAILURE.
- Consumer sources: [include/qd/events/message.hpp](../../include/qd/events/message.hpp), [src/events/misc_events.cpp](../../src/events/misc_events.cpp).

### `AttachmentEventLimits::totalBytes` — CFG-0332 {#cfg-0332}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate symbol and body bytes
- Accepted values: bodyBytes+symbol.size()*sizeof(char16_t)&lt;=totalBytes via subtraction/division before copying. Zero permits an empty symbol and null/empty body only.
- Read timing: Message/Configuration projection checks the envelope before copying symbols and creating SerializedAttachment.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: MappingError::RESOURCE_LIMIT through std::expected on excess; SerializedAttachment failure maps to RESOURCE_LIMIT or ALLOCATION_FAILURE.
- Consumer sources: [include/qd/events/message.hpp](../../include/qd/events/message.hpp), [src/events/misc_events.cpp](../../src/events/misc_events.cpp).

## AttachmentCodecRegistryLimits {#cfg-0338-owner}

[include/qd/events/serialized_attachment.hpp](../../include/qd/events/serialized_attachment.hpp)

### `AttachmentCodecRegistryLimits::registrations` — CFG-0338 {#cfg-0338}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Maximum registration count, including zero.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Maximum registration count, including zero.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;AttachmentCodecRegistry, AttachmentError&gt; create(std::span&lt;const AttachmentCodecRegistration&gt; registrations, AttachmentCodecRegistryLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: AttachmentError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `AttachmentError`.

### `AttachmentCodecRegistryLimits::keyBytes` — CFG-0339 {#cfg-0339}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes per nonempty local codec key
- Accepted values: key.size()&lt;=keyBytes and the key is nonempty. Zero prevents registrations but permits an empty list when storageBytes fits State. The key is not written to QTP.
- Read timing: AttachmentCodecRegistry::create before publishing an immutable snapshot.
- Mutability and lifetime: The parameters admit one AttachmentCodecRegistry::create call. The result retains independent registration copies; a new registry can use other limits. Decoder invocation receives separate AttachmentDecodeLimits.
- Failure: An overlong key makes create return AttachmentError::RESOURCE_LIMIT. Empty keys/invalid registration shape separately give INVALID_INPUT; duplicate key/version gives DUPLICATE_REGISTRATION. While copying registration state, std::bad_alloc maps to ALLOCATION_FAILURE and arbitrary callable/capture copy exceptions to CODEC_EXCEPTION. create does not invoke a decoder.
- Consumer sources: [include/qd/events/serialized_attachment.hpp](../../include/qd/events/serialized_attachment.hpp), [src/events/serialized_attachment.cpp](../../src/events/serialized_attachment.cpp).

### `AttachmentCodecRegistryLimits::storageBytes` — CFG-0340 {#cfg-0340}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative requested registry bytes and declared capture-state
- Accepted values: Registration retainedStateBytes are debited first; RegistryAllocator then accounts for State/control blocks, array capacity and keys. Zero cannot fit State even with no entries. External captures rely on truthful registration declarations.
- Read timing: AttachmentCodecRegistry::create before publishing an immutable snapshot.
- Mutability and lifetime: The parameters admit one AttachmentCodecRegistry::create call. The result retains independent registration copies; a new registry can use other limits. Decoder invocation receives separate AttachmentDecodeLimits.
- Failure: Excess declared state or accounted allocator backing returns AttachmentError::RESOURCE_LIMIT. Genuine std::bad_alloc maps to ALLOCATION_FAILURE; arbitrary callable/capture copy exceptions map to CODEC_EXCEPTION. No partial registry is published and registered decode functions do not run.
- Consumer sources: [include/qd/events/serialized_attachment.hpp](../../include/qd/events/serialized_attachment.hpp), [src/events/serialized_attachment.cpp](../../src/events/serialized_attachment.cpp).

## AttachmentDecodeLimits {#cfg-0333-owner}

[include/qd/events/serialized_attachment.hpp](../../include/qd/events/serialized_attachment.hpp)

### `AttachmentDecodeLimits::inputBytes` — CFG-0333 {#cfg-0333}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete body input bytes
- Accepted values: bytes.size()&lt;=inputBytes before invoking the codec. Null exits early without callback/body reads;0 permits a nonnull attachment with an empty body.
- Read timing: decodeValue checks input, creates AttachmentDecodeContext and invokes a trusted codec synchronously; afterward it checks exhausted().
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: Actual body length exceeding inputBytes returns AttachmentError::RESOURCE_LIMIT before codec invocation. std::bad_alloc from the codec or T extraction maps to ALLOCATION_FAILURE; other exceptions map to CODEC_EXCEPTION. An error explicitly returned by the codec is forwarded. Key/version/type checks have separate errors. Usage not declared by a trusted codec through the context is not measured automatically.
- Consumer sources: [include/qd/events/serialized_attachment.hpp](../../include/qd/events/serialized_attachment.hpp), [src/events/serialized_attachment.cpp](../../src/events/serialized_attachment.cpp).

### `AttachmentDecodeLimits::outputBytes` — CFG-0334 {#cfg-0334}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical owned codec-result bytes
- Accepted values: Initial outputBytes allowance. A trusted codec must call chargeOutput before allocation; every debit must fit. Zero permits 0 declared bytes only. External allocations and dishonest accounting are not intercepted. Subsequent extraction of T from std::any in public decode&lt;T&gt; does not invoke chargeOutput; user-defined T conversion can incur additional usage.
- Read timing: decodeValue checks input, creates AttachmentDecodeContext and invokes a trusted codec synchronously; afterward it checks exhausted().
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: After the codec, context.exhausted() returns AttachmentError::RESOURCE_LIMIT for an exhausted cooperative remainder. std::bad_alloc from the codec or T extraction maps to ALLOCATION_FAILURE; other exceptions map to CODEC_EXCEPTION. An error explicitly returned by the codec is forwarded. Key/version/type checks have separate errors. Usage not declared by a trusted codec through the context is not measured automatically.
- Consumer sources: [include/qd/events/serialized_attachment.hpp](../../include/qd/events/serialized_attachment.hpp), [src/events/serialized_attachment.cpp](../../src/events/serialized_attachment.cpp).

### `AttachmentDecodeLimits::scratchBytes` — CFG-0335 {#cfg-0335}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative codec scratch bytes
- Accepted values: Initial scratchBytes; chargeScratch consumes it without refunds on release. Zero permits zero debits only. Calling discipline remains the trusted codec obligation.
- Read timing: decodeValue checks input, creates AttachmentDecodeContext and invokes a trusted codec synchronously; afterward it checks exhausted().
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: After the codec, context.exhausted() returns AttachmentError::RESOURCE_LIMIT for an exhausted cooperative remainder. std::bad_alloc from the codec or T extraction maps to ALLOCATION_FAILURE; other exceptions map to CODEC_EXCEPTION. An error explicitly returned by the codec is forwarded. Key/version/type checks have separate errors. Usage not declared by a trusted codec through the context is not measured automatically.
- Consumer sources: [include/qd/events/serialized_attachment.hpp](../../include/qd/events/serialized_attachment.hpp), [src/events/serialized_attachment.cpp](../../src/events/serialized_attachment.cpp).

### `AttachmentDecodeLimits::work` — CFG-0336 {#cfg-0336}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: codec-defined work units
- Accepted values: Initial work; chargeWork must run before the work and monotonically decreases the remainder. Zero permits no positive debit. The parameter does not establish callback completion time.
- Read timing: decodeValue checks input, creates AttachmentDecodeContext and invokes a trusted codec synchronously; afterward it checks exhausted().
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: After the codec, context.exhausted() returns AttachmentError::RESOURCE_LIMIT for an exhausted cooperative remainder. std::bad_alloc from the codec or T extraction maps to ALLOCATION_FAILURE; other exceptions map to CODEC_EXCEPTION. An error explicitly returned by the codec is forwarded. Key/version/type checks have separate errors. Usage not declared by a trusted codec through the context is not measured automatically.
- Consumer sources: [include/qd/events/serialized_attachment.hpp](../../include/qd/events/serialized_attachment.hpp), [src/events/serialized_attachment.cpp](../../src/events/serialized_attachment.cpp).

### `AttachmentDecodeLimits::diagnosticBytes` — CFG-0337 {#cfg-0337}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: owned diagnostic-text bytes
- Accepted values: Current results contain fixed AttachmentError enums only; no diagnostic text is created and consumption is 0. diagnosticBytes is copied with the limits but no charge method reads it. All values currently behave identically.
- Read timing: decodeValue checks input, creates AttachmentDecodeContext and invokes a trusted codec synchronously; afterward it checks exhausted().
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: This field has no own failure: fixed enum diagnostics consume 0 bytes.
- Consumer sources: [include/qd/events/serialized_attachment.hpp](../../include/qd/events/serialized_attachment.hpp), [src/events/serialized_attachment.cpp](../../src/events/serialized_attachment.cpp).
