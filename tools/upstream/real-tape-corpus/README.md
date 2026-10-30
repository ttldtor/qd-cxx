# Offline production tape corpus

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Corpus.ps1` builds a dedicated exact Java QD producer, runs the production
`FileWriter`/`FileReader` paths and captures or verifies seven small artifacts. No C++ library,
wire contract or [compatibility baseline](../../../upstream.lock) is changed. The separate
[producer lock](producer.lock.json) selects baseline 3.355 at
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and current 3.356 at
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. The latter was freshly confirmed as the official
repository HEAD on 2026-10-30 UTC; it is an exact selected producer, not a floating latest version.

## Prerequisites and invocation

Working modes are qualified on Windows only and reject other platforms before path/source/file
work; help is portable. PowerShell 7.4+, Git, JDK 8, Maven 3.6+ and an explicit native
`qdcxx-run-utc` executable are required. Build the existing developer runner with the
[bootstrap](../../dev/README.md#first-build); this launcher does not download it.
Upstream's enforcer requires Java
`[1.8,1.9)`; the formatter separately requires JDK 21+. Use a dedicated clean clone at the selected
commit, with origin exactly `https://github.com/devexperts/QD.git`. Never run concurrently with
another build in that clone: the launcher runs `qds-file -am clean compile` and skips upstream
tests. Do not point it at another oracle's active checkout. Source and lock hashes are checked
before and after the build. Text hashes in the lock normalize CRLF to LF; metadata separately
records actual source-byte hashes. No host Git, Java, Maven or timezone settings are changed.

Help needs no working inputs and starts no build or file work:

```powershell
pwsh -NoProfile -File tools/upstream/real-tape-corpus/Invoke-Corpus.ps1 -Help
Get-Help ./tools/upstream/real-tape-corpus/Invoke-Corpus.ps1 -Full
pwsh -NoProfile -File tools/upstream/real-tape-corpus/Test-Help.ps1 -RunDirectory .work/producer-help
pwsh -NoProfile -File tools/upstream/real-tape-corpus/Test-Lifetime.ps1 `
    -Runner $utcRunner -JavaHome $oracleJdk -RunDirectory '.work/producer lifetime'
```

No arguments prints the same launcher help. Define `$producerCheckout`, `$oracleJdk`,
`$mavenHome`, `$utcRunner` and `$producerCache` as your dedicated checkout, JDK 8, Maven
installation, native UTC runner and local Maven cache paths. The cache and a fresh run directory
must be below repository `.work`; the
run/cache must be outside the producer checkout. Run from the repository root:

```powershell
$common = @{
    Producer = 'current'
    UpstreamRoot = $producerCheckout
    JavaHome = $oracleJdk
    MavenHome = $mavenHome
    Runner = $utcRunner
    MavenRepository = $producerCache
}
& ./tools/upstream/real-tape-corpus/Invoke-Corpus.ps1 @common `
    -Mode Capture -RunDirectory .work/producer-current-capture
& ./tools/upstream/real-tape-corpus/Invoke-Corpus.ps1 @common `
    -Mode Verify -RunDirectory .work/producer-current-verify
```

Use `Producer = 'baseline'` with a separate baseline clone for 3.355. Maven is offline by default;
`-AllowDependencyDownloads` explicitly permits dependency acquisition. Fixture generation and
replay never use a network endpoint. Capture retains private candidates and never promotes them;
Verify compares the exact committed producer-specific artifact set and bytes. Both modes repeat
generation and run negative checks. Exit 0 means all requested checks passed; input/build/replay,
comparison, timeout or output failures return nonzero. Interrupted runs remain `INCOMPLETE`.

The launcher calls Maven's own classworlds entry point with the installation's boot JAR and
`bin/m2.conf`, avoiding shell argument reparsing. It logs actual Maven/JDK versions, argument
vectors, UTC bounds, runner executable hash, source and harness hashes, every classpath file,
Maven runtime files, cached dependency JARs and output hashes. Cached dependencies are a build
inventory, not a claim that every JAR is loaded by the corpus process. `raw/` retains timestamped
merged UTF-8 runner stdout, separate runner diagnostics, the native UTC log and normalized merged
payload text. These are decoded text observations, not raw separate Java byte streams. Java help
tests separately redirect child stdout/stderr directly to files, assert exact help/empty stderr
and avoid descendant-held pipes. Those complete logs stay
private. [Portable provenance](../../../tests/fixtures/protocol/real-tape/producer/README.md)
contains no machine paths or raw build logs.

## Cases, bounds and completion

The writer explicitly selects BINARY, compression NONE, EMPTY_SET options and STREAM_DATA.
`compact-none` writes three compact integers (`INT_MIN`, zero, `INT_MAX`) and both cipher/raw
symbols. `quote-none` and `quote-message` use the full default Quote schema and production
`QuoteDelegate.putEvent`; only their NONE/MESSAGE timestamp setting differs. Fixed Quote values,
symbols and heartbeat times replace clocks. There is no schema trimming, wire rewriting or
diagnostic decoder. Each case contains three records; each artifact is limited to 65,536 bytes.

The reader uses the same full local scheme, a literal single file, MAX_SPEED, no cycle, no resync
and automatic format/time/compression detection. Callback cursors are borrowed only during the
callback; observations immediately copy names, symbols, flags and every integer slot with its
full field type. Exact source values and event-time sequences are asserted, alongside one
descriptor, zero parser errors, three deliveries and processed bytes equal to physical size.
Canonical UTF-8 TSV uses LF on every host. Positive writer/read logs must contain no error-level
or known file-parse failure messages. This matters because upstream can log write/read/close
failures and return normally; exit 0 alone does not establish completion. Observation flush/close
is also checked. The finite corpus is not a general validator for arbitrary recordings.

The seventh artifact is explicitly derived from `quote-message.qtp` by removing the final two
bytes. It is a negative input, not a production writer output. Production FileReader must report
an error and deliver only a prefix. Its observation says `rejected`, never `complete`.
Five further checks reject missing/extra/empty files, changed wire bytes and changed observation
bytes. The [Java helper](RealTapeCorpus.java) also provides no-argument/`--help`, `generate`,
`verify`, `negatives` and `help-tests` commands; run it through the launcher for enforced process bounds.

Default probe limits are 15 seconds and 128 MiB heap; configurable ranges are 1–120 seconds and
32–512 MiB. Maven has a separate 600-second default (1–1800) and 512 MiB heap. Each runner stream is
limited to 8 MiB; checks occur at bounded polling intervals and can overshoot by an in-flight
buffer. The existing native runner owns a Windows kill-on-close job before Java execution and
terminates remaining children when the Java root exits. The outer deadline includes stream drain
and payload finalization; a timeout kills the runner, closing that owned job, with a bounded
five-second confirmation wait. A resource stop cannot become completion. `Test-Lifetime.ps1`
reproduces root exit while a child retains both pipes and active-root timeout, checking only its
own recorded parent/child PIDs. Windows run directories with spaces and Unicode are checked;
relative harness/output arguments avoid the observed JDK 8 absolute-Unicode-classpath limitation.
Arbitrary Unicode JDK/Maven/source installation paths are not qualified.
The intentionally small fixture count, artifact size and mutation count bound retained test data.

## Evidence and limits

[Validation](../../../tests/conformance/protocol/real-tape/producer/VALIDATION.md) records executed
checks and source hashes. Source contracts are visible in the selected current
[FileWriterImpl](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileWriterImpl.java),
[FileReader](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReader.java)
and [Connect](https://github.com/devexperts/QD/blob/f71e4cb390d1586f2d4e3a83d9b9067e3326ef19/qd-tools/src/main/java/com/devexperts/qd/tools/Connect.java).
An actual current `Tools Help Connect` invocation was checked separately without connecting.
Source-class builds report `QDS-UNKNOWN`; the exact commit and hashes establish provenance.

Optional live demo recording is **not implemented or run** here. No endpoint, credentials,
subscription availability, network limits or populated demo is claimed. Offline production data
provides the deterministic populated fallback. Compression containers, sidecars, split files,
other event schemas and C++ value/order parity remain separate work. Linux/macOS producer-launcher
execution and POSIX descendant cleanup are not claimed. Portable fixture consumption by native
Linux tests is a separate integration check. Existing shared corpus
inventory and historical captures are untouched.
