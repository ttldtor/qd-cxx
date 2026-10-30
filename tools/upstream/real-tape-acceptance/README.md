# Ordinary real-tape acceptance

[Русская версия](README.ru.md). English is authoritative.

[Compare-RealTapes.ps1](../../../tests/conformance/protocol/real-tape/Compare-RealTapes.ps1)
compares ordinary C++ output7 or the strictly validated ordinary subset of output8 with
the locked Java production parser. It checks
all ordinary schema snapshots, generation numbers, original sender IDs, source field order
and bindings, local union IDs/order, record order, exact UTF-16 symbols, full field types,
raw integers and nullable exact UTF-16 values. RAW/MESSAGE also compares timestamp presence,
raw timestamp milliseconds, cursor sequence and cursor event time. Both paths require zero
event flags; this does not establish nonzero-flag support.

## Invocation

Use PowerShell 7.4+, JDK 8, a clean prebuilt checkout at [upstream.lock](../../../upstream.lock)
with `qds-file` dependencies, an explicit [native `qdcxx-run-utc`](../../dev/README.md),
and a built `qdcxx-tape-inspect` with
[`--wire-schema ordinary-v1`](../../../spec/schema/WIRE-INVENTORY.md). No upstream build,
dependency installation, source modification or network connection occurs. The JDK 21+
[formatter](../../format/README.md) is separate from the oracle JDK 8.

Help requires no working inputs and starts no file work:

```powershell
pwsh -NoProfile -File tests/conformance/protocol/real-tape/Compare-RealTapes.ps1 -Help
Get-Help ./tests/conformance/protocol/real-tape/Compare-RealTapes.ps1 -Full
pwsh -NoProfile -File tools/upstream/real-tape-acceptance/Test-Acceptance.ps1 -Help
```

Run from the repository root. Define `$tape`, `$inspector`, `$lockedCheckout` and `$oracleJdk`
as explicit local paths, and `$runner` as the native runner executable. Choose a fresh
result directory under repository `.work`. This PowerShell process workflow is qualified
on Windows; POSIX cleanup through this launcher is not qualified and is rejected:

```powershell
& ./tests/conformance/protocol/real-tape/Compare-RealTapes.ps1 `
    -InputFile $tape -Inspector $inspector -UpstreamRoot $lockedCheckout `
    -JavaHome $oracleJdk -Runner $runner -RunDirectory .work/tape-acceptance -TapeMode raw-message-v1
& ./tools/upstream/real-tape-acceptance/Test-Acceptance.ps1 `
    -Inspector $inspector -UpstreamRoot $lockedCheckout -JavaHome $oracleJdk -Runner $runner `
    -RunDirectory .work/tape-acceptance-regressions
```

`TapeMode` is mandatory: `legacy` selects Java NONE source timing and `raw-message-v1`
selects Java MESSAGE timing. Optional `CppMode` defaults to `TapeMode`; it selects C++
`legacy`, `raw-message-v1` or `binary-catalog-v1`. The last choice selects explicit
`--wire-schema binary-catalog-v1 --tape-mode binary-catalog-v1` and lossless output8.
No Java timing policy is inferred from C++ mode or descriptor text. Existing ordinary-v1/v7
mode restrictions and the four old STREAM/MESSAGE refusal checks remain unchanged.

The [milestone result](../../../tests/conformance/protocol/real-tape/MILESTONE-1.md) keeps both
producer pins' STREAM/MESSAGE files visible. An actual output8 artifact must pass them and
the private corpus before overall acceptance. The final named-corpus report records those
actual passes; a designed output8 row remains separate from production C++8 evidence.

## Output8 ordinary comparison bridge

The Java option `--cpp-projection ordinary-v1|binary-catalog-v1` defaults to ordinary-v1.
The PowerShell helper selects it from `CppMode`. Version8 requires the exact five-column
format header and every `wire-data-v8` row must have exactly 14 columns. Each value must
have exactly `fullId:raw:units:bytes`. The bridge accepts only the original eight full IDs,
canonical signed raw values, canonical UTF-16/null/empty spellings, bytes `-`, flags literal
`0` and absent removalTime `-`. It validates these fields before forming the old comparison
view. New full types, byte payloads, nonzero flags and removal metadata are refused, even
when a payload is empty or a removal time is zero.

Every descriptor-v8 row has exactly nine bounded columns, canonical presence bits/UTF-16
hex and a send mask containing only the four known family bits. Those header values are
grammar-checked; this does not add equality claims for ancillary descriptor properties.
NONE source timing requires absent timestamp plus sequence/eventTime both zero before
dropping the three fixed timing columns. MESSAGE compares all three exactly. Source IDs,
frame/order/generation, full schema snapshots and complete counters remain exact.
Broad empty description batches do not increment generation; compatible nonempty sender
replacements publish a new generation and retain local union names/types/order/defaults.
Old v7 retains its existing empty-batch and replacement behavior.

Run the designed grammar controls separately from actual artifact acceptance:

```powershell
& ./tools/upstream/real-tape-acceptance/Test-Acceptance.ps1 `
    -Inspector $inspector -UpstreamRoot $lockedCheckout -JavaHome $oracleJdk -Runner $runner `
    -RunDirectory .work/tape-acceptance-designed -DesignedV8
# Requires an actual binary-catalog-v1/output8 C++ artifact:
& ./tools/upstream/real-tape-acceptance/Test-Acceptance.ps1 `
    -Inspector $inspector -UpstreamRoot $lockedCheckout -JavaHome $oracleJdk -Runner $runner `
    -RunDirectory .work/tape-acceptance-binary -BinaryCatalog
& ./tests/conformance/protocol/real-tape/Compare-RealTapes.ps1 `
    -InputFile $tape -Inspector $inspector -UpstreamRoot $lockedCheckout -JavaHome $oracleJdk `
    -Runner $runner -RunDirectory .work/tape-acceptance-binary-file `
    -TapeMode raw-message-v1 -CppMode binary-catalog-v1
```

Without these switches the test retains the original 44 actual v7 checks. DesignedV8 adds
labelled rows derived from actual v7 captures and strict positive/negative grammar controls;
its metadata explicitly records that production C++8 was not executed. BinaryCatalog adds
two actual producer STREAM/MESSAGE successes and retains all four old-mode refusals.

| Parameter | Default and constraint |
| --- | --- |
| `CppMode` | `TapeMode`; legacy/raw-message-v1/binary-catalog-v1, independent of Java source time |
| `MaxInputBytes` | 8 MiB; 1..64 MiB |
| `MaxRecords` | 1,000,000; 1..10,000,000 wire records |
| `MaxOutputBytes` | 256 MiB; 1 byte..1 GiB per captured child stream and C++ projection |
| `TimeoutSeconds` | 60; 1..300 seconds for the native ownership root, plus at most 1 second for confirmed cleanup |
| `HeapMiB` | 128; 32..1024 MiB per Java runtime |

The Java reader additionally bounds each TSV row to 4 MiB, descriptions to 4096, source
fields to 65,536 and aggregate name units to 1,048,576. C++ receives explicit finite input,
output, record, field/value, frame and timing budgets; other existing library bounds remain.
The raw worker polls target files every 20 ms, with possible polling overshoot. Its heap is
32 MiB; its bootstrap compiler uses 128 MiB. Native runner diagnostics have a separate 8 MiB
cap. The PowerShell deadline remains active through owner exit and stream drain. These limits
are logical bounds and JVM heap settings, not measured peak RSS.

Exit 0 from the comparison requires complete equality and observed clean EOF. Failure,
timeout, output exhaustion, changed input/source or setup errors return nonzero/throw.
`metadata.json` remains `INCOMPLETE` after interrupted/failed work. The test harness reports
`PASS-WITH-NAMED-GATES` when its old-profile positive and negative checks pass; this status
alone does not establish broad STREAM/MESSAGE acceptance. `PASS-WITH-BINARY-CATALOG-NAMED-CASES` adds the
two actual v8 portable cases only; private full-file acceptance remains separately required.
Run directories are never reused. Only owned
children are terminated on timeout/output failure.

The runner establishes its Windows job before JVM execution. An internal
[RawChild.java](RawChild.java) uses `ProcessBuilder.redirectOutput/redirectError` to keep
actual C++ and Java target streams byte-exact in separate files. It publishes small terminal
status and exits on target exit or resource stop; the native owner removes all remaining
descendants, including grandchildren whose parent already exited. UTC runner diagnostics
are separate. Only the initial worker compiler uses merged UTC diagnostic text; that text
is never treated as raw C++ output. Source hashes for both Java files, worker/oracle class
hashes and native runner source/artifact hashes are recorded and checked before/after.

## Oracle independence and completion

[RealTapeAcceptance.java](RealTapeAcceptance.java) runs the unmodified locked production
`FileReader` and `BinaryFileQTPParser`. Protected hooks observe descriptions and original
sender IDs; C++ names or values never construct the Java expectation. Discovery uses an
empty Java scheme and genuine unknown-data skips. It uses NONE timing because the locked
parser can dereference a null skipped cursor when MESSAGE timing is active. Discovery
establishes the schema inventory only.

The mapped pass constructs an owned ordinary Java scheme from the observed descriptions,
using the exact serial field classes and physical VOID tails for WideDecimal. Every
descriptor operation must be direct INT/LONG/OBJ. Conversion, built-in event fields and
unsupported full types are explicit rejections. Ordinary-v1 retains replacement refusal;
binary-catalog-v1 accepts only compatible ordinary replacements described above. STREAM
callbacks retain the original wire order through an owned descriptor queue; borrowed
cursors are read only during callbacks. Earlier rows use the field prefix available in
their own generation. Missing source fields use production raw-zero/null defaults.

The production `FileReader` swallows some IOExceptions. Consequently, return plus matching
byte counts is insufficient. A child-JVM-local URL handler supplies one fixed actual-file
stream and observes EOF `-1`, returned bytes, opens/closes and read/close errors. BINARY,
compression NONE, MAX_SPEED, explicit time mode, no sidecar/cycle/resync and no network source
are fixed. This is an **instrumented-source production parser observation**. A separate
literal-file `FileReader` pass must compare identically and produce the same schema/data
sequence hashes; that literal pass has no independently observed EOF hook.

Native binary-catalog inspection qualifies input read, physical EOF, reader finish, output
write and final flush through the borrowed stream APIs. It does not establish that the
native source owner closed its stream before completion; owned-source close integration
belongs to later source work. The instrumented Java source's explicit open/close/error
proof is a separate, stronger observation and remains mandatory here.

Frame ordinals, their count and schema/data publication context are checked. Ancillary
frame offsets, message-type/payload columns, header properties and standalone heartbeat,
timing or marker diagnostic rows are not projected for equality. Record timing columns are
compared exactly. Existing library conformance checks own those additional envelope
contracts; this comparator does not independently prove every diagnostic row.

The comparator reads C++ rows as bounded ASCII TSV, compares expected metadata/data rows
exactly, rejects extra projection rows, checks terminal counts and rejects any bytes after
`complete`. Hashes summarize rows already compared exactly. Source SHA-256 is checked before
and after; metadata records source/helper/launcher/inspector/JDK hashes, actual upstream
commit, complete classpath file inventory, limits, commands, UTC bounds and raw streams.
Private paths, schema names and values can occur in those logs; retain them only in `.work`.

## Regressions and source references

[Test-Acceptance.ps1](Test-Acceptance.ps1) compares both producer pins' compact and complete
Quote NONE recordings, mixed eight-type STREAM/RAW recordings and a reordered/late sender
case. Mutations cover raw value/type, symbol, sender/local ID, generation, order, extra
records, schema type, source binding and all three timing columns. It checks truncation,
nonzero-flag refusal, input/record/output limits, a genuinely closed native output sink,
Java/PowerShell no-argument/help equivalence without working inputs, and rejection of
late read/close errors. The late-read injection returns every physical source byte before
throwing instead of EOF; byte equality cannot accidentally pass it.
[Test-ProcessOwnership.ps1](Test-ProcessOwnership.ps1) additionally reproduces an exiting
parent with inherited child streams, a one-second running-root deadline and raw output
exhaustion. Every recorded owned parent/child/worker/runner PID must be absent on return.
The intentionally closed native output-pipe test remains a distinct direct inspector
I/O test; it is not used as process-tree cleanup evidence.

Primary evidence at the locked commit:

- [`FileReader.createParser`, `readInto`, `process`, `tryOpenFile`](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReader.java).
- [`BinaryQTPParser.parseDescribeRecords`, `remapRecord`, `parseData`, message hooks](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java).
- [`BinaryRecordDesc` protected copy/fields, `readRecord` and direct operations](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java).
- [`MessageConsumerAdapter.expandRawData`](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/MessageConsumerAdapter.java)
  defines TICKER/STREAM/HISTORY fanout; wire records are compared once through STREAM.

The [milestone report](../../../tests/conformance/protocol/real-tape/MILESTONE-1.md) states
executed scope and remaining gates. No general indexed-schema, object deserialization,
nonzero-flag, compression, sidecar, split-file or business-conversion support is claimed.
