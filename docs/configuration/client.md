# Client composition

[Русский](client.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [EndpointBudgets](client.md#cfg-0191-owner) — `include/qd/client/endpoint_options.hpp`
- [EndpointRetry](client.md#cfg-0170-owner) — `include/qd/client/endpoint_options.hpp`
- [EndpointRuntimeWork](client.md#cfg-0184-owner) — `include/qd/client/endpoint_options.hpp`
- [EndpointSettings](client.md#cfg-0204-owner) — `include/qd/client/endpoint_options.hpp`
- [EndpointTcpLimits](client.md#cfg-0172-owner) — `include/qd/client/endpoint_options.hpp`
- [EndpointTimeouts](client.md#cfg-0179-owner) — `include/qd/client/endpoint_options.hpp`
- [EndpointTls](client.md#cfg-0163-owner) — `include/qd/client/endpoint_options.hpp`
- [ClientLimits](client.md#cfg-0212-owner) — `include/qd/client/stream_dispatcher.hpp`
- [DispatchWork](client.md#cfg-0228-owner) — `include/qd/client/stream_dispatcher.hpp`
- [SubmitWork](client.md#cfg-0225-owner) — `include/qd/client/stream_dispatcher.hpp`
- [ReceiverLimits](client.md#cfg-0237-owner) — `include/qd/client/subscription_receiver.hpp`
- [ReceiverProjectionWork](client.md#cfg-0242-owner) — `include/qd/client/subscription_receiver.hpp`
- [ReceiverWork](client.md#cfg-0240-owner) — `include/qd/client/subscription_receiver.hpp`

## EndpointBudgets {#cfg-0191-owner}

[include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp)

### `EndpointBudgets::schema` — CFG-0191 {#cfg-0191}

- Type: `core::SchemaLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete configuration-schema logical copy limits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::layout` — CFG-0192 {#cfg-0192}

- Type: `protocol::LayoutLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete explicit outgoing-layout limits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::writer` — CFG-0193 {#cfg-0193}

- Type: `protocol::SubscriptionWriterLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Whole-output and retained composer capacities.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::session` — CFG-0194 {#cfg-0194}

- Type: `protocol::LiveLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Pure-session capacities; credential eligibility must remain plain.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::desired` — CFG-0195 {#cfg-0195}

- Type: `core::SubscriptionLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete desired-state capacities.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::collector` — CFG-0196 {#cfg-0196}

- Type: `collectors::StreamLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Local whole-frame/record/resident capacities.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::client` — CFG-0197 {#cfg-0197}

- Type: `ClientLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Command/detached-batch capacities, separate from lower owners.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::tcp` — CFG-0198 {#cfg-0198}

- Type: `EndpointTcpLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Runtime-neutral socket/SDK/aggregate ceilings.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::writerPreparation` — CFG-0199 {#cfg-0199}

- Type: `protocol::SubscriptionWriterWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One writer-preparation allowance.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::submitTurn` — CFG-0200 {#cfg-0200}

- Type: `SubmitWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One application command-submission allowance.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::protocolTurn` — CFG-0201 {#cfg-0201}

- Type: `protocol::LiveWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One application protocol preparation/progress allowance.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::dispatchTurn` — CFG-0202 {#cfg-0202}

- Type: `DispatchWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One owner turn, also used during applicable stop/drain paths.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointBudgets::runtimeTurn` — CFG-0203 {#cfg-0203}

- Type: `EndpointRuntimeWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One application native-progress allowance.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

## EndpointRetry {#cfg-0170-owner}

[include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp)

### `EndpointRetry::maximumAttempts` — CFG-0170 {#cfg-0170}

- Type: `std::uint32_t`. Declared default: `= 1`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive finite attempt count, including the initial attempt.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive finite attempt count, including the initial attempt.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointRetry::delay` — CFG-0171 {#cfg-0171}

- Type: `std::chrono::milliseconds`. Declared default: `{0}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Zero only for one attempt; otherwise positive milliseconds &lt;=INT_MAX.
- Units: milliseconds
- Declared constraints: Zero only for one attempt; otherwise positive milliseconds &lt;=INT_MAX.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

## EndpointRuntimeWork {#cfg-0184-owner}

[include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp)

### `EndpointRuntimeWork::steps` — CFG-0184 {#cfg-0184}

- Type: `std::size_t`. Declared default: `{}`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Public progress attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointRuntimeWork::handlers` — CFG-0185 {#cfg-0185}

- Type: `std::size_t`. Declared default: `{}`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Internal handler dispatches.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointRuntimeWork::socketBytes` — CFG-0186 {#cfg-0186}

- Type: `std::size_t`. Declared default: `{}`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservative native read/write buffer bytes.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointRuntimeWork::dnsCalls` — CFG-0187 {#cfg-0187}

- Type: `std::size_t`. Declared default: `{}`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: DNS socket callback attempts; zero for the current numeric-only driver.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointRuntimeWork::dnsPackets` — CFG-0188 {#cfg-0188}

- Type: `std::size_t`. Declared default: `{}`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Successful DNS receive operations.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointRuntimeWork::dnsBytes` — CFG-0189 {#cfg-0189}

- Type: `std::size_t`. Declared default: `{}`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: DNS receive/send bytes, preserving datagrams.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointRuntimeWork::dnsUnits` — CFG-0190 {#cfg-0190}

- Type: `std::uint64_t`. Declared default: `{}`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservative DNS parser/copy work units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

## EndpointSettings {#cfg-0204-owner}

[include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp)

### `EndpointSettings::host` — CFG-0204 {#cfg-0204}

- Type: `std::string`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: ASCII DNS/IP host bytes
- Accepted values: Nonempty name&lt;=254 bytes. IPv4 has four octets 0..255 without leading zeroes; IPv6 excludes brackets/zones; DNS uses ASCII/A-label labels&lt;=63, no leading/trailing hyphen, optional final dot. Port is separately required.
- Read timing: client::EndpointOptions::create validates settings and stores an immutable copy; a separate driver activates it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected&lt;EndpointOptions,EndpointDiagnostic&gt;: EndpointError::INVALID_CONFIGURATION / endpoint-address. Allocation/unsupported schema have separate diagnostics.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp), [src/client/endpoint_host.hpp](../../src/client/endpoint_host.hpp).

### `EndpointSettings::port` — CFG-0205 {#cfg-0205}

- Type: `std::uint16_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Required numeric port in 1..65535.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Required numeric port in 1..65535.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointSettings::runtime` — CFG-0206 {#cfg-0206}

- Type: `EndpointRuntime`. Declared default: `= EndpointRuntime::CALLER_DRIVEN_TCP`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: EndpointRuntime value
- Accepted values: Only CALLER_DRIVEN_TCP; other enum values reject. Selects progress ownership in the lower API.
- Read timing: client::EndpointOptions::create validates settings and stores an immutable copy; a separate driver activates it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected&lt;EndpointOptions,EndpointDiagnostic&gt;: EndpointError::INVALID_CONFIGURATION / endpoint-security. Allocation/unsupported schema have separate diagnostics.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `EndpointSettings::security` — CFG-0207 {#cfg-0207}

- Type: `EndpointSecurity`. Declared default: `= EndpointSecurity::CREDENTIAL_FREE_PLAIN`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: EndpointSecurity value
- Accepted values: CREDENTIAL_FREE_PLAIN requires no tls; TLS requires a present structurally valid EndpointTls. Other values reject. This checks local shape before certificates/provider execution.
- Read timing: client::EndpointOptions::create validates settings and stores an immutable copy; a separate driver activates it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected&lt;EndpointOptions,EndpointDiagnostic&gt;: EndpointError::INVALID_CONFIGURATION / endpoint-security. Allocation/unsupported schema have separate diagnostics.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `EndpointSettings::tls` — CFG-0208 {#cfg-0208}

- Type: `std::optional<EndpointTls>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Required for TLS, forbidden for plain; handoff-only in current driver.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointSettings::retry` — CFG-0209 {#cfg-0209}

- Type: `EndpointRetry`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One-attempt initial policy; multiple attempts are handoff-only.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointSettings::timeouts` — CFG-0210 {#cfg-0210}

- Type: `EndpointTimeouts`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit finite millisecond deadlines/cadence.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointSettings::budgets` — CFG-0211 {#cfg-0211}

- Type: `EndpointBudgets`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit per-component limits/turn credits, never global defaults.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

## EndpointTcpLimits {#cfg-0172-owner}

[include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp)

### `EndpointTcpLimits::inputBytes` — CFG-0172 {#cfg-0172}

- Type: `std::size_t`. Declared default: `{}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Owned read capacity, in 1..INT_MAX bytes.
- Units: bytes; scope defined by field contract
- Declared constraints: Owned read capacity, in 1..INT_MAX bytes.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTcpLimits::writeBytes` — CFG-0173 {#cfg-0173}

- Type: `std::size_t`. Declared default: `{}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Maximum native write size, in 1..INT_MAX bytes.
- Units: bytes; scope defined by field contract
- Declared constraints: Maximum native write size, in 1..INT_MAX bytes.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTcpLimits::addresses` — CFG-0174 {#cfg-0174}

- Type: `std::size_t`. Declared default: `{}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Address capacity, in 1..64.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Address capacity, in 1..64.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTcpLimits::cnameDepth` — CFG-0175 {#cfg-0175}

- Type: `std::size_t`. Declared default: `{}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: CNAME depth ceiling, in 1..16; no DNS activation in the current driver.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: CNAME depth ceiling, in 1..16; no DNS activation in the current driver.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTcpLimits::contextBytes` — CFG-0176 {#cfg-0176}

- Type: `std::size_t`. Declared default: `{}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive fixed execution-context arena bytes.
- Units: bytes; scope defined by field contract
- Declared constraints: Positive fixed execution-context arena bytes.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTcpLimits::handlerBytes` — CFG-0177 {#cfg-0177}

- Type: `std::size_t`. Declared default: `{}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive fixed operation arena bytes.
- Units: bytes; scope defined by field contract
- Declared constraints: Positive fixed operation arena bytes.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTcpLimits::residentBytes` — CFG-0178 {#cfg-0178}

- Type: `std::size_t`. Declared default: `{}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: TCP resident allowance bytes
- Accepted values: Must be &gt;=session.residentBytes; sum of input/context/handler/session/client/collector/writer allowances must fit size_t. Zero can pass only with zero dependent values, while the lower runtime still checks actual feasibility.
- Read timing: client::EndpointOptions::create validates settings and stores an immutable copy; a separate driver activates it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected&lt;EndpointOptions,EndpointDiagnostic&gt;: EndpointError::RESOURCE_LIMIT / endpoint-budgets. Allocation/unsupported schema have separate diagnostics.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

## EndpointTimeouts {#cfg-0179-owner}

[include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp)

### `EndpointTimeouts::dns` — CFG-0179 {#cfg-0179}

- Type: `std::chrono::milliseconds`. Declared default: `{}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: &gt;0,&lt;=INT_MAX and &lt;=duration_cast&lt;milliseconds&gt;(steady_clock::duration::max()). Value initialization with{} yields 0 and does not form a valid configuration. Applies to its DNS/connect/read/write/poll phase.
- Read timing: EndpointOptions::create calls durationValid before copying; runtime receives the captured snapshot.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: EndpointOptions::create returns EndpointError::INVALID_CONFIGURATION at endpoint-timeout when durationValid rejects this field.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `EndpointTimeouts::connect` — CFG-0180 {#cfg-0180}

- Type: `std::chrono::milliseconds`. Declared default: `{}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: &gt;0,&lt;=INT_MAX and &lt;=duration_cast&lt;milliseconds&gt;(steady_clock::duration::max()). Value initialization with{} yields 0 and does not form a valid configuration. Applies to its DNS/connect/read/write/poll phase.
- Read timing: EndpointOptions::create calls durationValid before copying; runtime receives the captured snapshot.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: EndpointOptions::create returns EndpointError::INVALID_CONFIGURATION at endpoint-timeout when durationValid rejects this field.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `EndpointTimeouts::read` — CFG-0181 {#cfg-0181}

- Type: `std::chrono::milliseconds`. Declared default: `{}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: &gt;0,&lt;=INT_MAX and &lt;=duration_cast&lt;milliseconds&gt;(steady_clock::duration::max()). Value initialization with{} yields 0 and does not form a valid configuration. Applies to its DNS/connect/read/write/poll phase.
- Read timing: EndpointOptions::create calls durationValid before copying; runtime receives the captured snapshot.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: EndpointOptions::create returns EndpointError::INVALID_CONFIGURATION at endpoint-timeout when durationValid rejects this field.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `EndpointTimeouts::write` — CFG-0182 {#cfg-0182}

- Type: `std::chrono::milliseconds`. Declared default: `{}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: &gt;0,&lt;=INT_MAX and &lt;=duration_cast&lt;milliseconds&gt;(steady_clock::duration::max()). Value initialization with{} yields 0 and does not form a valid configuration. Applies to its DNS/connect/read/write/poll phase.
- Read timing: EndpointOptions::create calls durationValid before copying; runtime receives the captured snapshot.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: EndpointOptions::create returns EndpointError::INVALID_CONFIGURATION at endpoint-timeout when durationValid rejects this field.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

### `EndpointTimeouts::poll` — CFG-0183 {#cfg-0183}

- Type: `std::chrono::milliseconds`. Declared default: `{}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds
- Accepted values: &gt;0,&lt;=INT_MAX and &lt;=duration_cast&lt;milliseconds&gt;(steady_clock::duration::max()). Value initialization with{} yields 0 and does not form a valid configuration. Applies to its DNS/connect/read/write/poll phase.
- Read timing: EndpointOptions::create calls durationValid before copying; runtime receives the captured snapshot.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: EndpointOptions::create returns EndpointError::INVALID_CONFIGURATION at endpoint-timeout when durationValid rejects this field.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

## EndpointTls {#cfg-0163-owner}

[include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp)

### `EndpointTls::referenceIdentity` — CFG-0163 {#cfg-0163}

- Type: `std::string`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit 1..254-byte ASCII numeric IP/A-label DNS identity, no URI/wildcard.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Explicit 1..254-byte ASCII numeric IP/A-label DNS identity, no URI/wildcard.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTls::trustPem` — CFG-0164 {#cfg-0164}

- Type: `std::string`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit 1..512 KiB PEM bytes; cryptographic/anchor validation belongs to the adapter.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Explicit 1..512 KiB PEM bytes; cryptographic/anchor validation belongs to the adapter.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTls::handshakeTimeout` — CFG-0165 {#cfg-0165}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Required positive TLS deadline, at most INT_MAX ms.
- Units: milliseconds
- Declared constraints: Required positive TLS deadline, at most INT_MAX ms.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTls::authenticationTimeout` — CFG-0166 {#cfg-0166}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Required only with provider, in 1..INT_MAX ms.
- Units: milliseconds
- Declared constraints: Required only with provider, in 1..INT_MAX ms.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTls::closeTimeout` — CFG-0167 {#cfg-0167}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Required positive TLS-close deadline, at most INT_MAX ms.
- Units: milliseconds
- Declared constraints: Required positive TLS-close deadline, at most INT_MAX ms.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTls::tokenUnits` — CFG-0168 {#cfg-0168}

- Type: `std::optional<std::size_t>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Required only with provider; bounded UTF-16 token capacity, at least 7.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Required only with provider; bounded UTF-16 token capacity, at least 7.
- Read timing: EndpointOptions::create copies validated settings/schema into immutable instance; work consumed at explicit turns
- Mutability and lifetime: immutable snapshot; input mutation has no effect; create replacement explicitly
- Failure: EndpointError/EndpointDiagnostic; static labels; requirePlainStream separately rejects unsupported handoff policies
- Additional owner vocabulary (not a field-to-error mapping): `CredentialError, EndpointError`.

### `EndpointTls::credentials` — CFG-0169 {#cfg-0169}

- Type: `std::shared_ptr<CredentialProvider>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: shared credential provider
- Accepted values: A null shared_ptr means no authorization: authenticationTimeout and tokenUnits must also be absent. A nonnull provider requires timeout&gt;0&lt;=INT_MAX and tokenUnits&gt;=7,&lt;=protocol.text.codeUnits,&lt;=outputBytes/sizeof(char16_t), with checked multiplication.
- Read timing: client::EndpointOptions::create validates settings and stores an immutable copy; a separate driver activates it.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: std::expected&lt;EndpointOptions,EndpointDiagnostic&gt;: EndpointError::INVALID_CONFIGURATION / endpoint-security. Allocation/unsupported schema have separate diagnostics.
- Consumer sources: [include/qd/client/endpoint_options.hpp](../../include/qd/client/endpoint_options.hpp), [src/client/endpoint_options.cpp](../../src/client/endpoint_options.cpp).

## ClientLimits {#cfg-0212-owner}

[include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp)

### `ClientLimits::commandSlots` — CFG-0212 {#cfg-0212}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: pending command slots
- Accepted values: Zero supplies no command slots; the first command returns COMMAND_LIMIT. Participates in fixed=commandSlots*perCommand and products with commandSymbolUnits; arithmetic/resident are checked.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Producer::submit returns std::unexpected(ClientError::COMMAND_LIMIT) before publishing a command when this field admission fails. This command refusal alone does not terminate the owner. Checked create arithmetic can return INVALID_CONFIGURATION; factory allocation failure returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::resultSlots` — CFG-0213 {#cfg-0213}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retained command-result slots
- Accepted values: Zero provides no slots; a command without an available Completion returns RESULT_LIMIT. A result retains its slot until ready and only its internal reference remains.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Producer::submit returns std::unexpected(ClientError::RESULT_LIMIT) before publishing a command when this field admission fails. This command refusal alone does not terminate the owner. Checked create arithmetic can return INVALID_CONFIGURATION; factory allocation failure returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::commandEntries` — CFG-0214 {#cfg-0214}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: subscription command entries
- Accepted values: count&lt;=commandEntries before capturing a command. Zero permits an empty command only. perCommand includes request/key arrays of this size; overflow rejects creation.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Producer::submit returns std::unexpected(ClientError::RESOURCE_LIMIT) before publishing a command when this field admission fails. This command refusal alone does not terminate the owner. Checked create arithmetic can return INVALID_CONFIGURATION; factory allocation failure returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::commandSymbolUnits` — CFG-0215 {#cfg-0215}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 command-symbol units
- Accepted values: Sum of symbol lengths in one command&lt;=commandSymbolUnits, checking remaining units before copying. Zero permits zero total only. Reservation is multiplied by commandSlots.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Producer::submit returns std::unexpected(ClientError::RESOURCE_LIMIT) before publishing a command when this field admission fails. This command refusal alone does not terminate the owner. Checked create arithmetic can return INVALID_CONFIGURATION; factory allocation failure returns ALLOCATION_FAILURE.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::batchRecords` — CFG-0216 {#cfg-0216}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive maximum records in one immutable callback batch.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive maximum records in one immutable callback batch.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;StreamDispatcher, ClientError&gt; create(const core::LocalSchema &amp;schema, core::SubscriptionLimits desiredLimits, collectors::StreamLimits collectorLimits, ClientLimits limits, const std::shared_ptr&lt;Consumer&gt; &amp;consumer, DispatchWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: ClientError, CommandStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `ClientLimits::batchFields` — CFG-0217 {#cfg-0217}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: all legacy and typed fields per batch
- Accepted values: Total typedValues+values across offered records&lt;=batchFields. Zero permits fieldless records only; batchRecords separately bounds record count.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: measure throws ClientError::RESOURCE_LIMIT before copying the complete offer when this batch dimension exceeds its cap. progress retains the primary failure and returns CLOSING; acknowledgeBatch and user onBatch have not run. Work-credit exhaustion separately returns WORK_LIMIT.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::batchSymbolUnits` — CFG-0218 {#cfg-0218}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 batch-symbol units
- Accepted values: Sum of record.symbol.size() across the offer&lt;=batchSymbolUnits. Zero permits empty symbols only. Arena size is constrained independently.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: measure throws ClientError::RESOURCE_LIMIT before copying the complete offer when this batch dimension exceeds its cap. progress retains the primary failure and returns CLOSING; acknowledgeBatch and user onBatch have not run. Work-credit exhaustion separately returns WORK_LIMIT.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::batchFieldUnits` — CFG-0219 {#cfg-0219}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 text-field units per batch
- Accepted values: Sum of engaged field.units lengths&lt;=batchFieldUnits; null consumes no text and empty text consumes 0. Zero permits absent/empty text values.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: measure throws ClientError::RESOURCE_LIMIT before copying the complete offer when this batch dimension exceeds its cap. progress retains the primary failure and returns CLOSING; acknowledgeBatch and user onBatch have not run. Work-credit exhaustion separately returns WORK_LIMIT.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::batchByteUnits` — CFG-0220 {#cfg-0220}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate opaque-field bytes per batch
- Accepted values: Sum of engaged field.bytes lengths&lt;=batchByteUnits. Zero permits null/empty opaque payloads; nullable engagement is preserved separately.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: measure throws ClientError::RESOURCE_LIMIT before copying the complete offer when this batch dimension exceeds its cap. progress retains the primary failure and returns CLOSING; acknowledgeBatch and user onBatch have not run. Work-credit exhaustion separately returns WORK_LIMIT.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::batchArenaBytes` — CFG-0221 {#cfg-0221}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: fixed arena bytes per batch
- Accepted values: batchArenaBytes contributes to checked batchBytes=sizeof(Batch)+batchArenaBytes. Zero has no separate rejection; any request that cannot fit the remaining fixed arena reaches ExhaustedResource with no fallback and throws ArenaLimit. Required size depends on the complete offer and alignment.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: Fixed-arena exhaustion throws internal ArenaLimit; progress retains ClientError::RESOURCE_LIMIT and returns CLOSING before acknowledgeBatch/onBatch. A genuine host std::bad_alloc separately maps to ALLOCATION_FAILURE and CLOSING.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::batchOwners` — CFG-0222 {#cfg-0222}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneous batch owners
- Accepted values: Effective capacity=min(batchOwners,batchResidentBytes/batchBytes,(residentBytes-fixed)/batchBytes). Zero yields no capacity and prevents a new batch; old selected/retained owners keep reservations.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: At create, insufficient residentBytes for fixed returns RESOURCE_LIMIT and size overflow returns INVALID_CONFIGURATION. A nonempty offer at exhausted effective capacity makes BatchReservation throw RESOURCE_LIMIT; progress retains it and returns CLOSING before acknowledgeBatch/onBatch. This is terminal refusal, not a wait for owners to retire. Releasing batches cannot increase zero effective capacity.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::batchResidentBytes` — CFG-0223 {#cfg-0223}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate bytes of simultaneously live batches
- Accepted values: Available whole batches=floor(batchResidentBytes/batchBytes), also bounded by batchOwners and total resident. Zero permits 0 batches; the divisor includes sizeof(Batch), so is positive.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: At create, insufficient residentBytes for fixed returns RESOURCE_LIMIT and size overflow returns INVALID_CONFIGURATION. A nonempty offer at exhausted effective capacity makes BatchReservation throw RESOURCE_LIMIT; progress retains it and returns CLOSING before acknowledgeBatch/onBatch. This is terminal refusal, not a wait for owners to retire. Releasing batches cannot increase zero effective capacity.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

### `ClientLimits::residentBytes` — CFG-0224 {#cfg-0224}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: total accounted dispatcher and batch bytes
- Accepted values: Requires residentBytes&gt;=fixed as computed from schema/commands/results/metadata. Remaining bytes determine batch capacity. Zero cannot fit fixed; arithmetic overflow separately yields INVALID_CONFIGURATION.
- Read timing: StreamDispatcher::create checks fixed-storage accounting; Producer::submit and measure/copyBatch later consume the captured limits. Caller must supply initialized ClientLimits.
- Mutability and lifetime: The owner captures an immutable construction snapshot; changing it requires a new instance.
- Failure: At create, insufficient residentBytes for fixed returns RESOURCE_LIMIT and size overflow returns INVALID_CONFIGURATION. A nonempty offer at exhausted effective capacity makes BatchReservation throw RESOURCE_LIMIT; progress retains it and returns CLOSING before acknowledgeBatch/onBatch. This is terminal refusal, not a wait for owners to retire. Releasing batches cannot increase zero effective capacity.
- Consumer sources: [include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp), [src/client/stream_dispatcher.cpp](../../src/client/stream_dispatcher.cpp).

## DispatchWork {#cfg-0228-owner}

[include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp)

### `DispatchWork::operations` — CFG-0228 {#cfg-0228}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining public progress attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `DispatchWork::commands` — CFG-0229 {#cfg-0229}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining ordinary application/cancellation actions.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `DispatchWork::callbacks` — CFG-0230 {#cfg-0230}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining consumer invocations, including throwing invocations.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `DispatchWork::records` — CFG-0231 {#cfg-0231}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining context, offer, copy and disposal metadata visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `DispatchWork::fields` — CFG-0232 {#cfg-0232}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining typed/legacy field visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `DispatchWork::textUnits` — CFG-0233 {#cfg-0233}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining examined/copied symbol and field UTF-16 units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `DispatchWork::bytes` — CFG-0234 {#cfg-0234}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining fixed initialization, arena copy and conservative release byte work.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `DispatchWork::desired` — CFG-0235 {#cfg-0235}

- Type: `core::SubscriptionWork`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Existing atomic desired-state mutation credits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `DispatchWork::collector` — CFG-0236 {#cfg-0236}

- Type: `collectors::StreamWork`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Existing collector staging/offer/ack/release credits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

## SubmitWork {#cfg-0225-owner}

[include/qd/client/stream_dispatcher.hpp](../../include/qd/client/stream_dispatcher.hpp)

### `SubmitWork::operations` — CFG-0225 {#cfg-0225}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining command submission attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `SubmitWork::visits` — CFG-0226 {#cfg-0226}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining input, slot search, initialization and disposal visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

### `SubmitWork::textUnits` — CFG-0227 {#cfg-0227}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining exact examined/copied UTF-16 units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ClientError, CommandStatus`.

## ReceiverLimits {#cfg-0237-owner}

[include/qd/client/subscription_receiver.hpp](../../include/qd/client/subscription_receiver.hpp)

### `ReceiverLimits::bindings` — CFG-0237 {#cfg-0237}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Maximum prepared routes, including zero for a generic-only receiver.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Maximum prepared routes, including zero for a generic-only receiver.
- Read timing: Explicit argument of public operations: [[nodiscard]] std::expected&lt;mux::MuxHandle, ReceiverFailure&gt; openSubscriptionReceiver(mux::MuxQueue &amp;queue, const core::LocalSchema &amp;schema, core::SubscriptionFamily family, core::SubscriptionSelectorPolicy policy, std::span&lt;const PreparedSubscriptionBinding&gt; bindings, const std::shared_ptr&lt;SubscriptionConsumer&gt; &amp;consumer, ReceiverLimits receiverLimits, mux::ClientLimits clientLimits, ReceiverWork &amp;receiverWork, mux::QueueWork &amp;queueWork)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: ReceiverError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `ReceiverError`.

### `ReceiverLimits::shapeFields` — CFG-0238 {#cfg-0238}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete-shape fields across bindings
- Accepted values: Sum of complete record.fields.size() across bindings must fit, including unselected extra fields. Zero permits zero consumption only; each binding debits the remainder.
- Read timing: openObservedSubscriptionReceiver checks/debits its local receiverLimits copy for each binding before publishing a plan.
- Mutability and lifetime: ReceiverLimits are passed by value to openObservedSubscriptionReceiver and debited while preparing one plan. The plan retains prepared routes and their dimensions, not the remaining budgets. A later open may pass another set.
- Failure: std::expected: ReceiverFailure{ReceiverError::RESOURCE_LIMIT,bindingIndex}. Source shape/key/schema checks have separate causes.
- Consumer sources: [include/qd/client/subscription_receiver.hpp](../../include/qd/client/subscription_receiver.hpp), [src/client/subscription_receiver.cpp](../../src/client/subscription_receiver.cpp).

### `ReceiverLimits::selections` — CFG-0239 {#cfg-0239}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: selections reserved across bindings
- Accepted values: Sum of binding.mappingLimits.selections must fit. Each mapping declared limit is debited rather than its actual selected-field count; zero admits zero reservations only.
- Read timing: openObservedSubscriptionReceiver checks/debits its local receiverLimits copy for each binding before publishing a plan.
- Mutability and lifetime: ReceiverLimits are passed by value to openObservedSubscriptionReceiver and debited while preparing one plan. The plan retains prepared routes and their dimensions, not the remaining budgets. A later open may pass another set.
- Failure: std::expected: ReceiverFailure{ReceiverError::RESOURCE_LIMIT,bindingIndex}. Source shape/key/schema checks have separate causes.
- Consumer sources: [include/qd/client/subscription_receiver.hpp](../../include/qd/client/subscription_receiver.hpp), [src/client/subscription_receiver.cpp](../../src/client/subscription_receiver.cpp).

## ReceiverProjectionWork {#cfg-0242-owner}

[include/qd/client/subscription_receiver.hpp](../../include/qd/client/subscription_receiver.hpp)

### `ReceiverProjectionWork::visits` — CFG-0242 {#cfg-0242}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Index, route, complete shape, declared selection allowance and timestamp inspection.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ReceiverError`.

### `ReceiverProjectionWork::textUnits` — CFG-0243 {#cfg-0243}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservative per-route owned text allowance, not measured output length.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ReceiverError`.

### `ReceiverProjectionWork::bytes` — CFG-0244 {#cfg-0244}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservative logical result/text copy work, excluding allocator overhead and RSS.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ReceiverError`.

## ReceiverWork {#cfg-0240-owner}

[include/qd/client/subscription_receiver.hpp](../../include/qd/client/subscription_receiver.hpp)

### `ReceiverWork::visits` — CFG-0240 {#cfg-0240}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Input, duplicate-route and aggregate-plan inspections.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ReceiverError`.

### `ReceiverWork::preparationWork` — CFG-0241 {#cfg-0241}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservative sum of each mapper's declared count/unit/comparison allowances.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `ReceiverError`.
