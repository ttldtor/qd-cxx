# Primitive oracle (QDCXX-009)

[Русская версия](README.ru.md). English is authoritative.

Independently authored bounded stimuli call the pinned Java QD methods directly. No codec implementation or upstream source is copied. Reuses the [schema](../schema-oracle/README.md) and [protocol](../protocol-oracle/README.md) capture/provenance conventions and native 12-module source classpath. No released QD binary substitutes for source output.

Run sequentially in PowerShell 7.4+ from the repository root:

```powershell
$upstream = "$env:TEMP/qdcxx-003-qd"
$jdk = 'C:/Program Files/Zulu/zulu-8'
$maven = Get-ChildItem "$env:USERPROFILE/.m2/wrapper/dists/apache-maven-3.9.16" -Recurse -Filter mvn.cmd | Select-Object -First 1 -ExpandProperty FullName
$runner = './tools/upstream/encoding-oracle/Invoke-Oracle.ps1'
& $runner -Mode Capture -Upstream $upstream -JavaHome $jdk -Maven $maven -RunDirectory tools/upstream/encoding-oracle/.work/google-a
& $runner -Mode Capture -Upstream $upstream -JavaHome $jdk -Maven $maven -RunDirectory tools/upstream/encoding-oracle/.work/google-b
# Review, then explicitly promote google-a/normalized/*.tsv to tests/fixtures/encoding/expected.
./tools/upstream/encoding-oracle/Test-Verify.ps1 -Upstream $upstream -JavaHome $jdk -Maven $maven -TestDirectory tools/upstream/encoding-oracle/.work/google-verify
```

Each run requires a new directory and checks locked origin/HEAD/tag/version/source cleanliness before and after a native clean build. Tests/build-cache reuse are disabled; JDK 8 enforcer is enabled. Maven cache and JAXB working-directory accommodations match the earlier harness. Native processes have a 30-second limit (Maven: 600 seconds); oracle JVM heap is 64 MiB. Hand-authored byte inputs are capped at 1024 bytes; actual inputs are smaller. Only small positive string lengths and lengths rejected before allocation are exercised. Accepted extreme lengths are deliberately deferred.

Capture never updates expectations. Verify compares exact file sets and SHA-256 without writing expectations. Test-Verify changes only temporary I-zero writer bytes 00 to 01, requires exactly integers.tsv to fail, then verifies the baseline and both expectation sets' immutability.

Three fresh JVM families are listed in cases.json; each read gets a fresh ByteArrayInput at position zero. No reset/rollback is performed. Writer rows: kind, vector ID, method, decimal input or UTF-16 hexadecimal code units, exact bytes. Reader rows: kind, vector ID, method, input hex, ok/error, decoded decimal/code units or exception class, consumed position, remaining bytes, optional exception message. Empty cells are empty values; `\N` is null. Reader errors are at the named direct method; no internal failure stage is inferred. Prefix IDs append `/prefix-N` for every strict prefix including zero bytes. A complete row and successful exit guard completeness. Only IOException is captured; unexpected runtime/JVM failures abort the run.

Raw stdout/stderr is preserved. Normalization changes only CRLF to LF. Metadata retains exact commands/cwd, JDK/Maven/environment, upstream identity, tool and manifest hashes, compiled harness hashes, ordered classpath fingerprints and cache jar inventory (a superset, not runtime dependencies). Java source hashes identify all programmatic and hand-authored stimuli. Decimal strings retain all 64 bits; strings never pass through text Unicode normalization. See [coverage](../../../tests/fixtures/encoding/COVERAGE.md) and [validation](../../../tests/fixtures/encoding/VALIDATION.md).
