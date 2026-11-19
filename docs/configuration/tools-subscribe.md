# Receiving tools

[Русский](tools-subscribe.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [Configuration](tools-subscribe.md#cfg-0839-owner) — `tools/subscribe/configuration.hpp`
- [RecoveryConfiguration](tools-subscribe.md#cfg-0851-owner) — `tools/subscribe/recovery_configuration.hpp`
- [PreflightCapacity](tools-subscribe.md#cfg-0884-owner) — `tools/subscribe/tls_host_preflight.hpp`
- [PreflightWork](tools-subscribe.md#cfg-0881-owner) — `tools/subscribe/tls_host_preflight.hpp`

## Configuration {#cfg-0839-owner}

[tools/subscribe/configuration.hpp](../../tools/subscribe/configuration.hpp)

### `Configuration::host` — CFG-0839 {#cfg-0839}

- Type: `std::string`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: numeric-address bytes
- Accepted values: Required IPv4/IPv6 literal of 1..45 bytes accepted by inet_pton; URI/zone/DNS are unsupported by this program.
- Read timing: parseArguments checks/copies the address; effectiveEndpoint assigns configuration.host to settings.host before EndpointOptions::create.
- Mutability and lifetime: Configuration is an owned parse result; a running invocation does not observe later mutation of an external object.
- Failure: Parsing this field returns "--host requires a numeric IPv4/IPv6 literal" on refusal; legacy main exits with code 1. If its length fits but inet_pton refuses the address, the string is "--host requires a numeric IPv4/IPv6 literal without a zone". A missing host reaches the required --host/--port/--symbol check. A separate effectiveEndpoint call with an invalid address yields EndpointError::INVALID_CONFIGURATION, stage=endpoint-address.
- Consumer sources: [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `Configuration::port` — CFG-0840 {#cfg-0840}

- Type: `std::uint16_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: numeric remote TCP port
- Accepted values: Required complete decimal in 1..65535; 0 is rejected.
- Read timing: parseArguments checks a complete decimal port and stores uint16_t. effectiveEndpoint forwards configuration.port to settings.port and then EndpointOptions::create.
- Mutability and lifetime: Configuration is an owned parse result; a running invocation does not observe later mutation of an external object.
- Failure: Parsing this field returns "numeric option is outside its positive decimal range" on refusal; legacy main exits with code 1. A missing port/0 reaches the required --host/--port/--symbol check. In a separate effectiveEndpoint call, port=0 yields EndpointError::INVALID_CONFIGURATION, stage=endpoint-address.
- Consumer sources: [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `Configuration::schema` — CFG-0841 {#cfg-0841}

- Type: `SchemaProfile`. Declared default: `= SchemaProfile::QUOTE`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete built-in schema profile
- Accepted values: Only quote or profile, default quote. Prepared output must match the selected schema.
- Read timing: parseArguments validates the field and schema/output compatibility. After successful parsing, main chooses the schema from config.schema and creates OutputConsumer from config.output. effectiveEndpoint does not directly read either of these two fields.
- Mutability and lifetime: Configuration is an owned parse result; a running invocation does not observe later mutation of an external object.
- Failure: Parsing refusal returns a static string, e.g. "--schema-profile requires quote or profile". Prepared-output mismatch is "prepared output must match --schema-profile". Legacy main exits with code 1 on parse failure.

### `Configuration::output` — CFG-0842 {#cfg-0842}

- Type: `OutputProfile`. Declared default: `= OutputProfile::GENERIC`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: application output profile
- Accepted values: Only generic/quote/profile; quote/profile require matching schema-profile. Default generic.
- Read timing: parseArguments validates the field and schema/output compatibility. After successful parsing, main chooses the schema from config.schema and creates OutputConsumer from config.output. effectiveEndpoint does not directly read either of these two fields.
- Mutability and lifetime: Configuration is an owned parse result; a running invocation does not observe later mutation of an external object.
- Failure: Parsing refusal returns a static string, e.g. "--output requires generic, quote or profile". Prepared-output mismatch is "prepared output must match --schema-profile". Legacy main exits with code 1 on parse failure.

### `Configuration::symbols` — CFG-0843 {#cfg-0843}

- Type: `std::vector<std::u16string>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One to sixteen distinct ASCII selectors, at most 128 units each.
- Units: see declaration: UTF-16/code units
- Declared constraints: One to sixteen distinct ASCII selectors, at most 128 units each.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `Configuration::duration` — CFG-0844 {#cfg-0844}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive finite admission interval, at most 600000 ms.
- Units: milliseconds
- Declared constraints: Positive finite admission interval, at most 600000 ms.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `Configuration::removeAfter` — CFG-0845 {#cfg-0845}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds to initial-subscription removal or absence
- Accepted values: Absence disables removal; a present value is 1..600000 and strictly &lt;duration. 0 is rejected.
- Read timing: parseArguments checks positivity and ordering against duration. main forwards removeAfter in StreamOptions; validOptions checks it before work and the progress loop submits removal after addComposed when due.
- Mutability and lifetime: Configuration is an owned parse result; a running invocation does not observe later mutation of an external object.
- Failure: Parsing this field returns "numeric option is outside its positive decimal range" on refusal; legacy main exits with code 1. A value not earlier than duration returns "--remove-after-ms must be less than --duration-ms". An invalid optional time passed directly to the example yields StreamFailure::CONFIGURATION; it is unrelated to schema/output compatibility.

### `Configuration::connectTimeout` — CFG-0846 {#cfg-0846}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: whole connect timeout milliseconds or absence
- Accepted values: If present, 1..INT_MAX; otherwise controlledStreamSettings retains 1000 ms.
- Read timing: parseArguments checks an explicitly supplied positive interval. effectiveEndpoint reads connectTimeout through value_or and forwards the resulting duration into immutable EndpointOptions.
- Mutability and lifetime: Configuration is an owned parse result; a running invocation does not observe later mutation of an external object.
- Failure: Parsing this field returns "numeric option is outside its positive decimal range" on refusal; legacy main exits with code 1. effectiveEndpoint resolves the optional via value_or and calls EndpointOptions::create. If CLI is separately bypassed programmatically, an invalid interval yields EndpointError::INVALID_CONFIGURATION, stage=endpoint-timeout.
- Consumer sources: [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `Configuration::readTimeout` — CFG-0847 {#cfg-0847}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: connected read inactivity milliseconds or absence
- Accepted values: If present, 1..INT_MAX; otherwise the controlledStreamSettings value equals duration.
- Read timing: parseArguments checks an explicitly supplied positive interval. effectiveEndpoint reads readTimeout through value_or and forwards the resulting duration into immutable EndpointOptions.
- Mutability and lifetime: Configuration is an owned parse result; a running invocation does not observe later mutation of an external object.
- Failure: Parsing this field returns "numeric option is outside its positive decimal range" on refusal; legacy main exits with code 1. effectiveEndpoint resolves the optional via value_or and calls EndpointOptions::create. If CLI is separately bypassed programmatically, an invalid interval yields EndpointError::INVALID_CONFIGURATION, stage=endpoint-timeout.
- Consumer sources: [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `Configuration::writeTimeout` — CFG-0848 {#cfg-0848}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: pending write inactivity milliseconds or absence
- Accepted values: If present, 1..INT_MAX; otherwise the controlledStreamSettings value equals duration.
- Read timing: parseArguments checks an explicitly supplied positive interval. effectiveEndpoint reads writeTimeout through value_or and forwards the resulting duration into immutable EndpointOptions.
- Mutability and lifetime: Configuration is an owned parse result; a running invocation does not observe later mutation of an external object.
- Failure: Parsing this field returns "numeric option is outside its positive decimal range" on refusal; legacy main exits with code 1. effectiveEndpoint resolves the optional via value_or and calls EndpointOptions::create. If CLI is separately bypassed programmatically, an invalid interval yields EndpointError::INVALID_CONFIGURATION, stage=endpoint-timeout.
- Consumer sources: [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `Configuration::pollInterval` — CFG-0849 {#cfg-0849}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: progress polling cadence milliseconds or absence
- Accepted values: If present, 1..INT_MAX; otherwise controlledStreamSettings retains 1 ms.
- Read timing: parseArguments checks an explicitly supplied positive interval. effectiveEndpoint reads pollInterval through value_or and forwards the resulting duration into immutable EndpointOptions.
- Mutability and lifetime: Configuration is an owned parse result; a running invocation does not observe later mutation of an external object.
- Failure: Parsing this field returns "numeric option is outside its positive decimal range" on refusal; legacy main exits with code 1. effectiveEndpoint resolves the optional via value_or and calls EndpointOptions::create. If CLI is separately bypassed programmatically, an invalid interval yields EndpointError::INVALID_CONFIGURATION, stage=endpoint-timeout.
- Consumer sources: [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `Configuration::maximumRecords` — CFG-0850 {#cfg-0850}

- Type: `std::optional<std::size_t>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Optional positive completed-record target, at most 1000000.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Optional positive completed-record target, at most 1000000.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

## RecoveryConfiguration {#cfg-0851-owner}

[tools/subscribe/recovery_configuration.hpp](../../tools/subscribe/recovery_configuration.hpp)

### `RecoveryConfiguration::host` — CFG-0851 {#cfg-0851}

- Type: `std::string`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: route address/name bytes
- Accepted values: CLI requires nonempty numeric IPv4/IPv6 or ASCII/A-label DNS of at most 254 bytes, without URI/zone; an optional final DNS dot is allowed. A named host requires --dns-server. EndpointOptions/runtime later validates its own address profile.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "host or TLS reference requires a bounded numeric IP or ASCII DNS presentation"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::port` — CFG-0852 {#cfg-0852}

- Type: `std::uint16_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Required remote port in 1..65535.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Required remote port in 1..65535.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::family` — CFG-0853 {#cfg-0853}

- Type: `core::SubscriptionFamily`. Declared default: `= core::SubscriptionFamily::STREAM`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: delivery contract
- Accepted values: CLI: stream/ticker/history. HISTORY additionally requires schemaJson and historyFromRaw; the selected schema record must have indexedTimeLayout.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--family requires stream, ticker or history"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::schema` — CFG-0854 {#cfg-0854}

- Type: `SchemaProfile`. Declared default: `= SchemaProfile::QUOTE`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete built-in schema profile
- Accepted values: quote/profile, default quote; cannot be combined with --schema-json. A built-in schema permits recordId=0 and matching prepared output.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--schema-profile requires quote or profile"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::schemaJson` — CFG-0855 {#cfg-0855}

- Type: `std::optional<std::string>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native binary JSON path bytes
- Accepted values: Optional opaque path of 1..4096 non-NUL bytes, requiring a JSON-enabled build; requires explicit recordId and excludes schema-profile. Preparation reads a document of at most 1 MiB and applies the complete JSON/schema resource profile.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "file option requires a nonempty bounded opaque path"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::recordId` — CFG-0856 {#cfg-0856}

- Type: `std::int32_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: schema local record ID
- Accepted values: CLI accepts 0..INT32_MAX. External schemas require the option; built-in schemas permit only 0. Preparation requires an existing RecordContext; HISTORY requires indexed metadata.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "external schema selection requires explicit --record-id"; main returns 2. Preparation is separate, and its refusal makes main return 1. Missing record: "recovery selected local record does not exist".

### `RecoveryConfiguration::output` — CFG-0857 {#cfg-0857}

- Type: `OutputProfile`. Declared default: `= OutputProfile::GENERIC`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: output profile
- Accepted values: generic/quote/profile. A built-in schema requires matching prepared output; an external schema later prepares PreparedQuote/PreparedProfile against the selected recordId.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--output requires generic, quote or profile"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::symbols` — CFG-0858 {#cfg-0858}

- Type: `std::vector<std::u16string>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: distinct exact UTF-16 selectors
- Accepted values: Requires 1..16 distinct symbols, each 1..128 printable ASCII bytes 0x20..0x7e without leading *. Exact bytes are widened to UTF-16; duplicates are rejected.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--symbol requires distinct printable ASCII selectors under the finite CLI limits"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::historyFromRaw` — CFG-0859 {#cfg-0859}

- Type: `std::optional<std::int64_t>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: raw signed 64-bit HISTORY lower bound
- Accepted values: Complete signed int64 decimal parsed by from_chars, including 0 and negatives, without whitespace/suffix. Requires HISTORY and an external indexed schema; this is a raw index coordinate.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--history-from-raw requires a complete signed int64 decimal"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::selectors` — CFG-0860 {#cfg-0860}

- Type: `core::SubscriptionSelectorPolicy`. Declared default: `= core::SubscriptionSelectorPolicy::EXACT_ONLY`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: exact-selector policy
- Accepted values: Only exact-only/canonical-exact-v1; this CLI does not expose All. Preparation revalidates it and forwards it to the queue/receiver.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--selectors requires exact-only or canonical-exact-v1 without All"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::dnsServer` — CFG-0861 {#cfg-0861}

- Type: `std::optional<std::string>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: numeric IPv4 DNS-resolver bytes
- Accepted values: Optional single IPv4 address: 4 decimal octets 0..255 without leading zeros in multicharacter octets. Required for named hosts and explicit --dns-port; the library later rejects an all-zero server.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--dns-server requires one numeric IPv4 resolver"; main returns 2. Preparation is separate, and its refusal makes main return 1.
- Consumer sources: [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp).

### `RecoveryConfiguration::dnsPort` — CFG-0862 {#cfg-0862}

- Type: `std::uint16_t`. Declared default: `= 53`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit UDP/TCP resolver port in 1..65535.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Explicit UDP/TCP resolver port in 1..65535.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::attempts` — CFG-0863 {#cfg-0863}

- Type: `std::uint32_t`. Declared default: `= 1`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: lifetime recovery attempts including the first
- Accepted values: CLI accepts 1..UINT32_MAX. attempts=1 forbids an explicit retry-delay; with &gt;1 a missing delay becomes 250 ms. Preparation additionally requires (attempts+1)×sizeof(QueueIdentity)≤1 MiB for presentation labels.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "numeric recovery option is outside its complete decimal range"; main returns 2. Preparation is separate, and its refusal makes main return 1. Exceeding presentation storage: "recovery attempt count exceeds the presentation profile".

### `RecoveryConfiguration::retryDelay` — CFG-0864 {#cfg-0864}

- Type: `std::chrono::milliseconds`. Declared default: `{0}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Zero for one attempt; otherwise positive, default 250 ms.
- Units: milliseconds
- Declared constraints: Zero for one attempt; otherwise positive, default 250 ms.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::duration` — CFG-0865 {#cfg-0865}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Admission interval in 1..600000 ms, including negotiation.
- Units: milliseconds
- Declared constraints: Admission interval in 1..600000 ms, including negotiation.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::removeAfter` — CFG-0866 {#cfg-0866}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds to initial-subscription removal or absence
- Accepted values: Absence disables removal; a present value is 1..600000 ms and strictly &lt;duration. Preparation rechecks positivity and ordering.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--remove-after-ms must be strictly before the admission duration"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::maximumRecords` — CFG-0867 {#cfg-0867}

- Type: `std::optional<std::size_t>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Optional delivered-record target in 1..1000000.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Optional delivered-record target in 1..1000000.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::drain` — CFG-0868 {#cfg-0868}

- Type: `std::chrono::milliseconds`. Declared default: `{1000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Local admitted-prefix retirement allowance in 1..60000 ms.
- Units: milliseconds
- Declared constraints: Local admitted-prefix retirement allowance in 1..60000 ms.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::connectTimeout` — CFG-0869 {#cfg-0869}

- Type: `std::chrono::milliseconds`. Declared default: `{1000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit finite connect deadline, at most INT_MAX ms.
- Units: milliseconds
- Declared constraints: Explicit finite connect deadline, at most INT_MAX ms.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::readTimeout` — CFG-0870 {#cfg-0870}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: input-inactivity milliseconds
- Accepted values: CLI 1..INT_MAX; when omitted, duration is assigned after complete parsing regardless of argument order.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "numeric recovery option is outside its complete decimal range"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::writeTimeout` — CFG-0871 {#cfg-0871}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: output-inactivity milliseconds
- Accepted values: CLI 1..INT_MAX; when omitted, duration is assigned after complete parsing. 0 does not disable the timeout.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "numeric recovery option is outside its complete decimal range"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::pollInterval` — CFG-0872 {#cfg-0872}

- Type: `std::chrono::milliseconds`. Declared default: `{1}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit finite owner cadence, at most INT_MAX ms.
- Units: milliseconds
- Declared constraints: Explicit finite owner cadence, at most INT_MAX ms.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::dnsTimeout` — CFG-0873 {#cfg-0873}

- Type: `std::chrono::milliseconds`. Declared default: `{1000}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: whole DNS deadline milliseconds
- Accepted values: CLI 1..INT_MAX; when omitted, connectTimeout is assigned after parsing. Numeric routes may not use DNS, but CLI still validates an explicitly supplied timeout.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "numeric recovery option is outside its complete decimal range"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::security` — CFG-0874 {#cfg-0874}

- Type: `client::EndpointSecurity`. Declared default: `= client::EndpointSecurity::CREDENTIAL_FREE_PLAIN`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: connection security mode
- Accepted values: plain/tls; TLS requires features.tls, trust path and reference. Plain forbids TLS trust/reference/deadlines/credentials.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--security requires plain or tls"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::tlsTrustPem` — CFG-0875 {#cfg-0875}

- Type: `std::optional<std::string>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: PEM trust-path bytes
- Accepted values: A path of 1..4096 non-NUL bytes, required for TLS and forbidden for plain. Later reading is bounded by TRUST_INPUT_BYTES=524288; PEM framing and anchor count are checked separately.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "TLS requires explicit --tls-trust-pem and --tls-reference"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::tlsReference` — CFG-0876 {#cfg-0876}

- Type: `std::optional<std::string>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: TLS reference-identity bytes
- Accepted values: CLI applies validHost as for host: bounded numeric IP/ASCII DNS up to 254 bytes without URI/zone. Required only for TLS. Preparation separately requires nonempty printable identity ≤REFERENCE_BYTES and SDK verifies SAN against it.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "host or TLS reference requires a bounded numeric IP or ASCII DNS presentation"; main returns 2. Preparation is separate, and its refusal makes main return 1.

### `RecoveryConfiguration::tlsHandshake` — CFG-0877 {#cfg-0877}

- Type: `std::chrono::milliseconds`. Declared default: `{1000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Finite TLS handshake deadline, at most INT_MAX ms.
- Units: milliseconds
- Declared constraints: Finite TLS handshake deadline, at most INT_MAX ms.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::tlsClose` — CFG-0878 {#cfg-0878}

- Type: `std::chrono::milliseconds`. Declared default: `{1000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Finite TLS close deadline, at most INT_MAX ms.
- Units: milliseconds
- Declared constraints: Finite TLS close deadline, at most INT_MAX ms.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `RecoveryConfiguration::authBasicTokenFile` — CFG-0879 {#cfg-0879}

- Type: `std::optional<std::string>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: exact Basic-token file path bytes
- Accepted values: Optional path of 1..4096 non-NUL bytes, TLS only. The file is read later with a 4096-byte bound and must contain a complete exact ASCII Basic token without newline/normalization; canonical base64 is checked.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "file option requires a nonempty bounded opaque path"; main returns 2. Preparation is separate, and its refusal makes main return 1. Invalid contents: "recovery TLS credential input requires a complete exact ASCII Basic token".

### `RecoveryConfiguration::authTimeout` — CFG-0880 {#cfg-0880}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: authentication timeout milliseconds or absence
- Accepted values: If supplied, 1..INT_MAX and authBasicTokenFile is required; TLS only. If a token path exists without a timeout, 1000 ms is assigned.
- Read timing: assignOption/finishConfiguration run before working files/network; prepareRecovery then resolves the schema, endpoint, adapter and queue; the application uses its owned consistent result.
- Mutability and lifetime: One owned profile per run; repeated options except --symbol are rejected and there is no subsequent CLI live setter.
- Failure: A static parseRecoveryArguments refusal, e.g. "--auth-timeout-ms requires --auth-basic-token-file"; main returns 2. Preparation is separate, and its refusal makes main return 1.

## PreflightCapacity {#cfg-0884-owner}

[tools/subscribe/tls_host_preflight.hpp](../../tools/subscribe/tls_host_preflight.hpp)

### `PreflightCapacity::sharedBytes` — CFG-0884 {#cfg-0884}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: remaining bytes of host-scoped shared reservation
- Accepted values: capacity.sharedBytes ≥ preflightRequirements(limits).tls.sdk.sharedBytes. 0 cannot hold a positive computed reservation. It is debited atomically with work credits before first SDK entry and not refunded after later failure.
- Read timing: checkClosedTlsHost checks it after shape validation and before the SDK gate; reservedSharedBytes is returned in a detached receipt.
- Mutability and lifetime: This is a mutable remainder debited by one attempt; PreflightOnce is permanently consumed even on refusal.
- Failure: Insufficient storage is PreflightError::RESOURCE_LIMIT, stage=preflight.shared-capacity. A repeated attempt separately yields ALREADY_ENTERED.

## PreflightWork {#cfg-0881-owner}

[tools/subscribe/tls_host_preflight.hpp](../../tools/subscribe/tls_host_preflight.hpp)

### `PreflightWork::tlsShape` — CFG-0881 {#cfg-0881}

- Type: `transport::TlsWorkShape`. Declared default: `{}`
- Classification: qualification shape; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Same complete source/host/input shape used by pure TLS requirements.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `PreflightError`.

### `PreflightWork::preflightRevision` — CFG-0882 {#cfg-0882}

- Type: `std::uint32_t`. Declared default: `= 0`
- Classification: qualification shape; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Must match this exact expected catalogue and source proof.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `PreflightError`.

### `PreflightWork::remaining` — CFG-0883 {#cfg-0883}

- Type: `PreflightVector`. Declared default: `{}`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Caller credits, atomically debited before the first SDK entry.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `PreflightError`.
