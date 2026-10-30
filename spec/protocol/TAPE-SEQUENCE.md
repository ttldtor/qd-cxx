# Finite self-contained tape sequences

[Русская версия](TAPE-SEQUENCE.ru.md)

## Selection and independence

`SELF_CONTAINED_SEGMENTS_V1` is an explicit finite occurrence list in caller order. A repeated path
is a replay occurrence and consumes its full reservation again. No wildcard, sorting, deduplication,
directory discovery, follow, cycle, clock, scheduler or retention behavior is implied. Missing selected
files and physically empty selected data sources are terminal failures. Empty records in a valid,
independently described segment are permitted.

Each occurrence starts fresh parser, sender mapping, schema generation, container, scanner and timing
state. The sequence selects one homogeneous `NONE`, `MESSAGE`, `FIELD`, `LONG` or `TEXT` policy.
Each initial descriptor must agree with it, and sender declarations must precede their use. Only an
explicit caller-selected `NONE` permits the fresh initial descriptor to omit its time property, as
the producer does for its default. Other policies require an explicit matching declaration.
Headerless or dependent segments and mixed or missing required time declarations are refused before publishing
data from that occurrence. Compatible schema changes remain governed by the existing parser policy.

`NONE` rejects timestamp-bearing MESSAGE heartbeats. `MESSAGE` requires a timestamp-bearing heartbeat
before its first data-family frame publication, including an empty data frame; explicit zero is an
assignment. `FIELD` enables the explicit READ policy,
requires raw FIELD metadata for every published record and rejects MESSAGE overlays. `LONG` and `TEXT`
require an explicit companion and an applied first mark before the first data-family frame publication,
including an empty data frame; they reject
MESSAGE overlays. A first mark may follow non-data declarations. No missing assignment is inferred
from a packed zero. Relative or ambient-clock companion syntax remains unsupported.

The tool-owned interface is in
[`tape_sequence.hpp`](../../tools/tape-inspect/tape_sequence.hpp). Core and protocol do not open files,
resolve names, infer companions, or acquire an ambient clock. Per-source container and companion
semantics are specified in [TAPE-CONTAINERS](TAPE-CONTAINERS.md) and [SIDECAR-TIME](SIDECAR-TIME.md).
FIELD ownership and raw metadata follow [TAPE-TIME](TAPE-TIME.md).

## Admission and budgets

`admitSequence` validates all caller-owned specifications synchronously before path copying,
allocation, opening or stream work. It retains no caller span, path, schema, callback or dynamic text.
Its fixed diagnostic contains an error, zero-based occurrence checkpoint and static component label.
Admission proves configuration only, never source existence, content, integrity or completion.

Every supplied per-source limit is already fully resolved. Admission does not call `prepareLimits`,
derive a default, increase a capacity or equate independent parser limits. It checks the existing
frame requirement that pending capacity accommodates maximum body plus the nine-byte compact prefix,
positive representable IO chunks, container scratch compatibility, scanner retained scratch, and the
initial discovery identity's nonzero construction/snapshot capacities. Other zero capacities remain
literal finite refusals when their corresponding work would be attempted.

`SequenceReservations` contains componentwise sums of the full configured capacities for every
occurrence. The caller supplies explicit aggregate allowances; per-source defaults are never
implicitly multiplied. Unused capacities are not refunded after close, failure or cancellation, and
an occurrence cannot borrow another occurrence's capacity. Conservative rejection is possible even
when actual input would fit. A reservation is not a measured byte count, work count or RSS value.

The inventory includes every nested field of `TapeLimits`: frame storage; accepted input and envelopes;
descriptor properties, messages, encoded text and UTF-16 units; schema records, fields, lookups and
names; context descriptions and binding actions/names; construction and snapshot units; processed and
emitted records, values, symbols, wire fields, per-value string/payload limits, cumulative examined
bytes/work, temporary and owned payload/string storage, FIELD metadata owners and timing attempts.
Per-value and retained peak capacities are summed too.

Data and selected companion containers reserve separate complete `ContainerLimits` vectors: compressed
and inflated bytes, ratio and slack, GZIP members, ZIP entries, single/cumulative header bytes, work,
inflate calls, retained bytes, inflater allocation bytes and chunk capacity. Selected companion
scanners reserve all input, line-size, line-count, mark-count, work and retained capacities. Feed
buffers, encoded source output and event-copy units are separately summed.

Every container instance, including `NONE`, reserves one physical excess-probe byte and one discarded
inflated excess-probe byte, for every repeated occurrence and each selected companion. These explicit
aggregate allowances never enlarge admitted data. Published inflated bytes stay within normal limits;
diagnostic observations remain separate. Normal work/inflate-call gates still apply to probes, and
zero work permits no work.

Names are admitted as native code units without conversion, normalization or filesystem access.
Empty names and embedded native NUL are invalid. Defaults allow 256 occurrences, 4096 native units per
path and 1 Mi native name units in total. Total native units include repeated data/companion names.
Safe diagnostic encoding reserves two hexadecimal characters per native byte. Copied storage reserves
the occurrence objects plus all native path characters and terminators using checked products/sums.
The owner has at most one active source pair, with two physical handles; a smaller configured handle
limit must still accommodate the selected role count. Storage for four fixed diagnostics (primary,
data close, companion close and flush/reporting) is admitted as well. These are logical storage charges,
not allocator overhead or RSS measurements. No wall-clock deadline is promised.

One sequence-wide output allowance covers source boundaries, data, diagnostics and final summary.
The terminal reservation is unavailable to source publication. Sum of per-occurrence output capacities
must fit the remaining allowance. The owner must keep actual output counters separate from capacity
reservations and refuse unrepresentable sums before allocation or opening. The default protected
terminal capacity is 65536 encoded bytes; the checked minimum covers the fixed 92 aggregate resource
rows, one summary and at most four diagnostic rows. Per-source layer resource rows use that source's
independent publication capacity, and source identity grammar overhead is admitted separately.

## Identity, lifetime and completion

Identity retains the source occurrence ordinal, segment-local generation, sender/local IDs and
segment wire record ordinal. A checked aggregate emitted ordinal is separate. Raw FIELD sourceOrdinal
is never rewritten to pretend it is a global ID. Equal local IDs/generations in different segments
do not imply interchangeable schemas. Records own payload and FIELD bytes; schema views are consumed
synchronously before a new generation or segment releases them. Retaining test sinks copy bounded
schema snapshots rather than retaining reader pointers.

The owning operation records an observed container or parser failure before fallible completed-prefix
rendering. It processes only the available produced prefix under bounded feed/resubmit/render
checkpoints. If further prefix interpretation needs unread companion time, it stops at the safe
checkpoint with the original cause; no new companion read is permitted after that cause.
Source refusal, cancellation or sink stop prevents
later reads, opens and transitions. Cleanup never drains input. Logical accepted/frame/payload offsets,
compressed physical read/consumed positions, sidecar positions and checked aggregate counters remain
distinct; no inverse compressed mapping is invented.

Parser finish, every selected container's EOF/integrity, scanner END/strict tail/position finish and
physical close are separate requirements. Adapters are released before both opened files are closed.
Both physical close attempts occur even after the first fails. The first causal error is sticky;
bounded secondary cleanup diagnostics cannot replace it. Only data-close, companion-close and
flush/reporting secondary slots are retained; later semantic/sink/cancel causes can be omitted from
this fixed history while the original cause remains primary. A first cleanup error becomes primary when
there was no earlier cause. Repeated terminal checks perform no further IO or close attempts.

Aggregate COMPLETE is permitted only after the final occurrence and all its layers are closed, the
final summary is written, and the final sink flush succeeds. Summary or flush failure leaves the
sequence incomplete even if all records were emitted. The previous per-file inspector cannot be used
as a black-box sequence operation because its per-file COMPLETE and flush do not establish this.

## Compatibility boundary

Java FileReader may preserve sender mappings and heartbeat time across same-format files, skip missing
or malformed files and log some late integrity/close failures. This explicit native profile instead
requires independent segments and treats those failures as terminal. It is a finite selection policy,
not parity with Java wildcard selection or a general session-continuation API. The pinned production
FileWriter/FileReader investigation and native validation are separate evidence obligations. Primary
baseline sources are
[FileWriterImpl](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java)
and [FileReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReader.java),
with the compatibility baseline recorded in [upstream.lock](../../upstream.lock).

## Source-aware output version 9

The opt-in sequence path uses a separate lossless grammar. Existing output versions 1–8 and their
FIELD-metadata refusal remain unchanged. This presentation is not a QTP reserialization format.
The exact TSV header is:

```text
format<TAB>qdcxx-tape-inspect<TAB>9<TAB>binary-catalog-v1<TAB>self-contained-segments-v1
```

Before source opens, each occurrence has a five-cell `source` row: label, zero-based occurrence,
native encoding (`utf16le` on Windows or `bytes` on POSIX), lowercase native data-path byte hex,
and lowercase native companion-path byte hex or `-`. UTF-16 path units use little-endian byte order.
The paths are borrowed only while formatting; no transcoding, normalization or raw path text is emitted.
`sourceIdentityBytes` counts this exact row without allocation or IO, using checked arithmetic.

Each segment begins with `source-begin<TAB>ordinal`, immediately followed by a four-cell
`source-policy<TAB>POLICY<TAB>DATA_CONTAINER<TAB>TIME_CONTAINER_OR_-` row. Policy and container names
are lowercase. The policy row applies to the current occurrence; timing is homogeneous but explicit
container kinds may differ between occurrences. Existing frame, descriptor-v8, schema, source-schema,
schema-end, heartbeat and timing rows belong to this current occurrence. Schema generations and
sender/local record IDs remain segment local.

The record formatter consumes the borrowed immutable schema before any generation or source transition.
It does not retain a schema, record, input span, path or callback and does not copy payloads to erase FIELD
metadata. It validates the complete exact-type schema, provenance, value shape and explicit time
assignment before counting or allocating a row. The separate source owner enforces declaration order
and homogeneous timing requirements before publishing records.

`wire-data-v9` has exactly 27 cells, including its label:

```text
label,sourceOrdinal,frameIndex,segmentRecordIndex,aggregateRecordIndex,generation,senderId,localId,
recordNameHex,symbolHex,typedValues,externalTimestampMs,externalPacked,externalEventTimeMs,flags,
removalTime,fieldPresent,fieldRawSeconds,fieldRawSequence,fieldSourceOrdinal,fieldPacked,
fieldEventTimeMs,timePolicy,externalOrigin,effectiveOrigin,effectivePacked,effectiveEventTimeMs
```

Typed values, flags, removal time, UTF-16 names and symbols retain output8's exact grammar, all nineteen
full field identities and the null/empty/absent distinctions. `fieldPresent` is 0 or 1. Absent FIELD has
five following `-` cells; present raw seconds, optional raw sequence and segment-local source attempt
ordinal remain unchanged. Packed FIELD projection retains low-word wrap and millisecond subfields
1000–1023. Absence of raw sequence differs from an explicit zero.

An external assignment retains its exact optional timestamp and independent packed cursor, including
an assigned zero. Origins are `none|message|sidecar`. NONE policy selects effective packed zero;
otherwise a nonzero external packed value wins, then present FIELD, then zero. Effective origins are
`zero|external|field`. Ordinary record Time fields are never promoted to metadata. An assigned external
zero preserves its timestamp and origin even when FIELD supplies effective time.

After all segment layers and physical sources close successfully, `source-end` has nine cells: label,
ordinal, accepted logical data bytes, frames, emitted records, processed records, wire fields, actual
data physical bytes fetched and actual data-container bytes consumed. Resource rows have six cells:
`resource`, static scope (`sequence|data|time|scanner|parser`), ordinal or `-`, static counter name,
reserved capacity and actual observed count or `-`. Reserved values are never reported as measurements.

A single final `complete-v9` has seven cells: label, successfully closed sources, accepted logical data
bytes, frames, emitted records, processed records and wire fields. It is attempted only after all source
ends and the data preflush. Its bytes remain provisional until the final sink flush and successful result
or exit zero; a late flush failure still means FAILED even if marker bytes are visible.

Diagnostics have exactly thirteen cells: `incomplete-v3` (or secondary `cleanup-v3`), nonzero exit code,
static stage, static reason, optional source ordinal, layer (`data|time|parser|sequence|sink`), physical
bytes fetched, physical bytes consumed, logical bytes produced, parser bytes accepted, logical frame
start, logical payload start and sidecar line. Unknown coordinates are `-`; known zeros remain zero.
Coordinates are segment local and never imply an inverse compressed-to-logical mapping. Stage, reason
and resource-name tokens are bounded to 64 safe ASCII letters, digits or hyphens; callers supply only
the fixed documented inventory, never input text.

The owner uses these fixed stage tokens:
`admission`, `aggregate`, `allocation`, `close`, `configuration`, `container`, `flush`, `io`,
`open`, `parse`, `profile`, `record`, `schema`, `sidecar`, `stop`, `write`.
Its bounded reason vocabulary, including internal failure transfers, is:

```text
allocation-failure, cancelled, checksum, compressed-limit, configuration-refused,
container-failure, container-finish, counter-overflow, data-open, diagnostic-io,
diagnostic-output-limit, empty-segment, header-limit, identity-output-limit, identity-overflow,
inflated-limit, invalid-argument, invalid-container-state, invalid-data, invalid-directory,
invalid-header, invalid-presentation, length-limit, member-entry-limit, message-time-overlay,
missing-field-time, missing-source-header, missing-source-plan, missing-time-assignment,
missing-time-declaration, mixed-time-declaration, no-progress, none, not-finished,
ordinal-overflow, output-limit, parser-failure, parser-finish, ratio-limit,
recorded-source-failure, scanner-failure, scanner-finish, sink-io, source-close, source-io,
stream-failure, time-open, truncated, unsupported-container, work-limit
```

Every row uses the same subtraction-checked count-before-allocation builder. A zero budget authorizes
no nonempty row. `terminalReserveWorstCase` is allocation-free, includes the maximum-width complete,
resource and primary/cleanup diagnostic rows, and rejects sum/product overflow. The owner reserves it
before any source open and debits all source identities and all emitted row bytes from finite allowances.
The output9-only `sourceWireDescription` keeps every physical source field in its original order,
including the virtual FIELD prefix. `wire-record` and `wire-field` retain their existing cell grammar;
only the target-ID cell of an actual recognized virtual metadata action is `-`. Recognition requires
an authoritative nonzero plan prefix count exactly matching leading `EventTime`/full56 and the optional
immediately following `EventSequence`/full72. Those actions must have absent local targets and exact
matching primitive types. Every other action requires a present in-range local target and its exact
full type. Similar names, wrong full identities, nonleading Event fields and ordinary unbound actions
do not receive virtual treatment. An ordinary explicitly bound EventTime remains ordinary. Validation
precedes row counting/allocation; the schema/context and declaration remain borrowed only during the
call. The older `wireDescription` entrypoint retains its refusal for absent action targets.
