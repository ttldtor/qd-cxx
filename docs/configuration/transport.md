# Transport and recovery

[Русский](transport.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [RecoveryBlueprintCapacity](transport.md#cfg-0687-owner) — `include/qd/transport/recovery.hpp`
- [RecoveryCapacity](transport.md#cfg-0722-owner) — `include/qd/transport/recovery.hpp`
- [RecoveryDns](transport.md#cfg-0698-owner) — `include/qd/transport/recovery.hpp`
- [RecoveryLimits](transport.md#cfg-0692-owner) — `include/qd/transport/recovery.hpp`
- [RecoveryOptions](transport.md#cfg-0701-owner) — `include/qd/transport/recovery.hpp`
- [RecoveryPreparationWork](transport.md#cfg-0714-owner) — `include/qd/transport/recovery.hpp`
- [RecoveryQueues](transport.md#cfg-0707-owner) — `include/qd/transport/recovery.hpp`
- [RecoveryReplayLimits](transport.md#cfg-0690-owner) — `include/qd/transport/recovery.hpp`
- [RecoverySchemaCapacity](transport.md#cfg-0683-owner) — `include/qd/transport/recovery.hpp`
- [RecoveryTurnWork](transport.md#cfg-0717-owner) — `include/qd/transport/recovery.hpp`
- [RecoveryWork](transport.md#cfg-0710-owner) — `include/qd/transport/recovery.hpp`
- [TcpListenerOptions](transport.md#cfg-0734-owner) — `include/qd/transport/tcp_listener.hpp`
- [DnsServer](transport.md#cfg-0742-owner) — `include/qd/transport/tcp_runtime.hpp`
- [TcpLimits](transport.md#cfg-0743-owner) — `include/qd/transport/tcp_runtime.hpp`
- [TcpOptions](transport.md#cfg-0750-owner) — `include/qd/transport/tcp_runtime.hpp`
- [TcpWork](transport.md#cfg-0761-owner) — `include/qd/transport/tcp_runtime.hpp`
- [TlsHostLimits](transport.md#cfg-0773-owner) — `include/qd/transport/tls_resources.hpp`
- [TlsRequirements](transport.md#cfg-0804-owner) — `include/qd/transport/tls_resources.hpp`
- [TlsResourceLimits](transport.md#cfg-0768-owner) — `include/qd/transport/tls_resources.hpp`
- [TlsSdkRequirements](transport.md#cfg-0799-owner) — `include/qd/transport/tls_resources.hpp`
- [TlsWork](transport.md#cfg-0797-owner) — `include/qd/transport/tls_resources.hpp`
- [TlsWorkShape](transport.md#cfg-0794-owner) — `include/qd/transport/tls_resources.hpp`
- [TlsWorkVector](transport.md#cfg-0793-owner) — `include/qd/transport/tls_resources.hpp`
- [TlsDnsOptions](transport.md#cfg-0813-owner) — `include/qd/transport/tls_runtime.hpp`
- [TlsOptions](transport.md#cfg-0815-owner) — `include/qd/transport/tls_runtime.hpp`

## RecoveryBlueprintCapacity {#cfg-0687-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryBlueprintCapacity::records` — CFG-0687 {#cfg-0687}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: OutgoingRecord slots of a fresh copy
- Accepted values: For blueprintCopy.records, measured demand for one fresh copy must fit; adoptedBlueprint.records separately checks the actual capacity of supplied vectors/strings. 0 permits only zero demand.
- Read timing: Read by inspectRecoveryCopies before SubscriptionWriter creation; the same type is used for separate copied and adopted coordinates.
- Mutability and lifetime: Immutable in RecoveryOptions; the external blueprint transfers only after factory success.
- Failure: Insufficient coordinate capacity is RecoveryError::RESOURCE_LIMIT, stage=recovery-blueprint-capacity; overflow uses recovery-copy-arithmetic.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_copy_profile.cpp](../../src/transport/recovery_copy_profile.cpp).

### `RecoveryBlueprintCapacity::fields` — CFG-0688 {#cfg-0688}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: OutgoingField slots of a fresh copy
- Accepted values: For blueprintCopy.fields, measured demand for one fresh copy must fit; adoptedBlueprint.fields separately checks the actual capacity of supplied vectors/strings. 0 permits only zero demand.
- Read timing: Read by inspectRecoveryCopies before SubscriptionWriter creation; the same type is used for separate copied and adopted coordinates.
- Mutability and lifetime: Immutable in RecoveryOptions; the external blueprint transfers only after factory success.
- Failure: Insufficient coordinate capacity is RecoveryError::RESOURCE_LIMIT, stage=recovery-blueprint-capacity; overflow uses recovery-copy-arithmetic.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_copy_profile.cpp](../../src/transport/recovery_copy_profile.cpp).

### `RecoveryBlueprintCapacity::nameUnits` — CFG-0689 {#cfg-0689}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 capacity of outgoing names including terminators/SSO
- Accepted values: For blueprintCopy.nameUnits, measured demand for one fresh copy must fit; adoptedBlueprint.nameUnits separately checks the actual capacity of supplied vectors/strings. 0 permits only zero demand.
- Read timing: Read by inspectRecoveryCopies before SubscriptionWriter creation; the same type is used for separate copied and adopted coordinates.
- Mutability and lifetime: Immutable in RecoveryOptions; the external blueprint transfers only after factory success.
- Failure: Insufficient coordinate capacity is RecoveryError::RESOURCE_LIMIT, stage=recovery-blueprint-capacity; overflow uses recovery-copy-arithmetic.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_copy_profile.cpp](../../src/transport/recovery_copy_profile.cpp).

## RecoveryCapacity {#cfg-0722-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryCapacity::schemaCopies` — CFG-0722 {#cfg-0722}

- Type: `RecoverySchemaCapacity`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Enforced three-schema peak.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::blueprintCopy` — CFG-0723 {#cfg-0723}

- Type: `RecoveryBlueprintCapacity`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: One independent writer's copy envelope.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::adoptedBlueprint` — CFG-0724 {#cfg-0724}

- Type: `RecoveryBlueprintCapacity`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Actual retained blueprint capacity.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::endpointCopyBytes` — CFG-0725 {#cfg-0725}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Enforced transient settings string capacity.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::replayEntries` — CFG-0726 {#cfg-0726}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Sum of all three fixed banks per enabled family.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::replaySymbolUnits` — CFG-0727 {#cfg-0727}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Sum of all fixed UTF16 slabs.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::replayBytes` — CFG-0728 {#cfg-0728}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Known replay array requests; overlaps the two replay count coordinates.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::queueBytes` — CFG-0729 {#cfg-0729}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Sum of original queue/Mux/collector byte ceilings, excluding desired scopes.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::queues` — CFG-0730 {#cfg-0730}

- Type: `std::array<mux::QueueCapacity, 3>`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Original desired owner/count ceilings remain separate.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::sourceSchemaBytes` — CFG-0731 {#cfg-0731}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Observed borrowed input backing; not a new recovery allocation.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::sourceEndpointTextBytes` — CFG-0732 {#cfg-0732}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Actual borrowed settings string capacities.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryCapacity::writerConfigurationBytes` — CFG-0733 {#cfg-0733}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Actual retained first-writer configuration after tuple preflight.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

## RecoveryDns {#cfg-0698-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryDns::servers` — CFG-0698 {#cfg-0698}

- Type: `std::array<DnsServer, 4>`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit numeric IPv4 server slots, without OS discovery.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryDns::serverCount` — CFG-0699 {#cfg-0699}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: explicit dependency or data input; lifetime must be established by its owner. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Number of supplied slots, in0..4.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Number of supplied slots, in0..4.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryDns::port` — CFG-0700 {#cfg-0700}

- Type: `std::uint16_t`. Declared default: `= 53`
- Classification: explicit dependency or data input; lifetime must be established by its owner. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Shared UDP/TCP DNS port, required nonzero for a named route.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

## RecoveryLimits {#cfg-0692-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryLimits::schemaCopies` — CFG-0692 {#cfg-0692}

- Type: `RecoverySchemaCapacity`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Simultaneous runtime-entry peak, including three schema copies.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryLimits::blueprintCopy` — CFG-0693 {#cfg-0693}

- Type: `RecoveryBlueprintCapacity`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: One fresh writer's copied declaration backing.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryLimits::adoptedBlueprint` — CFG-0694 {#cfg-0694}

- Type: `RecoveryBlueprintCapacity`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual caller vector/string capacity retained on success.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryLimits::endpointCopyBytes` — CFG-0695 {#cfg-0695}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: string bytes of two endpoint-settings instances
- Accepted values: The sum of copiedText(host and TLS referenceIdentity/trustPem when present), multiplied by SETTINGS_PREPARATION_PEAK=2, must fit endpointCopyBytes. SSO and NUL are included. 0 cannot hold nonempty strings.
- Read timing: inspectRecoveryCopies checks before allocation; settingsCopyFits rechecks the actual copy.
- Mutability and lifetime: Fixed in the profile.
- Failure: Insufficient capacity is RecoveryError::RESOURCE_LIMIT, stage=recovery-endpoint-capacity; an actual copy beyond the verified profile is UNSUPPORTED_PROFILE, recovery-schema-copy-profile.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_copy_profile.cpp](../../src/transport/recovery_copy_profile.cpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

### `RecoveryLimits::writerConfigurationBytes` — CFG-0696 {#cfg-0696}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: SubscriptionWriter configuration backing bytes
- Accepted values: Actual writer.storageBytes() ≤ writerConfigurationBytes after writer creation; 0 is not unlimited. Copies separately pass coordinate preflight before creation.
- Read timing: Checked after the first writer and after fresh writer construction on runtime entry.
- Mutability and lifetime: The RecoveryOptions bound is fixed.
- Failure: The first writer.storageBytes() above the bound is RecoveryError::RESOURCE_LIMIT, stage=recovery-writer-adoption. On a later entry, the same check yields RESOURCE_LIMIT, stage=recovery-attempt-writer-adoption.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp), [src/transport/recovery_core.cpp](../../src/transport/recovery_core.cpp).

### `RecoveryLimits::replay` — CFG-0697 {#cfg-0697}

- Type: `std::array<RecoveryReplayLimits, 3>`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: TICKER, STREAM, HISTORY future capacities.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

## RecoveryOptions {#cfg-0701-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryOptions::limits` — CFG-0701 {#cfg-0701}

- Type: `RecoveryLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Independent new backing reservations.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryOptions::heartbeatInterval` — CFG-0702 {#cfg-0702}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds or absent optional
- Accepted values: nullopt disables the corresponding check. A present value must be strictly &gt;0 and ≤INT_MAX milliseconds; 0 and negatives are rejected.
- Read timing: Checked by RecoveryState::create through prepareRecovery and captured in the scheduler; its deadline is updated for absence of locally admitted outgoing output.
- Mutability and lifetime: Immutable within the scheduler; observations move the deadline using the captured interval.
- Failure: An invalid interval is RecoveryConfigError::INVALID_CONFIGURATION, mapped to RecoveryError::INVALID_CONFIGURATION, stage=recovery-liveness.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_state.cpp](../../src/transport/recovery_state.cpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

### `RecoveryOptions::protocolInactivity` — CFG-0703 {#cfg-0703}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds or absent optional
- Accepted values: nullopt disables the corresponding check. A present value must be strictly &gt;0 and ≤INT_MAX milliseconds; 0 and negatives are rejected.
- Read timing: Checked by RecoveryState::create through prepareRecovery and captured in the scheduler; its deadline is updated for absence of validated current input after transport eligibility.
- Mutability and lifetime: Immutable within the scheduler; observations move the deadline using the captured interval.
- Failure: An invalid interval is RecoveryConfigError::INVALID_CONFIGURATION, mapped to RecoveryError::INVALID_CONFIGURATION, stage=recovery-liveness.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_state.cpp](../../src/transport/recovery_state.cpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

### `RecoveryOptions::selectors` — CFG-0704 {#cfg-0704}

- Type: `core::SubscriptionSelectorPolicy`. Declared default: `= core::SubscriptionSelectorPolicy::EXACT_ONLY`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: selector policy of the recovery owner
- Accepted values: Must match every adopted queue policy; the writer admits EXACT_ONLY/CANONICAL_EXACT_V1/CANONICAL_STREAM_ALL_V1 with its family rules.
- Read timing: Passed to every queue validateBinding and to SubscriptionWriter::create during preparation and later writer creation.
- Mutability and lifetime: Fixed for the owner.
- Failure: An incompatible queue is RecoveryError::INVALID_CONFIGURATION, stage=recovery-queue-binding with nested QueueFailure; unsupported writer policy uses recovery-first-writer with the nested error.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

### `RecoveryOptions::dns` — CFG-0705 {#cfg-0705}

- Type: `RecoveryDns`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit finite route; reference identity remains the endpoint TLS identity.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryOptions::requireAllContracts` — CFG-0706 {#cfg-0706}

- Type: `bool`. Declared default: `= false`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Require all three negotiated families before any subscription replay.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: Owner error vocabulary: RecoveryError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

## RecoveryPreparationWork {#cfg-0714-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryPreparationWork::recovery` — CFG-0714 {#cfg-0714}

- Type: `RecoveryWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: New configuration/admission work.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create borrows preparation work; no refunds
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryPreparationWork::writer` — CFG-0715 {#cfg-0715}

- Type: `protocol::SubscriptionWriterWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual first writer's preparation work.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create borrows preparation work; no refunds
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryPreparationWork::queues` — CFG-0716 {#cfg-0716}

- Type: `std::array<mux::QueueWork, 3>`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual queue binding-inspection work by family.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::create borrows preparation work; no refunds
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

## RecoveryQueues {#cfg-0707-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryQueues::ticker` — CFG-0707 {#cfg-0707}

- Type: `std::optional<mux::MuxQueue>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional TICKER MuxQueue owner
- Accepted values: Absence disables this family when requireAllContracts=false; true requires all three. A supplied queue must match the endpoint schema, TICKER family and selectors.
- Read timing: Borrowed by inspectQueues; ownership transfers only after factory success. RecoveryCore uses the stable queue across connection attempts.
- Mutability and lifetime: The adopted queue set is fixed; subscriptions/registrations within a queue change through its explicit APIs.
- Failure: If requireAllContracts=true and a required queue is missing, preparation returns RecoveryError::INVALID_CONFIGURATION, stage=recovery-required-contract-queues. A present incompatible queue yields INVALID_CONFIGURATION, stage=recovery-queue-binding with the original QueueFailure; inspection WORK_LIMIT remains WORK_LIMIT. With requireAllContracts=false, an absent queue for this family is skipped.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

### `RecoveryQueues::stream` — CFG-0708 {#cfg-0708}

- Type: `std::optional<mux::MuxQueue>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional STREAM MuxQueue owner
- Accepted values: Absence disables this family when requireAllContracts=false; true requires all three. A supplied queue must match the endpoint schema, STREAM family and selectors.
- Read timing: Borrowed by inspectQueues; ownership transfers only after factory success. RecoveryCore uses the stable queue across connection attempts.
- Mutability and lifetime: The adopted queue set is fixed; subscriptions/registrations within a queue change through its explicit APIs.
- Failure: If requireAllContracts=true and a required queue is missing, preparation returns RecoveryError::INVALID_CONFIGURATION, stage=recovery-required-contract-queues. A present incompatible queue yields INVALID_CONFIGURATION, stage=recovery-queue-binding with the original QueueFailure; inspection WORK_LIMIT remains WORK_LIMIT. With requireAllContracts=false, an absent queue for this family is skipped.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

### `RecoveryQueues::history` — CFG-0709 {#cfg-0709}

- Type: `std::optional<mux::MuxQueue>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional HISTORY MuxQueue owner
- Accepted values: Absence disables this family when requireAllContracts=false; true requires all three. A supplied queue must match the endpoint schema, HISTORY family and selectors.
- Read timing: Borrowed by inspectQueues; ownership transfers only after factory success. RecoveryCore uses the stable queue across connection attempts.
- Mutability and lifetime: The adopted queue set is fixed; subscriptions/registrations within a queue change through its explicit APIs.
- Failure: If requireAllContracts=true and a required queue is missing, preparation returns RecoveryError::INVALID_CONFIGURATION, stage=recovery-required-contract-queues. A present incompatible queue yields INVALID_CONFIGURATION, stage=recovery-queue-binding with the original QueueFailure; inspection WORK_LIMIT remains WORK_LIMIT. With requireAllContracts=false, an absent queue for this family is skipped.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

## RecoveryReplayLimits {#cfg-0690-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryReplayLimits::entries` — CFG-0690 {#cfg-0690}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: slots in each of three replay banks
- Accepted values: For an enabled family replay.entries ≥ desiredOwners×desiredPerOwner.entries. Each of three banks receives the configured capacity; 0 is valid only with zero future demand. An absent queue skips that family. The grant itself must also permit checked multiplication by 3, cross-family sums and conversion from elements to bytes; a large grant can be unrepresentable.
- Read timing: Checked by inspectQueues before allocation, captured by RecoveryReplay; prepareBank rechecks each snapshot/candidate.
- Mutability and lifetime: Fixed throughout owner lifetime including reconnect.
- Failure: A grant too small for future subscription capacity yields RecoveryError::RESOURCE_LIMIT, stage=recovery-replay-capacity. Overflow of 3×grant, reservation sums or element-byte sizing yields RESOURCE_LIMIT, stage=recovery-capacity-arithmetic before allocation. After preparation, an oversized actual buffer shape in prepareBank yields RESOURCE_LIMIT, stage=recovery-replay-shape.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp), [src/transport/recovery_core_replay.cpp](../../src/transport/recovery_core_replay.cpp).

### `RecoveryReplayLimits::symbolUnits` — CFG-0691 {#cfg-0691}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units in each of three replay slabs
- Accepted values: For an enabled family replay.symbolUnits ≥ desiredOwners×desiredPerOwner.totalSymbolUnits. Each of three banks receives the configured capacity; 0 is valid only with zero future demand. An absent queue skips that family. The grant itself must also permit checked multiplication by 3, cross-family sums and conversion from elements to bytes; a large grant can be unrepresentable.
- Read timing: Checked by inspectQueues before allocation, captured by RecoveryReplay; prepareBank rechecks each snapshot/candidate.
- Mutability and lifetime: Fixed throughout owner lifetime including reconnect.
- Failure: A grant too small for future subscription capacity yields RecoveryError::RESOURCE_LIMIT, stage=recovery-replay-capacity. Overflow of 3×grant, reservation sums or element-byte sizing yields RESOURCE_LIMIT, stage=recovery-capacity-arithmetic before allocation. After preparation, an oversized actual buffer shape in prepareBank yields RESOURCE_LIMIT, stage=recovery-replay-shape.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp), [src/transport/recovery_core_replay.cpp](../../src/transport/recovery_core_replay.cpp).

## RecoverySchemaCapacity {#cfg-0683-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoverySchemaCapacity::records` — CFG-0683 {#cfg-0683}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: LocalRecord slots of simultaneous copies
- Accepted values: The full-schema coordinate multiplied by SCHEMA_RUNTIME_PEAK=3 must be ≤ schemaCopies.records. 0 admits only zero demand in that coordinate.
- Read timing: inspectRecoveryCopies checks it before copying; the admitted profile is used on first and later runtime entries.
- Mutability and lifetime: The owner RecoveryOptions profile is fixed.
- Failure: Insufficient coordinate capacity is RecoveryError::RESOURCE_LIMIT, stage=recovery-schema-capacity; accounting overflow uses stage=recovery-copy-arithmetic. Unsupported copying profiles separately yield UNSUPPORTED_PROFILE.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_copy_profile.cpp](../../src/transport/recovery_copy_profile.cpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

### `RecoverySchemaCapacity::fields` — CFG-0684 {#cfg-0684}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: LocalField slots of simultaneous copies
- Accepted values: The full-schema coordinate multiplied by SCHEMA_RUNTIME_PEAK=3 must be ≤ schemaCopies.fields. 0 admits only zero demand in that coordinate.
- Read timing: inspectRecoveryCopies checks it before copying; the admitted profile is used on first and later runtime entries.
- Mutability and lifetime: The owner RecoveryOptions profile is fixed.
- Failure: Insufficient coordinate capacity is RecoveryError::RESOURCE_LIMIT, stage=recovery-schema-capacity; accounting overflow uses stage=recovery-copy-arithmetic. Unsupported copying profiles separately yield UNSUPPORTED_PROFILE.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_copy_profile.cpp](../../src/transport/recovery_copy_profile.cpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

### `RecoverySchemaCapacity::lookups` — CFG-0685 {#cfg-0685}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: FieldLookup slots of simultaneous copies
- Accepted values: The full-schema coordinate multiplied by SCHEMA_RUNTIME_PEAK=3 must be ≤ schemaCopies.lookups. 0 admits only zero demand in that coordinate.
- Read timing: inspectRecoveryCopies checks it before copying; the admitted profile is used on first and later runtime entries.
- Mutability and lifetime: The owner RecoveryOptions profile is fixed.
- Failure: Insufficient coordinate capacity is RecoveryError::RESOURCE_LIMIT, stage=recovery-schema-capacity; accounting overflow uses stage=recovery-copy-arithmetic. Unsupported copying profiles separately yield UNSUPPORTED_PROFILE.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_copy_profile.cpp](../../src/transport/recovery_copy_profile.cpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

### `RecoverySchemaCapacity::nameUnits` — CFG-0686 {#cfg-0686}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 name capacity including terminators and SSO
- Accepted values: The full-schema coordinate multiplied by SCHEMA_RUNTIME_PEAK=3 must be ≤ schemaCopies.nameUnits. 0 admits only zero demand in that coordinate. Each copied name includes its source length plus UTF16_COPY_PADDING.
- Read timing: inspectRecoveryCopies checks it before copying; the admitted profile is used on first and later runtime entries.
- Mutability and lifetime: The owner RecoveryOptions profile is fixed.
- Failure: Insufficient coordinate capacity is RecoveryError::RESOURCE_LIMIT, stage=recovery-schema-capacity; accounting overflow uses stage=recovery-copy-arithmetic. Unsupported copying profiles separately yield UNSUPPORTED_PROFILE.
- Consumer sources: [include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp), [src/transport/recovery_copy_profile.cpp](../../src/transport/recovery_copy_profile.cpp), [src/transport/recovery_configuration.cpp](../../src/transport/recovery_configuration.cpp).

## RecoveryTurnWork {#cfg-0717-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryTurnWork::recovery` — CFG-0717 {#cfg-0717}

- Type: `RecoveryWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Scheduler observations/actions and bounded replay/control copying.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::poll borrows common/native/session/queue credits only for owner turn
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryTurnWork::writer` — CFG-0718 {#cfg-0718}

- Type: `protocol::SubscriptionWriterWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual fresh-writer creation on a later factory entry.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::poll borrows common/native/session/queue credits only for owner turn
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryTurnWork::runtime` — CFG-0719 {#cfg-0719}

- Type: `TcpWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual native/DNS progress credits, unchanged in meaning.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::poll borrows common/native/session/queue credits only for owner turn
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryTurnWork::session` — CFG-0720 {#cfg-0720}

- Type: `protocol::LiveWork`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual session/factory/composition progress credits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::poll borrows common/native/session/queue credits only for owner turn
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryTurnWork::queues` — CFG-0721 {#cfg-0721}

- Type: `std::array<mux::QueueWork, 3>`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Independent real queue progress/reset/callback credits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpRecovery/TlsRecovery::poll borrows common/native/session/queue credits only for owner turn
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

## RecoveryWork {#cfg-0710-owner}

[include/qd/transport/recovery.hpp](../../include/qd/transport/recovery.hpp)

### `RecoveryWork::operations` — CFG-0710 {#cfg-0710}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Recovery operation/action attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryWork::visits` — CFG-0711 {#cfg-0711}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Metadata inspection, comparison, initialization and disposal visits.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryWork::textUnits` — CFG-0712 {#cfg-0712}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Examined/copied UTF16 or ASCII units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

### `RecoveryWork::bytes` — CFG-0713 {#cfg-0713}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit new array/control initialization and disposal work.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: TcpRecovery/TlsRecovery::create prepares immutable copy/replay/liveness/DNS candidates before transactional input adoption
- Mutability and lifetime: Existing recovery control methods do not refill attempts or replace configuration; poll work references do not escape turn
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `RecoveryError`.

## TcpListenerOptions {#cfg-0734-owner}

[include/qd/transport/tcp_listener.hpp](../../include/qd/transport/tcp_listener.hpp)

### `TcpListenerOptions::MAX_CONNECTIONS` — CFG-0734 {#cfg-0734}

- Type: `static constexpr std::size_t`. Declared default: `= 4096`
- Classification: fixed constant; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Maximum accepted descriptor slots per listener.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpListener, TcpListenerDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, TcpListenerOptions options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpIoStatus, TcpListenerError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpIoStatus, TcpListenerError`.

### `TcpListenerOptions::address` — CFG-0735 {#cfg-0735}

- Type: `std::string_view`. Declared default: `= "127.0.0.1"`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of a numeric IPv4/IPv6 address
- Accepted values: Nonempty, length&lt;INET6_ADDRSTRLEN, no embedded NUL; inet_pton(AF_INET/AF_INET6) must accept the text. DNS names and URIs are not accepted.
- Read timing: Checked and parsed by TcpListener::create before bind.
- Mutability and lifetime: The listener address is fixed; the source view is used synchronously during create.
- Failure: Invalid length/empty/NUL is TcpListenerError::INVALID_CONFIGURATION, listener-options; invalid numeric address is INVALID_CONFIGURATION, listener-address; actual bind failure is BIND_FAILURE.
- Consumer sources: [include/qd/transport/tcp_listener.hpp](../../include/qd/transport/tcp_listener.hpp), [src/transport/tcp_listener.cpp](../../src/transport/tcp_listener.cpp).

### `TcpListenerOptions::port` — CFG-0736 {#cfg-0736}

- Type: `std::uint16_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: local TCP port
- Accepted values: 0 is forwarded to bind as an OS-assigned-port request; it is not separately forbidden. Other uint16 values are passed through htons with no additional range validator. The actual bound port is read with getsockname.
- Read timing: Read during create when building sockaddr; boundPort is obtained after bind.
- Mutability and lifetime: Fixed after listener bind.
- Failure: There is no local range error for this field. Failed bind yields TcpListenerError::BIND_FAILURE, listener-bind; failed getsockname is SOCKET_FAILURE, listener-local-address.
- Consumer sources: [include/qd/transport/tcp_listener.hpp](../../include/qd/transport/tcp_listener.hpp), [src/transport/tcp_listener.cpp](../../src/transport/tcp_listener.cpp).

### `TcpListenerOptions::backlog` — CFG-0737 {#cfg-0737}

- Type: `int`. Declared default: `= 16`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive requested kernel listen backlog.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive requested kernel listen backlog.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpListener, TcpListenerDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, TcpListenerOptions options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpIoStatus, TcpListenerError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpIoStatus, TcpListenerError`.

### `TcpListenerOptions::maxConnections` — CFG-0738 {#cfg-0738}

- Type: `std::size_t`. Declared default: `= 16`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Accepted socket slots allocated at creation, in 1..MAX_CONNECTIONS.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Accepted socket slots allocated at creation, in 1..MAX_CONNECTIONS.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpListener, TcpListenerDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, TcpListenerOptions options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpIoStatus, TcpListenerError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpIoStatus, TcpListenerError`.

### `TcpListenerOptions::maxReadBytes` — CFG-0739 {#cfg-0739}

- Type: `std::size_t`. Declared default: `= 64 * 1024`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive per-call receive cap, at most INT_MAX.
- Units: bytes; scope defined by field contract
- Declared constraints: Positive per-call receive cap, at most INT_MAX.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpListener, TcpListenerDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, TcpListenerOptions options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpIoStatus, TcpListenerError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpIoStatus, TcpListenerError`.

### `TcpListenerOptions::maxWriteBytes` — CFG-0740 {#cfg-0740}

- Type: `std::size_t`. Declared default: `= 64 * 1024`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive per-call send cap, at most INT_MAX.
- Units: bytes; scope defined by field contract
- Declared constraints: Positive per-call send cap, at most INT_MAX.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpListener, TcpListenerDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, TcpListenerOptions options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpIoStatus, TcpListenerError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpIoStatus, TcpListenerError`.

### `TcpListenerOptions::noDelay` — CFG-0741 {#cfg-0741}

- Type: `bool`. Declared default: `= false`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Disable Nagle for accepted connections.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpListener, TcpListenerDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, TcpListenerOptions options = {})
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpIoStatus, TcpListenerError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpIoStatus, TcpListenerError`.

## DnsServer {#cfg-0742-owner}

[include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp)

### `DnsServer::address` — CFG-0742 {#cfg-0742}

- Type: `std::array<std::uint8_t, 4>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: explicit dependency or data input; lifetime must be established by its owner. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Network-order address octets; the all-zero address is rejected.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TcpOptions supplied to runtime create; retained endpoint/server values copied into fixed owned storage
- Mutability and lifetime: existing run uses copied explicit limits/resolvers; no aggregate live replacement route
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

## TcpLimits {#cfg-0743-owner}

[include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp)

### `TcpLimits::inputBytes` — CFG-0743 {#cfg-0743}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of complete owned read buffer
- Accepted values: Strictly 1..INT_MAX; 0 is rejected. Full capacity participates in reservation and is precharged from socketBytes for each read.
- Read timing: TcpRuntime::create forwards to RuntimeCore; the profile is validated before native resources and copied into the owner. Reads/writes then use stored bounds.
- Mutability and lifetime: Fixed for a run; a new run gets its own profile.
- Failure: Outside the domain is TcpError::INVALID_CONFIGURATION, runtime-configuration; insufficient aggregate capacity is RESOURCE_LIMIT, runtime-reservation.
- Consumer sources: [include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp), [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp).

### `TcpLimits::writeBytes` — CFG-0744 {#cfg-0744}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes per socket write
- Accepted values: Strictly 1..INT_MAX; write selects min(writeBytes,remaining wireBytes). 0 is rejected.
- Read timing: TcpRuntime::create forwards to RuntimeCore; the profile is validated before native resources and copied into the owner. Reads/writes then use stored bounds.
- Mutability and lifetime: Fixed for a run; a new run gets its own profile.
- Failure: Outside the domain is TcpError::INVALID_CONFIGURATION, runtime-configuration. Partial writing preserves the remainder; a small positive value is not itself an error.
- Consumer sources: [include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp), [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp).

### `TcpLimits::addresses` — CFG-0745 {#cfg-0745}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Eligible deduplicated endpoint capacity, in 1..64.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Eligible deduplicated endpoint capacity, in 1..64.
- Read timing: TcpOptions supplied to runtime create; retained endpoint/server values copied into fixed owned storage
- Mutability and lifetime: existing run uses copied explicit limits/resolvers; no aggregate live replacement route
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpLimits::cnameDepth` — CFG-0746 {#cfg-0746}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Maximum followed in-response CNAME links, in 1..16.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Maximum followed in-response CNAME links, in 1..16.
- Read timing: TcpOptions supplied to runtime create; retained endpoint/server values copied into fixed owned storage
- Mutability and lifetime: existing run uses copied explicit limits/resolvers; no aggregate live replacement route
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpLimits::contextBytes` — CFG-0747 {#cfg-0747}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of fixed Asio context/service arena
- Accepted values: Strictly &gt;0; no separate numeric upper threshold exists. Must fit safe reservation arithmetic and actually hold requested Asio blocks without fallback.
- Read timing: TcpRuntime::create forwards to RuntimeCore; the profile is validated before native resources and copied into the owner. Reads/writes then use stored bounds.
- Mutability and lifetime: Fixed for a run; a new run gets its own profile.
- Failure: 0 is INVALID_CONFIGURATION; insufficient total reservation is RESOURCE_LIMIT. FixedArena exhaustion throws bad_alloc and maps to ALLOCATION_FAILURE, runtime-storage.
- Consumer sources: [include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp), [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp), [src/transport/fixed_arena.hpp](../../src/transport/fixed_arena.hpp).

### `TcpLimits::handlerBytes` — CFG-0748 {#cfg-0748}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of reusable Asio handler arena
- Accepted values: Strictly &gt;0; upper admissibility follows safe reservation arithmetic. Actual placement includes alignment and the independent MAX_BLOCKS bound.
- Read timing: TcpRuntime::create forwards to RuntimeCore; the profile is validated before native resources and copied into the owner. Reads/writes then use stored bounds.
- Mutability and lifetime: Fixed for a run; a new run gets its own profile.
- Failure: 0 is INVALID_CONFIGURATION; insufficient aggregate reservation is RESOURCE_LIMIT. FixedArena exhaustion during progress is ALLOCATION_FAILURE, operation-storage.
- Consumer sources: [include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp), [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp), [src/transport/fixed_arena.hpp](../../src/transport/fixed_arena.hpp).

### `TcpLimits::residentBytes` — CFG-0749 {#cfg-0749}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of conservative complete run allowance
- Accepted values: Must hold RuntimeCore/bootstrap+input/context/handler, DNS envelope for names and sessionLimits.residentBytes. createBytes accounts for externalBytes instead of the session. 0 cannot hold mandatory structures.
- Read timing: TcpRuntime::create forwards to RuntimeCore; the profile is validated before native resources and copied into the owner. Reads/writes then use stored bounds.
- Mutability and lifetime: Fixed for a run; a new run gets its own profile.
- Failure: Insufficient sum or reservation overflow is TcpError::RESOURCE_LIMIT, runtime-reservation.
- Consumer sources: [include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp), [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp).

## TcpOptions {#cfg-0750-owner}

[include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp)

### `TcpOptions::endpoint` — CFG-0750 {#cfg-0750}

- Type: `std::string_view`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: ASCII bytes of endpoint address/name
- Accepted values: Nonempty text of length ≤MAX_HOST_BYTES+1=254 without NUL,%,/,@,[,]. Numeric IPv4/IPv6 is parsed by make_address. Otherwise, a DNS name after an optional final dot is ≤253 characters with 1..63-byte labels, ASCII letters/digits/interior hyphens; explicit dnsServers and dnsPort are required.
- Read timing: Checked before allocation and copied into fixed host storage by the constructor. Names use explicit DNS resolution; numeric routes bypass DNS.
- Mutability and lifetime: Fixed for the run; the source string_view is not retained after successful creation.
- Failure: Invalid general form is TcpError::INVALID_CONFIGURATION, runtime-configuration; invalid DNS name/route is INVALID_CONFIGURATION, dns-configuration. Later DNS/connection failure belongs to actual I/O.
- Consumer sources: [include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp), [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp), [src/transport/dns_resolver.cpp](../../src/transport/dns_resolver.cpp).

### `TcpOptions::port` — CFG-0751 {#cfg-0751}

- Type: `std::uint16_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Numeric remote TCP port, in 1..65535; no service-name lookup.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Numeric remote TCP port, in 1..65535; no service-name lookup.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpRuntime, TcpDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, core::LocalSchema &amp;&amp;schema, protocol::SubscriptionWriter &amp;&amp;composer, protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpOptions::dnsServers` — CFG-0752 {#cfg-0752}

- Type: `std::span<const DnsServer>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit 1..4 IPv4 servers for names; numeric endpoints bypass DNS.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Explicit 1..4 IPv4 servers for names; numeric endpoints bypass DNS.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpRuntime, TcpDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, core::LocalSchema &amp;&amp;schema, protocol::SubscriptionWriter &amp;&amp;composer, protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpOptions::dnsPort` — CFG-0753 {#cfg-0753}

- Type: `std::uint16_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: numeric UDP/TCP DNS port
- Accepted values: A named endpoint requires a nonzero uint16 port, yielding 1..65535 at use. Numeric endpoints bypass DNS and do not independently reject dnsPort=0.
- Read timing: Checked after numeric address parsing only in the named branch; captured in options and forwarded to DnsResolver.
- Mutability and lifetime: Fixed for the run.
- Failure: 0 for a named endpoint is TcpError::INVALID_CONFIGURATION, dns-configuration. For a numeric endpoint this field causes no refusal.
- Consumer sources: [include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp), [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp).

### `TcpOptions::dnsTimeout` — CFG-0754 {#cfg-0754}

- Type: `std::chrono::milliseconds`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Whole two-family lookup deadline, positive and representable.
- Units: milliseconds
- Declared constraints: Whole two-family lookup deadline, positive and representable.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpRuntime, TcpDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, core::LocalSchema &amp;&amp;schema, protocol::SubscriptionWriter &amp;&amp;composer, protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpOptions::connectTimeout` — CFG-0755 {#cfg-0755}

- Type: `std::chrono::milliseconds`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Whole serial connection-attempt deadline, positive.
- Units: milliseconds
- Declared constraints: Whole serial connection-attempt deadline, positive.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpRuntime, TcpDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, core::LocalSchema &amp;&amp;schema, protocol::SubscriptionWriter &amp;&amp;composer, protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpOptions::readTimeout` — CFG-0756 {#cfg-0756}

- Type: `std::chrono::milliseconds`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Connected input inactivity deadline, positive.
- Units: milliseconds
- Declared constraints: Connected input inactivity deadline, positive.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpRuntime, TcpDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, core::LocalSchema &amp;&amp;schema, protocol::SubscriptionWriter &amp;&amp;composer, protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpOptions::writeTimeout` — CFG-0757 {#cfg-0757}

- Type: `std::chrono::milliseconds`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Pending-output inactivity deadline, positive.
- Units: milliseconds
- Declared constraints: Pending-output inactivity deadline, positive.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpRuntime, TcpDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, core::LocalSchema &amp;&amp;schema, protocol::SubscriptionWriter &amp;&amp;composer, protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpOptions::pollInterval` — CFG-0758 {#cfg-0758}

- Type: `std::chrono::milliseconds`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive caller-owned DNS/socket progress cadence; no I/O notifier.
- Units: milliseconds
- Declared constraints: Positive caller-owned DNS/socket progress cadence; no I/O notifier.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpRuntime, TcpDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, core::LocalSchema &amp;&amp;schema, protocol::SubscriptionWriter &amp;&amp;composer, protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpOptions::limits` — CFG-0759 {#cfg-0759}

- Type: `TcpLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Required finite socket, SDK and aggregate resource ceilings.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpRuntime, TcpDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, core::LocalSchema &amp;&amp;schema, protocol::SubscriptionWriter &amp;&amp;composer, protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpOptions::socketSendBufferBytes` — CFG-0760 {#cfg-0760}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: SO_SNDBUF request, in0..INT_MAX; zero keeps the OS default.
- Units: bytes; scope defined by field contract
- Declared constraints: SO_SNDBUF request, in0..INT_MAX; zero keeps the OS default.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TcpRuntime, TcpDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, core::LocalSchema &amp;&amp;schema, protocol::SubscriptionWriter &amp;&amp;composer, protocol::LiveLimits sessionLimits, TcpOptions options, protocol::LiveWork &amp;work)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TcpError, TcpPollStatus; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

## TcpWork {#cfg-0761-owner}

[include/qd/transport/tcp_runtime.hpp](../../include/qd/transport/tcp_runtime.hpp)

### `TcpWork::steps` — CFG-0761 {#cfg-0761}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining bounded public progress-step attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpWork::handlers` — CFG-0762 {#cfg-0762}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining internal Asio handler dispatches.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpWork::socketBytes` — CFG-0763 {#cfg-0763}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Conservative peer read/write buffer bytes admitted before each native operation.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpWork::dnsCalls` — CFG-0764 {#cfg-0764}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining native DNS socket callback attempts, shared across all descriptors.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpWork::dnsPackets` — CFG-0765 {#cfg-0765}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining successful DNS receive operations.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpWork::dnsBytes` — CFG-0766 {#cfg-0766}

- Type: `std::size_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining DNS receive/send bytes; complete UDP datagrams are preserved.
- Units: bytes; scope defined by field contract
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

### `TcpWork::dnsUnits` — CFG-0767 {#cfg-0767}

- Type: `std::uint64_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Remaining conservative complete-response parser/copy work units.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: explicit call/turn work input; reset only at documented owner boundary
- Mutability and lifetime: caller supplies remaining credits to operation/turn; no implied automatic refund or per-poll refill
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TcpError, TcpPollStatus`.

## TlsHostLimits {#cfg-0773-owner}

[include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp)

### `TlsHostLimits::namemapGroups` — CFG-0773 {#cfg-0773}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: ambient name-map groups/attempt/orphan highwater
- Accepted values: Locally namemapGroups≤INT_MAX; 0 passes this numeric check. It must remain a truthful highwater for the complete shared SDK image.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; The nameMap formula includes 152×groups and 104×min(groups,2048), plus fixed terms.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Violating the stated field-specific numeric predicate is TlsResourceError::INVALID_CONFIGURATION, stage tls.resource.limits. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.host/bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::queryCount` — CFG-0774 {#cfg-0774}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: historically distinct SDK queries
- Accepted values: Locally queryCount≥2; 0/1 are rejected. No separate scalar upper comparison exists; all formula arithmetic is checked.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Includes the two internal queries and participates in query hash and entry/bucket sizing.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Violating the stated field-specific numeric predicate is TlsResourceError::INVALID_CONFIGURATION, stage tls.resource.limits. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::queryBytes` — CFG-0775 {#cfg-0775}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of the largest stored query excluding NUL
- Accepted values: Locally queryBytes≥16; 0..15 are rejected. There is no separate upper threshold; formulas check overflow.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Multiplied by queryCount for retained queries and also included in current temporary storage.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Violating the stated field-specific numeric predicate is TlsResourceError::INVALID_CONFIGURATION, stage tls.resource.limits. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::propertyNames` — CFG-0776 {#cfg-0776}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: SDK property-name atom highwater
- Accepted values: Accepts 6≤propertyNames≤INT_MAX; the lower bound includes built-in names.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Participates in hash/stack storage, atom metadata and candidate names.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Violating the stated field-specific numeric predicate is TlsResourceError::INVALID_CONFIGURATION, stage tls.resource.limits. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.host/bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::propertyValues` — CFG-0777 {#cfg-0777}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: SDK property-value atom highwater
- Accepted values: Accepts 22≤propertyValues≤INT_MAX; the lower bound includes built-in values.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Participates in hash/stack storage, atom metadata and candidate values.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Violating the stated field-specific numeric predicate is TlsResourceError::INVALID_CONFIGURATION, stage tls.resource.limits. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.host/bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::propertyStringBytes` — CFG-0778 {#cfg-0778}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate stored atom-string bytes including NUL
- Accepted values: There is no independent scalar range or comparison against propertyStringMaxBytes. 0 is numerically admitted but must be a truthful bound; formula arithmetic is checked.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Appears as a separate term in retained property storage.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: The field has no independent numeric-range error. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::propertyStringMaxBytes` — CFG-0779 {#cfg-0779}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of the largest atom string including NUL
- Accepted values: No independent scalar range or comparison to aggregate propertyStringBytes exists. 0 declares no such storage, subject to the external truthfulness obligation.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Included in the candidate-atom size while the old set still exists.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: The field has no independent numeric-range error. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::participatingThreads` — CFG-0780 {#cfg-0780}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: participating shared-SDK threads
- Accepted values: participatingThreads≤INT_MAX and participatingThreads≥errorThreads≥1. Thus 0 is rejected by the cross-field check.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Sizes both stack scopes and the thread-pointer array.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Violating the stated field-specific numeric predicate is TlsResourceError::INVALID_CONFIGURATION, stage tls.resource.limits. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.host/bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::globalThreadHandlers` — CFG-0781 {#cfg-0781}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: shared live thread-handler nodes including candidates
- Accepted values: No separate scalar bound exists; 0 declares no nodes by the embedding application. The qualification-coordinate sum and resource arithmetic are checked.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Thread storage includes 32×globalThreadHandlers.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: The field has no independent numeric-range error. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.host/bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::ownerThreadHandlers` — CFG-0782 {#cfg-0782}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: handler nodes on the serialized owner thread
- Accepted values: There is no separate scalar bound or comparison to globalThreadHandlers. The value enters qualification shape and the checked metadata sum; actual external nodes are not enumerated.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; This module has no separate ownerThreadHandlers byte coefficient; the field is retained in qualification and bounds a transitive-work premise.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: The field has no independent numeric-range error. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.host. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::ambientThreadPairs` — CFG-0783 {#cfg-0783}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: dependent ambient context/thread pairs
- Accepted values: Checked 2×ambientThreadPairs must be representable; ambientGenerators≤2×ambientThreadPairs. 0 requires ambientGenerators=0.
- Read timing: tlsRequirements directly reads ambientThreadPairs for overflow-checked maxGenerators=2×ambientThreadPairs and the host.ambientGenerators bound. The field is also captured in TlsWorkShape. sharedBytes does not directly read the pair count; generator storage depends on declared ambientGenerators.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Overflow of 2×ambientThreadPairs yields TlsResourceError::ARITHMETIC_OVERFLOW, stage=tls.resource.host. ambientGenerators&gt;2×ambientThreadPairs yields INVALID_CONFIGURATION, stage=tls.resource.generators. The formula creates no direct tls.resource.bytes failure from the numerical pairs value; the application ensures the actual external pair count is truthful.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::ambientGenerators` — CFG-0784 {#cfg-0784}

- Type: `std::uint64_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Ambient public/private generators; at most twice the pair bound.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Ambient public/private generators; at most twice the pair bound.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: host qualification obligations remain truthful through final SDK retirement; not live SDK registry mutation
- Failure: Owner error vocabulary: TlsResourceError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsHostLimits::concurrentEntries` — CFG-0785 {#cfg-0785}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: concurrently admitted SDK/initialization entries
- Accepted values: concurrentEntries≥1; 0 is rejected. Upper admissibility follows checked products in the complete model.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Multiplies current ambient storage and an independent temporary ERR-slot scope.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Violating the stated field-specific numeric predicate is TlsResourceError::INVALID_CONFIGURATION, stage tls.resource.limits. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::errorThreads` — CFG-0786 {#cfg-0786}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: threads with retained ERR state
- Accepted values: 1≤errorThreads≤participatingThreads. 0 and exceeding the participating-thread count are rejected.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Multiplies fixed ERR structures, buffers and per-thread metadata.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Violating the stated field-specific numeric predicate is TlsResourceError::INVALID_CONFIGURATION, stage tls.resource.limits. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::errorCapacityBytes` — CFG-0787 {#cfg-0787}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate permitted external/preexisting ERR capacities
- Accepted values: There is no independent scalar minimum/maximum. 0 declares no such capacities; truthfulness of external state is a precondition.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Added directly to ERR storage alongside independent fixed components.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: The field has no independent numeric-range error. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::errorSlotCapacityBytes` — CFG-0788 {#cfg-0788}

- Type: `std::uint64_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Maximum one retained ERR data capacity, at most INT_MAX.
- Units: bytes; scope defined by field contract
- Declared constraints: Maximum one retained ERR data capacity, at most INT_MAX.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: host qualification obligations remain truthful through final SDK retirement; not live SDK registry mutation
- Failure: Owner error vocabulary: TlsResourceError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsHostLimits::errorDebugBytes` — CFG-0789 {#cfg-0789}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of retained ERR file/function strings
- Accepted values: No independent scalar minimum/maximum exists. 0 declares no such strings; all model arithmetic is checked.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Added directly to ERR storage independently of errorCapacityBytes.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: The field has no independent numeric-range error. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

### `TlsHostLimits::failedPrivatePrefixes` — CFG-0790 {#cfg-0790}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: reserved failed-private-preparation graphs
- Accepted values: 0 is admitted for an owner with no such residue. No separate scalar maximum exists; TLS recovery additionally requires its own failure-prefix reservation.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Multiplied by PRIVATE_PREFIX_BYTES to account for potentially retained graphs.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: The field has no independent numeric-range error. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked. TLS recovery additionally requires failedPrivatePrefixes≥3; otherwise RecoveryError::INVALID_CONFIGURATION, tls-recovery-host-or-route.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp), [src/transport/tls_recovery.cpp](../../src/transport/tls_recovery.cpp).

### `TlsHostLimits::failedAmbientPrefixes` — CFG-0791 {#cfg-0791}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: reserved failed ambient-generator graphs
- Accepted values: 0 is admitted with no such residue; no separate scalar maximum exists. Truthfulness must hold through final SDK retirement.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Multiplied by AMBIENT_PREFIX_BYTES.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: The field has no independent numeric-range error. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked. TLS recovery additionally requires failedAmbientPrefixes≥4; otherwise RecoveryError::INVALID_CONFIGURATION, tls-recovery-host-or-route.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp), [src/transport/tls_recovery.cpp](../../src/transport/tls_recovery.cpp).

### `TlsHostLimits::errorTableEntries` — CFG-0792 {#cfg-0792}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: shared error-string table highwater
- Accepted values: errorTableEntries≥1773; 0..1772 are rejected. The comment ties the minimum to builtin1773; no separate scalar upper bound exists.
- Read timing: Read by tlsRequirements before SDK actions, copied into TlsWorkShape and adapter qualification; Sizes the shared error table hash and buckets.
- Mutability and lifetime: Declared maxima are fixed; the application must ensure them for every user of the same SDK image until native/SDK retirement.
- Failure: Violating the stated field-specific numeric predicate is TlsResourceError::INVALID_CONFIGURATION, stage tls.resource.limits. Unrepresentable arithmetic using this field yields ARITHMETIC_OVERFLOW, stage tls.resource.bytes. Truthfulness of the external SDK-state declaration is not locally checked.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp).

## TlsRequirements {#cfg-0804-owner}

[include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp)

### `TlsRequirements::shape` — CFG-0804 {#cfg-0804}

- Type: `TlsWorkShape`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Identity shared by every requirement vector below.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsRequirements::sdk` — CFG-0805 {#cfg-0805}

- Type: `TlsSdkRequirements`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Requested backing, with shared ownership explicitly separated.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsRequirements::factory` — CFG-0806 {#cfg-0806}

- Type: `TlsWorkVector`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Factory work excluding its separately owned disposal reserve.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsRequirements::initialHandshake` — CFG-0807 {#cfg-0807}

- Type: `TlsWorkVector`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Once-only initial peer/flight/verification phase permit.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsRequirements::initialStart` — CFG-0808 {#cfg-0808}

- Type: `TlsWorkVector`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Atomic initialHandshake+entry admission before first TLS SDK entry.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsRequirements::entry` — CFG-0809 {#cfg-0809}

- Type: `TlsWorkVector`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: One bounded ordinary SDK entry in the current caller turn.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsRequirements::close` — CFG-0810 {#cfg-0810}

- Type: `TlsWorkVector`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: One orderly TLS-close entry; shutdown zero remains pending.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsRequirements::retirement` — CFG-0811 {#cfg-0811}

- Type: `TlsWorkVector`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Owned request-only abort and eventual SDK graph disposal reserve.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsRequirements::factoryAdmission` — CFG-0812 {#cfg-0812}

- Type: `TlsWorkVector`. Declared default: `{}`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Atomic factory+retirement admission before any SDK allocation.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

## TlsResourceLimits {#cfg-0768-owner}

[include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp)

### `TlsResourceLimits::trustBytes` — CFG-0768 {#cfg-0768}

- Type: `std::uint64_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Whole ordinary certificate PEM bytes, in1..524288.
- Units: bytes; scope defined by field contract
- Declared constraints: Whole ordinary certificate PEM bytes, in1..524288.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: new shape/requirements needed for different profile; returned requirements are observations
- Failure: Owner error vocabulary: TlsResourceError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsResourceLimits::anchors` — CFG-0769 {#cfg-0769}

- Type: `std::uint64_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: All certificate blocks before decoding, in1..256.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: All certificate blocks before decoding, in1..256.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: new shape/requirements needed for different profile; returned requirements are observations
- Failure: Owner error vocabulary: TlsResourceError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsResourceLimits::certificateBytes` — CFG-0770 {#cfg-0770}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of the complete Certificate message body
- Accepted values: Exactly 65536 is accepted; 0 and smaller/larger values are rejected. This is a fixed initial TLS profile value, not a QTP requirement.
- Read timing: tlsRequirements validates before formulas and SDK actions. The value then participates in size/qualified-work calculations; TlsPolicy configures SSL_CTX for 65536.
- Mutability and lifetime: Immutable in session qualification.
- Failure: Any other number is TlsResourceError::INVALID_CONFIGURATION, tls.resource.limits; the adapter maps it to TlsError::INVALID_CONFIGURATION. Calculation overflow after validation is separately ARITHMETIC_OVERFLOW.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp), [src/transport/tls_policy.cpp](../../src/transport/tls_policy.cpp).

### `TlsResourceLimits::certificateRequestBytes` — CFG-0771 {#cfg-0771}

- Type: `std::uint64_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of the complete CertificateRequest message body
- Accepted values: Exactly 65536 is accepted; 0 and smaller/larger values are rejected. This is a fixed initial TLS profile value, not a QTP requirement.
- Read timing: tlsRequirements validates before formulas and SDK actions. The value then participates in size/qualified-work calculations; TlsPolicy configures SSL_CTX for 65536.
- Mutability and lifetime: Immutable in session qualification.
- Failure: Any other number is TlsResourceError::INVALID_CONFIGURATION, tls.resource.limits; the adapter maps it to TlsError::INVALID_CONFIGURATION. Calculation overflow after validation is separately ARITHMETIC_OVERFLOW.
- Consumer sources: [include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp), [src/transport/tls_resources.cpp](../../src/transport/tls_resources.cpp), [src/transport/tls_policy.cpp](../../src/transport/tls_policy.cpp).

### `TlsResourceLimits::identityBytes` — CFG-0772 {#cfg-0772}

- Type: `std::uint64_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Reference identity byte ceiling, in1..255.
- Units: bytes; scope defined by field contract
- Declared constraints: Reference identity byte ceiling, in1..255.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: new shape/requirements needed for different profile; returned requirements are observations
- Failure: Owner error vocabulary: TlsResourceError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

## TlsSdkRequirements {#cfg-0799-owner}

[include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp)

### `TlsSdkRequirements::instanceBytes` — CFG-0799 {#cfg-0799}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Conservative private instance/temporary/disposal envelope.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsSdkRequirements::sharedBytes` — CFG-0800 {#cfg-0800}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Conservative shared host/current/history/ERR envelope.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsSdkRequirements::totalBytes` — CFG-0801 {#cfg-0801}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Checked sum, useful only with its declared ownership scopes.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsSdkRequirements::failedPrivatePrefixBytes` — CFG-0802 {#cfg-0802}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Possible persistent private preparation debt; not an error refund.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsSdkRequirements::failedAmbientPrefixBytes` — CFG-0803 {#cfg-0803}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: returned observation; not a configuration input. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Possible persistent ambient generator debt per failed prefix.
- Units: bytes; scope defined by field contract
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Returned requirements/capacity observation; not a configurable input
- Mutability and lifetime: NOT_APPLICABLE: detached or owned returned observation
- Failure: NOT_APPLICABLE to this field as a setting; querying operation has its own typed failure
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

## TlsWork {#cfg-0797-owner}

[include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp)

### `TlsWork::shape` — CFG-0797 {#cfg-0797}

- Type: `TlsWorkShape`. Declared default: `{}`
- Classification: qualification shape; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Qualification identity for every remaining credit.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: explicit remaining vector debited without automatic refill/refund
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsWork::remaining` — CFG-0798 {#cfg-0798}

- Type: `TlsWorkVector`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Componentwise remaining credits for this explicit caller turn.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: explicit remaining vector debited without automatic refill/refund
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

## TlsWorkShape {#cfg-0794-owner}

[include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp)

### `TlsWorkShape::profileRevision` — CFG-0794 {#cfg-0794}

- Type: `std::uint32_t`. Declared default: `= 0`
- Classification: qualification shape; not a user setting. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. No property-layer resolution applies.
- Declared meaning: Must match the compiled qualified resource model revision.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: new shape/requirements needed for different profile; returned requirements are observations
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsWorkShape::limits` — CFG-0795 {#cfg-0795}

- Type: `TlsResourceLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Copied immutable input ceilings.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: new shape/requirements needed for different profile; returned requirements are observations
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

### `TlsWorkShape::host` — CFG-0796 {#cfg-0796}

- Type: `TlsHostLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Copied persistent same-image host qualification.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: new shape/requirements needed for different profile; returned requirements are observations
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

## TlsWorkVector {#cfg-0793-owner}

[include/qd/transport/tls_resources.hpp](../../include/qd/transport/tls_resources.hpp)

### `TlsWorkVector::units` — CFG-0793 {#cfg-0793}

- Type: `std::array<std::uint64_t, TLS_WORK_UNIT_COUNT>`. Declared default: `{}`
- Classification: work-credit input; consumable work, separate from retained capacity. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Exact coordinate order is TlsWorkUnit.
- Units: units defined by field contract; UTF-16 where stated
- Declared constraints: Finite explicit credits. Exhaustion semantics belong to the owner contract; a scalar capacity range is not established by this entry.
- Read timing: Pure requirements validation binds immutable input/host shape; runtime consumes explicit matching shape and remaining work
- Mutability and lifetime: new shape/requirements needed for different profile; returned requirements are observations
- Failure: component-specific WORK_LIMIT/YIELD/status; exact mapping remains owner-specific
- Additional owner vocabulary (not a field-to-error mapping): `TlsResourceError`.

## TlsDnsOptions {#cfg-0813-owner}

[include/qd/transport/tls_runtime.hpp](../../include/qd/transport/tls_runtime.hpp)

### `TlsDnsOptions::servers` — CFG-0813 {#cfg-0813}

- Type: `std::span<const DnsServer>`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Borrowed 1..4 numeric IPv4 servers for a named route host.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Borrowed 1..4 numeric IPv4 servers for a named route host.
- Read timing: TlsOptions.dns copied at TLS create; numeric route bypasses resolver
- Mutability and lifetime: immutable per-run DNS selection; RecoveryOptions.dns authoritative where recovery wrapper used
- Failure: Owner error vocabulary: TlsError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsError`.

### `TlsDnsOptions::port` — CFG-0814 {#cfg-0814}

- Type: `std::uint16_t`. Declared default: `= 0`
- Classification: explicit dependency or data input; lifetime must be established by its owner. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit DNS port; required for named routes, never read from the OS.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: TlsOptions.dns copied at TLS create; numeric route bypasses resolver
- Mutability and lifetime: immutable per-run DNS selection; RecoveryOptions.dns authoritative where recovery wrapper used
- Failure: Owner error vocabulary: TlsError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsError`.

## TlsOptions {#cfg-0815-owner}

[include/qd/transport/tls_runtime.hpp](../../include/qd/transport/tls_runtime.hpp)

### `TlsOptions::limits` — CFG-0815 {#cfg-0815}

- Type: `TlsResourceLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit proof/input ceilings; actual immutable snapshot must fit.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TlsRuntime, TlsDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, protocol::SubscriptionWriter &amp;&amp;composer, TlsOptions options, TlsWork &amp;tlsWork, protocol::LiveWork &amp;sessionWork)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TlsError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsError`.

### `TlsOptions::host` — CFG-0816 {#cfg-0816}

- Type: `TlsHostLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Persistent truthful same-SDK state, history and concurrency maxima.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TlsRuntime, TlsDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, protocol::SubscriptionWriter &amp;&amp;composer, TlsOptions options, TlsWork &amp;tlsWork, protocol::LiveWork &amp;sessionWork)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TlsError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsError`.

### `TlsOptions::dns` — CFG-0817 {#cfg-0817}

- Type: `TlsDnsOptions`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit route resolver configuration, copied into the common core.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TlsRuntime, TlsDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, protocol::SubscriptionWriter &amp;&amp;composer, TlsOptions options, TlsWork &amp;tlsWork, protocol::LiveWork &amp;sessionWork)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TlsError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsError`.

### `TlsOptions::sdkInstanceBytes` — CFG-0818 {#cfg-0818}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive private SDK allowance, at least the computed requirement.
- Units: bytes; scope defined by field contract
- Declared constraints: Positive private SDK allowance, at least the computed requirement.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TlsRuntime, TlsDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, protocol::SubscriptionWriter &amp;&amp;composer, TlsOptions options, TlsWork &amp;tlsWork, protocol::LiveWork &amp;sessionWork)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TlsError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsError`.

### `TlsOptions::sdkSharedBytes` — CFG-0819 {#cfg-0819}

- Type: `std::size_t`. Declared default: `= 0`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive shared SDK allowance, including preexisting and failed prefixes.
- Units: bytes; scope defined by field contract
- Declared constraints: Positive shared SDK allowance, including preexisting and failed prefixes.
- Read timing: Explicit argument of public operations: [[nodiscard]] static std::expected&lt;TlsRuntime, TlsDiagnostic&gt; create(RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, protocol::SubscriptionWriter &amp;&amp;composer, TlsOptions options, TlsWork &amp;tlsWork, protocol::LiveWork &amp;sessionWork)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: Owner error vocabulary: TlsError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `TlsError`.

### `TlsOptions::residentBytes` — CFG-0820 {#cfg-0820}

- Type: `std::size_t`. Declared default: `= 0`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of complete conservative TLS run allowance
- Accepted values: residentBytes must be &gt;0 and hold adapterBytes+snapshotBytes+settings.budgets.tcp.residentBytes+sdkInstanceBytes+sdkSharedBytes. All sums are checked; 0 is explicitly rejected.
- Read timing: Checked by the TLS factory before activation, after tlsRequirements and actual snapshot-capacity sizing.
- Mutability and lifetime: Fixed for the TLS resource owner.
- Failure: 0 yields TlsError::RESOURCE_LIMIT, tls-sdk-allowance. Overflow while constructing adapterBytes/snapshotBytes yields RESOURCE_LIMIT, tls-resident-arithmetic. Addition overflow in reservation.totalBytes and exceeding options.residentBytes yield RESOURCE_LIMIT, tls-resident-allowance.
- Consumer sources: [include/qd/transport/tls_runtime.hpp](../../include/qd/transport/tls_runtime.hpp), [src/transport/tls_runtime.cpp](../../src/transport/tls_runtime.cpp).
