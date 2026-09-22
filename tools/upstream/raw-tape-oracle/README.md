# RAW/MESSAGE production oracle

[Русская версия](README.ru.md). English is authoritative.

This QDCXX-023 harness observes the locked, unchanged QD through `FileWriter/FileReader` and
actual `DXEndpoint → TapeConnector → FileWriterHandler`. Default Quote uses unchanged
`DXFeedScheme` and `QuoteDelegate`, including fractional sizes. S/R/E remains a focused exact-type-8
scheme. Production artifacts are never header-stripped or message-ID rewritten. Separately named
`diagnostic-wire` cases construct/mutate envelopes and replay them through production FileReader.

Run from the repository root with PowerShell 7.4+, JDK 8, Maven and the existing QTP inspector.
Each invocation requires a fresh directory. The baseline tape launcher checks the clean locked
checkout, performs a clean upstream reactor build, captures classpath/provider hashes, and verifies
all old tape artifacts. The new harness has a 30-second bound; timeout is failure, never EOF evidence.

```powershell
$rawArgs = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/profile/.m2/wrapper/dists/apache-maven-3.9.16/0daed3be3ebd1c706f0e69e8b07c6b73f5cc4ea3dfce72a8d0ec2e849ca2ddb0/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
& tools/upstream/raw-tape-oracle/Invoke-Oracle.ps1 @rawArgs -RunDirectory .work/raw-a
& tools/upstream/raw-tape-oracle/Invoke-Oracle.ps1 @rawArgs -RunDirectory .work/raw-b
& tools/upstream/raw-tape-oracle/Compare-RawTape.ps1 -Actual .work/raw-a/raw-tape -Expected .work/raw-b/raw-tape
& tools/upstream/raw-tape-oracle/Test-Negatives.ps1 -RunDirectory .work/raw-negatives
& tools/upstream/raw-tape-oracle/Invoke-Oracle.ps1 @rawArgs -Mode Verify -RunDirectory .work/raw-verify
```

`Compare-RawTape.ps1` is the strict Verify core: exact finite artifact set, typed row arities,
canonical signed integers, UTF-16 and IEEE bit grammars, full type/field/record/callback order,
completion/counts and byte-for-byte equality of every artifact. Capture checks structure against
itself; cross-capture and Verify establish equality to independently retained expectations. Negative
checks mutate bytes, field values/types, timing, order, arity, file set and completion; the unchanged
baseline is checked afterward. Expectations are never rewritten by Verify.

`*.qtp` are input bytes, `*.tsv` are real parser/delegate observations, and `*-frames.tsv` are
supplemental envelope scans. `settings.tsv` retains effective requested settings, unchanged schema
shape and original Quote API input; `packing.tsv` retains independent TimeSequenceUtil values.
Timestamps/paths in metadata/logs remain run-specific. No normalization is performed.

RAW_DATA is delivered by Java as callbacks 10,15,20; the C++ comparator uses only STREAM_DATA (15)
as the one-wire-record projection. See [contract](../../../spec/protocol/RAW-TAPE.md),
[coverage](../../../tests/fixtures/protocol/raw-tape/COVERAGE.md),
[captures](../../../tests/fixtures/protocol/raw-tape/CAPTURES.md) and
[validation](../../../tests/fixtures/protocol/raw-tape/VALIDATION.md). Java's permissive descriptor,
option and heartbeat behavior does not expand the bounded C++ policy.

The separate `RawTapeInventory.java` / `Check-Inventory.ps1` path inventories opaque data envelopes
and structurally valid descriptions; it is not the production acceptance oracle. Its source hashes
are excluded from the oracle tool manifest and recorded by its own runs. Private input is never
copied into these fixtures; generated acceptance does not claim private-file compatibility.
