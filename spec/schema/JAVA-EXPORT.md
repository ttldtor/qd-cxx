# Offline export of a resolved Java schema

[Русская версия](JAVA-EXPORT.ru.md). English is authoritative.

This contract defines an opt-in developer bridge from the locked Java `DataScheme` to the existing
[native JSON version 1](NATIVE-JSON.md), following [ADR-0003](../../docs/decisions/ADR-0003-configurable-schemas-and-event-mappings.md).
Java constructs and exports the schema; ordinary C++ loading, inspection and prepared event APIs
consume the resulting files without Java. This does not implement native XML construction or a
general settings engine. The [usage guide](../../tools/schema-export/README.md) describes invocation;
[validation](../../tests/conformance/schema-export/VALIDATION.md) records executed checks and
independent acceptance separately.

## Source boundary

The reusable exporter takes an already resolved `DataScheme`, an optional exact record selection
and explicit finite limits. It reads no properties, files or environment, and changes neither the
supplied objects nor global state. Callers must keep the scheme and aliases stable during export.
Arbitrary implementations and concurrent mutation are outside its contract.

The offline command runs in a fresh JDK 8 JVM with a verified locked classpath. Its separate source
adapter invokes the production Java construction paths. `--source` is exactly one of:

- `default`: the locked `DXFeedScheme` singleton, initialized after explicit startup settings.
- `endpoint`: a disconnected FEED endpoint's resolved scheme, constructed with explicit settings;
  endpoint construction is not authorization to connect, discover a properties file or fetch data.
- `xml`: `DXScheme.Loader` with explicitly configured property options and preflighted sources.

The command requires `--output NEW_DIR`. No arguments and standalone `--help` produce identical help
on stdout, empty stderr and exit 0 after successful output, without work or file IO. Nonempty invalid,
duplicate or incomplete arguments are usage errors. A help-output failure remains nonzero.
Repeated `--record NAME` arguments select exact record names; repeated selection names are errors.
Omitting selection means **all** source records, not an implicit Quote/Profile subset.

`--startup KEY=VALUE` supplies startup settings. `--properties FILE` explicitly selects one endpoint
or XML properties file; `--set KEY=VALUE` supplies endpoint builder or XML loader overrides. Repeated keys
within one source fail, while values of the same key in different sources retain their source
identity and resolve through the locked production path. An absent property is not explicit false.
The allowlist is exactly the investigated subset from [CONFIGURED-EVENTS](CONFIGURED-EVENTS.md):

| Setting | Accepted explicit value |
| --- | --- |
| `dxscheme.nanoTime`, `dxscheme.wide` | `true` or `false` |
| `dxscheme.enabled.Sequence`, `dxscheme.enabled.TimeNanoPart` | 1–128 characters matching `[A-Za-z*][A-Za-z0-9*]*`; only `*` is a wildcard |
| `dxscheme.price`, `dxscheme.size`, `dxscheme.shares` | `int`, `wide`, `tiny`, `decimal`, case insensitive |

Unsupported resulting full types still fail projection: accepting a configuration value does not
authorize type coercion. Unknown keys, connectivity options and implicit properties-file discovery
are rejected. Properties-file syntax is `Properties.load(InputStream)`; duplicate decoded keys fail.
Settings are validated before Java schema initialization. Endpoint resolution is builder override
over explicit file over startup for this measured subset. Default mode accepts startup settings
only. XML options `--xml-sysprops true|false` and `--xml-dxprops true|false` default to false and
control the locked loader's system/explicit-property stages; supplied settings that their selected
source/options cannot consume are rejected rather than reported as effective. In particular,
`dxscheme.shares` is rejected in XML mode because that source does not consume it. XML explicit settings
override startup settings when both stages are enabled. Explicit field selections override the
nanoTime shortcut independently. No universal precedence beyond these consumers is implied.

The launcher rejects `JAVA_TOOL_OPTIONS`, `_JAVA_OPTIONS` and `JDK_JAVA_OPTIONS`. The command uses
a controlled properties snapshot and does not accept arbitrary service providers or classpath
extensions. Runtime, classpath and upstream verification are tool-boundary responsibilities, not
features of the C++ loader or proof that arbitrary embedded Java callers are trusted.

## XML resource boundary

Local XML mode requires one declared `--input-root DIR`. Repeated `--xml RELATIVE` arguments name
ordered roots/overlays under that root. `--resource dxlink` explicitly selects the pinned bundled
`dxlink.xml` and its required `dxfeed.xml` dependency. It does not authorize arbitrary resources.

Before `DXScheme.Loader` runs, a hardened XML reader validates a bounded local import closure and
copies the accepted bytes into a private temporary tree preserving relative paths. Relative imports
are resolved against the importing file, must remain within the declared root after real-path
resolution, and cannot contain URL schemes, absolute paths or path escapes. Reject import cycles,
DOCTYPE, external entities, XInclude, `xml:base` and remote roots/imports. Relative path spelling is
1–240 ASCII letters/digits, `_`, `.`, space, `/` or `-`, with no leading/trailing whitespace.
Relative `..` components are allowed only when resolution remains within the root. Pinned resources and their dependency
bytes are accounted for explicitly; importing an arbitrary bundled resource is not a fallback.

The exact pinned namespace/schema declaration
`https://www.dxfeed.com/datascheme dxfeed-schema.xsd` is allowed, backed by the locked classpath XSD.
Other user-provided or remote schema locations are rejected. The preflight parser disables external
DTD and schema access and verifies those settings. Before QD parser initialization, JAXP DTD and
stylesheet access are denied; schema access is restricted to `file`. The pinned JDK 8 requires this
for the exact permitted local declaration. The same locked XSD is copied beside each snapshot XML;
user XSD replacement and imported/included XSD dependencies are rejected. Failure to enforce this
combined preflight/snapshot boundary fails the operation; a blanket schema-access denial is not claimed.
Only the snapshot is passed to upstream loading. The preflight controls resource access, leaving
record, alias, type and overlay semantics to Java. This is a trusted-local-input developer tool,
not a general hostile-XML service or an exact heap-bound guarantee for upstream construction.

## Records, logical fields and complete lookup relation

Records retain source order regardless of request order. Every requested name must occur exactly
once in the source; unknown/repeated names fail the whole operation. Exported IDs are contiguous
array positions starting at zero. Provenance records original and exported IDs, including identity
mappings. A supported empty scheme remains a valid empty export.

Selected records must be the exact locked `DefaultRecord` implementation and have `hasTime:false`.
Subclasses, including XML's special `VersionedRecord`, are unsupported. Record IDs, names, field
ownership and slot indices must be internally consistent. Selecting supported records from a larger
scheme is an explicit scope choice; it is not evidence that the whole scheme is supported.

Logical field order is Java integer-index order followed by object-index order. Preserve the full
type ID, accepting only **2/8/10/40/56/72/88/120**. Never restore assumed interleaved XML declaration
order or infer a wire ID from a Java storage index. A supported multi-slot integer field is followed
by exactly one physical `VoidIntField` with full type zero, the next integer index, the same record
identity and the exact name `ownerName + "$VoidTail"`. Only that verified tail is omitted. Standalone
VOID, missing/malformed tails, unsupported full identities and inconsistent ownership fail.
The sidecar records each retained source slot and the owning logical field of each omitted tail.

Enumerate the actual private final `DefaultRecord.fieldsByName` map through a small read-only
reflection adapter. Validate declaring class, member shape, accessible state, entry types and the
locked implementation identity. Every enumerated key must satisfy
`record.findFieldByName(key) == enumeratedField` by **object identity**. Each target must be either
a retained logical field or a verified omitted tail. Every retained field needs at least one key.
Never fall back to guessed full/local/property names if introspection fails.

All keys targeting retained fields appear in native `lookups`; sort them by Java `String.compareTo`
(UTF-16 code-unit order). Keys targeting omitted tails are listed separately in provenance and never
redirected to owner fields. Lookup scope remains per record. Adding an alias changes the Java
field's latest property name while retaining older map keys; exporting only current property names
would lose observable lookup behavior. JSON has no extra alias-generation rule.

Source evidence is the locked `DefaultRecord` constructor, `putDefaultFieldNames`,
`putNonDefaultFieldNames`, `addFieldAlias`, `findFieldByName`, `SchemeBuilder` slot creation and
`DXScheme.buildOneRecord`; see the [source inventory](SOURCES.md) and [upstream lock](../../upstream.lock).
The independent [projection characterization](../../tools/schema-export/tests/ProjectionCharacterization.java)
constructs production objects and checks accumulated aliases, complete map targets, wide/tail/int/
object slots, class/access/type rejection and source-order selection. It also checks default and
pinned dxlink Quote/Profile objects. These observations establish projection mechanics, not tape
or event compatibility; production writer/reader observations and C++ consumption establish that
separate boundary.

## Determinism, budgets and publication

Output is UTF-8 without BOM, LF terminated, with fixed JSON member order. Root order is
`formatVersion`, `profile`, `records`; profile order `id`, `version`; record order `id`, `name`,
`hasTime`, `fields`, `lookups`; lookup order `name`, `fieldId`. Exact scalar names are neither
normalized nor trimmed. Controls and punctuation are escaped deterministically. Supplementary
scalars are preserved; unpaired name surrogates are rejected, never replaced. The existing native
name/count/lookup invariants remain authoritative.

| Limit | Default | Meaning |
| --- | --- | --- |
| `--max-schema-bytes` | 1048576 | Complete generated native JSON bytes |
| `--max-schema-records` | 1024 | Selected records |
| `--max-schema-fields` | 16384 | Retained logical fields in total |
| `--max-schema-lookups` | 32768 | All source lookup entries, including omitted-tail keys |
| `--max-schema-name-units` | 1024 | UTF-16 units in one record/source lookup name |
| `--max-schema-total-name-units` | 1048576 | Sum of record/source lookup name units, including omitted-tail keys |
| `--max-input-files` | 64 | XML input/import closure, explicit properties file and pinned XSD |
| `--max-input-bytes` | 1048576 | Total accepted XML/properties/pinned XSD input bytes |
| `--max-import-depth` | 16 | Active XML import chain |
| `--max-sidecar-bytes` | 4194304 | Complete generated provenance bytes |

Limits use unsigned decimal values in `0..2147483647` with finite checked arithmetic; equality passes
and one-over fails, including zero limits. Input file/byte/depth defaults are hard maxima: overrides
can only lower them. Counting omitted-tail lookup keys is intentionally stricter than the native
consumer's retained-key accounting. Native output depth is at most five containers; there is no need to
widen the consumer's default depth 16. Provenance has an independent byte budget.
The portable CMake launcher uses a 30-second process timeout and 256 MiB Java heap by default;
its finite process budget does not promise an exact bound on resident memory. Budget failures
publish no successful schema or partial bundle.

After complete source and projection validation, write `schema.json` and `provenance.json` into a
private sibling staging directory and publish the new export directory without replacing an
existing destination. A failed publication removes only the tool's own staging content. An existing
destination, including an empty directory, must remain intact; no success is reported before both
files are completely written and publication succeeds. Two fresh runs with identical verified
inputs produce identical schema and sidecar bytes; run timestamps belong only in transient logs.

Provenance contains only bounded relevant data: locked revision, exporter/input/classpath hashes,
source mode, permitted source settings, selected-ID mapping, retained slot mapping and omitted-tail
lookup accounting. It contains no ambient properties dump, credentials, environment dump,
machine-specific absolute paths or private input content. The native C++ loader neither interprets
nor dereferences this sidecar.

Errors emit one stderr row `schema-export-error<TAB>1<TAB>CODE<LF>` and no stdout. Diagnostics do not
echo input paths, XML, property values or stack traces.

| Code | Exit | Meaning |
| --- | --- | --- |
| `USAGE` | 2 | Missing, duplicate, conflicting or unknown command argument |
| `IO_ERROR`, `PUBLICATION_FAILED` | 3 | Input/output failure or inability to publish a new bundle |
| `UNSUPPORTED_RECORD`, `UNSUPPORTED_TYPE`, `UNSUPPORTED_INDEXED`, `UNSUPPORTED_LOOKUPS`, `UNSUPPORTED_SOURCE` | 4 | Unsupported implementation, full identity, indexing, lookup introspection or source runtime |
| `INVALID_SCHEMA`, `INVALID_INPUT`, `INVALID_CONFIGURATION` | 5 | Invalid projection invariants, disallowed XML/input or settings |
| `RESOURCE_LIMIT` | 6 | Finite input, output, count, name or process budget exceeded |

These rows describe Java command errors; CMake verification, compilation and process-timeout failures
use CMake diagnostics and a nonzero launcher exit rather than promising a Java error row.
Launcher origin/classpath verification failures also stop before export; verification failure is
not permission to use an unverified classpath. The usage guide lists invocation syntax.
Neither an export nor a JSON round trip alone proves
standard-event compatibility. No new wire type, indexed schema, conversion, live replacement,
network transport, private tape or historical fixture change is authorized by this contract.
