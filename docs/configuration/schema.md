# JSON schema loading

[Русский](schema.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [BinaryJsonSchemaLimits](schema.md#cfg-0667-owner) — `include/qd/schema/json_schema.hpp`
- [JsonSchemaLimits](schema.md#cfg-0664-owner) — `include/qd/schema/json_schema.hpp`

## BinaryJsonSchemaLimits {#cfg-0667-owner}

[include/qd/schema/json_schema.hpp](../../include/qd/schema/json_schema.hpp)

### `BinaryJsonSchemaLimits::loading` — CFG-0667 {#cfg-0667}

- Type: `JsonSchemaLimits`. Declared default: `{}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Entire input/depth and original core count/name budgets.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Each load*JsonSchema/loadBinaryJsonWireNames call: independent finite document/parser/work/two-copy bounds
- Mutability and lifetime: new explicit load per schema; no file I/O/source resolution/live schema replacement in loader
- Failure: Owner error vocabulary: JsonSchemaError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `JsonSchemaError`.

### `BinaryJsonSchemaLimits::parserBytes` — CFG-0668 {#cfg-0668}

- Type: `std::size_t`. Declared default: `= DEFAULT_BINARY_JSON_PARSER_BYTES`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: conservative DOM parser request bytes
- Accepted values: parserRequestBytes(input.size(),min(loading.depth,input.size())+1) ≤ parserBytes; arithmetic overflow also refuses. 0 cannot hold the mandatory parser model.
- Read timing: Limits are passed by value into loadJsonSchema/loadBinaryJsonSchema; checks precede parser allocation or owned-model staging. The rebind path repeats checks.
- Mutability and lifetime: A new call may receive another profile; a loaded LocalSchema is immutable.
- Failure: Violating the specific resource model is JsonSchemaError::RESOURCE_LIMIT. Invalid JSON separately yields MALFORMED_JSON; allocation admitted by the model may still produce ALLOCATION_FAILURE.
- Consumer sources: [include/qd/schema/json_schema.hpp](../../include/qd/schema/json_schema.hpp), [src/schema/json_schema.cpp](../../src/schema/json_schema.cpp).

### `BinaryJsonSchemaLimits::workUnits` — CFG-0669 {#cfg-0669}

- Type: `std::size_t`. Declared default: `= DEFAULT_BINARY_JSON_WORK_UNITS`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: conservative work units of complete binary JSON loading
- Accepted values: Before parsing, 32×input.size()≤workUnits is required; linear, pairwise and coverage model costs are added later. 0 admits no nonempty input.
- Read timing: Limits are passed by value into loadJsonSchema/loadBinaryJsonSchema; checks precede parser allocation or owned-model staging. The rebind path repeats checks.
- Mutability and lifetime: A new call may receive another profile; a loaded LocalSchema is immutable.
- Failure: Violating the specific resource model is JsonSchemaError::RESOURCE_LIMIT. Invalid JSON separately yields MALFORMED_JSON; allocation admitted by the model may still produce ALLOCATION_FAILURE.
- Consumer sources: [include/qd/schema/json_schema.hpp](../../include/qd/schema/json_schema.hpp), [src/schema/json_schema.cpp](../../src/schema/json_schema.cpp).

### `BinaryJsonSchemaLimits::schemaModelBytes` — CFG-0670 {#cfg-0670}

- Type: `std::size_t`. Declared default: `= DEFAULT_BINARY_JSON_MODEL_BYTES`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of logical schema staging/result model
- Accepted values: FIXED_MODEL+2×oneCopy ≤ schemaModelBytes with overflow checks; includes declarations, names and terminators. Capacity growth/SSO and allocator bookkeeping are explicitly excluded.
- Read timing: Limits are passed by value into loadJsonSchema/loadBinaryJsonSchema; checks precede parser allocation or owned-model staging. The rebind path repeats checks.
- Mutability and lifetime: A new call may receive another profile; a loaded LocalSchema is immutable.
- Failure: Violating the specific resource model is JsonSchemaError::RESOURCE_LIMIT. Invalid JSON separately yields MALFORMED_JSON; allocation admitted by the model may still produce ALLOCATION_FAILURE.
- Consumer sources: [include/qd/schema/json_schema.hpp](../../include/qd/schema/json_schema.hpp), [src/schema/json_schema.cpp](../../src/schema/json_schema.cpp).

### `BinaryJsonSchemaLimits::allowProfileV2` — CFG-0671 {#cfg-0671}

- Type: `bool`. Declared default: `= true`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Select false when the caller promises only binary profile 1.
- Units: boolean
- Declared constraints: Declared boolean domain: false/true; profile cross-constraints remain separate and are not independently revalidated.
- Read timing: Each load*JsonSchema/loadBinaryJsonWireNames call: independent finite document/parser/work/two-copy bounds
- Mutability and lifetime: new explicit load per schema; no file I/O/source resolution/live schema replacement in loader
- Failure: Owner error vocabulary: JsonSchemaError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `JsonSchemaError`.

## JsonSchemaLimits {#cfg-0664-owner}

[include/qd/schema/json_schema.hpp](../../include/qd/schema/json_schema.hpp)

### `JsonSchemaLimits::documentBytes` — CFG-0664 {#cfg-0664}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: source JSON document bytes
- Accepted values: input.size() ≤ documentBytes, MAX_SAFE_INPUT and simdjson::SIMDJSON_MAXSIZE_BYTES; 0 admits only empty input at the resource boundary, and empty JSON then yields MALFORMED_JSON.
- Read timing: Limits are passed by value into loadJsonSchema/loadBinaryJsonSchema; checks precede parser allocation or owned-model staging. The rebind path repeats checks.
- Mutability and lifetime: A new call may receive another profile; a loaded LocalSchema is immutable.
- Failure: Violating the specific resource model is JsonSchemaError::RESOURCE_LIMIT. Invalid JSON separately yields MALFORMED_JSON; allocation admitted by the model may still produce ALLOCATION_FAILURE.
- Consumer sources: [include/qd/schema/json_schema.hpp](../../include/qd/schema/json_schema.hpp), [src/schema/json_schema.cpp](../../src/schema/json_schema.cpp).

### `JsonSchemaLimits::depth` — CFG-0665 {#cfg-0665}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: nested JSON container levels
- Accepted values: depthFits increases depth for structural { and [ outside strings, including empty containers; a new level at depth==limit is refused. Brackets inside JSON strings are ignored. parserDepth=min(depth,input.size())+1 includes bookkeeping. 0 disallows a structural container.
- Read timing: Before parsing, depthFits tracks object/array nesting: only { and [ outside JSON strings increase depth. Inside strings it tracks quotes and escapes and ignores brackets. simdjson subsequently validates JSON syntax.
- Mutability and lifetime: A new call may receive another profile; a loaded LocalSchema is immutable.
- Failure: Violating the specific resource model is JsonSchemaError::RESOURCE_LIMIT. Invalid JSON separately yields MALFORMED_JSON; allocation admitted by the model may still produce ALLOCATION_FAILURE.
- Consumer sources: [include/qd/schema/json_schema.hpp](../../include/qd/schema/json_schema.hpp), [src/schema/json_schema.cpp](../../src/schema/json_schema.cpp).

### `JsonSchemaLimits::schema` — CFG-0666 {#cfg-0666}

- Type: `core::SchemaLimits`. Declared default: `{1024, 16384, 32768, 1024, 1048576}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Counts and exact UTF-16 name units.
- Units: see declaration: UTF-16/code units
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Each load*JsonSchema/loadBinaryJsonWireNames call: independent finite document/parser/work/two-copy bounds
- Mutability and lifetime: new explicit load per schema; no file I/O/source resolution/live schema replacement in loader
- Failure: Owner error vocabulary: JsonSchemaError; the exact field-to-error branch is UNKNOWN and has not been individually established.
- Additional owner vocabulary (not a field-to-error mapping): `JsonSchemaError`.
