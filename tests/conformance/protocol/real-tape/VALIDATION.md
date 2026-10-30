# Named finite binary tape validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Result: the named finite profiles passed the final-source checks below on
2026-10-30 UTC. This is a compatibility result, not a released version.

The compatibility targets are QD 3.355
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and the separately selected QD 3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`, fixed in the
[producer lock](../../../../tools/upstream/real-tape-corpus/producer.lock.json).
The [catalog](COVERAGE.md) accounts for every row in the finite matrix. It does not
extend the narrower legacy profiles or imply full QD, network, subscription,
collector or business-event adapter compatibility.

## Profiles and comparison authority

`BINARY_CATALOG_V1` preserves the 19 documented finite field identities, wire
schema generations, sender/local IDs, record order, owned symbols and payloads,
flags and removal metadata. See the
[field validation](../field-types/VALIDATION.md) and
[dispatch validation](../field-types/DISPATCH-VALIDATION.md).

`SELF_CONTAINED_SEGMENTS_V1` owns a finite explicit occurrence list, closes every
opened source and validates parser, sidecar and container completion before the
single final result. Its homogeneous time policy is explicit: NONE, MESSAGE,
FIELD, LONG or TEXT. Container kinds may differ through the API; the CLI selects
one data kind and one companion kind for the whole invocation. See the
[sequence contract](../../../../spec/protocol/TAPE-SEQUENCE.md).

The maintained complete-file comparator's source profile is deliberately narrower:
one uncompressed MESSAGE source, ordinary schemas, exact types
2/8/10/40/56/72/88/120, zero flags and absent FIELD/removal metadata. It checks all
27 output9 cells, source identity, resource inventory, source end and final sums.
It refuses broader time/value families rather than silently discarding them.
Other catalog cells use their named production fixtures and native regression
checks; the ordinary corpus is not evidence for those cells.

MESSAGE raw timestamp authority is the actual production heartbeat callback.
Packed cursor time is read from Java `RecordCursor`; event milliseconds use the
production `TimeSequenceUtil` projection. Effective time in this narrow profile
selects that external cursor, or zero when packed time is zero. These are distinct
observations and derived projections. A separate record-counted timing digest
prevents an unchanged schema/value digest from standing in for a fresh time check.
The actual Java protocol descriptor must declare exact `message` or `MESSAGE`
before any record attempt; forced FileReader settings do not prove that property.

The instrumented stream observes physical EOF, open/close counts and I/O errors.
A second literal-filename FileReader pass must produce the same full projection.
That literal pass is not separately instrumented for physical EOF or close counts.
Parser corruption callbacks must be zero; mapped wire records and STREAM
projections must agree with zero skipped values.

## Reproduction and qualification

| Executed check | Result and boundary |
| --- | --- |
| Windows MSVC 19.42 x64 Release full CTest | 74/74 passed in 129.68 seconds, including finite source I/O, CLI, large-stream and negative checks. |
| Linux GCC 13.3 Debug ASan/UBSan full CTest | 74/74 passed; 391.70 seconds CTest elapsed. Leak detection was disabled; this is not separate LeakSanitizer certification. |
| Production-only profiles | Three Release profiles on each OS passed: minimal, inspection/writer/replay with JSON OFF, and explicit JSON ON. Separate dependency gates verified optional compression OFF and QTP-only no extra C language/acquisition. |
| Existing public Java acceptance | Fresh 93-check run passed after the source change. |
| Extended public Java acceptance | 120 checks passed: the existing 93 plus 27 actual output9/source controls. Both producer MESSAGE files, matching descriptor update, physical missing/unknown/conflicting time, raw/effective time, identity/resource/closure mutations and late read/close errors are covered. This PowerShell workflow is Windows-qualified. |
| Complete private ordinary source, both parser pins | Complete equality at both pins with zero mapped skips or corruption callbacks. |
| Java style and review | Global pinned google-java-format 1.36.1 Format/Check passed for 36 sources using JDK 23. Independent final-source comparator and wrapper contract review passed; actual executions remain separate from review. |

The Windows inspector artifact SHA-256 is
`de06575cf11bcea4652f6e9b537b7314244670994ddc2841533ed1bc3ea1cb6b`.
The Linux GCC Debug ASan/UBSan inspector artifact SHA-256 is
`c49da0e58eccffc087b188000dcb620990d0daeb1cb4ebe392481c67d277337a`.
The native runtime source revision is
`73c7259ed216f81c1950afe1fb05ca07edbc9441`; the final CLI test script was also
rechecked on both operating systems after its whitespace cleanup.
The public Java comparator source SHA-256 is
`18f1f23e4cbba20ed292c94d8632a178aeb9fe9260220c43d856ea5dffe1c983`;
the comparison wrapper is
`0e56f68f22973eec30d01455645ebcdbb28569716574c6b40f4583d1c5ba02d8`,
and the acceptance test script is
`30c244b4425a8f9ac328a87d4a16df7402582548069feae18e0d8c5103df1c8d`.
Exact commands, JDK/worker/comparator-class/input hashes and all 1,317 classpath
file hashes per parser pin (1,290 classes plus 27 resources) were retained privately.
Input, pin, source, classpath and inspector identities were unchanged across the
complete comparisons. Historical metadata was preserved rather than rewritten.


Use [Compare-RealTapes.ps1](Compare-RealTapes.ps1) with
`-SourceProfile self-contained-segments-v1 -TapeMode raw-message-v1` and explicit
`-OracleProfile baseline` or `current`. The default baseline retains the historical
lock check. The workflow uses prebuilt clean source classes, JDK 8, finite child
heap/time/output limits and unchanged-input/source/classpath hashes. It never
builds or modifies upstream. All source-dependent raw output remains private.

The native source checks are reproducible in the configured build tree:

```powershell
ctest --test-dir $buildDirectory --output-on-failure `
  -R '^(protocol_field_time|sidecar_.*|compressed_input|source_format|tape_sequence_admission|source_sequence_.*)$'
```

The full-suite results above additionally include all pre-existing tests. The
three-million-record source check distinguishes logical resource charges from
process RSS; it is a bounded streaming check, not a throughput benchmark. An
output marker is provisional until sink flush and process exit succeed.

macOS, live following/cycling, implicit file discovery, general ZIP archives,
mixed CLI time policies, object deserialization and additional event adapters
remain outside the claim. Logical resource counters do not measure RSS.
