# Protocol and tapes

[Русский](protocol.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [DescriptionLimits](protocol.md#cfg-0519-owner) — `include/qd/protocol/describe_records.hpp`
- [FrameLimits](protocol.md#cfg-0525-owner) — `include/qd/protocol/frame_reader.hpp`
- [IncomingSubscriptionLimits](protocol.md#cfg-0527-owner) — `include/qd/protocol/incoming_subscription.hpp`
- [IncomingSubscriptionWork](protocol.md#cfg-0533-owner) — `include/qd/protocol/incoming_subscription.hpp`
- [LiveLimits](protocol.md#cfg-0541-owner) — `include/qd/protocol/live_session.hpp`
- [LiveWork](protocol.md#cfg-0537-owner) — `include/qd/protocol/live_session.hpp`
- [BindingLimits](protocol.md#cfg-0557-owner) — `include/qd/protocol/static_binding.hpp`
- [ContextLimits](protocol.md#cfg-0560-owner) — `include/qd/protocol/stream_data.hpp`
- [StreamDataLimits](protocol.md#cfg-0563-owner) — `include/qd/protocol/stream_data.hpp`
- [SubscriptionWriterLimits](protocol.md#cfg-0584-owner) — `include/qd/protocol/subscription_writer.hpp`
- [SubscriptionWriterWork](protocol.md#cfg-0580-owner) — `include/qd/protocol/subscription_writer.hpp`
- [AdmissionLimits](protocol.md#cfg-0593-owner) — `include/qd/protocol/tape_admission.hpp`
- [TapeDescriptorLimits](protocol.md#cfg-0599-owner) — `include/qd/protocol/tape_reader.hpp`
- [TapeLimits](protocol.md#cfg-0603-owner) — `include/qd/protocol/tape_reader.hpp`
- [LayoutLimits](protocol.md#cfg-0612-owner) — `include/qd/protocol/tape_writer.hpp`
- [WriterLimits](protocol.md#cfg-0617-owner) — `include/qd/protocol/tape_writer.hpp`

## DescriptionLimits {#cfg-0519-owner}

[include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp)

### `DescriptionLimits::payloadBytes` — CFG-0519 {#cfg-0519}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of complete payload excluding frame envelope
- Accepted values: payload.size() ≤ payloadBytes. 0 permits only empty payload.
- Read timing: parseDescribeRecords, detail::parseSessionDescriptions and detail::parsePmrDescriptions pass a DescriptionLimits copy to DescriptionReader; the field is checked at its corresponding payload parsing action.
- Mutability and lifetime: A separate call receives a fresh budget; this setting does not subsequently mutate the parse result.
- Failure: Violating the stated bound is DescriptionStatus::RESOURCE_LIMIT. String/header syntax failures retain their own statuses and are not replaced with a generic configuration error.
- Consumer sources: [include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp), [src/protocol/describe_records.cpp](../../src/protocol/describe_records.cpp).

### `DescriptionLimits::descriptions` — CFG-0520 {#cfg-0520}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: completed descriptions including repeated sender IDs
- Accepted values: Every new description needs space below descriptions; repeated sender IDs also consume a slot. 0 admits no description.
- Read timing: parseDescribeRecords, detail::parseSessionDescriptions and detail::parsePmrDescriptions pass a DescriptionLimits copy to DescriptionReader; the field is checked at its corresponding payload parsing action.
- Mutability and lifetime: A separate call receives a fresh budget; this setting does not subsequently mutate the parse result.
- Failure: Violating the stated bound is DescriptionStatus::RESOURCE_LIMIT. String/header syntax failures retain their own statuses and are not replaced with a generic configuration error.
- Consumer sources: [include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp), [src/protocol/describe_records.cpp](../../src/protocol/describe_records.cpp).

### `DescriptionLimits::fieldsPerDescription` — CFG-0521 {#cfg-0521}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: declared fields per description
- Accepted values: Declared fieldCount ≤ fieldsPerDescription; 0 permits a description with no fields.
- Read timing: parseDescribeRecords, detail::parseSessionDescriptions and detail::parsePmrDescriptions pass a DescriptionLimits copy to DescriptionReader; the field is checked at its corresponding payload parsing action.
- Mutability and lifetime: A separate call receives a fresh budget; this setting does not subsequently mutate the parse result.
- Failure: Violating the stated bound is DescriptionStatus::RESOURCE_LIMIT. String/header syntax failures retain their own statuses and are not replaced with a generic configuration error.
- Consumer sources: [include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp), [src/protocol/describe_records.cpp](../../src/protocol/describe_records.cpp).

### `DescriptionLimits::totalFields` — CFG-0522 {#cfg-0522}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: sum of declared fields of admitted headers
- Accepted values: fieldCount ≤ totalFields-fields_ at every header. 0 gives no aggregate field budget.
- Read timing: parseDescribeRecords, detail::parseSessionDescriptions and detail::parsePmrDescriptions pass a DescriptionLimits copy to DescriptionReader; the field is checked at its corresponding payload parsing action.
- Mutability and lifetime: A separate call receives a fresh budget; this setting does not subsequently mutate the parse result.
- Failure: Violating the stated bound is DescriptionStatus::RESOURCE_LIMIT. String/header syntax failures retain their own statuses and are not replaced with a generic configuration error.
- Consumer sources: [include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp), [src/protocol/describe_records.cpp](../../src/protocol/describe_records.cpp).

### `DescriptionLimits::name` — CFG-0523 {#cfg-0523}

- Type: `encoding::UtfStringLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Per-name encoded body bytes and decoded UTF-16 units.
- Units: see declaration: UTF-16/code units
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] DescriptionResult parseDescribeRecords(std::span&lt;const std::byte&gt; payload, DescriptionLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: DescriptionStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `DescriptionStatus`.

### `DescriptionLimits::totalNameUnits` — CFG-0524 {#cfg-0524}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of all record/field names including repeats
- Accepted values: One name read may use min(name.codeUnits,totalNameUnits-nameUnits_). After successful decoding, nameUnits_ increases by exactly the length of an engaged decoded.value; null adds 0. An empty string also costs 0 units. Required nonempty names are checked separately when validating the record or field header.
- Read timing: parseDescribeRecords, detail::parseSessionDescriptions and detail::parsePmrDescriptions pass a DescriptionLimits copy to DescriptionReader; the field is checked at its corresponding payload parsing action. Each name read computes the available remainder; successful decoding charges the actual value length, with null and empty strings charging 0 units.
- Mutability and lifetime: A separate call receives a fresh budget; this setting does not subsequently mutate the parse result.
- Failure: Violating the stated bound is DescriptionStatus::RESOURCE_LIMIT. String/header syntax failures retain their own statuses and are not replaced with a generic configuration error.
- Consumer sources: [include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp), [src/protocol/describe_records.cpp](../../src/protocol/describe_records.cpp).

## FrameLimits {#cfg-0525-owner}

[include/qd/protocol/frame_reader.hpp](../../include/qd/protocol/frame_reader.hpp)

### `FrameLimits::maximumBodyBytes` — CFG-0525 {#cfg-0525}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Largest accepted type-plus-payload length; zero is allowed.
- Units: bytes; scope defined by field contract
- Declared constraints: zero allowed; representable body+9; header constructor contract:91
- Read timing: FrameReader constructor validates budgets and allocates body+9 once; reset retains allocation/configuration
- Mutability and lifetime: immutable reader configuration; reset clears state/counters only
- Failure: constructor invalid_argument for incompatible/overflowing limits, allocation propagates; feed later exposes FrameError
- Additional owner vocabulary (not a field-to-error mapping): `FrameError, FrameStatus`.

### `FrameLimits::maximumPendingBytes` — CFG-0526 {#cfg-0526}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Must accommodate maximumBodyBytes plus nine prefix bytes.
- Units: bytes; scope defined by field contract
- Declared constraints: must accommodate maximumBodyBytes+9; checked representability; header:18,91
- Read timing: FrameReader constructor validates budgets and allocates body+9 once; reset retains allocation/configuration
- Mutability and lifetime: immutable reader configuration; reset clears state/counters only
- Failure: constructor invalid_argument for incompatible/overflowing limits, allocation propagates; feed later exposes FrameError
- Additional owner vocabulary (not a field-to-error mapping): `FrameError, FrameStatus`.

## IncomingSubscriptionLimits {#cfg-0527-owner}

[include/qd/protocol/incoming_subscription.hpp](../../include/qd/protocol/incoming_subscription.hpp)

### `IncomingSubscriptionLimits::payloadBytes` — CFG-0527 {#cfg-0527}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of complete subscription payload
- Accepted values: frame.payload.size() ≤ payloadBytes; 0 permits an empty message.
- Read timing: Passed by value into decodeIncomingSubscription and applied to the whole message and sequential tuple traversal.
- Mutability and lifetime: A new call may receive another profile; there is no live configuration owner here.
- Failure: Violating its own resource bound is IncomingSubscriptionError::RESOURCE_LIMIT. If work.textUnits is tighter, the same symbol-reader stopping condition maps to WORK_LIMIT through workLimited.
- Consumer sources: [include/qd/protocol/incoming_subscription.hpp](../../include/qd/protocol/incoming_subscription.hpp), [src/protocol/incoming_subscription.cpp](../../src/protocol/incoming_subscription.cpp).

### `IncomingSubscriptionLimits::schemaRecords` — CFG-0528 {#cfg-0528}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: local schema records
- Accepted values: schema.records().size() ≤ schemaRecords even if individual remote records are uninteresting; 0 requires an empty schema.
- Read timing: Passed by value into decodeIncomingSubscription and applied to the whole message and sequential tuple traversal.
- Mutability and lifetime: A new call may receive another profile; there is no live configuration owner here.
- Failure: Violating its own resource bound is IncomingSubscriptionError::RESOURCE_LIMIT. If work.textUnits is tighter, the same symbol-reader stopping condition maps to WORK_LIMIT through workLimited.
- Consumer sources: [include/qd/protocol/incoming_subscription.hpp](../../include/qd/protocol/incoming_subscription.hpp), [src/protocol/incoming_subscription.cpp](../../src/protocol/incoming_subscription.cpp).

### `IncomingSubscriptionLimits::senderRecords` — CFG-0529 {#cfg-0529}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: rows in the incoming sender-ID mapping
- Accepted values: sortedRecords.size() ≤ senderRecords; 0 requires an empty mapping.
- Read timing: Passed by value into decodeIncomingSubscription and applied to the whole message and sequential tuple traversal.
- Mutability and lifetime: A new call may receive another profile; there is no live configuration owner here.
- Failure: Violating its own resource bound is IncomingSubscriptionError::RESOURCE_LIMIT. If work.textUnits is tighter, the same symbol-reader stopping condition maps to WORK_LIMIT through workLimited.
- Consumer sources: [include/qd/protocol/incoming_subscription.hpp](../../include/qd/protocol/incoming_subscription.hpp), [src/protocol/incoming_subscription.cpp](../../src/protocol/incoming_subscription.cpp).

### `IncomingSubscriptionLimits::entries` — CFG-0530 {#cfg-0530}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: wire subscription tuples including duplicates and ignored entries
- Accepted values: entriesLeft is debited for every wire tuple before filtering. 0 permits only a payload with no tuples.
- Read timing: Passed by value into decodeIncomingSubscription and applied to the whole message and sequential tuple traversal.
- Mutability and lifetime: A new call may receive another profile; there is no live configuration owner here.
- Failure: Violating its own resource bound is IncomingSubscriptionError::RESOURCE_LIMIT. If work.textUnits is tighter, the same symbol-reader stopping condition maps to WORK_LIMIT through workLimited.
- Consumer sources: [include/qd/protocol/incoming_subscription.hpp](../../include/qd/protocol/incoming_subscription.hpp), [src/protocol/incoming_subscription.cpp](../../src/protocol/incoming_subscription.cpp).

### `IncomingSubscriptionLimits::symbolUnits` — CFG-0531 {#cfg-0531}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of one decoded symbol
- Accepted values: The readBounded bound is min(symbolUnits,remaining totalSymbolUnits,possibleUnits,work.textUnits/SYMBOL_UNIT_WORK). Ignored entries undergo the same check.
- Read timing: Passed by value into decodeIncomingSubscription and applied to the whole message and sequential tuple traversal.
- Mutability and lifetime: A new call may receive another profile; there is no live configuration owner here.
- Failure: Violating its own resource bound is IncomingSubscriptionError::RESOURCE_LIMIT. If work.textUnits is tighter, the same symbol-reader stopping condition maps to WORK_LIMIT through workLimited.
- Consumer sources: [include/qd/protocol/incoming_subscription.hpp](../../include/qd/protocol/incoming_subscription.hpp), [src/protocol/incoming_subscription.cpp](../../src/protocol/incoming_subscription.cpp).

### `IncomingSubscriptionLimits::totalSymbolUnits` — CFG-0532 {#cfg-0532}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of all wire tuples
- Accepted values: Each symbol length is debited from unitsLeft before deciding to ignore a record; repeats are charged again. 0 is an empty text budget.
- Read timing: Passed by value into decodeIncomingSubscription and applied to the whole message and sequential tuple traversal.
- Mutability and lifetime: A new call may receive another profile; there is no live configuration owner here.
- Failure: Violating its own resource bound is IncomingSubscriptionError::RESOURCE_LIMIT. If work.textUnits is tighter, the same symbol-reader stopping condition maps to WORK_LIMIT through workLimited.
- Consumer sources: [include/qd/protocol/incoming_subscription.hpp](../../include/qd/protocol/incoming_subscription.hpp), [src/protocol/incoming_subscription.cpp](../../src/protocol/incoming_subscription.cpp).

## IncomingSubscriptionWork {#cfg-0533-owner}

[include/qd/protocol/incoming_subscription.hpp](../../include/qd/protocol/incoming_subscription.hpp)

### `IncomingSubscriptionWork::frames` — CFG-0533 {#cfg-0533}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining decode attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `IncomingSubscriptionError`.

### `IncomingSubscriptionWork::inputBytes` — CFG-0534 {#cfg-0534}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining complete payload bytes admitted for primitive examination.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `IncomingSubscriptionError`.

### `IncomingSubscriptionWork::visits` — CFG-0535 {#cfg-0535}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining mapping validation, tuple and searched mapping row visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `IncomingSubscriptionError`.

### `IncomingSubscriptionWork::textUnits` — CFG-0536 {#cfg-0536}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining logical UTF-16 units admitted for symbol reads and copies.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `IncomingSubscriptionError`.

## LiveLimits {#cfg-0541-owner}

[include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp)

### `LiveLimits::frame` — CFG-0541 {#cfg-0541}

- Type: `FrameLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Per-frame envelope bounds and fixed framing allocation.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; create(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, LiveWork &amp;work) / [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; createServer(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, IncomingSubscriptionLimits subscriptions, LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: LiveError, LiveStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

### `LiveLimits::protocol` — CFG-0542 {#cfg-0542}

- Type: `TapeDescriptorLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete descriptor collection and text bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; create(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, LiveWork &amp;work) / [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; createServer(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, IncomingSubscriptionLimits subscriptions, LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: LiveError, LiveStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

### `LiveLimits::descriptions` — CFG-0543 {#cfg-0543}

- Type: `DescriptionLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One incoming description batch bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; create(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, LiveWork &amp;work) / [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; createServer(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, IncomingSubscriptionLimits subscriptions, LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: LiveError, LiveStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

### `LiveLimits::context` — CFG-0544 {#cfg-0544}

- Type: `ContextLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete retained peer-binding generation bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; create(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, LiveWork &amp;work) / [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; createServer(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, IncomingSubscriptionLimits subscriptions, LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: LiveError, LiveStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

### `LiveLimits::data` — CFG-0545 {#cfg-0545}

- Type: `StreamDataLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One complete data-frame logical work and value bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; create(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, LiveWork &amp;work) / [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; createServer(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, IncomingSubscriptionLimits subscriptions, LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: LiveError, LiveStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

### `LiveLimits::residentBytes` — CFG-0546 {#cfg-0546}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of all reservations in one epoch
- Accepted values: Must hold base+sizeof(Storage), then old and new objects/arenas. 0 cannot hold mandatory structures.
- Read timing: Copied during LiveSession::create; the initial snapshot is created immediately. Other bounds are read when creating the next snapshot/scratch/event/output.
- Mutability and lifetime: Fixed for a session/epoch; retained old owners remain in the ledger until released.
- Failure: An owner/block not fitting or a bounded arena exhausted is LiveError::RESOURCE_LIMIT; failure to allocate admitted memory is ALLOCATION_FAILURE. sum overflow also produces RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `LiveLimits::owners` — CFG-0547 {#cfg-0547}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneous snapshot/event/output owners
- Accepted values: 0 cannot admit the initial snapshot, so create refuses it. A new candidate needs a free owner before the old owner is released.
- Read timing: Copied during LiveSession::create; the initial snapshot is created immediately. Other bounds are read when creating the next snapshot/scratch/event/output.
- Mutability and lifetime: Fixed for a session/epoch; retained old owners remain in the ledger until released.
- Failure: An owner/block not fitting or a bounded arena exhausted is LiveError::RESOURCE_LIMIT; failure to allocate admitted memory is ALLOCATION_FAILURE. sum overflow also produces RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `LiveLimits::snapshotBytes` — CFG-0548 {#cfg-0548}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes per complete snapshot arena
- Accepted values: 0 is a finite empty arena; the field has no separate numeric minimum. LiveSession::create immediately reserves the first owner with snapshotBytes and calls complete; the actual complete initial snapshot must fit before the session returns. Each later candidate reserves its arena while the old snapshot is still alive and under aggregate residentBytes/owners bounds.
- Read timing: Read immediately in LiveSession::create for the initial makeOwner and complete. After creation, read again for each next snapshot; old owners remain charged until released.
- Mutability and lifetime: Fixed for a session/epoch; retained old owners remain in the ledger until released.
- Failure: If the initial snapshot/arena or its complete does not fit, create returns LiveError::RESOURCE_LIMIT through LiveStorageLimit; actual bad_alloc/length_error maps to ALLOCATION_FAILURE. No session is published on refusal. The same resource category can later occur while building a candidate under the old/new peak; that is a separate operational phase.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `LiveLimits::scratchBytes` — CFG-0549 {#cfg-0549}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of temporary control parsing arena
- Accepted values: 0 is an empty arena. Scratch coexists with old/candidate owners and consumes residentBytes.
- Read timing: Copied during LiveSession::create; the initial snapshot is created immediately. Other bounds are read when creating the next snapshot/scratch/event/output.
- Mutability and lifetime: Fixed for a session/epoch; retained old owners remain in the ledger until released.
- Failure: An owner/block not fitting or a bounded arena exhausted is LiveError::RESOURCE_LIMIT; failure to allocate admitted memory is ALLOCATION_FAILURE. sum overflow also produces RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `LiveLimits::eventBytes` — CFG-0550 {#cfg-0550}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes per fixed event arena
- Accepted values: 0 is an empty fixed arena. Applies to control and DATA when dataEventBytes=0; with nonzero dataEventBytes DATA uses an elastic bound.
- Read timing: Copied during LiveSession::create; the initial snapshot is created immediately. Other bounds are read when creating the next snapshot/scratch/event/output.
- Mutability and lifetime: Fixed for a session/epoch; retained old owners remain in the ledger until released.
- Failure: An owner/block not fitting or a bounded arena exhausted is LiveError::RESOURCE_LIMIT; failure to allocate admitted memory is ALLOCATION_FAILURE. sum overflow also produces RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `LiveLimits::outputBytes` — CFG-0551 {#cfg-0551}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of complete control envelope
- Accepted values: controlOutput reserves outputBytes+9 for prefix storage; finish separately bounds the complete result by outputBytes and the body by frame.maximumBodyBytes. 0 cannot emit a nonempty control frame.
- Read timing: Copied during LiveSession::create; the initial snapshot is created immediately. Other bounds are read when creating the next snapshot/scratch/event/output.
- Mutability and lifetime: Fixed for a session/epoch; retained old owners remain in the ledger until released.
- Failure: An owner/block not fitting or a bounded arena exhausted is LiveError::RESOURCE_LIMIT; failure to allocate admitted memory is ALLOCATION_FAILURE. sum overflow also produces RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `LiveLimits::credentials` — CFG-0552 {#cfg-0552}

- Type: `LiveCredentialPolicy`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: credential eligibility policy
- Accepted values: Only CREDENTIAL_FREE_PLAIN or VERIFIED_TLS_BASIC; an adapter-supplied policy does not itself prove TLS verification.
- Read timing: Checked and copied during create; Basic is admitted only with VERIFIED_TLS_BASIC; otherwise CREDENTIALS_FORBIDDEN.
- Mutability and lifetime: Immutable within the live session.
- Failure: Invalid value or incompatible profile is LiveError::INVALID_CONFIGURATION. Basic is admitted only with VERIFIED_TLS_BASIC; otherwise CREDENTIALS_FORBIDDEN.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `LiveLimits::fieldTime` — CFG-0553 {#cfg-0553}

- Type: `FieldTimePolicy`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: virtual field-time prefix mode
- Accepted values: Only DISABLED or READ. READ requires schema.profile()==BINARY_FIELDS_V1.
- Read timing: Checked and copied during create; The policy is passed into complete-description binding and bounded prefix decoding.
- Mutability and lifetime: Immutable within the live session.
- Failure: Invalid value or incompatible profile is LiveError::INVALID_CONFIGURATION. The policy is passed into complete-description binding and bounded prefix decoding.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `LiveLimits::events` — CFG-0554 {#cfg-0554}

- Type: `DataEventPolicy`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: event flags and binary metadata policy
- Accepted values: Only ZERO_FLAGS or BINARY_CATALOG_V1. BINARY_CATALOG_V1 requires BINARY_FIELDS_V1.
- Read timing: Checked and copied during create; Passed to decodePmrStreamData to select admissible event grammar.
- Mutability and lifetime: Immutable within the live session.
- Failure: Invalid value or incompatible profile is LiveError::INVALID_CONFIGURATION. Passed to decodePmrStreamData to select admissible event grammar.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `LiveLimits::historySnapshot` — CFG-0555 {#cfg-0555}

- Type: `bool`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Whether local negotiation advertises the selected hs capability.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; create(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, LiveWork &amp;work) / [[nodiscard]] static std::expected&lt;LiveSession, LiveError&gt; createServer(core::LocalSchema &amp;&amp;schema, SubscriptionWriter &amp;&amp;composer, LiveLimits limits, IncomingSubscriptionLimits subscriptions, LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: LiveError, LiveStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

### `LiveLimits::dataEventBytes` — CFG-0556 {#cfg-0556}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of elastic DATA arena
- Accepted values: 0 selects fixed eventBytes. A nonzero value starts with min(dataEventBytes,4096), with the remainder permitting growth; aggregate residentBytes still applies.
- Read timing: Copied during LiveSession::create; the initial snapshot is created immediately. Other bounds are read when creating the next snapshot/scratch/event/output.
- Mutability and lifetime: Fixed for a session/epoch; retained old owners remain in the ledger until released.
- Failure: An owner/block not fitting or a bounded arena exhausted is LiveError::RESOURCE_LIMIT; failure to allocate admitted memory is ALLOCATION_FAILURE. sum overflow also produces RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

## LiveWork {#cfg-0537-owner}

[include/qd/protocol/live_session.hpp](../../include/qd/protocol/live_session.hpp)

### `LiveWork::operations` — CFG-0537 {#cfg-0537}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining public operation attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

### `LiveWork::visits` — CFG-0538 {#cfg-0538}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining record, field, property and comparison visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

### `LiveWork::textUnits` — CFG-0539 {#cfg-0539}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining examined/copied UTF-16 units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

### `LiveWork::bytes` — CFG-0540 {#cfg-0540}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining input, parsing, output and arena initialization byte work.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `LiveError, LiveStatus`.

## BindingLimits {#cfg-0557-owner}

[include/qd/protocol/static_binding.hpp](../../include/qd/protocol/static_binding.hpp)

### `BindingLimits::fields` — CFG-0557 {#cfg-0557}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: field actions in one wire description
- Accepted values: description.fields.size() ≤ fields; 0 admits only a description without field actions.
- Read timing: Read before building a binding; passed by value and locally debited during preparation. Context may additionally debit an aggregate remainder across descriptions.
- Mutability and lifetime: A prepared plan is immutable; a different budget requires a new plan.
- Failure: Exceeding the corresponding fields/nameUnits/totalNameUnits bound is BindingError::RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/static_binding.hpp](../../include/qd/protocol/static_binding.hpp), [src/protocol/static_binding.cpp](../../src/protocol/static_binding.cpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `BindingLimits::nameUnits` — CFG-0558 {#cfg-0558}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units per record/field name
- Accepted values: Each description.name and field.name ≤ nameUnits; 0 admits no nonempty name.
- Read timing: Read before building a binding; passed by value and locally debited during preparation. Context may additionally debit an aggregate remainder across descriptions.
- Mutability and lifetime: A prepared plan is immutable; a different budget requires a new plan.
- Failure: Exceeding the corresponding fields/nameUnits/totalNameUnits bound is BindingError::RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/static_binding.hpp](../../include/qd/protocol/static_binding.hpp), [src/protocol/static_binding.cpp](../../src/protocol/static_binding.cpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `BindingLimits::totalNameUnits` — CFG-0559 {#cfg-0559}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of all names in one binding including repeats
- Accepted values: The record name and every field name are successively debited from totalNameUnits; 0 gives no aggregate budget.
- Read timing: Read before building a binding; passed by value and locally debited during preparation. Context may additionally debit an aggregate remainder across descriptions.
- Mutability and lifetime: A prepared plan is immutable; a different budget requires a new plan.
- Failure: Exceeding the corresponding fields/nameUnits/totalNameUnits bound is BindingError::RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/static_binding.hpp](../../include/qd/protocol/static_binding.hpp), [src/protocol/static_binding.cpp](../../src/protocol/static_binding.cpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

## ContextLimits {#cfg-0560-owner}

[include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp)

### `ContextLimits::schema` — CFG-0560 {#cfg-0560}

- Type: `core::SchemaLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Bounds inspected local schema, including names and lookup storage.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;StreamDataContext, BindingError&gt; create(const core::LocalSchema &amp;schema, std::span&lt;const WireRecordDescription&gt; descriptions, ContextLimits limits) / [[nodiscard]] static std::expected&lt;StreamDataContext, BindingError&gt; createWithFieldTimePolicy(const core::LocalSchema &amp;schema, std::span&lt;const WireRecordDescription&gt; descriptions, ContextLimits limits, FieldTimePolicy policy)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: StreamDataStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `StreamDataStatus`.

### `ContextLimits::descriptions` — CFG-0561 {#cfg-0561}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: unique sender-ID descriptions
- Accepted values: descriptions.size() ≤ ContextLimits.descriptions; 0 permits an empty mapping. Duplicate sender IDs have a separate structural error.
- Read timing: Read when creating StreamDataContext and rebuilding an effective LiveSnapshot.
- Mutability and lifetime: The context is immutable; a new description set creates a new context/generation.
- Failure: Exceeding it is BindingError::RESOURCE_LIMIT in StreamDataContext; LiveSession maps snapshot construction failure into its own error.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp), [src/protocol/live_session.cpp](../../src/protocol/live_session.cpp).

### `ContextLimits::bindings` — CFG-0562 {#cfg-0562}

- Type: `BindingLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Aggregate actions/names and per-name bound across all descriptions.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;StreamDataContext, BindingError&gt; create(const core::LocalSchema &amp;schema, std::span&lt;const WireRecordDescription&gt; descriptions, ContextLimits limits) / [[nodiscard]] static std::expected&lt;StreamDataContext, BindingError&gt; createWithFieldTimePolicy(const core::LocalSchema &amp;schema, std::span&lt;const WireRecordDescription&gt; descriptions, ContextLimits limits, FieldTimePolicy policy)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: StreamDataStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `StreamDataStatus`.

## StreamDataLimits {#cfg-0563-owner}

[include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp)

### `StreamDataLimits::payloadBytes` — CFG-0563 {#cfg-0563}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of complete DATA payload
- Accepted values: payload.size() ≤ payloadBytes; 0 permits an empty payload.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::processedRecords` — CFG-0564 {#cfg-0564}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: processed wire records including skipped and zero-field records
- Accepted values: Every record requires a positive processedRecords remainder; it is debited regardless of later local output. 0 admits no record.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::wireFields` — CFG-0565 {#cfg-0565}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: read wire fields including skips and repeated targets
- Accepted values: wireFields is debited before each wire field; 0 permits only the absence of such fields.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::emittedRecords` — CFG-0566 {#cfg-0566}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: completed output records
- Accepted values: A locally mapped record needs emittedRecords&gt;0, then debits it on completion. A skipped record does not consume it.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::emittedValues` — CFG-0567 {#cfg-0567}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical local-field values including absent fields
- Accepted values: plan.localFieldCount ≤ emittedValues before output; localFieldCount is debited at completion. 0 permits only a zero-field local layout.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::ownedSymbolUnits` — CFG-0568 {#cfg-0568}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of owned output symbols including repeats
- Accepted values: Output symbol length ≤ ownedSymbolUnits; every completed copy debits its length. 0 gives no symbol budget.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::symbol` — CFG-0569 {#cfg-0569}

- Type: `encoding::SymbolLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining-span, per-symbol and retained-state limits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] StreamDataResult decodeStreamData(std::span&lt;const std::byte&gt; payload, const StreamDataContext &amp;context, StreamDataLimits limits) / [[nodiscard]] StreamDataResult decodeStreamDataWithEventPolicy(std::span&lt;const std::byte&gt; payload, const StreamDataContext &amp;context, StreamDataLimits limits, DataEventPolicy policy) / [[nodiscard]] StreamDataResult decodeRawData(std::span&lt;const std::byte&gt; payload, const StreamDataContext &amp;context, StreamDataLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: StreamDataStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `StreamDataStatus`.

### `StreamDataLimits::fieldString` — CFG-0570 {#cfg-0570}

- Type: `encoding::CharArrayLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Type10 encoded bound; per-value reserved units for both text tags.
- Units: see declaration: UTF-16/code units
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] StreamDataResult decodeStreamData(std::span&lt;const std::byte&gt; payload, const StreamDataContext &amp;context, StreamDataLimits limits) / [[nodiscard]] StreamDataResult decodeStreamDataWithEventPolicy(std::span&lt;const std::byte&gt; payload, const StreamDataContext &amp;context, StreamDataLimits limits, DataEventPolicy policy) / [[nodiscard]] StreamDataResult decodeRawData(std::span&lt;const std::byte&gt; payload, const StreamDataContext &amp;context, StreamDataLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: StreamDataStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `StreamDataStatus`.

### `StreamDataLimits::fieldBytes` — CFG-0571 {#cfg-0571}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate examined type10 bytes including prefixes
- Accepted values: readCharArray receives min(fieldString.encodedBytes,fieldBytes); consumed bytes are charged even on failure. 0 cannot read a type10 prefix.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::fieldUnits` — CFG-0572 {#cfg-0572}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate admitted UTF-16 field units
- Accepted values: Bounds CharArray and STRING reserved units together with per-field codeUnits. chargedUnits are debited for skips, replacements and failures without refunds. 0 is an empty budget.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::ownedFieldUnits` — CFG-0573 {#cfg-0573}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical UTF-16 units of completed, current and temporary strings
- Accepted values: A temporary string must fit ownedFieldUnits-currentUnits; CharArray uses the same remainder. 0 permits no nonzero text units.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::fieldPayloadEncodedBytes` — CFG-0574 {#cfg-0574}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: examined prefix+body bytes of one new payload
- Accepted values: Available input is bounded by min(fieldPayloadEncodedBytes,payloadEncodedBytes). 0 cannot read even a null prefix; excessive declared length has a separate grammar check.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::fieldPayloadBytes` — CFG-0575 {#cfg-0575}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: declared body bytes of one new payload
- Accepted values: After grammar and complete-body checks, count ≤ fieldPayloadBytes and payloadWorkBytes; null returns before body charging. 0 admits null/empty body.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::payloadEncodedBytes` — CFG-0576 {#cfg-0576}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate examined prefix+body bytes of new payloads
- Accepted values: The minimum with fieldPayloadEncodedBytes bounds each read; consumed is debited from the aggregate remainder including failures and skips. 0 admits no prefix read.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::payloadWorkBytes` — CFG-0577 {#cfg-0577}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate admitted new-payload body work bytes
- Accepted values: count ≤ payloadWorkBytes; chargedBytes are debited before result evaluation without skip/replacement/error refunds. Null/empty bodies consume 0 bytes.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::ownedPayloadBytes` — CFG-0578 {#cfg-0578}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical bytes of completed, current and temporary payloads
- Accepted values: Temporary bytes must fit ownedPayloadBytes-currentBytes. STRING uses reservedUnits×sizeof(char16_t), bytes/object use count. 0 admits empty content.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `StreamDataLimits::ownedFieldTimeRecords` — CFG-0579 {#cfg-0579}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: fixed FIELD metadata owners
- Accepted values: An emitted record with eventTimeFieldCount!=0 needs nonzero ownedFieldTimeRecords and debits 1 on completion. 0 disallows such metadata.
- Read timing: StreamDataLimits is passed by value for one DATA decode; remainders are spent within the message. LiveSession additionally precharges work and bounds arena storage.
- Mutability and lifetime: There is no live setter; each standalone decode receives its profile and a session uses its captured copy.
- Failure: Exceeding its own bound is StreamDataStatus::RESOURCE_LIMIT; payload/CharArray encoding::Status::RESOURCE_LIMIT maps to that status. A standalone result retains only completed prior records; LiveSession publishes DATA atomically.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

## SubscriptionWriterLimits {#cfg-0584-owner}

[include/qd/protocol/subscription_writer.hpp](../../include/qd/protocol/subscription_writer.hpp)

### `SubscriptionWriterLimits::frameBytes` — CFG-0584 {#cfg-0584}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Body bytes including message identity; at most Java INT_MAX.
- Units: bytes; scope defined by field contract
- Declared constraints: Body bytes including message identity; at most Java INT_MAX.
- Read timing: Explicit argument of public operations: SubscriptionWriter(core::LocalSchema schema, std::vector&lt;OutgoingRecord&gt; records, SubscriptionWriterLimits limits, std::shared_ptr&lt;PendingSubscriptionOutput::Budget&gt; budget, core::SubscriptionSelectorPolicy policy) / [[nodiscard]] static std::expected&lt;SubscriptionWriter, SubscriptionWriterError&gt; create(const core::LocalSchema &amp;schema, std::span&lt;const OutgoingRecord&gt; records, LayoutLimits layoutLimits, SubscriptionWriterLimits limits, SubscriptionWriterWork &amp;work, core::SubscriptionSelectorPolicy policy = core::SubscriptionSelectorPolicy::EXACT_ONLY)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: SubscriptionWriterError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionWriterError`.

### `SubscriptionWriterLimits::operationBytes` — CFG-0585 {#cfg-0585}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete envelope bytes per successful operation
- Accepted values: Total body+length prefix ≤ operationBytes; 0 cannot hold a nonempty envelope. Stream-prefix additionally uses min(maxFrameBytes,operationBytes).
- Read timing: SubscriptionWriter::create captures limits; numeric bounds are consumed during compose/stream-prefix and new-output reservation.
- Mutability and lifetime: Limits remain fixed for the composer, including issued output owners.
- Failure: Exceeding the stated field-specific comparison/sum is SubscriptionWriterError::RESOURCE_LIMIT; the new output is not published.
- Consumer sources: [include/qd/protocol/subscription_writer.hpp](../../include/qd/protocol/subscription_writer.hpp), [src/protocol/subscription_writer.cpp](../../src/protocol/subscription_writer.cpp).

### `SubscriptionWriterLimits::operationEntries` — CFG-0586 {#cfg-0586}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input requests/description IDs/data records including duplicates
- Accepted values: requests.size()/ids.size() ≤ operationEntries; full data.size() is checked without prefixLimits. Stream-prefix selects at most min(data.size(),operationEntries). 0 selects no records.
- Read timing: SubscriptionWriter::create captures limits; numeric bounds are consumed during compose/stream-prefix and new-output reservation.
- Mutability and lifetime: Limits remain fixed for the composer, including issued output owners.
- Failure: Exceeding the stated field-specific comparison/sum is SubscriptionWriterError::RESOURCE_LIMIT; the new output is not published. If stream-prefix selects no record, walkStreamPrefix also returns RESOURCE_LIMIT.
- Consumer sources: [include/qd/protocol/subscription_writer.hpp](../../include/qd/protocol/subscription_writer.hpp), [src/protocol/subscription_writer.cpp](../../src/protocol/subscription_writer.cpp).

### `SubscriptionWriterLimits::symbolUnits` — CFG-0587 {#cfg-0587}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units per exact nonnull symbol
- Accepted values: symbol.size() ≤ symbolUnits and MAX_WIRE_COUNT=INT32_MAX; null data is rejected separately. 0 permits only an empty symbol where selector policy allows it.
- Read timing: SubscriptionWriter::create captures limits; numeric bounds are consumed during compose/stream-prefix and new-output reservation.
- Mutability and lifetime: Limits remain fixed for the composer, including issued output owners.
- Failure: Exceeding the stated field-specific comparison/sum is SubscriptionWriterError::RESOURCE_LIMIT; the new output is not published.
- Consumer sources: [include/qd/protocol/subscription_writer.hpp](../../include/qd/protocol/subscription_writer.hpp), [src/protocol/subscription_writer.cpp](../../src/protocol/subscription_writer.cpp).

### `SubscriptionWriterLimits::text` — CFG-0588 {#cfg-0588}

- Type: `encoding::UtfStringLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One description name's UTFString body bytes and units.
- Units: see declaration: UTF-16/code units
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: SubscriptionWriter(core::LocalSchema schema, std::vector&lt;OutgoingRecord&gt; records, SubscriptionWriterLimits limits, std::shared_ptr&lt;PendingSubscriptionOutput::Budget&gt; budget, core::SubscriptionSelectorPolicy policy) / [[nodiscard]] static std::expected&lt;SubscriptionWriter, SubscriptionWriterError&gt; create(const core::LocalSchema &amp;schema, std::span&lt;const OutgoingRecord&gt; records, LayoutLimits layoutLimits, SubscriptionWriterLimits limits, SubscriptionWriterWork &amp;work, core::SubscriptionSelectorPolicy policy = core::SubscriptionSelectorPolicy::EXACT_ONLY)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: SubscriptionWriterError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionWriterError`.

### `SubscriptionWriterLimits::residentBytes` — CFG-0589 {#cfg-0589}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of all retained output arrays
- Accepted values: budget.retained + new total ≤ residentBytes, including fully consumed but still retained output owners. 0 cannot hold a nonempty new output.
- Read timing: SubscriptionWriter::create captures limits; numeric bounds are consumed during compose/stream-prefix and new-output reservation.
- Mutability and lifetime: Limits remain fixed for the composer, including issued output owners.
- Failure: Exceeding the stated field-specific comparison/sum is SubscriptionWriterError::RESOURCE_LIMIT; the new output is not published.
- Consumer sources: [include/qd/protocol/subscription_writer.hpp](../../include/qd/protocol/subscription_writer.hpp), [src/protocol/subscription_writer.cpp](../../src/protocol/subscription_writer.cpp).

### `SubscriptionWriterLimits::peakBytes` — CFG-0590 {#cfg-0590}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of old outstanding output plus new candidate
- Accepted values: budget.retained + new total ≤ peakBytes; checked independently of residentBytes. 0 cannot hold a nonempty candidate.
- Read timing: SubscriptionWriter::create captures limits; numeric bounds are consumed during compose/stream-prefix and new-output reservation.
- Mutability and lifetime: Limits remain fixed for the composer, including issued output owners.
- Failure: Exceeding the stated field-specific comparison/sum is SubscriptionWriterError::RESOURCE_LIMIT; the new output is not published.
- Consumer sources: [include/qd/protocol/subscription_writer.hpp](../../include/qd/protocol/subscription_writer.hpp), [src/protocol/subscription_writer.cpp](../../src/protocol/subscription_writer.cpp).

### `SubscriptionWriterLimits::outputs` — CFG-0591 {#cfg-0591}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneous output owners including candidate
- Accepted values: budget.outputs + 1 ≤ outputs; 0 admits no output owner. Full consume does not substitute for owner destruction.
- Read timing: SubscriptionWriter::create captures limits; numeric bounds are consumed during compose/stream-prefix and new-output reservation.
- Mutability and lifetime: Limits remain fixed for the composer, including issued output owners.
- Failure: Exceeding the stated field-specific comparison/sum is SubscriptionWriterError::RESOURCE_LIMIT; the new output is not published.
- Consumer sources: [include/qd/protocol/subscription_writer.hpp](../../include/qd/protocol/subscription_writer.hpp), [src/protocol/subscription_writer.cpp](../../src/protocol/subscription_writer.cpp).

### `SubscriptionWriterLimits::omitVoidDescriptions` — CFG-0592 {#cfg-0592}

- Type: `bool`. Declared default: `= false`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Emit Java-compatible descriptions while retaining logical VOID fields.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: Explicit argument of public operations: SubscriptionWriter(core::LocalSchema schema, std::vector&lt;OutgoingRecord&gt; records, SubscriptionWriterLimits limits, std::shared_ptr&lt;PendingSubscriptionOutput::Budget&gt; budget, core::SubscriptionSelectorPolicy policy) / [[nodiscard]] static std::expected&lt;SubscriptionWriter, SubscriptionWriterError&gt; create(const core::LocalSchema &amp;schema, std::span&lt;const OutgoingRecord&gt; records, LayoutLimits layoutLimits, SubscriptionWriterLimits limits, SubscriptionWriterWork &amp;work, core::SubscriptionSelectorPolicy policy = core::SubscriptionSelectorPolicy::EXACT_ONLY)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: SubscriptionWriterError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionWriterError`.

## SubscriptionWriterWork {#cfg-0580-owner}

[include/qd/protocol/subscription_writer.hpp](../../include/qd/protocol/subscription_writer.hpp)

### `SubscriptionWriterWork::operations` — CFG-0580 {#cfg-0580}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining create/compose/describe attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionWriterError`.

### `SubscriptionWriterWork::visits` — CFG-0581 {#cfg-0581}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining metadata, key and lookup/comparison visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionWriterError`.

### `SubscriptionWriterWork::textUnits` — CFG-0582 {#cfg-0582}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining examined units, including primitive passes and comparisons.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionWriterError`.

### `SubscriptionWriterWork::outputBytes` — CFG-0583 {#cfg-0583}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining size, initialization and encoding byte work.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `SubscriptionWriterError`.

## AdmissionLimits {#cfg-0593-owner}

[include/qd/protocol/tape_admission.hpp](../../include/qd/protocol/tape_admission.hpp)

### `AdmissionLimits::inputBytes` — CFG-0593 {#cfg-0593}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of complete retained input
- Accepted values: input.size() ≤ inputBytes before copying; 0 admits only empty input at this field boundary, with grammar checked separately.
- Read timing: AdmissionBuilder copies limits for one run; checks precede the corresponding allocation/copy and counters are retained in diagnostics.
- Mutability and lifetime: Immutable during admission; a new admission receives a new profile.
- Failure: Exceeding it is AdmissionError::RESOURCE_LIMIT, stage=input-bytes. add/multiply overflow is also RESOURCE_LIMIT, stage=size-arithmetic.
- Consumer sources: [include/qd/protocol/tape_admission.hpp](../../include/qd/protocol/tape_admission.hpp), [src/protocol/tape_admission.cpp](../../src/protocol/tape_admission.cpp).

### `AdmissionLimits::reader` — CFG-0594 {#cfg-0594}

- Type: `TapeLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Fresh RAW_MESSAGE_V1 reader bounds; also used by each independent second pass.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: AdmittedTape(std::vector&lt;std::byte&gt; bytes, OutgoingLayouts layouts, OutgoingDescriptor descriptor, AdmissionLimits limits, AdmissionCounters counters) noexcept / [[nodiscard]] std::expected&lt;AdmittedTape, AdmissionDiagnostic&gt; admitTape(const core::LocalSchema &amp;schema, std::span&lt;const std::byte&gt; input, AdmissionLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: AdmissionError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `AdmissionError`.

### `AdmissionLimits::layouts` — CFG-0595 {#cfg-0595}

- Type: `LayoutLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete retained schema and explicit outgoing layout bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: AdmittedTape(std::vector&lt;std::byte&gt; bytes, OutgoingLayouts layouts, OutgoingDescriptor descriptor, AdmissionLimits limits, AdmissionCounters counters) noexcept / [[nodiscard]] std::expected&lt;AdmittedTape, AdmissionDiagnostic&gt; admitTape(const core::LocalSchema &amp;schema, std::span&lt;const std::byte&gt; input, AdmissionLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: AdmissionError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `AdmissionError`.

### `AdmissionLimits::workUnits` — CFG-0596 {#cfg-0596}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: additional byte/element/unit visits and copies
- Accepted values: Each work(units) must fit workUnits-counters.workUnits. 0 forbids positive work; the initial copy debits input.size().
- Read timing: AdmissionBuilder copies limits for one run; checks precede the corresponding allocation/copy and counters are retained in diagnostics.
- Mutability and lifetime: Immutable during admission; a new admission receives a new profile.
- Failure: Exceeding it is AdmissionError::RESOURCE_LIMIT, stage=admission-work. add/multiply overflow is also RESOURCE_LIMIT, stage=size-arithmetic.
- Consumer sources: [include/qd/protocol/tape_admission.hpp](../../include/qd/protocol/tape_admission.hpp), [src/protocol/tape_admission.cpp](../../src/protocol/tape_admission.cpp).

### `AdmissionLimits::scratchBytes` — CFG-0597 {#cfg-0597}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of coexisting source, containers and text
- Accepted values: Initially input.size() ≤ scratchBytes, then bytes_.size()+descriptorStorage+inventory+temporary ≤ scratchBytes. 0 cannot hold nonempty input.
- Read timing: AdmissionBuilder copies limits for one run; checks precede the corresponding allocation/copy and counters are retained in diagnostics.
- Mutability and lifetime: Immutable during admission; a new admission receives a new profile.
- Failure: Exceeding it is AdmissionError::RESOURCE_LIMIT, stage=scratch-bytes. add/multiply overflow is also RESOURCE_LIMIT, stage=size-arithmetic.
- Consumer sources: [include/qd/protocol/tape_admission.hpp](../../include/qd/protocol/tape_admission.hpp), [src/protocol/tape_admission.cpp](../../src/protocol/tape_admission.cpp).

### `AdmissionLimits::scratchUnits` — CFG-0598 {#cfg-0598}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of coexisting additional text
- Accepted values: Logical units in descriptorStorage+inventory+temporary ≤ scratchUnits. Raw source bytes consume 0 text units; 0 is not unlimited.
- Read timing: AdmissionBuilder copies limits for one run; checks precede the corresponding allocation/copy and counters are retained in diagnostics.
- Mutability and lifetime: Immutable during admission; a new admission receives a new profile.
- Failure: Exceeding it is AdmissionError::RESOURCE_LIMIT, stage=scratch-units. add/multiply overflow is also RESOURCE_LIMIT, stage=size-arithmetic.
- Consumer sources: [include/qd/protocol/tape_admission.hpp](../../include/qd/protocol/tape_admission.hpp), [src/protocol/tape_admission.cpp](../../src/protocol/tape_admission.cpp).

## TapeDescriptorLimits {#cfg-0599-owner}

[include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp)

### `TapeDescriptorLimits::properties` — CFG-0599 {#cfg-0599}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: global and message property pairs combined
- Accepted values: Each decoded count is added to properties_; total ≤ properties. 0 permits only empty property lists. Grammar separately requires count in 0..INT32_MAX.
- Read timing: TapeReader captures the profile. A catalog DESCRIBE_PROTOCOL creates DescriptorParser and calls parseCatalog; the ordinary header calls parse. Both boundaries read the corresponding field and map DescriptorFailure to descriptor diagnostics.
- Mutability and lifetime: No setter exists; state and cumulative counters belong to one reader.
- Failure: Exhausting this bound in DescriptorParser creates DescriptorFailure with TapeError::RESOURCE_LIMIT. TapeReader preserves the category and stage descriptor in both catalog DESCRIBE_PROTOCOL parsing and ordinary initial-header parsing. This is a terminal refusal retaining the first diagnostic; the prior admitted state is not declared a new complete descriptor.
- Consumer sources: [include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp).

### `TapeDescriptorLimits::messages` — CFG-0600 {#cfg-0600}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: send and receive entries combined
- Accepted values: All decoded send/receive counts combined ≤ messages. 0 permits only empty lists; negative count is a separate grammar error.
- Read timing: TapeReader captures the profile. A catalog DESCRIBE_PROTOCOL creates DescriptorParser and calls parseCatalog; the ordinary header calls parse. Both boundaries read the corresponding field and map DescriptorFailure to descriptor diagnostics.
- Mutability and lifetime: No setter exists; state and cumulative counters belong to one reader.
- Failure: Exhausting this bound in DescriptorParser creates DescriptorFailure with TapeError::RESOURCE_LIMIT. TapeReader preserves the category and stage descriptor in both catalog DESCRIBE_PROTOCOL parsing and ordinary initial-header parsing. This is a terminal refusal retaining the first diagnostic; the prior admitted state is not declared a new complete descriptor.
- Consumer sources: [include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp).

### `TapeDescriptorLimits::text` — CFG-0601 {#cfg-0601}

- Type: `encoding::UtfStringLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One encoded/decoded string.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TapeLimits.descriptor captured by TapeReader constructor; bounded initial descriptor parsing
- Mutability and lifetime: nested reader configuration; no descriptor-limit live setter
- Failure: Owner error vocabulary: TapeError, TapeStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TapeError, TapeStatus`.

### `TapeDescriptorLimits::textUnits` — CFG-0602 {#cfg-0602}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of successfully decoded descriptor strings
- Accepted values: Each string is bounded by min(text.codeUnits,textUnits-units_); null costs 0, a nonempty string debits its length.
- Read timing: TapeReader captures the profile. A catalog DESCRIBE_PROTOCOL creates DescriptorParser and calls parseCatalog; the ordinary header calls parse. Both boundaries read the corresponding field and map DescriptorFailure to descriptor diagnostics.
- Mutability and lifetime: No setter exists; state and cumulative counters belong to one reader.
- Failure: Exhausting this bound in DescriptorParser creates DescriptorFailure with TapeError::RESOURCE_LIMIT. TapeReader preserves the category and stage descriptor in both catalog DESCRIBE_PROTOCOL parsing and ordinary initial-header parsing. This is a terminal refusal retaining the first diagnostic; the prior admitted state is not declared a new complete descriptor.
- Consumer sources: [include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp).

## TapeLimits {#cfg-0603-owner}

[include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp)

### `TapeLimits::frame` — CFG-0603 {#cfg-0603}

- Type: `FrameLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: FrameReader storage configuration.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode, FieldTimePolicy fieldTimePolicy, TapeExternalTimePolicy externalTimePolicy, SchemaProvenance schemaProvenance, std::optional&lt;StreamDataLimits&gt; dataPerFrame) / TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode = TapeMode::LEGACY) / TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode, FieldTimePolicy fieldTimePolicy)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TapeError, TapeStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TapeError, TapeStatus`.

### `TapeLimits::inputBytes` — CFG-0604 {#cfg-0604}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: accepted bytes of the whole tape
- Accepted values: Cumulative framer.totalAccepted ≤ inputBytes. feed admits only min(input.size(),remaining); a new nonempty feed after exhaustion fails. 0 admits no nonempty input.
- Read timing: Construction captures the value without a separate allocation proportional to the counter. Actual validation occurs in feed for nonempty input at input.
- Mutability and lifetime: No setter exists; state and cumulative counters belong to one reader.
- Failure: When this counter is exhausted, TapeReader calls fail(TapeError::RESOURCE_LIMIT,"input"); it returns FAILED with the first diagnostic and stage input. With positive remaining capacity, feed limits its accepted portion to that remainder. A new nonempty feed with zero remainder yields input; an empty feed returns before this check.
- Consumer sources: [include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp).

### `TapeLimits::frames` — CFG-0605 {#cfg-0605}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: tape envelopes including empty markers
- Accepted values: Before the next nonempty feed, frames_ must be below frames; 0 prevents advancing the first frame.
- Read timing: Construction captures the value without a separate allocation proportional to the counter. Actual validation occurs in feed for nonempty input at frames.
- Mutability and lifetime: No setter exists; state and cumulative counters belong to one reader.
- Failure: When this counter is exhausted, TapeReader calls fail(TapeError::RESOURCE_LIMIT,"frames"); it returns FAILED with the first diagnostic and stage frames. The check precedes the next nonempty feed; a previously accepted frame is counted by its own counter.
- Consumer sources: [include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp).

### `TapeLimits::descriptor` — CFG-0606 {#cfg-0606}

- Type: `TapeDescriptorLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Initial descriptor bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode, FieldTimePolicy fieldTimePolicy, TapeExternalTimePolicy externalTimePolicy, SchemaProvenance schemaProvenance, std::optional&lt;StreamDataLimits&gt; dataPerFrame) / TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode = TapeMode::LEGACY) / TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode, FieldTimePolicy fieldTimePolicy)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TapeError, TapeStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TapeError, TapeStatus`.

### `TapeLimits::context` — CFG-0607 {#cfg-0607}

- Type: `ContextLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Schema and retained descriptions/actions/names.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode, FieldTimePolicy fieldTimePolicy, TapeExternalTimePolicy externalTimePolicy, SchemaProvenance schemaProvenance, std::optional&lt;StreamDataLimits&gt; dataPerFrame) / TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode = TapeMode::LEGACY) / TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode, FieldTimePolicy fieldTimePolicy)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TapeError, TapeStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TapeError, TapeStatus`.

### `TapeLimits::constructionUnits` — CFG-0608 {#cfg-0608}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate schema/context construction units
- Accepted values: Every construction charge must fit constructionUnits-construction_. In BINARY_CATALOG_V1 the complete initial schema and empty-context units must fit min(snapshotUnits,constructionUnits), so an insufficient positive grant is also rejected by the constructor. 0 is not unlimited.
- Read timing: Catalog mode reads the value during initial preparation before construction returns. Thereafter construction_ is one cumulative reader counter, charged by direct traversal and nested SessionBudget without refunding completed work.
- Mutability and lifetime: No setter exists; state and cumulative counters belong to one reader.
- Failure: In BINARY_CATALOG_V1, construction immediately checks initial schema/context units against min(snapshotUnits,constructionUnits); failure throws invalid_argument("initial session metadata exceeds configured bounds"). The discovery constructor separately rejects zero constructionUnits with invalid_argument("initial discovery identity exceeds bounds"). During operation, direct charge/work exhaustion returns TapeError::RESOURCE_LIMIT at construction. If the same aggregate budget is exhausted inside session-description or catalog-descriptor parsing, the outer path preserves RESOURCE_LIMIT and its own description or descriptor stage, respectively.
- Consumer sources: [include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp), [src/protocol/session_budget.hpp](../../src/protocol/session_budget.hpp).

### `TapeLimits::data` — CFG-0609 {#cfg-0609}

- Type: `StreamDataLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Aggregate file work/output bounds and per-symbol bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode, FieldTimePolicy fieldTimePolicy, TapeExternalTimePolicy externalTimePolicy, SchemaProvenance schemaProvenance, std::optional&lt;StreamDataLimits&gt; dataPerFrame) / TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode = TapeMode::LEGACY) / TapeReader(const core::LocalSchema &amp;schema, TapeLimits limits, TapeMode mode, FieldTimePolicy fieldTimePolicy)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TapeError, TapeStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TapeError, TapeStatus`.

### `TapeLimits::timingEvents` — CFG-0610 {#cfg-0610}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: timing metadata attempts: heartbeat and DATA
- Accepted values: RAW/broad modes count explicit HEARTBEAT and corresponding DATA. An EMPTY_HEARTBEAT marker returns earlier and does not consume timingEvents. 0 forbids a counted event.
- Read timing: Construction captures the value without a separate allocation proportional to the counter. Actual validation occurs in processing an applicable frame at timing.
- Mutability and lifetime: No setter exists; state and cumulative counters belong to one reader.
- Failure: When this counter is exhausted, TapeReader calls fail(TapeError::RESOURCE_LIMIT,"timing"); it returns FAILED with the first diagnostic and stage timing. RAW/catalog modes count explicit HEARTBEAT and applicable DATA. EMPTY_HEARTBEAT does not charge this counter.
- Consumer sources: [include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp).

### `TapeLimits::snapshotUnits` — CFG-0611 {#cfg-0611}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: broad schema/description/context/descriptor/temporary peak units
- Accepted values: 0 provides empty capacity. In BINARY_CATALOG_V1 the initial schema and empty context must fit entirely in min(snapshotUnits,constructionUnits); an insufficient positive grant is also rejected. Later SessionBudget checks old and candidate state simultaneously.
- Read timing: In catalog mode, construction reads snapshotUnits for initial complete state; SessionBudget later accounts for published metadata together with temporary/candidate copies.
- Mutability and lifetime: No setter exists; state and cumulative counters belong to one reader.
- Failure: In BINARY_CATALOG_V1, initial state units are checked by construction together with constructionUnits; insufficient capacity throws invalid_argument("initial session metadata exceeds configured bounds"). Discovery zero throws invalid_argument("initial discovery identity exceeds bounds"). Later peak/copy checks return TapeError::RESOURCE_LIMIT at construction; if SessionBudget refuses inside description/descriptor parsing, the outer diagnostic checkpoint is description or descriptor.
- Consumer sources: [include/qd/protocol/tape_reader.hpp](../../include/qd/protocol/tape_reader.hpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp), [src/protocol/session_budget.hpp](../../src/protocol/session_budget.hpp).

## LayoutLimits {#cfg-0612-owner}

[include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp)

### `LayoutLimits::schema` — CFG-0612 {#cfg-0612}

- Type: `core::SchemaLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete local schema copy bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: OutgoingLayouts(core::LocalSchema schema, std::vector&lt;OutgoingRecord&gt; records, LayoutLimits limits) noexcept / [[nodiscard]] static std::expected&lt;OutgoingLayouts, WriterDiagnostic&gt; create(const core::LocalSchema &amp;schema, std::span&lt;const OutgoingRecord&gt; records, LayoutLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: WriterError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `WriterError`.

### `LayoutLimits::records` — CFG-0613 {#cfg-0613}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: outgoing record descriptions
- Accepted values: records.size() ≤ LayoutLimits.records; 0 admits an empty table at the resource boundary. limits.schema separately checks the complete local schema.
- Read timing: Read once by OutgoingLayouts::create before copying the complete admitted schema and outgoing declarations.
- Mutability and lifetime: Prepared layouts are immutable; another profile requires new preparation.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT in layoutFailure, stage=layout-count. Identity/type errors are checked separately.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `LayoutLimits::fields` — CFG-0614 {#cfg-0614}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate outgoing fields
- Accepted values: Sum of record.fields.size() ≤ fields; charge uses the remainder without overflow. 0 permits only no fields.
- Read timing: Read once by OutgoingLayouts::create before copying the complete admitted schema and outgoing declarations.
- Mutability and lifetime: Prepared layouts are immutable; another profile requires new preparation.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT in layoutFailure, stage=field-count. Identity/type errors are checked separately.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `LayoutLimits::nameUnits` — CFG-0615 {#cfg-0615}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units per outgoing record/field name
- Accepted values: Every record/field wireName ≤ nameUnits; name emptiness/validity is checked separately.
- Read timing: Read once by OutgoingLayouts::create before copying the complete admitted schema and outgoing declarations.
- Mutability and lifetime: Prepared layouts are immutable; another profile requires new preparation.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT in layoutFailure, stage=record-name/field-name. Identity/type errors are checked separately.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `LayoutLimits::totalNameUnits` — CFG-0616 {#cfg-0616}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of all outgoing names including repeats
- Accepted values: Every wireName is charged against totalNameUnits; repeated names are charged again and 0 gives no budget.
- Read timing: Read once by OutgoingLayouts::create before copying the complete admitted schema and outgoing declarations.
- Mutability and lifetime: Prepared layouts are immutable; another profile requires new preparation.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT in layoutFailure, stage=record-name/field-name. Identity/type errors are checked separately.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

## WriterLimits {#cfg-0617-owner}

[include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp)

### `WriterLimits::frameBytes` — CFG-0617 {#cfg-0617}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: frame body bytes including compact message ID
- Accepted values: bodyBytes ≤ frameBytes and INT64_MAX; the outer length prefix is excluded. 0 permits only a zero body where the operation creates a marker.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Preflight first checks bodyBytes&gt;frameBytes, yielding WriterError::RESOURCE_LIMIT, stage=frame-bytes. Only after that check passes does bodyBytes&gt;INT64_MAX yield the same error category at stage=frame-size. These are running-operation checks; execute retains that operation diagnostic and makes the existing writer FAILED without publishing a partial chunk.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::outputBytes` — CFG-0618 {#cfg-0618}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative committed envelope bytes including prefixes
- Accepted values: The next envelope size ≤ outputBytes-counters.outputBytes; committed usage is added only after successful encode.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=output-bytes. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::frames` — CFG-0619 {#cfg-0619}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative committed envelopes including markers
- Accepted values: Before output, counters.frames &lt; frames; 0 admits no first envelope. Output-free FINISH returns before this check.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=frames. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::records` — CFG-0620 {#cfg-0620}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: attempted DATA records and description IDs
- Accepted values: Each traversed record/ID consumes records; 0 admits no positive attempt count. Sizing-pass charges are not refunded on later failure.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=records. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::fields` — CFG-0621 {#cfg-0621}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: attempted DATA/description fields
- Accepted values: Every visited field consumes fields; 0 permits only the absence of such visits.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=fields. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::text` — CFG-0622 {#cfg-0622}

- Type: `encoding::UtfStringLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Per descriptor/name UTFString body bytes and input units.
- Units: see declaration: UTF-16/code units
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: TapeWriter(OutgoingLayouts layouts, OutgoingDescriptor descriptor, WriterLimits limits, WriterCounters counters) / [[nodiscard]] static std::expected&lt;TapeWriter, WriterDiagnostic&gt; create(const OutgoingLayouts &amp;layouts, const OutgoingDescriptor &amp;descriptor, WriterLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: WriterError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `WriterError`.

### `WriterLimits::textUnits` — CFG-0623 {#cfg-0623}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 visits of descriptor/name text
- Accepted values: textUnits accumulates actual UTF-16 visit units. During CREATE, version/options/time strings are charged twice; options/time comparison adds one unit per actually compared position. Later sizing/encoding passes continue the same counter. 0 is not a disabling sentinel.
- Read timing: TapeWriter::create first charges two descriptor-string passes, then options/time comparisons; that usage enters the initial WriterCounters. After creation, Operation::text and the encoding pass use the captured aggregate remainder.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: During CREATE, charging textUnits for two descriptor-string passes may yield WriterError::RESOURCE_LIMIT, operation=CREATE, stage=descriptor-text; per-character comparison of admissible options/time may yield CREATE, stage=descriptor-text-comparison. No writer is published on factory refusal. After successful creation, running operations can exhaust the same cumulative counter at text-total; execute then publishes no partial chunk and makes the existing writer FAILED.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::fieldString` — CFG-0624 {#cfg-0624}

- Type: `encoding::CharArrayLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One type-10 prefix/body encoded bytes and units.
- Units: see declaration: UTF-16/code units
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: TapeWriter(OutgoingLayouts layouts, OutgoingDescriptor descriptor, WriterLimits limits, WriterCounters counters) / [[nodiscard]] static std::expected&lt;TapeWriter, WriterDiagnostic&gt; create(const OutgoingLayouts &amp;layouts, const OutgoingDescriptor &amp;descriptor, WriterLimits limits)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: WriterError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `WriterError`.

### `WriterLimits::fieldBytes` — CFG-0625 {#cfg-0625}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate canonical type10 byte work
- Accepted values: Every pass size is charged against fieldBytes, including the prefix. 0 is not unlimited.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=field-bytes. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::fieldUnits` — CFG-0626 {#cfg-0626}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 type10 visits
- Accepted values: type10 units are charged during sizing and encoding passes; 0 admits no nonempty field.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=field-units. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::symbolUnits` — CFG-0627 {#cfg-0627}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of one symbol
- Accepted values: record.symbol.size() ≤ symbolUnits; additional symbol-grammar checks are independent. 0 limits a symbol to empty where allowed.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=symbol-units. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::totalSymbolUnits` — CFG-0628 {#cfg-0628}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 symbol visits
- Accepted values: Comparison, copy and encoding debit totalSymbolUnits; a repeated symbol also has comparison cost. 0 is a literal empty budget.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=symbol-total. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::scratchBytes` — CFG-0629 {#cfg-0629}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of coexisting private envelope storage
- Accepted values: Complete envelope size including prefix ≤ scratchBytes before chunk.bytes.resize. 0 cannot hold a nonempty envelope.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=scratch-bytes. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::scratchUnits` — CFG-0630 {#cfg-0630}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of temporary/previous symbol at peak
- Accepted values: symbolPeak ≤ scratchUnits; coexisting temporary storage is counted separately from aggregate symbol visits. 0 is an empty capacity.
- Read timing: TapeWriter::create captures WriterLimits. This field is checked or its counter charged during the corresponding execute operation; construction is not presented as a separate validation of this numerical value.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: Exceeding it is WriterError::RESOURCE_LIMIT, stage=scratch-units. execute publishes no partial chunk and becomes FAILED; previously charged attempts remain in counters.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).

### `WriterLimits::workUnits` — CFG-0631 {#cfg-0631}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate visits/copies/sizing/encoding
- Accepted values: Every work(amount) debits workUnits; TapeWriter creation also charges construction and descriptor/layout metadata copies. 0 admits no positive work.
- Read timing: workUnits is read by TapeWriter::create for preparation passes, copies and comparisons; cumulative WriterCounters are passed into the created object. Each later Operation::work action spends the remaining capacity of that same counter.
- Mutability and lifetime: No setter exists; FAILED is not recovered by changing a limit value.
- Failure: During CREATE, insufficient workUnits for validating/copying the schema and outgoing layouts returns WriterError::RESOURCE_LIMIT, operation=CREATE, stage=construction-work. Insufficient copied(text.size()) for a descriptor string returns CREATE/descriptor-text. Insufficient two work units for an options/time character comparison returns the original CREATE/construction-work. No writer is published on these refusals. After successful creation, Operation::work reports RESOURCE_LIMIT at stage work; execute failure makes the existing writer FAILED.
- Consumer sources: [include/qd/protocol/tape_writer.hpp](../../include/qd/protocol/tape_writer.hpp), [src/protocol/tape_writer.cpp](../../src/protocol/tape_writer.cpp).
