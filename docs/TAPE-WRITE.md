# Strict offline tape rewriting

[Русская версия](TAPE-WRITE.ru.md). English is authoritative.

`qdcxx-tape-write` is a native offline normalization utility defined by this contract;
implementation and its executed checks are separate deliverables. It accepts only the
explicit [RAW_MESSAGE_V1 writer subset](../spec/protocol/TAPE-WRITER.md), preserves the
admitted actions/metadata/values, and publishes a new destination. It reads no private
inputs, environment settings, Java properties, transport address, sidecar or external include.
It does not record a live connector or claim byte-identical Java FileWriter output.

## Command line and configuration ownership

```text
qdcxx-tape-write --input INPUT --output OUTPUT --tape-mode raw-message-v1 --profile BUILTIN [LIMITS]
qdcxx-tape-write --input INPUT --output OUTPUT --tape-mode raw-message-v1 --schema SCHEMA [LIMITS]
```

Exactly one source is required: `--profile oracle-sre-v1|default-quote-v1|default-profile-v1|oracle-field-types-v1`
or `--schema PATH`. Built-ins are explicit validated schemas already available to inspection;
no adapter or event projection is added. The supplied profile restricts exact local types;
it does not select tape mode. `--tape-mode raw-message-v1` is required, with no implicit
legacy fallback. Native `--schema` uses the unchanged [version-1 grammar](../spec/schema/NATIVE-JSON.md).
Its optional loader stays separate; loader OFF reports LOADER_DISABLED before tape I/O.

No arguments and standalone `--help` produce identical stdout, empty stderr and exit 0
on successful help output, without opening schema/input/output. A help write/flush failure
is exit 3. Other combinations with `--help`, unknown/duplicate/missing arguments, positional
arguments or conflicting sources are usage errors. Values are separate argv tokens; there
is no `--key=value`, short-option, automatic path/source discovery or general property loader.
Paths are supplied literally as native paths; empty paths are invalid. Limit values are
ASCII unsigned decimal without sign/whitespace; zero is valid, overflow is usage error.
Options are resolved once into an explicit owned configuration before any file I/O.

The optional target is `qdcxx_tape_write`, filename `qdcxx-tape-write`, controlled by
`ORG_TTLDTOR_QDCXX_BUILD_TAPE_WRITE` (default OFF). No new dependency is required.
Library-only/utility-OFF builds remain usable; JSON-OFF does not fetch simdjson;
BUILD_TESTING=OFF does not fetch doctest. No Python launcher/dependency is added.

## Strict admission: complete preflight, then deterministic second pass

Reader success alone is insufficient: existing binding permits unknown records/fields,
repeated target assignments and raw-zero defaults. Those reader policies stay unchanged.
The utility first loads the complete bounded input into owned bytes and closes the input
successfully. It then runs a fresh RAW_MESSAGE_V1 `TapeReader` with the selected schema.
It checks **every** complete frame/description, including unused descriptions, and calls
`finish()` only after actual operating-system EOF. No destination temporary exists yet.

The reusable boundary is `admitTape(const LocalSchema&, span<const byte>, AdmissionLimits)`
in `qd/protocol/tape_admission.hpp`, namespace `org::ttldtor::qdcxx::protocol`.
It returns `expected<AdmittedTape, AdmissionDiagnostic>`. `AdmittedTape` is immutable,
move-only, with a private constructor. It owns a bounded copy of bytes, schema, explicit
outgoing layouts and exact outgoing descriptor. Caller cannot set a public EOF boolean
or forge a finished token. Library EOF means exhaustion of that supplied complete byte
sequence; the filesystem caller separately authenticates physical read EOF/close.
The source bytes are retained, not an unbounded list of decoded records or frame actions.
Feed results are released after each bounded frame. No context/view survives its owner.

| Admission checkpoint | Required condition |
| --- | --- |
| Mode, framing and initial header | Existing RAW-TAPE syntax/policy succeeds; exactly one initial descriptor. Unknown/duplicate properties, receive entries, extensions, other timing/options/messages are refused by the reader. |
| Descriptor projection | Convert every accepted member exactly to OutgoingDescriptor, including absent/empty `version`, absent/empty/hs `opt`, MESSAGE/message spelling and empty/singleton RAW send. Refuse legacy flags, inconsistent or future unrepresented members before output. Property ordering/primitive widths are normalization details. |
| Description payload | Reparse the complete owning `frame.payload` through existing `parseDescribeRecords`; charge this extra work. Do not derive names/order from final decoded values. |
| Each record binding | `bindRecord` must return known localRecordId. Check full exact field types, all actions have targets, each local target occurs exactly once and every local target occurs. Any skip, repeated target, missing target or conversion refuses admission. This also applies to zero-field/unused unknown records. |
| Immutable inventory | No repeated sender ID, local ID or record name, even identical. Appended descriptions are allowed at their original placement; they are immutable. No data uses an unannounced ID. Retain exact supplied wire names/order/sender IDs, never first aliases. |
| Outgoing ID domain | The writer admits 0..INT32_MAX-1. INT32_MAX must be refused before rewriting, never renumbered. See the [writer contract](../spec/protocol/TAPE-WRITER.md#immutable-layouts-and-names) for the locked Java overflow and separate dense-array resource limits. |
| Data and heartbeats | Every result succeeds without failed-prefix laundering. Zero flags; canonical tagged values; all accepted empty/bits0/bits1 heartbeat distinctions/timestamps and envelope markers are represented. |
| Terminal input | Physical read EOF and close succeeded; reader `finish()==FINISHED`. Marker is never EOF authentication. Truncation after a valid prefix or marker refuses the entire rewrite. |

Complete syntactic description parsing precedes strict binding. Binding's existing full-type
validation precedes known/skip/coverage checks. For each description, strict checks run in
wire record/field order; unknown record, skipped action, repeated target, then missing target
at record end. Duplicate inventory checks occur before inventory publication. Names/types
are preserved even if unused. One valid target-order permutation is accepted; a mutation
that changes the target multiset is rejected. The final immutable inventory is published
only after complete admission. No default insertion, silent pruning or schema simplification.

After admission, a new reader walks the same immutable bytes using independently charged
second-pass budgets. Emit descriptor, descriptions, heartbeat forms, data batches and markers
in original envelope order. Description groups select final immutable layout IDs at the original
positions. Data preserves record order and raw local values, symbol units and inherited raw
time/cursor sequence; mapping back to sender ID is unique because admission requires unique
local IDs. Preserve each empty data/description frame. Do not coalesce frames, synthesize final
marker or replace MESSAGE timing with business field time. Canonical primitive/property/symbol
encoding may change byte length and input offsets; semantic equality is the promise.

## Finite limits

All limits are finite and explicit. The following native defaults are policy, not Java defaults.
Every option appears in help; zero means zero and equality is accepted. Library callers supply
their own AdmissionLimits/WriterLimits/TapeLimits rather than reading these CLI defaults.

| Option | Default | Scope |
| --- | --- | --- |
| `--max-input-bytes` | 16777216 | Complete owning input bytes; check while reading, before growth. |
| `--max-output-bytes` | 16777216 | All emitted chunks including descriptor/length/type/markers. |
| `--max-frame-bytes` | 1048576 | Envelope body/pending and private writer envelope sizing (length prefix additionally counts in scratch/output). |
| `--max-frames` | 65536 | All frames in each parse and committed writer frames, including empty markers. |
| `--max-records` | 1048576 | Record attempts in each parse and writer. |
| `--max-fields` | 8388608 | Wire field attempts in each parse and writer. |
| `--max-values` | 8388608 | Decoded local values in each parse. |
| `--max-descriptions` | 1024 | Retained unique outgoing/incoming records. |
| `--max-description-fields` | 16384 | Retained total outgoing/incoming fields. |
| `--max-name-units` | 1024 | One record/field/lookup name. |
| `--max-description-name-units` | 1048576 | Retained incoming and outgoing names, each collection separately bounded. |
| `--max-construction-units` | 67108864 | Existing reader full candidate+schema reconstruction work, per parse. |
| `--max-admission-work-units` | 67108864 | Additional reparsing/binding/inventory/copy work; no refund after rejection. |
| `--max-admission-scratch-bytes` | 16777216 | Peak extra admission-owned byte storage for source copying and temporary reparse/binding/inventory storage; separate from writer scratch. |
| `--max-admission-scratch-units` | 4194304 | Peak extra admission-owned logical UTF-16 units in description/binding/layout candidates and their simultaneous copies; separate from writer scratch. |
| `--max-writer-work-units` | 67108864 | Writer traversal work defined in TAPE-WRITER. |
| `--max-descriptor-properties` | 16 | Combined global/message pairs. |
| `--max-descriptor-messages` | 1 | Combined send/receive entries. |
| `--descriptor-string-bytes` | 65536 | One descriptor UTFString body. |
| `--descriptor-string-units` | 16384 | One descriptor UTFString units. |
| `--max-descriptor-units` | 65536 | All decoded descriptor text, per parse; copied output descriptor separately bounded identically. |
| `--max-timing-events` | 65536 | Explicit heartbeat/data metadata attempts, per parse. |
| `--symbol-units` | 16384 | One decoded/encoded symbol and previous-symbol state. |
| `--max-symbol-units` | 4194304 | Output symbol copies per parse; writer cumulative unit visits separately limited to this value. |
| `--field-string-bytes` | 65536 | One type-10 prefix+body. |
| `--field-string-units` | 16384 | One type-10 UTF-16 value. |
| `--max-field-bytes` | 16777216 | Examined/encoded type-10 bytes, per parse/writer. |
| `--max-field-units` | 4194304 | Type-10 unit decoding/encoding visits, per parse/writer. |
| `--max-owned-field-units` | 4194304 | Completed/current/temporary decoded string units, per parse. |
| `--max-scratch-bytes` | 1048585 | Simultaneous private writer frame buffers, including length prefix. |
| `--max-scratch-units` | 32768 | Writer owned previous symbol plus temporary text/field units. |

Native schema limits/options/defaults are exactly the seven in NATIVE-JSON, without alteration.
Built-in schema copying uses the same SchemaLimits; schema-byte/depth limits apply only to JSON.
For old COMPACT_INT records the normalizer creates exact type-8 FieldValue views within value/work
bounds; it never interprets dual legacy/typed arrays heuristically. Type-8-only and Quote profiles
retain their exact profile validation rather than automatically broadening to EXTENDED_FIELDS_V1.

`AdmissionLimits` contains owned input bytes, existing `TapeLimits`, `LayoutLimits`, additional
`workUnits`, and peak `scratchBytes/scratchUnits` for extra reparsing/binding. Extra budgets must
cover simultaneous result frame+description candidate+binding+layout candidate; if allocation
would exceed them, refuse before allocation.
The two `--max-admission-scratch-*` options map only to AdmissionLimits scratch members;
`--max-scratch-*` map only to WriterLimits scratch members. Admission byte scratch counts the
whole temporary owned source-byte copy until it transfers into AdmittedTape, plus any additional
owned byte buffers; reader-owned frame/pending bytes remain separately bounded by TapeLimits.
Admission unit scratch counts simultaneously owned extra raw-description and outgoing candidate
name/text units while validating/copying, including old and new candidates until commit;
the final retained schema/layout/descriptor units have their own explicit limits. Binding action/
record containers use LayoutLimits counts; every copied element/unit also charges admission work.
Avoid duplicate whole-input staging copies: transfer the bounded completed copy at commit.
Each pass has explicit independent counters and the utility reports both internally for
verification. These are bounded repeated work, not a
claim that two passes are charged once. Reader cumulative construction/field work is not refunded.
Input copy, schema/layout duplication and temporary buffers must have checked size arithmetic.
No untrusted count reaches an unchecked reserve. Logical-unit limits do not bound allocator metadata.

## Publication and same-path refusal

After successful schema validation, resolve input and destination to absolute paths, resolve
existing parent aliases, inspect final entries with symlink-aware status and compare existing
file identity where possible. Refuse the same path through lexical/parent aliases, symlinks,
hard links or Windows case equivalence. A destination entry of any kind, including a dangling
symlink or directory, is pre-existing and refused. Output parent must already exist; no directory
creation, overwrite switch or stdout tape output is supplied. Alias checks catch ordinary aliases;
hostile concurrent parent replacement is outside the guarantee and must not be called a sandbox.

Only after complete admitted input exists, create an exclusively new private temporary in the
resolved destination directory. Use actual platform exclusive file creation, not `exists` then
ordinary `ofstream`. Keep only its owned handle/path. Write whole chunks using checked writes;
partial/zero-progress errors terminate output. Then require writer finish, final flush, checked
close and no pending errors. Destructors and Java's best-effort close are not completion evidence.

Publish with a create-only hard link from the closed temporary to the fresh destination, using
the actual filesystem operation (`create_hard_link` with error handling). On supported local
filesystems the destination creation fails if any target entry appeared after the earlier check;
there is no rename/existence-check fallback. If hard links are unsupported, disallowed, cross-volume
or the operation fails, return publication failure and remove only the owned temporary. The same
directory requirement avoids intentional cross-volume placement. Platform/filesystem guarantees
must be verified on each supported build environment; unsupported environments are explicit limits.
Ordinary POSIX rename can overwrite in a race, and is not used for this contract.

After successful link creation, remove the owned temporary name. If that cleanup fails, preserve
the valid published destination, report failure with `published=true`, and retain the exact owned
temporary path for operator cleanup; never delete the destination to pretend rollback. Before link
success all failures leave no new destination and attempt owned-temp cleanup. Cleanup failure is a
secondary diagnostic and cannot replace the first failure. Exclusive-name creation failures must
not authorize deleting someone else's entry. No hostile-name-race immunity, directory fsync,
power-loss durability or universal crash atomicity is promised. A successfully visible destination
contains fully written/closed validated bytes in the tested filesystem semantics.

## Outcomes and error precedence

Success is silent (empty stdout/stderr) and exit 0 after publication and owned-temp cleanup.
Failure emits at most one bounded UTF-8/LF diagnostic row to stderr:
`tape-write-error<TAB>1<TAB>STAGE<TAB>CODE<TAB>published=0|1<LF>`.
No input contents, schema names or credentials are echoed. Cleanup details/owned path belong to
an additional bounded diagnostic only when cleanup failed. Diagnostic write/flush failure remains
nonzero; it does not replace an already detected primary failure or undo a published file.

| Exit | Category |
| --- | --- |
| 2 | Usage/invalid limit spelling or conflicting options. |
| 3 | Path/input/schema file I/O, exclusive create/write/flush/close/link/cleanup or diagnostic-output failure. |
| 4 | Unsupported mode/profile/loader/type/flags/descriptor or strict information-loss admission refusal. |
| 5 | Malformed schema/tape/description/value, duplicates or truncated EOF. |
| 6 | Finite resource limit, checked arithmetic or allocation failure. |

Order is argument/help handling → loader availability → schema file read/close/full validation →
path checks → input open/read/EOF/close → complete preflight/admission → exclusive temporary create →
second parse/composition/write → reader finish → writer finish → flush → close → create-only link →
temp cleanup. Stop at first detected failure. An input read failure prevents parser/admission work;
a write failure prevents later parsing/composition rather than searching for a preferable input error.
If one second-pass result is FAILED with a completed prefix, report input error and do not write that
result's records. Observer/sink errors never become successful EOF. Simultaneous unrelated schema
defects retain NATIVE-JSON's limited precedence rather than inventing a global order.

Stable strict codes are UNKNOWN_RECORD, SKIPPED_FIELD, REPEATED_TARGET, MISSING_TARGET,
DUPLICATE_LAYOUT_IDENTITY and UNREPRESENTED_DESCRIPTOR. Native reader/writer errors retain typed
causes/checkpoints. Paths/byte offsets may be exposed by the reusable owning diagnostic, with
bounded path storage; the stable default row remains content-free. No output tape prefix is a
completed result, even when it parses independently.

## Examples and required checks

```text
qdcxx-tape-write --input tests/fixtures/protocol/tape-write/expected/extras/quote-raw.qtp --output quote-normalized.qtp --profile default-quote-v1 --tape-mode raw-message-v1
qdcxx-tape-write --input tests/fixtures/encoding/field-types/expected/mixed-raw.qtp --output fields-normalized.qtp --schema spec/schema/examples/native/field-types.json --tape-mode raw-message-v1
```

The first output must not already exist. Unsupported witnesses such as
[full24/full137](../tests/fixtures/protocol/tape-write/expected/extras/unsupported-full-identities.qtp)
remain refusals. Omitted/repeated/unknown-field descriptions need explicitly constructed
negative admission cases, labelled synthetic rather than production FileWriter observations.
Contract [fixture packet](../tests/fixtures/protocol/tape-write/contracts/README.md) enumerates them.

Required checks: built-ins and unchanged native schemas, real complete Quote/Profile plus custom
extra fields, exact semantic Java/C++ round trip, unused unknown record, skipped/repeated/missing
targets, valid field permutation, appended descriptions, exact descriptor/heartbeat/marker forms,
truncated input after marker, no-args/help and malformed args, all budget boundaries, aliases and
existing destination, target appearing during publication, real partial write/zero-progress/flush/
close/link/cleanup failures. Inject at the actual owning filesystem/write boundary used in production;
a fake standalone sink or failure flag bypassing the wrapper is insufficient. Add only a narrow
production boundary required to own/check these operations, not a general filesystem framework.
Every failure checks original input/old destination bytes and owned-temp disposition. Successful
link plus cleanup failure checks published validity separately from nonzero process outcome.
