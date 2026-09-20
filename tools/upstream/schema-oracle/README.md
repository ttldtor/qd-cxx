# Java schema oracle (QDCXX-005)

This bounded harness calls the locked upstream `SchemeModel.Loader` and `DXScheme.Loader` directly. It does not implement schema semantics. The 17 cases in [cases.json](../../../tests/fixtures/schema/cases.json) cover only the [coverage manifest](../../../tests/fixtures/schema/COVERAGE.md). No C++, QTP captures or upstream test-suite results are implied.

## Windows prerequisites and commands

Use PowerShell **7.4+**, Git, **JDK 8**, Maven **3.6+**, and the external checkout described in [UPSTREAM.md](../../../docs/UPSTREAM.md). Measured profile: Zulu 8.80.0.17, Java 1.8.0_422-b05, Maven 3.9.16. No installation was needed. Maven downloads the upstream POM's build dependencies into ignored `.work/m2`; it never uses released QD binaries as the oracle.

From the repository root (replace these machine-specific locations as needed):

```powershell
$upstream = "$env:TEMP/qdcxx-003-qd"
$jdk = 'C:/Program Files/Zulu/zulu-8'
$maven = Get-ChildItem "$env:USERPROFILE/.m2/wrapper/dists/apache-maven-3.9.16" -Recurse -Filter mvn.cmd |
    Select-Object -First 1 -ExpandProperty FullName
$runner = './tools/upstream/schema-oracle/Invoke-Oracle.ps1'
& $runner -Mode Capture -Upstream $upstream -JavaHome $jdk -Maven $maven `
    -RunDirectory tools/upstream/schema-oracle/.work/run-a
& $runner -Mode Capture -Upstream $upstream -JavaHome $jdk -Maven $maven `
    -RunDirectory tools/upstream/schema-oracle/.work/run-b
& $runner -Mode Verify -Upstream $upstream -JavaHome $jdk -Maven $maven `
    -RunDirectory tools/upstream/schema-oracle/.work/verify
```

Run directories must not already exist. Each invocation verifies origin, HEAD, peeled release tag, root POM/release-notes version and source cleanliness before and after rebuilding. It executes the native upstream reactor with `-pl dxfeed-scheme -am -Dmaven.build.cache.enabled=false -Dmaven.test.skip=true clean compile`. Outputs remain in upstream-ignored `target/` directories; the external source files are not changed. Concurrent runs sharing one checkout are unsupported because every run cleans and rebuilds it.

The harness is then compiled against those fresh class directories. Every independent case runs in a fresh JVM; model and runtime are loaded independently within that JVM. Runtime construction exceptions are captured separately from model loading. Serialization errors and JVM linkage errors fail the run. Expected rejections are observations, not process failures. No factory-wrapping path or rebuild sequence is exercised.

`Capture` writes only its new run directory. Review the observations against upstream before explicitly copying `normalized/*.tsv` into `tests/fixtures/schema/expected/`. There is no automatic golden-update switch. `Verify` generates the same observations separately and compares the exact expected file set and SHA-256 of every normalized file; mismatch throws/nonzero exit and never rewrites expectations.

## Profile and build accommodations

- The root upstream POM requires Java `[1.8,1.9)`. Initial discovery found JDK 11/17/21/25; a JDK 11 trial with the enforcer skipped failed compiling `mars` (missing dxlib packages). JDK 8 was subsequently found under `Program Files/Zulu` and used with the enforcer enabled. The unsupported JDK 11 trial is not oracle evidence.
- Maven initially attempted the sandbox's `C:/.m2`, then the non-writable user cache. The launcher explicitly sets `MAVEN_OPTS=-Duser.home=<USERPROFILE>` and `maven.repo.local` to its writable ignored cache. This is a build-location accommodation, not a source patch.
- The Maven process runs from the generated run's `build-work/`: JAXB schemagen briefly writes `schema1.xsd` in its process working directory before relocating/removing it. This keeps transient output inside the ignored run directory as well as keeping module outputs in upstream-ignored `target/`.
- No POM patch, compiler semantic workaround, replacement QD jar or added harness dependency is needed. The upstream build's Dgen annotation processor and JAXB generation run normally. Tests are deliberately skipped, and Maven build-cache reuse is disabled.
- Runtime classpath is the harness plus 12 upstream `target/classes` directories in the exact order recorded in metadata, including native service resources. Optional Log4j jars and test-only modules are not on it. This is a bounded direct-loader profile, not a full dxFeed application distribution.
- Explicit loader options disable `sysprops` and `dxprops`; there are no explicit loader properties. JVM locale is en/US, timezone UTC, encoding UTF-8. Ambient JVM injection variables (`JAVA_TOOL_OPTIONS`, `_JAVA_OPTIONS`, `JDK_JAVA_OPTIONS`) are rejected. The `environment.tsv` records JDK/JAXP, relevant system properties, and service resource URLs/entries. Mapping classes/default mapping are captured for each emitted record; the artificial names in these cases produce no mappings.

## Observation format and provenance

`SchemaOracle.java` and all input XML are independently authored for qd-cxx. No upstream source or XML is redistributed. The external upstream source retains its MPL-2.0 notices; generated descriptor observations are factual output, not extracted upstream fixtures. Immutable source references are in [SOURCES.md](../../../spec/schema/SOURCES.md).

TSV rows preserve model generation, record, field, alias, slot and mapping iteration order. Cells escape backslash, tab, CR and LF; `\N` denotes null. `lookup` columns are probe, record lookup, scheme int lookup, scheme object lookup. The format is diagnostic observation, not a proposed normalized C++ schema. Error rows record API stage, exception/cause class and message, and the first upstream stack frame. A `complete` row detects incomplete output.

`raw/` stores unchanged process stdout and stderr. `normalized/` changes **only** CRLF to LF and the exact fixture-input directory URL to `<INPUT>/`. No records, aliases or fields are sorted; no exception text, serial IDs, class names or line numbers are removed. Environment metadata is not normalized or compared as semantic output. In particular, JAXB-generated resource timestamps may change classpath content hashes without changing these descriptor observations.

Every run records exact executable/argument arrays and working directory, upstream identity, tool/input/case-manifest hashes, harness bytecode hashes, ordered classpath with per-directory content fingerprints, all jars present in the dedicated build cache, JVM/JAXP/provider metadata and build logs. A directory fingerprint hashes the UTF-8 LF-joined list of relative path + tab + SHA-256, sorted by file path solely for content identification. Build-cache jar inventory is a superset of dependencies used by the reactor, not a runtime classpath claim.

See [VALIDATION.md](../../../tests/fixtures/schema/VALIDATION.md) for executed capture/verify/mismatch checks and the preserved evidence.

To repeat the negative verification check without touching checked-in expectations:

```powershell
./tools/upstream/schema-oracle/Test-Verify.ps1 -Upstream $upstream -JavaHome $jdk -Maven $maven `
    -TestDirectory tools/upstream/schema-oracle/.work/verify-test
```

It copies expectations into a new temporary directory, changes S01-empty's runtime count from 0 to 999, requires exactly that verify failure, runs baseline verification, and checks both expectation sets remained unchanged. A different failure (including infrastructure failure) fails the self-test.
