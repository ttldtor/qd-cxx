# Bounded offline inspection, version 1

[Русская версия](OFFLINE-INSPECTION.ru.md). English is authoritative.

This is CLI policy for QDCXX-017, specified before CLI implementation, not new Java wire semantics.
Reuse [framing](FRAMING.md), [descriptions](DESCRIBE-RECORDS.md), [binding](STATIC-BINDING.md),
[data](STREAM-DATA.md) and [symbols](../encoding/SYMBOLS.md). The JSON interchange contract is unchanged.

## Input and sequencing

One named binary file contains raw QTP envelopes, not a tape container. No stdin, autodetection,
network, importer, recovery, resetSession or dynamic session. Empty input succeeds with zero frames
and records; description-only and heartbeat-only files succeed, without claiming populated coverage.
Accept any number of initial complete type-2 frames, including empty ones. Accumulate descriptions
in order; reject duplicate sender IDs within/across frames. Validate every description through the
existing binder immediately, including unknown local names and unsupported types in description-only
files. At the first type-15 frame require a successfully parsed description frame, then create one
immutable context. Reject every later type-2 frame, even identical or empty. EMPTY_HEARTBEAT is a
framing event anywhere. Reject every other message type, including explicit zero, as unsupported.
Only exact ordinary type 8, EMPTY_SET and raw flags zero are supported. Unknown local names are
consumed/skipped; undescribed IDs fail. Every data call starts fresh symbol state.

The tool integration takes an independently validated LocalSchema; the named built-in schema is
only a replaceable CLI adapter. No schema is inferred from sender descriptions. File I/O and
presentation stay under tools/qtp-inspect, outside the production libraries.

## Bounds and arithmetic

[Arguments/defaults](../../docs/QTP-INSPECT.md) define finite decimal limits. Zero is a real bound,
never unlimited. Chunk size must be positive and fit streamsize; frame size must allow checked +9
storage and fit signed 64 bits. All limits fit size_t and uint64_t. A smaller total than message
limit is compatible: pass their minimum. Reject malformed, negative, overflowing, duplicate or
unknown arguments and missing operands before input/output. --help is exclusive.

Read at most chunk-size bytes at a time and at most the remaining input budget. At an exact input
limit, peek only to distinguish actual EOF from excess input; do not feed the extra byte. A failed
read is I/O failure, never finish. Process gcount bytes of an EOF short read, drain delivered frames,
resubmit each unaccepted suffix, and call finish only at actual EOF. Do not process later frames
after the first error. Read-ahead within one bounded chunk does not authorize parsing later frames.

Frame count includes heartbeats. Accumulated description, action and UTF-16 name budgets count
repeated names/fields. Per-name encoded bytes are bounded by frame size; units by max-name-units.
Fields per description are bounded by remaining actions. The immutable context receives the same
aggregate bounds. Local schema inspection has fixed limits {records=1024, fields=65536,
lookups=65536, nameUnits=1024, totalNameUnits=1048576}; these also bound independent supplied schemas.

Data uses min(message limit, remaining file limit) for processed records, wire fields, emitted
values and owned symbol units. Emitted records are bounded by the processed-record allowance.
Subtract actual processedRecords/wireFields charges, including skipped/failed attempts, and actual
completed output values/symbol units. No per-message reset restores a file budget. Per-symbol
codeUnits/stateUnits share symbol-units; symbol remaining-span inputBytes and payloadBytes use
max-frame. Total emitted records cannot exceed total processed records. Exact bounds succeed;
excess fails at the existing parser checkpoint. Checked subtraction precedes any cumulative addition.
Input, frames, descriptions, actions, names, data work and stdout bytes all remain finite.
Budgets bound logical work/storage, not exact RSS or allocator overhead. Results are held for at most
one bounded payload; stdout is streamed, never accumulated for the whole file.

## Deterministic stdout

ASCII TSV, LF only on every host, no BOM, no locale-dependent formatting. One row per line,
single TAB separators; no escaping is needed because text is lowercase UTF-16 unit hex (four
digits/unit, no normalization or surrogate replacement). `-` denotes an empty hex/value sequence
or an unavailable frame field; empty TSV cells are forbidden. Decimal integers use no plus sign,
leading zeroes (except zero), or negative zero. Values are signed int32; IDs are nonnegative.

| Row | Ordered cells including row type | Arity |
| --- | --- | --- |
| format | format, qd-qtp-inspect, 1 | 3 |
| frame | frame, zero-based index, start, exclusive end, numeric type or -, payload start or -, payload byte count | 7 |
| record | record, frame index, zero-based emitted index, local ID, local record name hex, symbol hex, comma-separated local-order signed values or - | 7 |
| complete | complete, accepted input bytes, frames, emitted records, charged records, charged wire fields | 6 |

Exactly one format row precedes frame/record rows. Frame rows precede their complete record rows;
frame order and emitted indices are contiguous. Unknown local records have no record row.
Names come from the explicit local schema, IDs are local IDs, not sender IDs. Field IDs are their
positions in the ordered values cell; missing values are the decoder's raw zero. No per-record
wire offset or sender-ID reconstruction is claimed. Frame payload start is checked
`endOffset - payload.size()`, using actual boundaries, even for noncanonical length/type widths.
Heartbeat payload start/type are `-`, size zero. Empty symbols and zero-field records remain distinct.

Success writes exactly one final complete row after flushing preceding output. Each logical record
is assembled and budget-checked before writing. Payload failure may leave completed earlier records,
including a completed prefix of that payload; never intentionally publish a partial decoded record.
On detected failure emit no subsequent completion and label stderr incomplete. Physical output
failure can truncate any row or occur during the final completion write/flush; already delivered
bytes cannot be retracted. Exit status, not a marker alone, determines success. No Java callback-batch
parity is claimed. This unavoidable I/O property is not a wire compatibility deviation.

## Diagnostics and exits

Exit 0: success/help; 2: usage; 3: input/output I/O; 4: unsupported profile or sequencing;
5: malformed data (including duplicate/undescribed IDs and truncated final input); 6: resource or
allocation failure. Resource exhaustion is not malformed Java data. Unknown profile is usage.
First error is terminal. Usage/help happen before data output; help is human-readable ASCII, not TSV.

Stderr diagnostic is one LF-terminated TSV row:
`incomplete, exit, stage, frame-index, type, frame-start, payload-start, consumed, error,
absolute-consumed, absolute-error` (11 cells). Unavailable positions are `-`. Parser stages are
`description.N` / `data.N` where N is the documented enum ordinal in their headers; framing uses
`framing.N` with FrameError ordinal. Additional checkpoints are `usage`, `open`, `read`, `write`,
`flush`, `input-limit`, `frame-limit`, `sequence`, `type`, `duplicate-id`, `binding.N`, `allocation`.
For description/data failures consumed/error are distinct payload-relative positions; absolute
positions are checked payload-start plus the corresponding value. Binding and duplicate checks
occur after the complete description payload and report its end, not a fabricated primitive offset.
Framing failure reports frame start and absolute accepted position; type/payload/error point remain
unavailable. For I/O/resource output errors retain current frame context when known, without guessing
payload positions. If a new incomplete envelope is pending, read/input-limit diagnostics identify
its index/start and leave its type/payload positions unavailable instead of reusing the previous frame.
Diagnostics themselves may fail on an unusable stderr; return remains nonzero.

## Evidence boundary

Existing STREAM_DATA/binding/symbol captures supply reused semantic cases. Add a locked Java
production composer/parser workflow with raw .qtp bytes and observable records before CLI code.
Require two clean captures and negative/baseline Verify. Designed CLI fixtures define sequencing,
budgets, diagnostics and TSV; Java does not define those policies. Final validation and
independent review must remain distinguishable. No timing or full protocol compatibility claim.

## Opt-in human presentation, version 1

`--output-format tsv|human` selects presentation only; omitted means `tsv`. Duplicate options,
unknown values and missing operands are usage errors. No TTY detection. The existing TSV bytes,
stderr contract, decoding, order, charged counters and all non-output budgets are unchanged.
Human output is a designed C++ tool policy, not a Java observation.

UTF-8 without BOM, LF only, locale-independent decimal numbers. Exact line grammar (one space
between items) is illustrated below; braces contain comma-space separated local-order fields:

```text
qd-qtp-inspect human 1 profile=oracle-sre-v1 identities=local
Frame 0: DESCRIBE_RECORDS (2) bytes=[0,9) payload=2 size=7
Frame 1: STREAM_DATA (15) bytes=[9,14) payload=11 size=3
Record 0: frame=1 local=0 name="S" symbol="" fields={S.v=1}
Complete: bytes=14 frames=2 records=1 charged-records=1 wire-fields=1
```

Heartbeat name is `EMPTY_HEARTBEAT`, ID/payload `-`, size 0. Other types use `UNSUPPORTED`
with their numeric ID, followed by the unchanged error. No success summary after detected failure.
Names and symbols are quoted, empty symbol is `""`, zero fields are `fields={}`. Record IDs and
fields are local, never reconstructed sender identities. Built-in display metadata explicitly uses
`S.v`, `R.y`, `R.x`, `R.z` in local order. Lookup aliases and incoming descriptions never select
labels. Independently supplied schemas default to profile `local-schema` and `field[index]` labels.
This metadata stays in the tool, with no core/JSON changes.

Exact escaping table for quoted text (uppercase hexadecimal, four digits in `\uXXXX`):

| Input UTF-16 | Output |
| --- | --- |
| U+0022 quote | `\"` |
| U+005C backslash | `\\` |
| U+0009 TAB, U+000A LF, U+000D CR | `\t`, `\n`, `\r` |
| Other U+0000–001F, U+007F–009F | `\uXXXX` (including NUL and ESC) |
| U+2028–2029, U+061C, U+200E–200F, U+202A–202E, U+2066–206F | `\uXXXX` |
| Lone U+D800–DFFF surrogate | `\uXXXX` |
| Valid high/low surrogate pair | Matching Unicode scalar encoded in UTF-8 |
| All other units | Their scalar encoded in UTF-8 |

No normalization/replacement. Escaping backslashes distinguishes literal escape lookalikes.
No quoted input can introduce terminal escapes, physical lines or apparent diagnostic/completion rows.

`max-output` charges actual encoded bytes for every line including header and summary. Human costs
vary with UTF-8 and escaping (up to six bytes per UTF-16 unit plus quotes); TSV costs remain four
bytes per unit, or one for empty. Each row is first counted using checked subtraction against remaining
output bytes, then allocated and composed with that exact size. No expansion-sized temporary is
created before this check; at most one bounded row is retained. Insufficient row budget fails at
`write` before publishing any of that row. Logical sizes, not allocator capacity/RSS, are bounded.
Completed-prefix, first-error precedence and pre-summary/final flush checks remain unchanged.
A physically delivered summary never overrides a failing exit status.

Raw QTP envelopes are not QD tape. No tape reader, compression, time-file processing,
protocol-descriptor handling, broader fields, importer, transport or dynamic sessions are added.

## No-argument invocation

An invocation with no arguments is equivalent to standalone `--help`: identical help bytes on stdout,
empty stderr and exit 0 when writing/flushing succeeds. No file is opened and no inspection format or
completion row is emitted. Output failure still returns I/O exit 3. A nonempty incomplete/invalid
argument list remains usage exit 2; `--help` combined with other arguments remains invalid.
This is CLI policy only; supplied-input TSV/human output and wire semantics are unchanged.
