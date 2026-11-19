# Public API

[Русский](api.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [EndpointConfigurationLimits](api.md#cfg-0001-owner) — `include/qd/api/endpoint_configuration.hpp`
- [EventRegistryLimits](api.md#cfg-0005-owner) — `include/qd/api/event_registry.hpp`
- [EventRegistryWork](api.md#cfg-0008-owner) — `include/qd/api/event_registry.hpp`
- [FeedFilterContextLimits](api.md#cfg-0022-owner) — `include/qd/api/feed_filter.hpp`
- [FeedFilterIpfLimits](api.md#cfg-0032-owner) — `include/qd/api/feed_filter.hpp`
- [FeedFilterLimits](api.md#cfg-0010-owner) — `include/qd/api/feed_filter.hpp`
- [BasicTlsCredentials](api.md#cfg-0065-owner) — `include/qd/api/feed.hpp`
- [EndpointOptions](api.md#cfg-0080-owner) — `include/qd/api/feed.hpp`
- [HistoryReceivingOptions](api.md#cfg-0058-owner) — `include/qd/api/feed.hpp`
- [HistorySnapshotOptions](api.md#cfg-0109-owner) — `include/qd/api/feed.hpp`
- [RuntimeOptions](api.md#cfg-0042-owner) — `include/qd/api/feed.hpp`
- [TapeSourceOptions](api.md#cfg-0103-owner) — `include/qd/api/feed.hpp`
- [VerifiedTlsOptions](api.md#cfg-0070-owner) — `include/qd/api/feed.hpp`
- [IndependentSubscriptionLimits](api.md#cfg-0110-owner) — `include/qd/api/independent_subscription.hpp`
- [IndexedEventModelLimits](api.md#cfg-0113-owner) — `include/qd/api/indexed_event_model.hpp`
- [OrderBookCorrectionOptions](api.md#cfg-0122-owner) — `include/qd/api/order_book_model.hpp`
- [OrderBookViewOptions](api.md#cfg-0118-owner) — `include/qd/api/order_book_model.hpp`
- [PublisherCatalogOptions](api.md#cfg-0129-owner) — `include/qd/api/publisher.hpp`
- [PublisherDemandPolicy](api.md#cfg-0131-owner) — `include/qd/api/publisher.hpp`
- [PublisherOptions](api.md#cfg-0145-owner) — `include/qd/api/publisher.hpp`
- [PublisherPeerDemandPolicy](api.md#cfg-0134-owner) — `include/qd/api/publisher.hpp`
- [PublisherRecordLimits](api.md#cfg-0136-owner) — `include/qd/api/publisher.hpp`
- [PublisherSourceOptions](api.md#cfg-0126-owner) — `include/qd/api/publisher.hpp`
- [PublisherStreamByteLimits](api.md#cfg-0141-owner) — `include/qd/api/publisher.hpp`
- [TimeSeriesModelLimits](api.md#cfg-0157-owner) — `include/qd/api/time_series_subscription.hpp`

## EndpointConfigurationLimits {#cfg-0001-owner}

[include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp)

### `EndpointConfigurationLimits::properties` — CFG-0001 {#cfg-0001}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input properties across all layers, including shadowed entries
- Accepted values: Positive. Zero causes INVALID_ARGUMENT before reading input. Input entry count must be &lt;= properties; each duplicate counts. No separate numeric ceiling is imposed here; keyBytes/valueBytes/totalBytes also apply.
- Read timing: EndpointConfiguration::create: zero validation and sequential resolution of defaults, properties and address blocks.
- Mutability and lifetime: The limits belong to one EndpointConfiguration::create call and are consumed by InputEnvelope only. The result retains resolved EndpointOptions and effective properties; State does not retain these four input limits. A later create call can pass independent limits.
- Failure: api::Error: INVALID_ARGUMENT / endpoint-configuration-limits for zero; RESOURCE_LIMIT / endpoint-property-input on overflow. Invalid keys/values have separate errors; construction bad_alloc maps to ALLOCATION_FAILURE / endpoint-configuration-storage.
- Consumer sources: [include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp).

### `EndpointConfigurationLimits::keyBytes` — CFG-0002 {#cfg-0002}

- Type: `std::size_t`. Declared default: `= 96`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes per key
- Accepted values: Positive; zero rejects. Each key length must be &lt;= keyBytes and remaining totalBytes; an empty or nonprintable key separately causes INVALID_ARGUMENT. There is no fixed upper ceiling on the limit.
- Read timing: EndpointConfiguration::create: zero validation and sequential resolution of defaults, properties and address blocks.
- Mutability and lifetime: The limits belong to one EndpointConfiguration::create call and are consumed by InputEnvelope only. The result retains resolved EndpointOptions and effective properties; State does not retain these four input limits. A later create call can pass independent limits.
- Failure: api::Error: INVALID_ARGUMENT / endpoint-configuration-limits for zero; RESOURCE_LIMIT / endpoint-property-input on overflow. Invalid keys/values have separate errors; construction bad_alloc maps to ALLOCATION_FAILURE / endpoint-configuration-storage.
- Consumer sources: [include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp).

### `EndpointConfigurationLimits::valueBytes` — CFG-0003 {#cfg-0003}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes per property value, including an address string
- Accepted values: Positive; zero rejects. A value may be empty when its consumer permits it; length must be &lt;= valueBytes and remaining totalBytes. Address parsing applies its own grammar and bounds. There is no numeric upper ceiling on this limit.
- Read timing: EndpointConfiguration::create: zero validation and sequential resolution of defaults, properties and address blocks.
- Mutability and lifetime: The limits belong to one EndpointConfiguration::create call and are consumed by InputEnvelope only. The result retains resolved EndpointOptions and effective properties; State does not retain these four input limits. A later create call can pass independent limits.
- Failure: api::Error: INVALID_ARGUMENT / endpoint-configuration-limits for zero; RESOURCE_LIMIT / endpoint-property-input on overflow. Invalid keys/values have separate errors; construction bad_alloc maps to ALLOCATION_FAILURE / endpoint-configuration-storage.
- Consumer sources: [include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp).

### `EndpointConfigurationLimits::totalBytes` — CFG-0004 {#cfg-0004}

- Type: `std::size_t`. Declared default: `= 16384`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate bytes of keys and values of all inspected properties
- Accepted values: Positive; zero rejects. Each key and value must fit the remaining totalBytes; shadowed values also consume the allowance. Subtraction precedes addition. No additional upper ceiling is imposed; this does not guarantee that such memory can be allocated.
- Read timing: EndpointConfiguration::create: zero validation and sequential resolution of defaults, properties and address blocks.
- Mutability and lifetime: The limits belong to one EndpointConfiguration::create call and are consumed by InputEnvelope only. The result retains resolved EndpointOptions and effective properties; State does not retain these four input limits. A later create call can pass independent limits.
- Failure: api::Error: INVALID_ARGUMENT / endpoint-configuration-limits for zero; RESOURCE_LIMIT / endpoint-property-input on overflow. Invalid keys/values have separate errors; construction bad_alloc maps to ALLOCATION_FAILURE / endpoint-configuration-storage.
- Consumer sources: [include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp).

## EventRegistryLimits {#cfg-0005-owner}

[include/qd/api/event_registry.hpp](../../include/qd/api/event_registry.hpp)

### `EventRegistryLimits::schema` — CFG-0005 {#cfg-0005}

- Type: `core::SchemaLimits`. Declared default: `{16, 128, 256, 128, 16384}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete retained schema, including generic records.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;EventRegistry, EventRegistryFailure&gt; create(const core::LocalSchema &amp;schema, std::span&lt;const EventRegistration&gt; registrations, EventRegistryLimits limits, EventRegistryWork &amp;work) / [[nodiscard]] static std::expected&lt;EventRegistry, EventRegistryFailure&gt; builtIn(EventRegistryLimits limits, EventRegistryWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: EventRegistryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `EventRegistryError`.

### `EventRegistryLimits::registrations` — CFG-0006 {#cfg-0006}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: typed-route registrations
- Accepted values: Zero permits an empty registration list only; a positive value bounds registrations.size(). No separate upper ceiling applies. Even an empty registry must fit its schema and state in storageBytes; work and schema limits apply independently.
- Read timing: EventRegistry::create: checked before copying the schema and again when accounting for retained state.
- Mutability and lifetime: EventRegistryLimits apply by value to one create call. The registry retains schema, registrations, versionFields and measured storageBytes; requested ceilings do not become a policy for subsequent operations. Another create may use a different grant.
- Failure: std::expected with EventRegistryFailure{EventRegistryError::RESOURCE_LIMIT,index?} for excess/overflow; invalid schema/routes and allocation have separate causes.
- Consumer sources: [src/api/event_registry.cpp](../../src/api/event_registry.cpp), [include/qd/api/event_registry.hpp](../../include/qd/api/event_registry.hpp).

### `EventRegistryLimits::storageBytes` — CFG-0007 {#cfg-0007}

- Type: `std::size_t`. Declared default: `= 8 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: accounted retained bytes for schema, routes, their capacities and shared state
- Accepted values: Zero is a real allowance and cannot create a nonempty state. State, schema, registrations and their arrays must first fit; actual capacities and the allocate_shared block are then accounted for. Checked additions/multiplications may reject. There is no single constant minimum/maximum: requirements depend on the schema and registrations.
- Read timing: EventRegistry::create: checked before copying the schema and again when accounting for retained state.
- Mutability and lifetime: EventRegistryLimits apply by value to one create call. The registry retains schema, registrations, versionFields and measured storageBytes; requested ceilings do not become a policy for subsequent operations. Another create may use a different grant.
- Failure: std::expected with EventRegistryFailure{EventRegistryError::RESOURCE_LIMIT,index?} for excess/overflow; invalid schema/routes and allocation have separate causes.
- Consumer sources: [src/api/event_registry.cpp](../../src/api/event_registry.cpp), [include/qd/api/event_registry.hpp](../../include/qd/api/event_registry.hpp).

## EventRegistryWork {#cfg-0008-owner}

[include/qd/api/event_registry.hpp](../../include/qd/api/event_registry.hpp)

### `EventRegistryWork::visits` — CFG-0008 {#cfg-0008}

- Type: `std::size_t`. Declared default: `= 1000000`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Schema element/name-unit inspections and route/duplicate inspections.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `EventRegistryError`.

### `EventRegistryWork::preparation` — CFG-0009 {#cfg-0009}

- Type: `std::size_t`. Declared default: `= 100000000`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Sum of each attempted mapper's declared count/unit/comparison bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `EventRegistryError`.

## FeedFilterContextLimits {#cfg-0022-owner}

[include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp)

### `FeedFilterContextLimits::expressionUnits` — CFG-0022 {#cfg-0022}

- Type: `std::size_t`. Declared default: `= 8192`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per IPF token
- Accepted values: Zero is accepted at context creation; every nonempty valid IPF token then exceeds it. Complete expression length must be &lt;= expressionUnits; grammar is checked separately.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: RESOURCE_LIMIT / filter-ipf-config-units. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_ipf_config.cpp](../../src/api/feed_filter_ipf_config.cpp).

### `FeedFilterContextLimits::resources` — CFG-0023 {#cfg-0023}

- Type: `std::size_t`. Declared default: `= 8`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: registered context resources
- Accepted values: Zero forbids adding a new resource; replacing an existing resource does not consume another slot. Count must be &lt;= resources. The value also participates in update-work accounting.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: RESOURCE_LIMIT / filter-resource-count. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::symbols` — CFG-0024 {#cfg-0024}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: symbols or schedule descriptions per input update
- Accepted values: Symbols and schedule metadata each have count &lt;= symbols. Zero permits empty inputs. Input count is checked before deduplication; totalSymbolUnits independently bounds their aggregate length.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: RESOURCE_LIMIT / filter-resource-metadata-count or filter-resource-symbols. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::symbolUnits` — CFG-0025 {#cfg-0025}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per symbol
- Accepted values: Each symbol must be &lt;= symbolUnits; zero permits empty symbols only. Schedule descriptions are not individually checked against this field; they consume totalSymbolUnits.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: RESOURCE_LIMIT / filter-resource-symbol-units. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::totalSymbolUnits` — CFG-0026 {#cfg-0026}

- Type: `std::size_t`. Declared default: `= 8 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 code units of symbols and schedule descriptions
- Accepted values: Sum of input lengths must be &lt;= totalSymbolUnits, with checked addition. Zero permits a zero sum only; count and work limits still apply.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: RESOURCE_LIMIT / filter-resource-total-units; accounting arithmetic overflow RESOURCE_LIMIT / filter-storage. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::snapshots` — CFG-0027 {#cfg-0027}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneously retained resource/filter generations
- Accepted values: Zero is accepted by create but rejects the first reserveGeneration. Each retained generation occupies a slot until its charge is destroyed; storageBytes separately bounds aggregate bytes.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: RESOURCE_LIMIT / filter-live-snapshots. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::storageBytes` — CFG-0028 {#cfg-0028}

- Type: `std::size_t`. Declared default: `= 64 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate accounted bytes of live generations
- Accepted values: A generation must fit alongside all prior live generations. Zero forbids positive retention. Checks bytes &lt;= maximumBytes and previous &lt;= maximumBytes-bytes.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: RESOURCE_LIMIT / filter-live-storage. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::trackers` — CFG-0029 {#cfg-0029}

- Type: `std::size_t`. Declared default: `= 32`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: live context trackers
- Accepted values: Zero prevents build with an attached context. Expired weak_ptr entries are removed before checking; live tracker count must be &lt;= trackers. reserve may fail earlier for excessive values.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: RESOURCE_LIMIT / filter-context-trackers; length_error during build → RESOURCE_LIMIT / filter-build-capacity. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::listeners` — CFG-0030 {#cfg-0030}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: live listeners per dynamic tracker
- Accepted values: Zero rejects a dynamic tracker listener; static trackers return token 0 without registration. uint64_t token exhaustion also rejects. The value is used by reserve during build.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: RESOURCE_LIMIT / filter-tracker-listeners; ListenerNode allocation → ALLOCATION_FAILURE / filter-tracker-listener-allocation. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::updateWork` — CFG-0031 {#cfg-0031}

- Type: `std::size_t`. Declared default: `= 1024 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative update/recompilation work units
- Accepted values: Initial remainingWork equals updateWork with no general replenishment. Zero normally rejects the first update due to positive control cost. Cost depends on expression, resources/expressionUnits, symbol counts/lengths and live-filter rebuilding.
- Read timing: FeedFilterContext::create stores the numbers without numeric validation. replaceResourceSymbols, build and listener registration consume them later; remainingWork is spent over the context lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error: WORK_LIMIT / filter-resource-work or filter-resource-rebuild-work; normalization may yield WORK_LIMIT / filter-expression. Context allocation: ALLOCATION_FAILURE / filter-context-allocation.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

## FeedFilterIpfLimits {#cfg-0032-owner}

[include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp)

### `FeedFilterIpfLimits::schedulerInterval` — CFG-0032 {#cfg-0032}

- Type: `std::chrono::milliseconds`. Declared default: `{10}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Internal listener-activation cadence; strictly positive.
- Units: milliseconds
- Declared constraints: Internal listener-activation cadence; strictly positive.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::unique_ptr&lt;FeedFilterIpfRuntime&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const ipf::IpfTransferOptions &amp;transfer, FeedFilterIpfLimits limits = {}, FeedFilterIpfSchedule schedule = {}, FeedFilterContextLimits contextLimits = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp); the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `FeedFilterIpfLimits::schedulerTurns` — CFG-0033 {#cfg-0033}

- Type: `std::size_t`. Declared default: `= 10000000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: shared scheduler turns
- Accepted values: Strictly &gt;0. The worker terminates when schedulerTurns reaches the limit; every iteration increments the count. This is an owner-wide limit rather than one grant per source.
- Read timing: FeedFilterIpfRuntime::create first validates positivity. Control captures the limits; the worker and snapshot copying consume them over the owner lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero/nonpositive value: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Consumption failure: WORK_LIMIT / filter-ipf-owner-lifetime-work. transferDiagnostic maps transfer results into api::Error/retained diagnostics.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::sources` — CFG-0034 {#cfg-0034}

- Type: `std::size_t`. Declared default: `= 8`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: IPF source registrations
- Accepted values: Strictly &gt;0. ports.size() must be &lt; sources before adding a new source; an existing registration can be reused under add rules. reserve depends on container capacity and allocation.
- Read timing: FeedFilterIpfRuntime::create first validates positivity. Control captures the limits; the worker and snapshot copying consume them over the owner lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero/nonpositive value: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Consumption failure: RESOURCE_LIMIT / filter-ipf-source-registrations. transferDiagnostic maps transfer results into api::Error/retained diagnostics.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::initialTimeout` — CFG-0035 {#cfg-0035}

- Type: `std::chrono::milliseconds`. Declared default: `{300000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds for initial-snapshot wait
- Accepted values: Strictly positive duration. Deadline=min(owner deadline, now+initialTimeout), using saturating addition. The set of sources shares this wait deadline; zero and negative values reject.
- Read timing: FeedFilterIpfRuntime::create first validates positivity. Control captures the limits; the worker and snapshot copying consume them over the owner lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero/nonpositive value: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Consumption failure: INVALID_STATE / filter-ipf-initial-snapshot-deadline or retained source diagnostic. transferDiagnostic maps transfer results into api::Error/retained diagnostics.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::attempts` — CFG-0036 {#cfg-0036}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate owner attempts
- Accepted values: Strictly &gt;0. Attempts across sources consume the shared attempts allowance; the worker stops at attempts &gt;= limit. Per-source counters are separate observations.
- Read timing: FeedFilterIpfRuntime::create first validates positivity. Control captures the limits; the worker and snapshot copying consume them over the owner lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero/nonpositive value: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Consumption failure: WORK_LIMIT / filter-ipf-owner-lifetime-work. transferDiagnostic maps transfer results into api::Error/retained diagnostics.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::expressionUnits` — CFG-0037 {#cfg-0037}

- Type: `std::size_t`. Declared default: `= 8192`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per complete IPF token
- Accepted values: Strictly &gt;0. add passes this limit to parseFilterIpfConfig; the complete token must be &lt;= expressionUnits and satisfy its grammar. No separate upper ceiling applies.
- Read timing: FeedFilterIpfRuntime::create checks positivity. Control stores the value; every add passes expressionUnits to parseFilterIpfConfig before parsing the expression.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Excess complete-expression length: api::Error RESOURCE_LIMIT / filter-ipf-config-units directly from parseFilterIpfConfig.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp), [src/api/feed_filter_ipf_config.cpp](../../src/api/feed_filter_ipf_config.cpp).

### `FeedFilterIpfLimits::snapshotSymbols` — CFG-0038 {#cfg-0038}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: symbols per copied snapshot
- Accepted values: Strictly &gt;0. Adding requires symbols.size()&lt;snapshotSymbols. snapshotUnits and copyWork constrain the same copy independently.
- Read timing: FeedFilterIpfRuntime::create first validates positivity. Control captures the limits; the worker and snapshot copying consume them over the owner lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero/nonpositive value: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Consumption failure: IpfTransferError::RESOURCE_LIMIT / filter-ipf-snapshot-symbols. transferDiagnostic maps transfer results into api::Error/retained diagnostics.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::snapshotUnits` — CFG-0039 {#cfg-0039}

- Type: `std::size_t`. Declared default: `= 8 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 symbol and schedule units per snapshot
- Accepted values: Strictly &gt;0. Symbols and schedules must successively fit the shared remaining snapshotUnits; subtraction checks prevent overflow. A symbol may exhaust it before its schedule is added.
- Read timing: FeedFilterIpfRuntime::create first validates positivity. Control captures the limits; the worker and snapshot copying consume them over the owner lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero/nonpositive value: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Consumption failure: IpfTransferError::RESOURCE_LIMIT / filter-ipf-snapshot-symbols or filter-ipf-snapshot-schedules. transferDiagnostic maps transfer results into api::Error/retained diagnostics.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::copyWork` — CFG-0040 {#cfg-0040}

- Type: `std::size_t`. Declared default: `= 64 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative copy visits/code units over owner lifetime
- Accepted values: Strictly &gt;0. charge debits one per record plus symbol/schedule lengths before copying; copyWork accumulates, checking amount&lt;=limit and spent&lt;=limit-amount.
- Read timing: FeedFilterIpfRuntime::create first validates positivity. Control captures the limits; the worker and snapshot copying consume them over the owner lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero/nonpositive value: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Consumption failure: IpfTransferError::WORK_LIMIT / filter-ipf-snapshot-copy-work. transferDiagnostic maps transfer results into api::Error/retained diagnostics.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::lifetime` — CFG-0041 {#cfg-0041}

- Type: `std::chrono::milliseconds`. Declared default: `{86400000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: owner lifetime milliseconds
- Accepted values: Strictly positive duration. Control computes a deadline with saturating steady_clock addition; worker/checkpoints enforce it and initial-snapshot waiting is additionally bounded by it.
- Read timing: FeedFilterIpfRuntime::create first validates positivity. Control captures the limits; the worker and snapshot copying consume them over the owner lifetime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero/nonpositive value: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Consumption failure: IpfTransferError::DEADLINE / filter-ipf-owner-lifetime; worker: WORK_LIMIT / filter-ipf-owner-lifetime-work. transferDiagnostic maps transfer results into api::Error/retained diagnostics.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

## FeedFilterLimits {#cfg-0010-owner}

[include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp)

### `FeedFilterLimits::categories` — CFG-0010 {#cfg-0010}

- Type: `std::size_t`. Declared default: `= 32`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: categories
- Accepted values: Zero is captured by Builder but rejects the first withCategory; category count is bounded by categories. An empty build separately rejects. No additional upper ceiling applies.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-category-limit.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterLimits::categoryNameUnits` — CFG-0011 {#cfg-0011}

- Type: `std::size_t`. Declared default: `= 128`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per category name
- Accepted values: Names must be nonempty, unique and &lt;= categoryNameUnits. Zero prevents adding a category. No separate maximum is imposed.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-category-limit.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterLimits::expressionUnits` — CFG-0012 {#cfg-0012}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 expression code units
- Accepted values: A nonempty expression must be &lt;= expressionUnits; zero excludes a valid category. The same limit reaches the compiler. No separate upper ceiling applies.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-category-limit / filter-expression.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterLimits::expressionNodes` — CFG-0013 {#cfg-0013}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: expression syntax-tree nodes
- Accepted values: Zero rejects the first node; each node increments the count up to expressionNodes. expressionDepth, storageBytes and normalizationWork also constrain compilation. No fixed upper ceiling applies.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-expression.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::expressionDepth` — CFG-0014 {#cfg-0014}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: parse/tree depth levels
- Accepted values: Nonempty expressions start parsing at depth 1, so expressionDepth=0 rejects them; Builder disallows empty expressions. Depth &lt;= expressionDepth is also checked after normalization. No separate native-stack ceiling is imposed.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-expression.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::patternPositions` — CFG-0015 {#cfg-0015}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: pattern automaton positions
- Accepted values: Each created position consumes patternPositions; zero rejects the first position. Grammar determines forms that create no positions. No additional maximum applies.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-expression.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::finiteSymbols` — CFG-0016 {#cfg-0016}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: symbols in a normalized finite set
- Accepted values: The finiteSymbols remainder is consumed when finite sets are constructed. Zero forbids consuming one symbol, although not every expression needs a finite set. No separate upper bound applies.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-expression.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::normalizationWork` — CFG-0017 {#cfg-0017}

- Type: `std::size_t`. Declared default: `= 100000000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: normalization work units
- Accepted values: Initial normalizationWork allowance; a context also constrains it by shared remainingWork. Zero admits only operations without a debit. Exhaustion has WORK_LIMIT; there is no separate upper ceiling.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with WORK_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-expression.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::symbolUnits` — CFG-0018 {#cfg-0018}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per coordinate symbol
- Accepted values: Symbol length must be &lt;= symbolUnits for typed and generic classification. Zero permits an empty symbol only where route grammar permits one. Record names have a separate limit.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-symbol-units / filter-record-limit.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_coordinates.cpp](../../src/api/feed_filter_coordinates.cpp).

### `FeedFilterLimits::coordinates` — CFG-0019 {#cfg-0019}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: physical coordinates per request
- Accepted values: Zero rejects a Candle coordinate and the first selected ordinary-route coordinate. Selected count must be &lt;= coordinates; routes/schema also limit the actual count.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-coordinate-count.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_coordinates.cpp](../../src/api/feed_filter_coordinates.cpp).

### `FeedFilterLimits::storageBytes` — CFG-0020 {#cfg-0020}

- Type: `std::size_t`. Declared default: `= 16 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: estimated retained filter bytes
- Accepted values: Checks sizeof(State)+categories+UTF-16 texts+2048*(expression length+1) per category. Zero cannot fit the state. Addition/multiplication are checked; no numeric upper ceiling applies.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-storage.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterLimits::recordNameUnits` — CFG-0021 {#cfg-0021}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per generic record name
- Accepted values: acceptRecord requires a nonempty name &lt;= recordNameUnits; zero excludes every valid call. No separate upper bound applies.
- Read timing: Builder captures the limits; withCategory and build check input, compile expressions and publish an immutable filter. accept uses captured coordinate limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error with RESOURCE_LIMIT and an operation-specific label; expression syntax errors map to INVALID_ARGUMENT. build maps bad_alloc to ALLOCATION_FAILURE and length_error to RESOURCE_LIMIT. Rejection label for this condition: filter-record-limit.
- Consumer sources: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

## BasicTlsCredentials {#cfg-0065-owner}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `BasicTlsCredentials::MINIMUM_TOKEN_UNITS` — CFG-0065 {#cfg-0065}

- Type: `static constexpr std::size_t`. Declared default: `= 7`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Scheme prefix plus a nonempty value.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `BasicTlsCredentials::MAXIMUM_TOKEN_UNITS` — CFG-0066 {#cfg-0066}

- Type: `static constexpr std::size_t`. Declared default: `= 1024`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Fits the qualified Feed control envelope.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `BasicTlsCredentials::provider` — CFG-0067 {#cfg-0067}

- Type: `std::shared_ptr<client::CredentialProvider>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: shared ownership of a Basic credential provider
- Accepted values: No provider is needed without credentials. When credentials exists, provider must be nonnull; the copied policy retains its shared_ptr and invokes it after verified TLS on the worker.
- Read timing: EndpointConfiguration::create validates fallback TLS before copying or I/O; receiving-profile preparation transfers data and activation creates the TLS runtime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-tls-credentials. Unsupported tlsRequirements → UNSUPPORTED_PROFILE; no TLS build → UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `BasicTlsCredentials::timeout` — CFG-0068 {#cfg-0068}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive cooperative authentication deadline after verification.
- Units: milliseconds
- Declared constraints: Positive cooperative authentication deadline after verification.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `BasicTlsCredentials::tokenUnits` — CFG-0069 {#cfg-0069}

- Type: `std::size_t`. Declared default: `= MAXIMUM_TOKEN_UNITS`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units for the complete Basic token
- Accepted values: With credentials: 7..1024 inclusive, including Basic prefix and nonempty value. Unused without credentials. It also enlarges protocol control/token budgets.
- Read timing: EndpointConfiguration::create validates fallback TLS before copying or I/O; receiving-profile preparation transfers data and activation creates the TLS runtime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-tls-credentials. Unsupported tlsRequirements → UNSUPPORTED_PROFILE; no TLS build → UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

## EndpointOptions {#cfg-0080-owner}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `EndpointOptions::MAXIMUM_BATCH_RECORDS` — CFG-0080 {#cfg-0080}

- Type: `static constexpr std::size_t`. Declared default: `= 4`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Qualified physical callback record ceiling.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `EndpointOptions::MAXIMUM_PENDING_RECORDS` — CFG-0081 {#cfg-0081}

- Type: `static constexpr std::size_t`. Declared default: `= 128`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Qualified detached record slot ceiling.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `EndpointOptions::DEFAULT_RECEIVE_MESSAGE_BYTES` — CFG-0082 {#cfg-0082}

- Type: `static constexpr std::size_t`. Declared default: `= 8192`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Standard incoming QTP body budget.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `EndpointOptions::MAXIMUM_RECEIVE_MESSAGE_BYTES` — CFG-0083 {#cfg-0083}

- Type: `static constexpr std::size_t`. Declared default: `= 65536`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Largest supported incoming QTP body budget.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `EndpointOptions::contract` — CFG-0084 {#cfg-0084}

- Type: `DeliveryContract`. Declared default: `= DeliveryContract::TICKER`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: DeliveryContract value
- Accepted values: TICKER, STREAM, HISTORY or MIXED. Other enum values reject. Wildcard requires STREAM; MIXED rejects an explicit eventRegistry and tape.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. Receiving-lane preparation selects the collector family; MIXED builds three coordinated lanes.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options; UNSUPPORTED_PROFILE / endpoint-wildcard-contract or endpoint-mixed-registry. Other coupled fields are checked before activation. A later Endpoint::connectTape on MIXED yields UNSUPPORTED_PROFILE / endpoint-mixed-tape.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `EndpointOptions::connectTimeout` — CFG-0085 {#cfg-0085}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: connection-attempt milliseconds
- Accepted values: 1..INT_MAX inclusive; zero/negative reject. Bounds the complete native connection attempt.
- Read timing: Profile preparation passes connectTimeout to transport. With a numeric address start sets connectDeadline; with a name it first starts dnsDeadline, then successful resolution sets a new connectDeadline on entering CONNECTING.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp).

### `EndpointOptions::dnsTimeout` — CFG-0086 {#cfg-0086}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: DNS lookup milliseconds
- Accepted values: 1..INT_MAX inclusive; used for the two-family lookup in the selected DNS profile.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. prepareReceivingLane copies dnsTimeout into its corresponding settings.timeouts field.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::readTimeout` — CFG-0087 {#cfg-0087}

- Type: `std::chrono::milliseconds`. Declared default: `{30000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input-inactivity milliseconds
- Accepted values: 1..INT_MAX inclusive. This positive duration is an inactivity deadline, not a total endpoint lifetime.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. prepareReceivingLane copies readTimeout into its corresponding settings.timeouts field.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::writeTimeout` — CFG-0088 {#cfg-0088}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: pending-output inactivity milliseconds
- Accepted values: 1..INT_MAX inclusive. Applies when output is pending.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. prepareReceivingLane copies writeTimeout into its corresponding settings.timeouts field.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::pollInterval` — CFG-0089 {#cfg-0089}

- Type: `std::chrono::milliseconds`. Declared default: `{1}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: worker cadence milliseconds
- Accepted values: 1..INT_MAX inclusive. Zero rejects; controls shared-worker progress cadence.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. prepareReceivingLane copies pollInterval into its corresponding settings.timeouts field.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::retryDelay` — CFG-0090 {#cfg-0090}

- Type: `std::chrono::milliseconds`. Declared default: `{250}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retry-delay milliseconds
- Accepted values: With maximumAttempts&gt;1:1..INT_MAX. With maximumAttempts=1 the original value is not checked for positivity and effective delay becomes 0; requested policy is retained separately.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. prepareReceivingLane constructs settings.retry; one attempt uses an effective zero delay.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options when retries are enabled. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::drainTimeout` — CFG-0091 {#cfg-0091}

- Type: `std::chrono::milliseconds`. Declared default: `{1000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: local admitted-prefix drain milliseconds
- Accepted values: 1..INT_MAX inclusive. Expiry escalates local drain toward abort; it does not acknowledge remote delivery.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. publishEndpointCutoff fixes admitted+drainTimeout once when close is admitted; source transitions also use this interval.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `EndpointOptions::maximumAttempts` — CFG-0092 {#cfg-0092}

- Type: `std::uint32_t`. Declared default: `= 3`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: connection attempts including the first
- Accepted values: Any representable nonzero uint32_t, with no smaller fixed ceiling. One disables retries and makes effective retryDelay=0; &gt;1 requires positive retryDelay&lt;=INT_MAX.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. prepareReceivingLane constructs settings.retry; one attempt uses an effective zero delay.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::eventRegistry` — CFG-0093 {#cfg-0093}

- Type: `std::optional<EventRegistry>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional owning EventRegistry
- Accepted values: nullopt selects the built-in registry; a present object must be valid(). MIXED rejects explicit registries. It is an immutable registry copy bound to its schema.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. selectRegistry selects an explicit valid EventRegistry or builds defaults; the captured registry drives event binding.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-registry; UNSUPPORTED_PROFILE / endpoint-mixed-registry. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::enableWildcard` — CFG-0094 {#cfg-0094}

- Type: `bool`. Declared default: `= false`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit controlled-peer STREAM All permission; never enabled by text or peer.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `EndpointOptions::requestedAggregationPeriod` — CFG-0095 {#cfg-0095}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional server-aggregation request milliseconds
- Accepted values: nullopt requests server default;0 requests no aggregation;1..86400000 inclusive sets an interval. Negative/larger values reject. Observed server interval is stored separately.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. ApiRuntime copies initial intent into EndpointPort; the setter changes its revision and the worker forwards current intent to recovery.requestAggregationPeriod.
- Mutability and lifetime: Initial policy is copied at construction; a separate setter changes current aggregation intent with a revision and range revalidation.
- Failure: api::Error INVALID_ARGUMENT / endpoint-aggregation-period. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `EndpointOptions::history` — CFG-0096 {#cfg-0096}

- Type: `HistoryReceivingOptions`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Used only by the explicit HISTORY contract; never application work credits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `EndpointOptions::unitaryOrderSources` — CFG-0097 {#cfg-0097}

- Type: `bool`. Declared default: `= false`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: SDK default/plain/negative sources use unitary bid/ask pairs when true;
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `EndpointOptions::batchRecords` — CFG-0098 {#cfg-0098}

- Type: `std::size_t`. Declared default: `= MAXIMUM_BATCH_RECORDS`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: physical records per callback selection
- Accepted values: 1..4 inclusive and &lt;=pendingRecords. This qualifies selection of whole physical records; it does not define the wire-format range.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. Profile preparation maps batchRecords to physical client-selection count, network STREAM routeRows and the QueueLimits callback quantum.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::pendingRecords` — CFG-0099 {#cfg-0099}

- Type: `std::size_t`. Declared default: `= MAXIMUM_PENDING_RECORDS`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: detached record slots per subscription
- Accepted values: 1..128 inclusive and &gt;=batchRecords. Sizes the subscription queue; transient retention/bytes are checked separately.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. Profile preparation maps pendingRecords to the client detached-record queue and aggregate clients*pendingRecords reservation; HISTORY result capacity accounts for it separately.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-options. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::aggregationPeriod` — CFG-0100 {#cfg-0100}

- Type: `std::chrono::milliseconds`. Declared default: `{0}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: local-delivery milliseconds
- Accepted values: 0..86400000 inclusive;0 selects immediate delivery. Initial EndpointOptions and later live aggregation updates are checked separately.
- Read timing: EndpointConfiguration::create validates initial policy before endpoint construction and I/O; the field consumer is identified below. Subscription creation captures immutable EndpointOptions.aggregationPeriod as its local floor. A separate subscription setter chooses an optional override; it does not mutate the endpoint field.
- Mutability and lifetime: EndpointOptions remains immutable in the constructed endpoint. Each subscription can independently set/reset its aggregationPeriod through a validated setter; that does not mutate the endpoint field.
- Failure: api::Error INVALID_ARGUMENT / endpoint-local-aggregation-period. Other coupled fields are checked before activation.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `EndpointOptions::tls` — CFG-0101 {#cfg-0101}

- Type: `std::optional<VerifiedTlsOptions>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit verified network transport. Absence selects plain TCP.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `EndpointOptions::maximumReceiveMessageBytes` — CFG-0102 {#cfg-0102}

- Type: `std::size_t`. Declared default: `= DEFAULT_RECEIVE_MESSAGE_BYTES`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Incoming QTP type and payload bytes, 8192..65536, excluding length prefix.
- Units: bytes; scope defined by field contract
- Declared constraints: Incoming QTP type and payload bytes, 8192..65536, excluding length prefix.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

## HistoryReceivingOptions {#cfg-0058-owner}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `HistoryReceivingOptions::maximumPoints` — CFG-0058 {#cfg-0058}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: shared indexed points
- Accepted values: EndpointOptions does not reject zero; points=0 reaches HistoryCollector and cannot retain a point. Upper capacity depends on checked storage formulas and quadratic prepaid work.
- Read timing: EndpointOptions captures the field; HISTORY/MIXED lane preparation maps it to HistoryLimits, checks storage/work before activation and later delivery. Other families do not use it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Before activation, overflow of corresponding derived sums or products yields api::Error RESOURCE_LIMIT / profile-arithmetic. maximumPoints affects requested collector storage; insufficient residentBytes yields HistoryError::RESOURCE_LIMIT during construction. When the queue is opened, a HistoryCollector::create failure maps to api::Error RESOURCE_LIMIT / endpoint-queue. While processing an admitted event, history-&gt;stage failure is recorded as QueueError::SOURCE_FAILURE with its original cause and invokes failAll; HistoryError::WORK_LIMIT is handled separately as QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295).
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::maximumFrames` — CFG-0059 {#cfg-0059}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retained source frames
- Accepted values: Zero is not locally rejected. Sets cacheFrames and each view pendingFrames; retaining a new owner requires available capacity. Total slots use checked cacheFrames+views*pendingFrames+1.
- Read timing: EndpointOptions captures the field; HISTORY/MIXED lane preparation maps it to HistoryLimits, checks storage/work before activation and later delivery. Other families do not use it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Before activation, overflow of corresponding derived sums or products yields api::Error RESOURCE_LIMIT / profile-arithmetic. maximumFrames affects requested collector storage; insufficient residentBytes yields HistoryError::RESOURCE_LIMIT during construction. When the queue is opened, a HistoryCollector::create failure maps to api::Error RESOURCE_LIMIT / endpoint-queue. While processing an admitted event, history-&gt;stage failure is recorded as QueueError::SOURCE_FAILURE with its original cause and invokes failAll; HistoryError::WORK_LIMIT is handled separately as QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295).
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::maximumBytes` — CFG-0060 {#cfg-0060}

- Type: `std::size_t`. Declared default: `= 64 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: peak cache source-frame bytes
- Accepted values: Zero is accepted as an empty budget. HISTORY checks maximumBytes+clients*pendingBytesPerSubscription&lt;=maximumResidentBytes; individual incoming owners must wholly fit cacheBytes.
- Read timing: EndpointOptions captures the field; HISTORY/MIXED lane preparation maps it to HistoryLimits, checks storage/work before activation and later delivery. Other families do not use it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Before activation, checked-sum or product overflow yields api::Error RESOURCE_LIMIT / profile-arithmetic; exceeding total admitted storage yields RESOURCE_LIMIT / profile-history-resident-capacity. When the queue is opened, a HistoryCollector::create failure maps to api::Error RESOURCE_LIMIT / endpoint-queue. While processing an admitted event, history-&gt;stage failure is recorded as QueueError::SOURCE_FAILURE with its original cause and invokes failAll; HistoryError::WORK_LIMIT is handled separately as QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295).
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::maximumResidentBytes` — CFG-0061 {#cfg-0061}

- Type: `std::size_t`. Declared default: `= 128 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: collector fixed-state and peak-retention bytes
- Accepted values: Zero cannot fit a real state. Must cover fixed backing plus maximumBytes+clients*pendingBytesPerSubscription; requested storage is checked before allocation and actual capacities afterward.
- Read timing: EndpointOptions captures the field; HISTORY/MIXED lane preparation maps it to HistoryLimits, checks storage/work before activation and later delivery. Other families do not use it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Before activation, checked-sum or product overflow yields api::Error RESOURCE_LIMIT / profile-arithmetic; exceeding total admitted storage yields RESOURCE_LIMIT / profile-history-resident-capacity. When the queue is opened, a HistoryCollector::create failure maps to api::Error RESOURCE_LIMIT / endpoint-queue. While processing an admitted event, history-&gt;stage failure is recorded as QueueError::SOURCE_FAILURE with its original cause and invokes failAll; HistoryError::WORK_LIMIT is handled separately as QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295).
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::pendingUpdatesPerSubscription` — CFG-0062 {#cfg-0062}

- Type: `std::size_t`. Declared default: `= 128`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: update FIFO items per view
- Accepted values: EndpointOptions allows zero, producing zero pendingItems; the first delivery needing a FIFO cannot be queued. Sums with maximumPoints/pendingRecords and products by view count are checked.
- Read timing: EndpointOptions captures the field; HISTORY/MIXED lane preparation maps it to HistoryLimits, checks storage/work before activation and later delivery. Other families do not use it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Before activation, overflow of corresponding derived sums or products yields api::Error RESOURCE_LIMIT / profile-arithmetic. pendingUpdatesPerSubscription affects requested collector storage; insufficient residentBytes yields HistoryError::RESOURCE_LIMIT during construction. When the queue is opened, a HistoryCollector::create failure maps to api::Error RESOURCE_LIMIT / endpoint-queue. While processing an admitted event, history-&gt;stage failure is recorded as QueueError::SOURCE_FAILURE with its original cause and invokes failAll; HistoryError::WORK_LIMIT is handled separately as QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295). Overflow of a particular view FIFO records a local HistoryViewFailure with RESOURCE_LIMIT and the failing record ordinal.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::maximumObservationBytes` — CFG-0063 {#cfg-0063}

- Type: `std::size_t`. Declared default: `= 8 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: accounted bytes for two observation-publication banks
- Accepted values: For HISTORY publicationBytes=2*(sizeof(snapshot)+subscriptionEntries*rowBytes) must be &lt;=maximumObservationBytes. Zero cannot cover even the header; minimum depends on entry count and symbol length.
- Read timing: EndpointOptions captures the field; HISTORY/MIXED lane preparation maps it to HistoryLimits, checks storage/work before activation and later delivery. Other families do not use it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error RESOURCE_LIMIT / profile-history-observation-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::pendingBytesPerSubscription` — CFG-0064 {#cfg-0064}

- Type: `std::size_t`. Declared default: `= 4 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: peak whole-source-owner bytes for one view FIFO
- Accepted values: Zero supplies an empty pendingBytes allowance. A new owner must fit wholly; pendingItems and pendingFrames independently constrain the FIFO. The grant is multiplied by clients and included in maximumResidentBytes.
- Read timing: EndpointOptions captures the field; HISTORY/MIXED lane preparation maps it to HistoryLimits, checks storage/work before activation and later delivery. Other families do not use it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Before activation, checked-sum or product overflow yields api::Error RESOURCE_LIMIT / profile-arithmetic; exceeding total admitted storage yields RESOURCE_LIMIT / profile-history-resident-capacity. When the queue is opened, a HistoryCollector::create failure maps to api::Error RESOURCE_LIMIT / endpoint-queue. While processing an admitted event, history-&gt;stage failure is recorded as QueueError::SOURCE_FAILURE with its original cause and invokes failAll; HistoryError::WORK_LIMIT is handled separately as QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295). Overflow of a particular view FIFO records a local HistoryViewFailure with RESOURCE_LIMIT and the failing record ordinal.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

## HistorySnapshotOptions {#cfg-0109-owner}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `HistorySnapshotOptions::maximumResults` — CFG-0109 {#cfg-0109}

- Type: `std::size_t`. Declared default: `= 128`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: result events per snapshot request
- Accepted values: Strictly&gt;0. Ordinary indexed snapshots pass this cap to HistoryRetainedQuery; the collector counts rows and rejects before allocating its result. Time-series paths cap raw rows by history.maximumPoints, then check filtered business results against maximumResults. Neither this count nor order-book depth sets an overall prior-memory/work budget.
- Read timing: Checked during snapshot-request preparation; captured options apply when projecting results.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: Zero: api::Error INVALID_ARGUMENT / indexed-snapshot-options. An ordinary indexed query passes maximumResults into HistoryRetainedQuery: excess row count yields HistoryError::RESOURCE_LIMIT before reserve, then QueueError::RESOURCE_LIMIT → RecoveryDiagnostic through retainedHistoryRange/queueFailure → api::Error RESOURCE_LIMIT / recovery-queue through recoveryDiagnostic. A time-series query first obtains raw rows bounded by history.maximumPoints; after filtering, selected&gt;maximumResults yields the separate RESOURCE_LIMIT / time-series-result-limit.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/transport/recovery_core.cpp](../../src/transport/recovery_core.cpp).

## RuntimeOptions {#cfg-0042-owner}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `RuntimeOptions::maximumEndpoints` — CFG-0042 {#cfg-0042}

- Type: `std::size_t`. Declared default: `= 4`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: endpoints per ApiRuntime
- Accepted values: &gt;0. Combined with maximumSubscriptionsPerEndpoint, product, observer leases and table size are checked. The upper bound depends on other fields and sizeof; a free endpoint slot must exist.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. RuntimeImpl sizes the endpoint table; ApiRuntime::createEndpoint finds a free table entry.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Unrepresentable coupled storage: RESOURCE_LIMIT / runtime-change-storage / runtime-change-leases. Operation admission refusal: RESOURCE_LIMIT / runtime-endpoint-count. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumSubscriptionsPerEndpoint` — CFG-0043 {#cfg-0043}

- Type: `std::size_t`. Declared default: `= 8`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: subscriptions per endpoint
- Accepted values: &gt;0. Sizes subscription/request tables and participates in the checked product with maximumEndpoints. Active/selected/retiring subscriptions may retain their slot until released.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. Endpoint creation sizes its subscription table; registration needs a free entry including retiring subscriptions.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Unrepresentable coupled storage: RESOURCE_LIMIT / runtime-change-storage / runtime-change-leases. Operation admission refusal: RESOURCE_LIMIT / endpoint-subscription-count. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumSymbolsPerSubscription` — CFG-0044 {#cfg-0044}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: distinct symbols/selectors per subscription
- Accepted values: &gt;0. Upper bound is max_size/(selectorBytes*selectorBanks); selectorBytes depends on maximumSymbolUnits and banks depend on pending changes/listeners. Distinct-symbol additions and some input lists are checked separately.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. Subscription creation copies the value to SubscriptionPort::maximumSymbols; mutateDesired and owning-selector staging check the actual set.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Unrepresentable coupled storage: RESOURCE_LIMIT / runtime-selector-storage. Operation admission refusal: RESOURCE_LIMIT / subscription-symbol-count / subscription-selector-arguments. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumListenersPerSubscription` — CFG-0045 {#cfg-0045}

- Type: `std::size_t`. Declared default: `= 4`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retained data listeners per subscription
- Accepted values: &gt;0. Occupied reservations include selected and retiring captures; removing a listener from the list does not immediately release its reservation. No separate constant maximum applies.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. Subscription creation sets ListenerLedger::maximum; listener registration reserves a ledger slot.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Operation admission refusal: RESOURCE_LIMIT / subscription-listener-count. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumSymbolUnits` — CFG-0046 {#cfg-0046}

- Type: `std::size_t`. Declared default: `= 128`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per symbol
- Accepted values: 1..128 inclusive. Used before taking ownership during selector parsing/validation; also affects selectorBytes and bank-size accounting. 128 is a qualified high-level API ceiling.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. Subscription creation copies maximumSymbolUnits; validateSelector checks length and ASCII grammar before symbol admission.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for values outside 1..128; RESOURCE_LIMIT / runtime-selector-storage for unrepresentable coupled storage. Symbols exceeding the admitted maximumSymbolUnits or printable ASCII grammar reject with INVALID_ARGUMENT / subscription-symbol.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::dnsServers` — CFG-0047 {#cfg-0047}

- Type: `std::vector<transport::DnsServer>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit IPv4 resolvers; empty discovers a bounded system snapshot.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: ApiRuntime construction; owned registry/worker settings
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `RuntimeOptions::dnsPort` — CFG-0048 {#cfg-0048}

- Type: `std::uint16_t`. Declared default: `= 53`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Shared UDP/TCP resolver port, in 1..65535.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Shared UDP/TCP resolver port, in 1..65535.
- Read timing: ApiRuntime construction; owned registry/worker settings
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `RuntimeOptions::maximumStateListenersPerEndpoint` — CFG-0049 {#cfg-0049}

- Type: `std::size_t`. Declared default: `= 4`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retained endpoint state listeners
- Accepted values: &gt;0. Sets the endpoint and OnDemand ledgers; registration requires a free reservation and token identity is finite. No separate numeric ceiling applies. EndpointState and OnDemand listeners use separate ledgers with this maximum; this field does not establish one combined counter across both kinds.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. makeStateListeners creates separate EndpointState and OnDemand ledgers; each listener kind uses its own maximum.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Operation admission refusal: RESOURCE_LIMIT / state-change-listener-count. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumStateListenersPerSubscription` — CFG-0050 {#cfg-0050}

- Type: `std::size_t`. Declared default: `= 4`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retained subscription state listeners
- Accepted values: &gt;0. Captured by each subscription ledger; selected/retiring listeners remain charged. No separate numeric ceiling applies.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. Subscription creation passes this maximum to makeStateListeners; addStateListener reserves selected and retiring observations.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Operation admission refusal: RESOURCE_LIMIT / state-change-listener-count. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumEventTypeArgumentsPerSubscription` — CFG-0051 {#cfg-0051}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: event-type arguments before deduplication
- Accepted values: &gt;0. types.size() is checked before content analysis/sorting; an empty list separately rejects. Duplicate input arguments count even if fewer distinct types remain.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. Feed::createSubscriptionPort checks the raw types list before inspecting/deduplicating it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Operation admission refusal: RESOURCE_LIMIT / subscription-event-type-arguments. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumPendingStateChangesPerEndpoint` — CFG-0052 {#cfg-0052}

- Type: `std::size_t`. Declared default: `= 8`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: pending endpoint state transitions
- Accepted values: &gt;0. Sizes stateChanges and the dispatchStateChanges quantum; accepting new control requires notification space. Upper capacity depends on memory/container limits.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. Endpoint creation sizes stateChanges; control operations reserve notifications before admitting a transition.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Operation admission refusal: RESOURCE_LIMIT / endpoint-state-notification-count. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumChangeListenersPerSubscription` — CFG-0053 {#cfg-0053}

- Type: `std::size_t`. Declared default: `= 4`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: subscription-change listeners
- Accepted values: &gt;0. Participates in phase storage, lease and selector-bank checks. Each selected/retiring listener retains its reservation; registration needs free capacity.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. initializeSubscriptionObservers copies this cap into its change-observer ListenerLedger.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Unrepresentable coupled storage: RESOURCE_LIMIT / runtime-change-storage / runtime-change-leases / runtime-change-snapshots. Operation admission refusal: RESOURCE_LIMIT / subscription-change-listeners. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/subscription_observer.cpp](../../src/api/subscription_observer.cpp).

### `RuntimeOptions::maximumPendingSubscriptionChanges` — CFG-0054 {#cfg-0054}

- Type: `std::size_t`. Declared default: `= 8`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: pending subscription-change phases
- Accepted values: &gt;0. Checks phase-array storage and the sum with change listeners and two preparing frames, then three selector banks per frame. A phase requires a free slot before accepting a change.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. initializeSubscriptionObservers sizes phases; staging a change needs a free phase before publishing symbols.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for zero. Unrepresentable coupled storage: RESOURCE_LIMIT / runtime-change-storage / runtime-change-snapshots. Operation admission refusal: RESOURCE_LIMIT / subscription-change-pending. producerAction maps bad_alloc to ALLOCATION_FAILURE / api-allocation and length_error to RESOURCE_LIMIT / api-capacity.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/subscription_observer.cpp](../../src/api/subscription_observer.cpp).

### `RuntimeOptions::maximumSubscriptionChangesPerTurn` — CFG-0055 {#cfg-0055}

- Type: `std::size_t`. Declared default: `= 32`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: shared notification credits per callback turn
- Accepted values: &gt;0. Checks sizeof(SubscriptionContinuation) and lease-count compatibility; RuntimeImpl sizes continuations and sets maximumWork. This is a progress quantum, not a lifetime subscription count.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. RuntimeImpl sizes continuations and sets ChangeDomain::maximumWork; an external admission cycle resets remainingWork to this value.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / runtime-options for 0. Continuation sizing may yield RESOURCE_LIMIT / runtime-change-storage or runtime-change-leases. Exhausted callback-turn credits yield WORK_LIMIT / subscription-change-turn or subscription-change-continuations; a new external admission begins another cycle under domain rules.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/subscription_observer.cpp](../../src/api/subscription_observer.cpp).

### `RuntimeOptions::maximumCustomCallbackSubscriptions` — CFG-0056 {#cfg-0056}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneous custom callback packets
- Accepted values: Zero is accepted at ApiRuntime creation and prevents custom callback admission: first opt-in computes min(maximumCustomCallbackSubscriptions,(maximumCallbackBytes-storage)/PACKET_BYTES), which must be&gt;0. The effective cap is coupled to bytes.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. Only first custom-executor selection calls callbackPool, which computes available packet count. General construction does not validate this field positivity.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: ApiRuntime construction does not reject this number. First custom executor use can yield api::Error RESOURCE_LIMIT / callback-pool-budget or callback-packet-budget; table-size overflow separately uses callback-pool-bytes. Without this capability, the field causes no own failure.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/callback_execution.cpp](../../src/api/callback_execution.cpp).

### `RuntimeOptions::maximumCallbackBytes` — CFG-0057 {#cfg-0057}

- Type: `std::size_t`. Declared default: `= 32 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical bytes for shared callback pool and packets
- Accepted values: Zero is accepted at ApiRuntime creation; first custom opt-in requires budget&gt;storageBytes(endpointSlots) and a remainder &gt;= PACKET_BYTES. Packet count is min(subscription cap,remainder/PACKET_BYTES). Application captures are excluded.
- Read timing: ApiRuntime checks applicable initial conditions in prepareRuntimeOptions. On first custom-executor use, callbackPool subtracts fixed table bytes from maximumCallbackBytes and divides the remainder by PACKET_BYTES.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: ApiRuntime construction does not reject this number. First custom executor use can yield api::Error RESOURCE_LIMIT / callback-pool-budget or callback-packet-budget; table-size overflow separately uses callback-pool-bytes. Without this capability, the field causes no own failure.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/callback_execution.cpp](../../src/api/callback_execution.cpp).

## TapeSourceOptions {#cfg-0103-owner}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `TapeSourceOptions::maximumFileBytes` — CFG-0103 {#cfg-0103}

- Type: `std::size_t`. Declared default: `= 64 * 1024 * 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: physical file input bytes
- Accepted values: &gt;0. file_size&lt;=maximumFileBytes is checked before stream reading; the same limit reaches the tape reader and later accounting. Oversized files reject; no separate fixed maximum applies.
- Read timing: Endpoint::connectTape validates before accepting the command. The worker creates TapeFileSource; reader/clock later consume captured values.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error RESOURCE_LIMIT / tape-file-size. Zero file/frame limits and invalid policy → INVALID_ARGUMENT / tape-source-options.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp).

### `TapeSourceOptions::maximumFrames` — CFG-0104 {#cfg-0104}

- Type: `std::size_t`. Declared default: `= 65'536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: tape envelopes including markers and controls
- Accepted values: &gt;0. maximumFrames feeds several reader counters and multiplies DATA/snapshot allowances; each multiplication is checked. Admissibility depends on these products, not only the field type.
- Read timing: Endpoint::connectTape validates before accepting the command. The worker creates TapeFileSource; reader/clock later consume captured values.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Zero or invalid policy: api::Error INVALID_ARGUMENT / tape-source-options. Derived-budget overflow: RESOURCE_LIMIT / tape-limit-product. On the next nonempty input after maximumFrames, TapeReader enters FAILED with TapeError::RESOURCE_LIMIT / frames; TapeFileSource returns api::Diagnostic RESOURCE_LIMIT preserving stage frames.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp).

### `TapeSourceOptions::playback` — CFG-0105 {#cfg-0105}

- Type: `TapePlaybackPolicy`. Declared default: `= TapePlaybackPolicy::IMMEDIATE`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: playback policy
- Accepted values: Only IMMEDIATE or SOURCE_TIME. IMMEDIATE neither validates nor uses speedPercent/maximumSourceSpan; SOURCE_TIME requires valid speed and span.
- Read timing: Endpoint::connectTape validates before accepting the command. The worker creates TapeFileSource; reader/clock later consume captured values.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / tape-source-options. Zero file/frame limits and invalid policy → INVALID_ARGUMENT / tape-source-options.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_playback_clock.hpp](../../src/api/tape_playback_clock.hpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp).

### `TapeSourceOptions::speedPercent` — CFG-0106 {#cfg-0106}

- Type: `std::uint32_t`. Declared default: `= 100`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: percent of normal speed
- Accepted values: For SOURCE_TIME:1..10000 inclusive. Zero rejects. IMMEDIATE does not read the field in validTapePlayback and it does not affect scheduling. Delay rounds up using accumulated high-water time.
- Read timing: Endpoint::connectTape validates before accepting the command. The worker creates TapeFileSource; reader/clock later consume captured values.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / tape-source-options; deadline representability failure → TAPE_TIMING_RANGE / tape-playback-time. Zero file/frame limits and invalid policy → INVALID_ARGUMENT / tape-source-options.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_playback_clock.hpp](../../src/api/tape_playback_clock.hpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp).

### `TapeSourceOptions::maximumSourceSpan` — CFG-0107 {#cfg-0107}

- Type: `std::chrono::milliseconds`. Declared default: `{std::chrono::hours{24}}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds of source-time span from the first timestamp
- Accepted values: For SOURCE_TIME:&gt;0 and &lt;=365 days. Positive elapsed from the first timestamp is checked; backward timestamps cannot reduce high-water. An absent timestamp creates no timing origin. IMMEDIATE imposes no field bound.
- Read timing: Endpoint::connectTape validates before accepting the command. The worker creates TapeFileSource; reader/clock later consume captured values.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / tape-source-options; span overflow → TAPE_TIMING_RANGE / tape-playback-time. Zero file/frame limits and invalid policy → INVALID_ARGUMENT / tape-source-options.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_playback_clock.hpp](../../src/api/tape_playback_clock.hpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp).

### `TapeSourceOptions::initiallyPaused` — CFG-0108 {#cfg-0108}

- Type: `bool`. Declared default: `= false`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Open the file and wait for resume before reading the first envelope.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: explicit tape-source start; playback controls have separate methods
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

## VerifiedTlsOptions {#cfg-0070-owner}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `VerifiedTlsOptions::referenceIdentity` — CFG-0070 {#cfg-0070}

- Type: `std::string`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: ASCII DNS/IP identity bytes
- Accepted values: With TLS, validEndpointHost requires a nonempty validated DNS/IP identity&lt;=254 bytes, with DNS labels&lt;=63 bytes. Length must additionally be &lt;= tls.limits.identityBytes; the connection route is independent. A build without TLS rejects every tls option.
- Read timing: EndpointConfiguration::create validates fallback TLS before copying or I/O; receiving-profile preparation transfers data and activation creates the TLS runtime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-tls-options or RESOURCE_LIMIT / endpoint-tls-allowance. Unsupported tlsRequirements → UNSUPPORTED_PROFILE; no TLS build → UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/client/endpoint_host.hpp](../../src/client/endpoint_host.hpp).

### `VerifiedTlsOptions::trustPem` — CFG-0071 {#cfg-0071}

- Type: `std::string`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: explicit PEM trust bytes
- Accepted values: With TLS, text is nonempty, &lt;=512*1024 bytes and &lt;=tls.limits.trustBytes. Lower TLS validates certificate syntax/suitability. Automatic system-root lookup is not configured.
- Read timing: EndpointConfiguration::create validates fallback TLS before copying or I/O; receiving-profile preparation transfers data and activation creates the TLS runtime.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / endpoint-tls-options or RESOURCE_LIMIT / endpoint-tls-allowance. Unsupported tlsRequirements → UNSUPPORTED_PROFILE; no TLS build → UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `VerifiedTlsOptions::handshakeTimeout` — CFG-0072 {#cfg-0072}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive verified-handshake deadline.
- Units: milliseconds
- Declared constraints: Positive verified-handshake deadline.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `VerifiedTlsOptions::closeTimeout` — CFG-0073 {#cfg-0073}

- Type: `std::chrono::milliseconds`. Declared default: `{1000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive TLS close phase deadline.
- Units: milliseconds
- Declared constraints: Positive TLS close phase deadline.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `VerifiedTlsOptions::limits` — CFG-0074 {#cfg-0074}

- Type: `transport::TlsResourceLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Input and verification ceilings of the qualified profile.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `VerifiedTlsOptions::host` — CFG-0075 {#cfg-0075}

- Type: `transport::TlsHostLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Finite same-image limits, including external users and failed prefixes.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

### `VerifiedTlsOptions::sdkInstanceBytes` — CFG-0076 {#cfg-0076}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: private per-instance TLS SDK backing bytes
- Accepted values: Must be &gt;=tlsRequirements(limits,host).sdk.instanceBytes. Default zero is an unfilled grant, not an automatic calculation. Lower TLS rechecks profile and resource admission. Passing these initial conditions does not establish sufficiency for a concrete attempt: its total reserve includes adapter, settings snapshot, TCP and both SDK allowances.
- Read timing: EndpointConfiguration::create checks initial minima before copying or I/O. On activation feed passes all three allowances into TlsOptions. TlsRecovery::prepare constructs the runtime of each concrete attempt; TlsRuntimeFactory::create rechecks minima and calculates complete backing.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Configuration admission: api::Error RESOURCE_LIMIT / endpoint-tls-allowance. Activating a concrete TLS attempt: RESOURCE_LIMIT / tls-sdk-allowance for SDK minima; RESOURCE_LIMIT / tls-resident-arithmetic for component-construction overflow; RESOURCE_LIMIT / tls-resident-allowance for total-sum overflow or exceeding residentBytes. Stages pass through TlsRecovery/common → recoveryDiagnostic. Unsupported tlsRequirements yields UNSUPPORTED_PROFILE; a build without TLS yields UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/transport/tls_recovery.cpp](../../src/transport/tls_recovery.cpp), [src/transport/tls_runtime.cpp](../../src/transport/tls_runtime.cpp).

### `VerifiedTlsOptions::sdkSharedBytes` — CFG-0077 {#cfg-0077}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: shared same-image TLS SDK backing bytes
- Accepted values: Must be &gt;=tlsRequirements(limits,host).sdk.sharedBytes. Lower layers also account for shared users/host profile; a nonzero grant alone does not establish sufficiency. Passing these initial conditions does not establish sufficiency for a concrete attempt: its total reserve includes adapter, settings snapshot, TCP and both SDK allowances.
- Read timing: EndpointConfiguration::create checks initial minima before copying or I/O. On activation feed passes all three allowances into TlsOptions. TlsRecovery::prepare constructs the runtime of each concrete attempt; TlsRuntimeFactory::create rechecks minima and calculates complete backing.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Configuration admission: api::Error RESOURCE_LIMIT / endpoint-tls-allowance. Activating a concrete TLS attempt: RESOURCE_LIMIT / tls-sdk-allowance for SDK minima; RESOURCE_LIMIT / tls-resident-arithmetic for component-construction overflow; RESOURCE_LIMIT / tls-resident-allowance for total-sum overflow or exceeding residentBytes. Stages pass through TlsRecovery/common → recoveryDiagnostic. Unsupported tlsRequirements yields UNSUPPORTED_PROFILE; a build without TLS yields UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/transport/tls_recovery.cpp](../../src/transport/tls_recovery.cpp), [src/transport/tls_runtime.cpp](../../src/transport/tls_runtime.cpp).

### `VerifiedTlsOptions::residentBytes` — CFG-0078 {#cfg-0078}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: whole-run logical TLS resident allowance bytes
- Accepted values: High-level validation requires &gt;0. TLS runtime checks sufficiency against SDK, environment and retained buffers on activation. No universal fixed minimum applies. Passing these initial conditions does not establish sufficiency for a concrete attempt: its total reserve includes adapter, settings snapshot, TCP and both SDK allowances.
- Read timing: EndpointConfiguration::create checks initial minima before copying or I/O. On activation feed passes all three allowances into TlsOptions. TlsRecovery::prepare constructs the runtime of each concrete attempt; TlsRuntimeFactory::create rechecks minima and calculates complete backing.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Configuration admission: api::Error RESOURCE_LIMIT / endpoint-tls-allowance. Activating a concrete TLS attempt: RESOURCE_LIMIT / tls-sdk-allowance for SDK minima; RESOURCE_LIMIT / tls-resident-arithmetic for component-construction overflow; RESOURCE_LIMIT / tls-resident-allowance for total-sum overflow or exceeding residentBytes. Stages pass through TlsRecovery/common → recoveryDiagnostic. Unsupported tlsRequirements yields UNSUPPORTED_PROFILE; a build without TLS yields UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Consumer sources: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/transport/tls_recovery.cpp](../../src/transport/tls_recovery.cpp), [src/transport/tls_runtime.cpp](../../src/transport/tls_runtime.cpp).

### `VerifiedTlsOptions::credentials` — CFG-0079 {#cfg-0079}

- Type: `std::optional<BasicTlsCredentials>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Opt-in Basic provider; called on the serialized worker.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: endpoint creation; worker-owned applied policy; separate explicit setters where declared
- Mutability and lifetime: input aggregate mutation does not reconfigure owner; use only separately documented live setters
- Failure: api::ErrorCode and Diagnostic; exact field-to-error branch not individually revalidated
- Additional owner vocabulary (not a field-to-error mapping): `ErrorCode, WaitStatus`.

## IndependentSubscriptionLimits {#cfg-0110-owner}

[include/qd/api/independent_subscription.hpp](../../include/qd/api/independent_subscription.hpp)

### `IndependentSubscriptionLimits::maximumAttachments` — CFG-0110 {#cfg-0110}

- Type: `std::size_t`. Declared default: `= 4`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: live and retiring attachment generations
- Accepted values: &gt;0 and &lt;=attachments_.max_size(). Reattaching the same Feed returns its existing ID; expired gates are removed before checking. Active/callback-retiring generations count; IDs are separately finite.
- Read timing: IndependentSubscription constructor validates limits; attach/setSelectors apply captured values under the mutex.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / independent-subscription-options for 0. Exceeding the corresponding container max_size: RESOURCE_LIMIT / independent-subscription-options. Later admission: RESOURCE_LIMIT / independent-subscription-attachments. std::bad_alloc from reserve/object construction can propagate separately.
- Consumer sources: [include/qd/api/independent_subscription.hpp](../../include/qd/api/independent_subscription.hpp).

### `IndependentSubscriptionLimits::maximumSelectors` — CFG-0111 {#cfg-0111}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input arguments and retained selectors
- Accepted values: &gt;0 and &lt;=selectors_.max_size(). setSelectors checks raw count before deduplication; final symbols must also fit each attached Feed limit.
- Read timing: IndependentSubscription constructor validates limits; attach/setSelectors apply captured values under the mutex.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / independent-subscription-options for 0. Exceeding the corresponding container max_size: RESOURCE_LIMIT / independent-subscription-options. Later admission: RESOURCE_LIMIT / independent-subscription-selector-arguments. std::bad_alloc from reserve/object construction can propagate separately.
- Consumer sources: [include/qd/api/independent_subscription.hpp](../../include/qd/api/independent_subscription.hpp).

### `IndependentSubscriptionLimits::maximumSymbolUnits` — CFG-0112 {#cfg-0112}

- Type: `std::size_t`. Declared default: `= 128`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per selector symbol
- Accepted values: &gt;0. Each argument requires eventSymbol().size()&lt;=maximumSymbolUnits before retention; there is no local 128 ceiling here. An attached Feed additionally applies its own maximumSymbolUnits/grammar.
- Read timing: IndependentSubscription constructor validates limits; attach/setSelectors apply captured values under the mutex.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: api::Error INVALID_ARGUMENT / independent-subscription-options for 0. Later admission: RESOURCE_LIMIT / independent-subscription-symbol-units. std::bad_alloc from reserve/object construction can propagate separately.
- Consumer sources: [include/qd/api/independent_subscription.hpp](../../include/qd/api/independent_subscription.hpp).

## IndexedEventModelLimits {#cfg-0113-owner}

[include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp)

### `IndexedEventModelLimits::partitions` — CFG-0113 {#cfg-0113}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: symbol/source partitions
- Accepted values: Constructor permits zero. A new nonempty partition requires partitions_.size()&lt;partitions; an empty result may remove a partition without a new slot. No separate upper ceiling applies.
- Read timing: IndexedEventModel only stores limits in its constructor; processEventsChecked checks input before copying and while staging a complete candidate.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected with IndexedEventModelError::RESOURCE_LIMIT on excess or length_error; ALLOCATION_FAILURE for bad_alloc. Event-key mismatch separately yields WRONG_KEY; refusal preserves the prior model.
- Consumer sources: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

### `IndexedEventModelLimits::symbolUnits` — CFG-0114 {#cfg-0114}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per symbol key
- Accepted values: Zero allows symbol.size()==0 in this check; a nonempty key rejects. Each event is separately checked against symbol/source.
- Read timing: IndexedEventModel only stores limits in its constructor; processEventsChecked checks input before copying and while staging a complete candidate.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected with IndexedEventModelError::RESOURCE_LIMIT on excess or length_error; ALLOCATION_FAILURE for bad_alloc. Event-key mismatch separately yields WRONG_KEY; refusal preserves the prior model.
- Consumer sources: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

### `IndexedEventModelLimits::inputEvents` — CFG-0115 {#cfg-0115}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input events per call
- Accepted values: input.size()&lt;=inputEvents;0 permits an empty call returning empty Changes. Nonempty calls also depend on global pendingEvents/retainedEvents.
- Read timing: IndexedEventModel only stores limits in its constructor; processEventsChecked checks input before copying and while staging a complete candidate.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected with IndexedEventModelError::RESOURCE_LIMIT on excess or length_error; ALLOCATION_FAILURE for bad_alloc. Event-key mismatch separately yields WRONG_KEY; refusal preserves the prior model.
- Consumer sources: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

### `IndexedEventModelLimits::pendingEvents` — CFG-0116 {#cfg-0116}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: pending incomplete transaction/snapshot events across partitions
- Accepted values: Constructor accepts zero. Current partition allowance equals the limit minus other partitions pending events; an unfinished event needs a free slot. A completing event may be appended transiently before atomic application/clear.
- Read timing: IndexedEventModel only stores limits in its constructor; processEventsChecked checks input before copying and while staging a complete candidate.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected with IndexedEventModelError::RESOURCE_LIMIT on excess or length_error; ALLOCATION_FAILURE for bad_alloc. Event-key mismatch separately yields WRONG_KEY; refusal preserves the prior model.
- Consumer sources: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

### `IndexedEventModelLimits::retainedEvents` — CFG-0117 {#cfg-0117}

- Type: `std::size_t`. Declared default: `= 8192`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retained active events across partitions
- Accepted values: Constructor accepts zero and permits a result with no active events. Current partition capacity subtracts active events in other partitions; after a transaction completes next.active.size() must fit.
- Read timing: IndexedEventModel only stores limits in its constructor; processEventsChecked checks input before copying and while staging a complete candidate.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected with IndexedEventModelError::RESOURCE_LIMIT on excess or length_error; ALLOCATION_FAILURE for bad_alloc. Event-key mismatch separately yields WRONG_KEY; refusal preserves the prior model.
- Consumer sources: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

## OrderBookCorrectionOptions {#cfg-0122-owner}

[include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp)

### `OrderBookCorrectionOptions::policy` — CFG-0122 {#cfg-0122}

- Type: `OrderBookCorrectionPolicy`. Declared default: `= OrderBookCorrectionPolicy::DISABLED`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate-order correction policy
- Accepted values: DISABLED or SAME_SYMBOL; other values reject in the constructor. DISABLED bypasses correction; SAME_SYMBOL compares eligible orders across sources of the exact symbol.
- Read timing: OrderBookModel constructor captures corrections; correct consumes them while staging completed changes.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::invalid_argument("Invalid order-book correction options") for unknown policy/negative ages. entries exhaustion later returns IndexedEventModelError::RESOURCE_LIMIT.
- Consumer sources: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookCorrectionOptions::keepAge` — CFG-0123 {#cfg-0123}

- Type: `std::chrono::milliseconds`. Declared default: `= std::chrono::hours(24)`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: business-age milliseconds for unrelated orders
- Accepted values: keepAge&gt;=0;0 is allowed. Age uses order business time without sampling system time; no smaller ceiling than representable chrono::milliseconds is imposed.
- Read timing: OrderBookModel constructor captures corrections; correct consumes them while staging completed changes.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::invalid_argument("Invalid order-book correction options") for unknown policy/negative ages. entries exhaustion later returns IndexedEventModelError::RESOURCE_LIMIT.
- Consumer sources: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookCorrectionOptions::flipAge` — CFG-0124 {#cfg-0124}

- Type: `std::chrono::milliseconds`. Declared default: `= std::chrono::minutes(1)`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: business-age milliseconds for crossing sides
- Accepted values: flipAge&gt;=0;0 is allowed. Used for the crossed condition independently of keepAge; no flipAge&lt;=keepAge relation is checked.
- Read timing: OrderBookModel constructor captures corrections; correct consumes them while staging completed changes.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::invalid_argument("Invalid order-book correction options") for unknown policy/negative ages. entries exhaustion later returns IndexedEventModelError::RESOURCE_LIMIT.
- Consumer sources: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookCorrectionOptions::entries` — CFG-0125 {#cfg-0125}

- Type: `std::size_t`. Declared default: `= 8192`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate live and suppressed correction stamps
- Accepted values: Constructor permits zero. With correction enabled, current-symbol capacity equals entries minus stamps for other symbols; a new stamp needs a free slot. DISABLED consumes none.
- Read timing: OrderBookModel retains entries in correctionOptions_. When correction is enabled, applyEvents subtracts other-symbol stamps from total capacity and passes the remainder to stageCorrections.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: stageCorrections returns IndexedEventModelError::RESOURCE_LIMIT when a new stamp needs an exhausted capacity. Corrections are staged in draft with processEventsChecked; refusal publishes neither correction nor indexed-model candidates. std::length_error maps to RESOURCE_LIMIT and std::bad_alloc to ALLOCATION_FAILURE.
- Consumer sources: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

## OrderBookViewOptions {#cfg-0118-owner}

[include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp)

### `OrderBookViewOptions::filter` — CFG-0118 {#cfg-0118}

- Type: `OrderBookModelFilter`. Declared default: `= OrderBookModelFilter::ALL`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: scope-filter value
- Accepted values: Only OrderBookModelFilter values recognized by scopeMask: UNFILTERED, COMPOSITE, REGIONAL, AGGREGATE, ORDER, COMPOSITE_REGIONAL, COMPOSITE_REGIONAL_AGGREGATE, ALL. Other enum values yield INVALID_OPTIONS.
- Read timing: buildSnapshot reads supplied options while constructing one book view. These options do not change retained orders.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected: IndexedEventModelError::INVALID_OPTIONS for invalid filter/lotSize; RESOURCE_LIMIT for exhausted capacity/length_error; ALLOCATION_FAILURE for bad_alloc. sourceId/depth have no independent range rejection.
- Consumer sources: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookViewOptions::sourceId` — CFG-0119 {#cfg-0119}

- Type: `std::optional<std::int32_t>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional signed source ID
- Accepted values: nullopt selects all sources; any int32_t selects that exact partition. Sign is not independently validated here; absent partitions follow the model empty-observation rules.
- Read timing: buildSnapshot reads supplied options while constructing one book view. These options do not change retained orders.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected: IndexedEventModelError::INVALID_OPTIONS for invalid filter/lotSize; RESOURCE_LIMIT for exhausted capacity/length_error; ALLOCATION_FAILURE for bad_alloc. sourceId/depth have no independent range rejection.
- Consumer sources: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookViewOptions::depth` — CFG-0120 {#cfg-0120}

- Type: `std::optional<std::size_t>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: entries per book side
- Accepted values: nullopt retains all eligible orders;0 truncates both sides to empty; positive values truncate each already-sorted side independently. No additional maximum applies; depth does not bound prior copying/sorting.
- Read timing: buildSnapshot reads supplied options while constructing one book view. These options do not change retained orders.
- Mutability and lifetime: Each call supplies its options; a later call may pass another initialized set. Existing results remain unchanged.
- Failure: std::expected: IndexedEventModelError::INVALID_OPTIONS for invalid filter/lotSize; RESOURCE_LIMIT for exhausted capacity/length_error; ALLOCATION_FAILURE for bad_alloc. sourceId/depth have no independent range rejection.
- Consumer sources: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookViewOptions::lotSize` — CFG-0121 {#cfg-0121}

- Type: `std::int32_t`. Declared default: `= 1`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive multiplier for nonindividual sizes.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive multiplier for nonindividual sizes.
- Read timing: Explicit argument of public operations: [[nodiscard]] std::expected&lt;OrderBookSnapshot, IndexedEventModelError&gt; buildSnapshot(std::u16string_view symbol, const OrderBookViewOptions &amp;options) const / [[nodiscard]] std::expected&lt;OrderBookSnapshot, IndexedEventModelError&gt; getFilteredSnapshot(std::u16string_view symbol, const OrderBookViewOptions &amp;options) const
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp); the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherCatalogOptions {#cfg-0129-owner}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherCatalogOptions::schema` — CFG-0129 {#cfg-0129}

- Type: `const core::LocalSchema&`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: explicit dependency or data input; lifetime must be established by its owner. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Validated binary record catalog.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherCatalogOptions::wireFieldNames` — CFG-0130 {#cfg-0130}

- Type: `std::span<const std::vector<std::u16string>>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: explicit dependency or data input; lifetime must be established by its owner. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Record/field order, exact QTP names.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherDemandPolicy {#cfg-0131-owner}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherDemandPolicy::ticker` — CFG-0131 {#cfg-0131}

- Type: `std::optional<FeedFilter>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional TICKER demand filter
- Accepted values: nullopt imposes no filter for this family. A present FeedFilter must be valid; initial policies reject dynamic filters. The update API permits dynamic and applies snapshots under source policy rules.
- Read timing: Publisher creation validates source and configuredDemandPolicies; the server retains address/policy copies. Filters are consumed during peer-demand admission.
- Mutability and lifetime: Initial policy is copied; an explicit update method admits a new policy and subsequent dynamic-filter snapshots.
- Failure: PublisherEndpoint::create returns std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; internal api::Error becomes Diagnostic: INVALID_ARGUMENT / publisher-demand-filter. Configuration errors occur before server work. std::bad_alloc maps to ALLOCATION_FAILURE / publisher-storage.
- Consumer sources: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherDemandPolicy::stream` — CFG-0132 {#cfg-0132}

- Type: `std::optional<FeedFilter>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional STREAM demand filter
- Accepted values: nullopt imposes no STREAM filter; present filters must be valid. Construction permits static filters; a separate live update permits dynamic. Filtering checks generic record name and symbol.
- Read timing: Publisher creation validates source and configuredDemandPolicies; the server retains address/policy copies. Filters are consumed during peer-demand admission.
- Mutability and lifetime: Initial policy is copied; an explicit update method admits a new policy and subsequent dynamic-filter snapshots.
- Failure: PublisherEndpoint::create returns std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; internal api::Error becomes Diagnostic: INVALID_ARGUMENT / publisher-demand-filter. Configuration errors occur before server work. std::bad_alloc maps to ALLOCATION_FAILURE / publisher-storage.
- Consumer sources: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherDemandPolicy::history` — CFG-0133 {#cfg-0133}

- Type: `std::optional<FeedFilter>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional HISTORY demand filter
- Accepted values: nullopt imposes no HISTORY filter. Filters must be valid and initially static; separate live updates allow dynamic. This is demand-admission policy, subject to other server resource bounds.
- Read timing: Publisher creation validates source and configuredDemandPolicies; the server retains address/policy copies. Filters are consumed during peer-demand admission.
- Mutability and lifetime: Initial policy is copied; an explicit update method admits a new policy and subsequent dynamic-filter snapshots.
- Failure: PublisherEndpoint::create returns std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; internal api::Error becomes Diagnostic: INVALID_ARGUMENT / publisher-demand-filter. Configuration errors occur before server work. std::bad_alloc maps to ALLOCATION_FAILURE / publisher-storage.
- Consumer sources: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

## PublisherOptions {#cfg-0145-owner}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherOptions::MAX_PEER_DEMAND_POLICIES` — CFG-0145 {#cfg-0145}

- Type: `static constexpr std::size_t`. Declared default: `= 32`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Maximum address overrides per endpoint.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::MAX_STREAM_BATCH_RECORDS` — CFG-0146 {#cfg-0146}

- Type: `static constexpr std::size_t`. Declared default: `= 8`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Maximum local STREAM rows in one batch.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::listener` — CFG-0147 {#cfg-0147}

- Type: `transport::TcpListenerOptions`. Declared default: `{"127.0.0.1", 0, 16, 4}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Plain numeric bind; address is borrowed at create.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::pendingQuotes` — CFG-0148 {#cfg-0148}

- Type: `std::size_t`. Declared default: `= 64`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Maximum queued publication requests across enabled contracts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: 1..1024, src/api/publisher.cpp:3335; queue capacity, not lifetime record quota
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::source` — CFG-0149 {#cfg-0149}

- Type: `std::optional<PublisherSourceOptions>`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remote upstream in place of local publication.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::mixed` — CFG-0150 {#cfg-0150}

- Type: `bool`. Declared default: `= false`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Enable Quote/TICKER, TimeAndSale/STREAM and default Order/HISTORY on each connection.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::catalog` — CFG-0151 {#cfg-0151}

- Type: `std::optional<PublisherCatalogOptions>`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Local record publisher or full relay. Requires mixed=false.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::demand` — CFG-0152 {#cfg-0152}

- Type: `PublisherDemandPolicy`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Default policy. Filters must be static and built before create.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::peerDemand` — CFG-0153 {#cfg-0153}

- Type: `std::vector<PublisherPeerDemandPolicy>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Owned overrides, matched by numeric IP on connection.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::recordLimits` — CFG-0154 {#cfg-0154}

- Type: `PublisherRecordLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Pending storage limits for local record publication.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::streamBatchRecords` — CFG-0155 {#cfg-0155}

- Type: `std::size_t`. Declared default: `= 1`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Enable batching in local mixed or catalog mode by setting 2 through 8.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: when streamBytes absent: 1..8; &gt;1 only local mixed/catalog; byte mode derives selection separately, src/api/publisher.cpp:3343
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::streamBytes` — CFG-0156 {#cfg-0156}

- Type: `std::optional<PublisherStreamByteLimits>`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Enable byte-bounded local STREAM publication.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherPeerDemandPolicy {#cfg-0134-owner}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherPeerDemandPolicy::address` — CFG-0134 {#cfg-0134}

- Type: `std::string`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: numeric peer-policy IP bytes
- Accepted values: Nonempty IPv4/IPv6,&lt;=45 bytes, no NUL/% and successful make_address. IPv4 maps to v4-mapped IPv6 for comparison; duplicate policies for a normalized address reject.
- Read timing: Publisher creation validates source and configuredDemandPolicies; the server retains address/policy copies. Filters are consumed during peer-demand admission.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: PublisherEndpoint::create returns std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; internal api::Error becomes Diagnostic: INVALID_ARGUMENT / publisher-demand-peer-address or publisher-duplicate-demand-peer. Configuration errors occur before server work. std::bad_alloc maps to ALLOCATION_FAILURE / publisher-storage.
- Consumer sources: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherPeerDemandPolicy::demand` — CFG-0135 {#cfg-0135}

- Type: `PublisherDemandPolicy`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Immutable per-peer filter policy replacing endpoint default for matching numeric remote IP.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherRecordLimits {#cfg-0136-owner}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherRecordLimits::MAX_FIELDS` — CFG-0136 {#cfg-0136}

- Type: `static constexpr std::size_t`. Declared default: `= 128`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Supported logical fields per submitted row.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherRecordLimits::MAX_ROW_BYTES` — CFG-0137 {#cfg-0137}

- Type: `static constexpr std::size_t`. Declared default: `= 16 * 1024`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Largest configurable row reservation.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherRecordLimits::MAX_PENDING_BYTES` — CFG-0138 {#cfg-0138}

- Type: `static constexpr std::size_t`. Declared default: `= 8 * 1024 * 1024`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Largest configurable aggregate reservation.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherRecordLimits::rowBytes` — CFG-0139 {#cfg-0139}

- Type: `std::size_t`. Declared default: `= MAX_ROW_BYTES`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Maximum one row's field views, UTF-16 units and opaque bytes.
- Units: bytes; scope defined by field contract
- Declared constraints: when local catalog or byte mode: 1..16384; mixed byte mode also &gt;=sizeof(SourceRow), src/api/publisher.cpp:3350 and 3362
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherRecordLimits::pendingBytes` — CFG-0140 {#cfg-0140}

- Type: `std::size_t`. Declared default: `= 256 * 1024`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Maximum total while queued or awaiting collector processing.
- Units: bytes; scope defined by field contract
- Declared constraints: when local catalog or byte mode: 1..8388608; mixed byte mode also &gt;=sizeof(SourceRow), src/api/publisher.cpp:3350 and 3362
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherSourceOptions {#cfg-0126-owner}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherSourceOptions::address` — CFG-0126 {#cfg-0126}

- Type: `std::string_view`. Declared default: `= "127.0.0.1"`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: numeric source IP address bytes
- Accepted values: With source: nonempty numeric IPv4/IPv6,&lt;=45 bytes; NUL and % reject, DNS names are not accepted. asio::ip::make_address must parse it.
- Read timing: Publisher creation validates source and configuredDemandPolicies; the server retains address/policy copies. Filters are consumed during peer-demand admission.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: PublisherEndpoint::create returns std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; internal api::Error becomes Diagnostic: INVALID_ARGUMENT / publisher-source-options or publisher-source-address. Configuration errors occur before server work. std::bad_alloc maps to ALLOCATION_FAILURE / publisher-storage.
- Consumer sources: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherSourceOptions::port` — CFG-0127 {#cfg-0127}

- Type: `std::uint16_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: source TCP port
- Accepted values: With source:1..65535 from uint16_t plus the nonzero check. Default 0 does not select a remote port automatically; it is invalid for source.
- Read timing: Publisher creation validates source and configuredDemandPolicies; the server retains address/policy copies. Filters are consumed during peer-demand admission.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: PublisherEndpoint::create returns std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; internal api::Error becomes Diagnostic: INVALID_ARGUMENT / publisher-source-options. Configuration errors occur before server work. std::bad_alloc maps to ALLOCATION_FAILURE / publisher-storage.
- Consumer sources: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherSourceOptions::retryDelay` — CFG-0128 {#cfg-0128}

- Type: `std::chrono::milliseconds`. Declared default: `{100}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: source reconnect delay milliseconds
- Accepted values: 1..60000 inclusive when source exists. Used to compute retryAt after upstream-source failure.
- Read timing: Publisher creation validates source and configuredDemandPolicies; the server retains address/policy copies. Filters are consumed during peer-demand admission.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: PublisherEndpoint::create returns std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; internal api::Error becomes Diagnostic: INVALID_ARGUMENT / publisher-source-options. Configuration errors occur before server work. std::bad_alloc maps to ALLOCATION_FAILURE / publisher-storage.
- Consumer sources: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

## PublisherStreamByteLimits {#cfg-0141-owner}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherStreamByteLimits::MAX_DELIVERY_WINDOW_BYTES` — CFG-0141 {#cfg-0141}

- Type: `static constexpr std::size_t`. Declared default: `= 8 * 1024 * 1024`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Maximum configured window.
- Units: bytes; scope defined by field contract
- Declared constraints: byte mode: 0&lt;targetPayloadBytes&lt;maxFrameBytes&lt;=65536; 0&lt;deliveryWindowBytes&lt;=8388608; local mixed or catalog only, src/api/publisher.cpp:3355
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherStreamByteLimits::targetPayloadBytes` — CFG-0142 {#cfg-0142}

- Type: `std::size_t`. Declared default: `= 8000`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Soft encoded payload threshold before taking another record.
- Units: bytes; scope defined by field contract
- Declared constraints: byte mode: 0&lt;targetPayloadBytes&lt;maxFrameBytes&lt;=65536; 0&lt;deliveryWindowBytes&lt;=8388608; local mixed or catalog only, src/api/publisher.cpp:3355
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherStreamByteLimits::maxFrameBytes` — CFG-0143 {#cfg-0143}

- Type: `std::size_t`. Declared default: `= 64 * 1024`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Hard complete-frame limit including the length prefix.
- Units: bytes; scope defined by field contract
- Declared constraints: byte mode: 0&lt;targetPayloadBytes&lt;maxFrameBytes&lt;=65536; 0&lt;deliveryWindowBytes&lt;=8388608; local mixed or catalog only, src/api/publisher.cpp:3355
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherStreamByteLimits::deliveryWindowBytes` — CFG-0144 {#cfg-0144}

- Type: `std::size_t`. Declared default: `= 256 * 1024`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Detached record bytes selected per peer; one large row may pass.
- Units: bytes; scope defined by field contract
- Declared constraints: byte mode: 0&lt;targetPayloadBytes&lt;maxFrameBytes&lt;=65536; 0&lt;deliveryWindowBytes&lt;=8388608; local mixed or catalog only, src/api/publisher.cpp:3355
- Read timing: PublisherEndpoint::create; owned worker configuration; borrowed catalog/source text copied before return
- Mutability and lifetime: no aggregate mutation updates endpoint; explicit peer-demand policy replacement has separate API
- Failure: api::Diagnostic INVALID_ARGUMENT at publisher-* stage; admission/runtime errors remain separate
- Additional owner vocabulary (not a field-to-error mapping): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## TimeSeriesModelLimits {#cfg-0157-owner}

[include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp)

### `TimeSeriesModelLimits::eventTypes` — CFG-0157 {#cfg-0157}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: event-type descriptor arguments
- Accepted values: &gt;0 and &lt;=max_size/sizeof(TimeSeriesEventDescriptor). Nonempty raw eventTypes.size() must fit before deduplication; each descriptor capability is checked separately.
- Read timing: TimeSeriesSubscriptionModel::create validates limits/types; symbol mutations stage a complete candidate under captured limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: TimeSeriesModelError::INVALID_LIMITS for 0/basic capacity overflow; RESOURCE_LIMIT on field excess/length_error; ALLOCATION_FAILURE on bad_alloc. Refusal does not publish a partial set.
- Consumer sources: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::typeIdentityUnits` — CFG-0158 {#cfg-0158}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per stable type identity
- Accepted values: &gt;0. Each descriptor stableIdentity().size() must be &lt;=typeIdentityUnits. No additional constant ceiling applies.
- Read timing: TimeSeriesSubscriptionModel::create validates limits/types; symbol mutations stage a complete candidate under captured limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: TimeSeriesModelError::INVALID_LIMITS for 0/basic capacity overflow; RESOURCE_LIMIT on field excess/length_error; ALLOCATION_FAILURE on bad_alloc. Refusal does not publish a partial set.
- Consumer sources: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::symbols` — CFG-0159 {#cfg-0159}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: distinct model selectors
- Accepted values: &gt;0 and &lt;=max_size/sizeof(SubscriptionSelector). Every newly distinct desired selector needs a free slot; raw duplicates are also bounded by inputSymbols.
- Read timing: TimeSeriesSubscriptionModel::create validates limits/types; symbol mutations stage a complete candidate under captured limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: TimeSeriesModelError::INVALID_LIMITS for 0/basic capacity overflow; RESOURCE_LIMIT on field excess/length_error; ALLOCATION_FAILURE on bad_alloc. Refusal does not publish a partial set.
- Consumer sources: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::inputSymbols` — CFG-0160 {#cfg-0160}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: selector arguments per mutation
- Accepted values: &gt;0. input.size()&lt;=inputSymbols is checked before staging; a zero setting rejects while an empty operation input is allowed.
- Read timing: TimeSeriesSubscriptionModel::create validates limits/types; symbol mutations stage a complete candidate under captured limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: TimeSeriesModelError::INVALID_LIMITS for 0/basic capacity overflow; RESOURCE_LIMIT on field excess/length_error; ALLOCATION_FAILURE on bad_alloc. Refusal does not publish a partial set.
- Consumer sources: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::symbolUnits` — CFG-0161 {#cfg-0161}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units per symbol
- Accepted values: &gt;0. Each eventSymbol().size() must be &lt;=symbolUnits before copying. This model introduces no separate ASCII/128 ceiling.
- Read timing: TimeSeriesSubscriptionModel::create validates limits/types; symbol mutations stage a complete candidate under captured limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: TimeSeriesModelError::INVALID_LIMITS for 0/basic capacity overflow; RESOURCE_LIMIT on field excess/length_error; ALLOCATION_FAILURE on bad_alloc. Refusal does not publish a partial set.
- Consumer sources: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::totalSymbolUnits` — CFG-0162 {#cfg-0162}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 code units of distinct symbols
- Accepted values: &gt;0. Distinct candidates must successively fit totalSymbolUnits-units after set normalization. symbols independently bounds count.
- Read timing: TimeSeriesSubscriptionModel::create validates limits/types; symbol mutations stage a complete candidate under captured limits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected: TimeSeriesModelError::INVALID_LIMITS for 0/basic capacity overflow; RESOURCE_LIMIT on field excess/length_error; ALLOCATION_FAILURE on bad_alloc. Refusal does not publish a partial set.
- Consumer sources: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).
