# First complete real-tape reading milestone

[Русская версия](MILESTONE-1.ru.md). English is authoritative.

**Result: complete canonical equality for the named private and portable ordinary corpus.**
The final binary-catalog/output8 artifact passes both producer STREAM/MESSAGE recordings
and the complete private RAW/MESSAGE file. The seven prior ordinary cases remain verified
through output7. No unsupported record is silently skipped. This is acceptance of the named
ordinary comparison boundary; it does not establish every binary catalog cell.

The output8 bridge validates its narrow ordinary subset before normalization and keeps
`CppMode` independent of Java source timing. The final suite has **93 checks: 44 previous
workflow checks, 47 DESIGNED grammar/generation controls and two NEW actual C++8
STREAM/MESSAGE successes**. The designed rows are derived from actual v7 captures and are
not production C++8 execution. The complete private output8 comparison is a separate actual
run. All four old-mode STREAM/MESSAGE refusals remain mandatory regressions.

## Executed comparison

Windows x64 checks on 2026-10-30 used the C++ `ordinary-v1` output7 and
`binary-catalog-v1` output8 inspector,
Java QD 3.355 at [upstream.lock](../../../../upstream.lock), Zulu JDK 8
`1.8.0_422-b05`, a 128 MiB Java heap and a 60-second bound per child. The original schema
boundary is documented by the [ordinary-schema validation](../wire-inventory/VALIDATION.md).
Native build/integration checks remain separate from this Java/PowerShell workflow. This report concerns the
actual inspector artifact hashes and tool-source hashes recorded in private run metadata.

The final Windows MSVC 19.42 Release artifact has SHA-256
`a199a657cc75bc6d19486dfa692422f60bf51c3fed6a65e5a4ae4229ef0ce6a1`.
The Java comparator source hash is
`fb3f54cd89e00251aa605b02eb825e48144d1df5ba6dfd869963054bcff43764`.
The complete 93-check run was observed from 18:14:00.214 to 18:15:29.362 UTC; the separate
actual private run from 18:14:00.444 to 18:14:30.401 UTC. These are overlapping calendar
intervals, not active engineering hours or performance benchmarks. Source, class, runner,
inspector and input hashes were stable across their runs.

[Compare-RealTapes.ps1](Compare-RealTapes.ps1) and the
[production Java oracle](../../../../tools/upstream/real-tape-acceptance/README.md)
compare every schema generation, local record/field name and ID, source description order,
sender ID, wire field name/full type/order/target, record frame/order, exact UTF-16 symbol,
raw scalar and nullable exact UTF-16 field value. RAW/MESSAGE also compares timestamp
presence/raw milliseconds, cursor sequence and cursor event time. Every observed event
flag is explicitly verified as zero. Native Java descriptors must use direct INT/LONG/OBJ
operations; business conversions are excluded.

Frame ordinals/count and schema/data publication context are checked. Ancillary frame
offset/type/payload columns, header properties and standalone heartbeat/timing/marker
diagnostics are not equality-projected; the record timing columns listed above are exact.

| Named recording | Result |
| --- | --- |
| Private ordinary RAW/MESSAGE sample | Complete equality of the mapped projection with zero mapped skips. |
| Producer 3.355 `compact-none` and full `quote-none` | Complete schema/source/value/symbol/order equality, three records each |
| Producer 3.356 `compact-none` and full `quote-none` | Same checks against the locked 3.355 parser, three records each; input producer identity remains separately pinned |
| Existing `mixed-stream` / `mixed-raw` | All eight exact full types 2/8/10/40/56/72/88/120; nullable/empty/nonempty UTF-16, raw scalars and MESSAGE timing where present |
| Existing `mixed-order` | Reordered wire fields, sparse sender 77 then late sender 78 sharing one record; current local defaults and original source identity preserved |
| Both producer pins' full `quote-message` | Actual output8 complete equality, three records each, including all three timing columns. Legacy descriptor/TIME and raw-message-v1 descriptor/MESSAGE still reject with native exit 4 |

The producer cases retain their [separate immutable provenance](../../../../tests/fixtures/protocol/real-tape/producer/README.md).
Mixed-field inputs retain their existing [field-type source and captures](../../../../tools/upstream/field-types-oracle/README.md).
The private input, path, schema names, symbols and decoded rows remain local. Its SHA-256
was unchanged before/after the comparison; no private expected values were promoted.

## Completion and regression evidence

The mapped production Java pass has zero unknown-data skips. Discovery is a separate
production pass with a genuinely empty scheme and NONE timing; all discovery skips
are explicitly reported. They do not count as decoding acceptance. Java RAW callbacks
expand into TICKER/STREAM/HISTORY; the comparator compares each wire record once through
STREAM, keeping callback fanout separate from wire counts.

EOF is observed on a child-JVM-local, single-handle actual-file input stream passed to the
unmodified production FileReader/parser. The instrumented-source pass requires EOF `-1`,
exact returned bytes, one open/close, zero read/close/parser errors and an empty sender-ID
queue. A separate literal-file production FileReader pass gives identical exact schema/data
rows and sequence hashes; this literal pass has no independent EOF hook. These are two
explicitly qualified observations, not a claim that byte counts alone prove EOF.

The Windows-qualified launcher requires an explicit native runner that owns its job before
child execution. A small Java worker redirects actual target stdout/stderr directly to raw
files; the runner's UTC diagnostics stay separate. Only worker-bootstrap compiler diagnostics
are merged UTC text. Root deadlines remain active through owner exit and stream drain, with
at most one additional second for confirmed cleanup. Regression probes require the final
absence of every recorded owned parent/child/worker/runner PID, including an exiting parent
whose child inherited its output handles. POSIX cleanup through this launcher is unqualified.

For native broad inspection, success qualifies read, physical EOF, reader finish, write
and final flush through borrowed streams. Native owner close-before-completion is outside
that API and belongs to later source integration. This does not weaken or inherit the
separate Java instrumented source's explicit close/error proof.

The portable [regression command](../../../../tools/upstream/real-tape-acceptance/README.md#invocation)
checks positive parity and rejects changed raw values/types, symbol, sender/local IDs,
generation, record/frame order, schema publication order, extra records, schema types,
source bindings and each timing column. It also checks selected description/data truncation,
nonzero-flag refusal, input/record/output limits, and a native output pipe closed while the
inspector is still writing. The closed sink returns I/O exit 3; exhausted output returns 6;
malformed truncation returns 5. None produces `complete`.

Injected late I/O returns every physical input byte and then throws before EOF. Production
FileReader can return after swallowing that error; acceptance still rejects it. A separate
injected error after closing the actual handle is also rejected. Java and PowerShell help
are checked without operational inputs, and incomplete arguments cannot start work.

The Java source is formatted with pinned google-java-format 1.36.1 using a separate JDK 23;
the coordinated project Format/Check passed across 34 sources without changing bytes.
An independent frozen-source review and ten separate Java probes found no P1/P2 issues;
final runtime evidence remains separately recorded. Full raw stdout/stderr, command arrays, UTC bounds,
classpath/JDK/both-Java-source/worker-and-oracle-class/runner/inspector/input hashes and actual limits remain in ignored local
metadata. Configured heap/time/output limits do not establish measured peak RSS or throughput.

## Qualified boundary

Both STREAM/MESSAGE recordings are accepted through the explicit binary-catalog profile;
their four old-mode refusals remain visible. The positive cases establish their named
ordinary profiles only. Indexed/time schemas, built-in event fields, new full types,
nonzero flags, objects, compression, sidecar/split sources and business projections are
outside this result. A zero-record run or a resource stop cannot satisfy populated
whole-file acceptance. Independent bridge review, final evidence hashes and broader native
integration checks remain distinct evidence categories; they do not enlarge this comparator's
schema/data/order/symbol/raw-time projection scope.
