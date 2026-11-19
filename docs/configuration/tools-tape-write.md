# Tape writer

[Русский](tools-tape-write.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [Configuration](tools-tape-write.md#cfg-0920-owner) — `tools/tape-write/configuration.hpp`
- [FileLimits](tools-tape-write.md#cfg-0926-owner) — `tools/tape-write/file_boundary.hpp`

## Configuration {#cfg-0920-owner}

[tools/tape-write/configuration.hpp](../../tools/tape-write/configuration.hpp)

### `Configuration::input` — CFG-0920 {#cfg-0920}

- Type: `std::filesystem::path`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native complete-input path
- Accepted values: CLI requires nonempty --input and other mandatory parameters. resolvePaths then requires an existing regular canonical input without NUL within FileLimits.pathUnits.
- Read timing: CLI checks --input presence/nonemptiness and captures its path. run calls resolvePaths before opening: the original/canonical paths are checked, then readInput opens and reads complete input.
- Mutability and lifetime: One fixed owned Configuration per run; the tool has no live setter.
- Failure: Missing/empty --input yields ArgumentStatus::INVALID and exit 2, arguments/USAGE. In resolvePaths, NUL/empty input yields path/INVALID_PATH (exit 3); an overlong path yields path/PATH_LIMIT (6); failed canonicalization or a non-regular file yields path/INPUT_PATH_FAILED (3); matching destination spelling or a confirmed alias yields path/INPUT_OUTPUT_ALIAS (3). bad_alloc during resolution yields path/ALLOCATION_FAILED (6). After resolution, actual opening may yield input-open/OPEN_FAILED, reading input-read/READ_FAILED, and closing input-close/CLOSE_FAILED, each with exit 3.

### `Configuration::output` — CFG-0921 {#cfg-0921}

- Type: `std::filesystem::path`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native fresh-output path
- Accepted values: CLI requires nonempty --output. resolvePaths rejects input aliases and every existing destination entry; OutputFile publishes a new file via exclusive hard link without overwrite.
- Read timing: CLI checks --output presence/nonemptiness. resolvePaths checks path form, existing destination directory, absence of an input alias and of every existing destination entry. Only after complete input preparation does OutputFile exclusively create its own temporary name and publish through a fresh hard link.
- Mutability and lifetime: One fixed owned Configuration per run; the tool has no live setter.
- Failure: Missing/empty --output yields exit 2, arguments/USAGE. In resolvePaths, NUL/empty yields path/INVALID_PATH; an invalid absolute path/empty filename yields path/OUTPUT_PATH_FAILED; missing/non-directory parent yields path/OUTPUT_PARENT_FAILED; status-query failure yields path/OUTPUT_STATUS_FAILED; an input alias yields path/INPUT_OUTPUT_ALIAS; another existing entry yields path/OUTPUT_EXISTS. These filesystem refusals have exit 3. path/PATH_LIMIT and path/ALLOCATION_FAILED have exit 6. Later creation may yield output-create/CREATE_FAILED or EXCLUSIVE_COLLISION (3), and publication output-link/LINK_FAILED (3); overwrite remains forbidden.

### `Configuration::schemaPath` — CFG-0922 {#cfg-0922}

- Type: `std::filesystem::path`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native JSON-schema document path
- Accepted values: Nonempty --schema is admitted exactly instead of --profile. parseArguments imposes no independent numeric schemaPath length bound. With the loader enabled, run reads this path directly through readInput using schemaLimits document bounds, then calls loadJsonSchema; input/destination resolvePaths validation concerns different paths and happens later.
- Read timing: parseArguments captures schema-source selection without reading. run first checks loader availability; when enabled, it reads the document and loads the schema before resolving working input/output paths.
- Mutability and lifetime: One fixed owned Configuration per run; the tool has no live setter.
- Failure: Both --schema/--profile together or neither yields ArgumentStatus::INVALID: exit 2, arguments/USAGE. Nonempty schemaPath with a disabled loader yields exit 4, schema/LOADER_DISABLED. A document-read refusal preserves its input-open/input-read/input-close FileDiagnostic: fileExit maps resource tokens to 6 and ordinary file errors to 3. In loadJsonSchema, RESOURCE_LIMIT/ALLOCATION_FAILURE map to 6; INVALID_SCHEMA/MALFORMED_JSON to 5; UNSUPPORTED_PROFILE/VERSION/TYPE/INDEXED to 4. Stage=schema, token=jsonSchemaErrorName(error).
- Consumer sources: [src/schema/json_schema.cpp](../../src/schema/json_schema.cpp).

### `Configuration::profile` — CFG-0923 {#cfg-0923}

- Type: `std::string`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: complete built-in schema profile
- Accepted values: Only oracle-sre-v1/default-quote-v1/default-profile-v1/oracle-field-types-v1, mutually exclusive with --schema. The only tool mode is raw-message-v1.
- Read timing: parseArguments validates the name and excludes --schema. After successful parsing, run calls builtinSchema only when schemaPath is empty; the validated profile selects the corresponding built-in builder.
- Mutability and lifetime: One fixed owned Configuration per run; the tool has no live setter.
- Failure: Missing --profile with no --schema, or choosing both, yields exit 2, arguments/USAGE. A name outside oracle-sre-v1/default-quote-v1/default-profile-v1/oracle-field-types-v1 yields ArgumentStatus::UNSUPPORTED: exit 4, arguments/UNSUPPORTED. If a validated built-in builder returns SchemaError::RESOURCE_LIMIT or ALLOCATION_FAILURE, the result is exit 6, schema/INVALID_SCHEMA; other schema errors yield exit 5 with the same schema/INVALID_SCHEMA. LOADER_DISABLED belongs to a selected schemaPath; built-in profiles do not require the JSON loader.

### `Configuration::schemaLimits` — CFG-0924 {#cfg-0924}

- Type: `schema::JsonSchemaLimits`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Unchanged native loading bounds.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `ArgumentStatus`.

### `Configuration::limits` — CFG-0925 {#cfg-0925}

- Type: `std::array<std::size_t, 31>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: The array holds 31 named coordinates with different units. effectiveCliDefaults lists the effective CLI value and unit of every position; the original declaration expression is retained separately.
- Accepted values: Before checking --help, parseArguments fills all 31 positions from LIMIT_OPTIONS, then working options override only their matching named cell. Values use complete nonnegative decimal syntax representable in uint64_t and size_t; 0 is syntactically valid. admissionLimits separately checks FRAME_BYTES+9, FRAME_BYTES≤INT64_MAX and DESCRIPTOR_UNITS+DESCRIPTION_NAME_UNITS, which becomes WriterLimits.textUnits. Other bounds are enforced by the specific reader, admission and writer. writerLimits does not itself check the final addition; normal main calls admissionLimits first.
- Read timing: parseArguments first assigns the complete named default set, including a help invocation. After overrides, admissionLimits/writerLimits read specific indices and forward them into independent library consumers; the table includes exact field paths.
- Mutability and lifetime: One fixed owned Configuration per run; the tool has no live setter.
- Failure: Invalid syntax/duplicate/conflict is ArgumentStatus::INVALID; a known unsupported mode/profile is UNSUPPORTED. Unrepresentable admission relationships return code 6; later file/schema failures have their own static stage/code.

## FileLimits {#cfg-0926-owner}

[tools/tape-write/file_boundary.hpp](../../tools/tape-write/file_boundary.hpp)

### `FileLimits::inputBytes` — CFG-0926 {#cfg-0926}

- Type: `std::size_t`. Declared default: `{16777216}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of completed owned input array
- Accepted values: readInput admits a read block only if read≤inputBytes-bytes.size(); 0 admits only an empty physical file with sufficient work. Complete EOF and successful close are required.
- Read timing: FileLimits is passed explicitly to resolvePaths/readInput/create; OutputFile captures its own copy. Validation precedes the stated copy/operation.
- Mutability and lifetime: Every new file owner has independent counters; its bound is fixed until retirement.
- Failure: Static FileDiagnostic: code=INPUT_BYTE_LIMIT, stage=input-read. Resource failure does not publish successful partial input or authorize destination overwrite.

### `FileLimits::workUnits` — CFG-0927 {#cfg-0927}

- Type: `std::size_t`. Declared default: `{67108864}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: transferred/copied bytes and operation attempts per owner
- Accepted values: 0 forbids input-open and the first output-create attempt. Reading charges the attempt+bytes and the old copy on growth; output has independent work_ for create/write/flush/close/publication.
- Read timing: FileLimits is passed explicitly to resolvePaths/readInput/create; OutputFile captures its own copy. Validation precedes the stated copy/operation.
- Mutability and lifetime: Every new file owner has independent counters; its bound is fixed until retirement.
- Failure: FileDiagnostic.code=WORK_LIMIT with the exact stage input-open, input-read, input-close, output-create, output-write, output-flush, output-close, output-link or output-cleanup. A successful exclusive link sets published_=true. If the following output-cleanup work check fails, the command reports failure even though the complete destination already exists; discard removes only the operation-owned temporary name. Diagnostics retain published and any cleanupFailed.

### `FileLimits::pathUnits` — CFG-0928 {#cfg-0928}

- Type: `std::size_t`. Declared default: `{4096}`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native code units per retained/resolved path
- Accepted values: pathUnits≤INT_MAX; original input/destination, canonical source/target and generated temporary lengths must fit pathUnits. 0 cannot hold a nonempty path; NUL/empty separately yields INVALID_PATH.
- Read timing: FileLimits is passed explicitly to resolvePaths/readInput/create; OutputFile captures its own copy. Validation precedes the stated copy/operation.
- Mutability and lifetime: Every new file owner has independent counters; its bound is fixed until retirement.
- Failure: Static FileDiagnostic: code=PATH_LIMIT, stage=path/output-create. Resource failure does not publish successful partial input or authorize destination overwrite.
