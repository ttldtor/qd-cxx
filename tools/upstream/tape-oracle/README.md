# Tape oracle

[Русская версия](README.ru.md). English is authoritative.

This is Java QD FileWriter/FileReader research tooling, not a tape implementation. See the
[profile and source inventory](../../../spec/protocol/TAPE-FILES.md) and
[evidence](../../../tests/fixtures/protocol/tape/VALIDATION.md). No new Python script or Python
dependency is used by this workflow; existing Python tools are unchanged.

## Capture and Verify

Use PowerShell 7.4+, Git, the locked clean upstream checkout, JDK 8, Maven and the current built
`qdcxx-qtp-inspect`. Use JDK 21+ separately for the pinned Java formatter. Run from the repository root:

```powershell
$oracleArgs = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/apache-maven/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
./tools/upstream/tape-oracle/Invoke-Oracle.ps1 @oracleArgs -Mode Capture -RunDirectory .work/tape-review-a
./tools/upstream/tape-oracle/Invoke-Oracle.ps1 @oracleArgs -Mode Capture -RunDirectory .work/tape-review-b
$java = Join-Path $oracleArgs.JavaHome 'bin/java.exe'
$classes = '.work/tape-review-b/harness'
& $java -cp $classes qdcxx.oracle.TapeVerify compare .work/tape-review-a/artifacts .work/tape-review-b/artifacts
& $java -cp $classes qdcxx.oracle.TapeVerify negative tests/fixtures/protocol/tape/expected .work/tape-review-negatives
./tools/upstream/tape-oracle/Invoke-Oracle.ps1 @oracleArgs -Mode Verify -ExpectedDirectory .work/tape-review-negatives/value -RunDirectory .work/tape-review-negative-verify
# The preceding command MUST fail. Baseline must pass:
./tools/upstream/tape-oracle/Invoke-Oracle.ps1 @oracleArgs -Mode Verify -RunDirectory .work/tape-review-verify
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
```

Replace machine paths; the captured metadata contains the actual executable paths and argument
arrays. Check every exit code; do not treat the intentional negative Verify failure as a baseline
failure. All run/negative directories must be new. Capture never updates expectations; promotion
is a separate review action. Verify reads both artifact sets and never rewrites either.

Each invocation sequentially executes upstream `-pl qds-file -am clean compile`, skips upstream
tests, disables Maven build cache, compiles the new Java sources and the unchanged SchemaOracle
environment observer, and launches the real file paths. Do not run two launchers concurrently on
the same upstream checkout. Maven artifacts reuse the existing schema-oracle local cache; clean
reactor classes are rebuilt. No project runtime or build dependency was added.

The launcher rejects a dirty/wrong upstream, wrong origin/tag/POM/release notes and ambient JVM
injection variables. It requires JDK 8. Child logs use TZ/Java timezone UTC; host settings are restored.
The capture process has a 30-second limit (including the truncated-file probe); inspector processes
have 10 seconds. A timeout kills the child tree and fails, never becomes accepted EOF evidence.
Maven build time is not bounded by the probe limit. There are no harness sleeps.

## Artifacts and verification

`artifacts/` contains original writer files, the explicitly derived truncated file, production
reader TSV, diagnostic envelope TSV and exact inspector stdout/stderr/exit. There is no byte
rewriting, stripping or normalization. `raw/tape.stderr` preserves production logs, including the
expected EOF diagnostic. Metadata records commands/times/exits, upstream identity, JDK/Maven
versions, input/tool/harness-class hashes, classpath directory content digests, cached dependency
JAR hashes and inspector executable hash. Classpath digests hash sorted `relative-path<TAB>sha256`
lines joined by LF without a final LF. Dependency inventory includes cached JARs, not a claim that
every cached JAR is loaded at runtime. Source classes, rather than packaged JARs, explain QDS-UNKNOWN.

TapeOracle checks input arity/profile and every delivered local identity, symbol, value, flag and
time against the input, including R field permutation. The fixed profile has seven records across
three input messages; each artifact is at most 64 KiB. It asserts full replay or a partial prefix with one EOF error; the verifier requires exactly four records for truncation. TapeVerify uses only JDK APIs and checks the exact 22-file set, bounded nonempty
files, every TSV row kind/arity/value domain, counts/order/completion, envelope sequence/extent,
truncation derivation, inspector result and exact raw bytes against expectations. Exact comparison
also constrains property contents, values and time sequences beyond structural validation.

The 19 negative cases cover byte/value mutation, empty/missing/extra artifacts, unknown row,
missing completion, inspector stdout/stderr/exit and wrong arity for all nine observation row kinds
(profile, descriptor, property, heartbeat, record, end, complete, error, frame). Mutations are made
only in newly created copies. Both the validator/comparator and full clean Verify reject the
intentional value mutation; the latter preserves failure logs and metadata.

This strict comparator is deliberately for these small fixtures, not a general tape validator.
For example all captured frame lengths fit a one-byte compact header. Do not extrapolate those
restrictions to Java QD. Java's source-level resync, connector and broader format features are not
tested by this harness. C++ remains unchanged.
