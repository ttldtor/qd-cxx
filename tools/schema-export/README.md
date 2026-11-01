# Offline Java schema export

[Русская версия](README.ru.md). English is authoritative.

This opt-in tool exports a resolved locked Java `DataScheme` to the existing native JSON version 1.
The [export contract](../../spec/schema/JAVA-EXPORT.md) defines exact source, projection, budget and
publication rules. Java 8, Maven and Git are developer requirements for preparation/regeneration;
ordinary C++ loading, build and CTest use committed files without Java, Maven, Python or PowerShell.
The CMake launchers themselves require CMake 3.25+ and native Git/JDK executables. PowerShell below
only illustrates argument-file creation; the JSON arrays can be authored with any editor.

## Prepare and export

Use a separate clean checkout at the origin, tag and commit in [upstream.lock](../../upstream.lock).
Supply absolute paths. `RUN` must be a new scratch directory on each invocation. Preparation builds
the locked `qds-file` reactor with explicit Maven/cache paths and produces a classpath receipt;
dependency acquisition can require network access. Export verifies that receipt and the clean source
identity again, compiles only the additive exporter/tests and runs a fresh JVM offline. It never
connects the endpoint. Classpath extensions are unsupported. Nonempty `JAVA_TOOL_OPTIONS`,
`_JAVA_OPTIONS` and `JDK_JAVA_OPTIONS` are rejected; preparation also rejects ambient `MAVEN_OPTS`,
`MAVEN_ARGS` and `MAVEN_CONFIG`. Do not change upstream sources to prepare the tool.

The following paths are illustrative and must refer to installed tools and an existing locked checkout:

```powershell
$repo = (Get-Location).Path.Replace('\', '/')
$jdk8 = 'C:/tools/jdk8'
$upstream = 'C:/work/QD-locked'
$maven = 'C:/tools/maven/bin/mvn.cmd'
New-Item -ItemType Directory -Force "$repo/.work/schema-export" | Out-Null
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DMAVEN=$maven" `
  "-DCACHE=$repo/.work/schema-export/maven-cache" "-DRUN=$repo/.work/schema-export/prepare" `
  -P tools/schema-export/Prepare.cmake
$receipt = "$repo/.work/schema-export/prepare/classpath-receipt.json"
$exportArgs = @('--source', 'endpoint', '--output', "$repo/.work/schema-export/events", `
  '--record', 'Quote', '--record', 'Profile', '--set', 'dxscheme.nanoTime=true')
$exportArgs | ConvertTo-Json | Set-Content -Encoding utf8NoBOM "$repo/.work/schema-export/args.json"
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DARGS=$repo/.work/schema-export/args.json" "-DRUN=$repo/.work/schema-export/export-run" `
  -P tools/schema-export/Export.cmake
```

On Linux use the same CMake parameters with absolute POSIX paths, JDK 8 and the Maven executable.
`ARGS` is a UTF-8 JSON array of strings, at most 65536 bytes/256 arguments. Paths inside it resolve
from `RUN`; use absolute output/input paths. The launcher reserves `--provenance`. Preparation has
a 600-second build timeout, compilation 60 seconds; command execution is bounded to 30 seconds
and `-Xmx256m`. These limits are not an exact resident-memory bound for upstream construction.

The successful bundle contains `schema.json` and deterministic `provenance.json`, including relevant
source settings, input/exporter/classpath hashes and source-slot/exported-ID mapping. The output's
parent must exist and the destination must not exist, even as an empty directory. Files are fully
written in a sibling temporary directory before publication. A rejected input publishes no bundle;
an existing destination is preserved. Run timestamps stay in ignored `.work` logs.

No arguments and standalone `--help` give identical Java command help without opening files. To
exercise those paths through the launcher use `[]` or `["--help"]` in `ARGS`; the operational launcher
still verifies and compiles. Other malformed/duplicate arguments fail. Java errors are one bounded
stderr row `schema-export-error<TAB>1<TAB>CODE`; exit codes are 2 usage, 3 I/O/publication, 4 unsupported,
5 invalid input/configuration/schema and 6 resource limits. CMake launch failures use CMake diagnostics.

## Sources, settings and selection

`--source default` uses `DXFeedScheme` with `--startup KEY=VALUE`; it rejects properties files and
`--set`. `--source endpoint` uses a disconnected FEED endpoint: startup settings, one explicit
`--properties FILE` and `--set KEY=VALUE` retain their distinct sources. For the measured subset,
builder overrides beat the file, which beats startup settings. No implicit file is discovered.
Java properties files use `Properties.load(InputStream)` syntax; duplicate decoded keys fail.

Only `dxscheme.nanoTime`, `dxscheme.wide`, `dxscheme.enabled.Sequence`,
`dxscheme.enabled.TimeNanoPart`, `dxscheme.price`, `dxscheme.size` and `dxscheme.shares` are accepted.
Booleans are exactly `true`/`false`; field selectors match `[A-Za-z*][A-Za-z0-9*]*` with at most
128 characters and only `*` as wildcard; representations are case-insensitive `int/wide/tiny/decimal`.
Accepted settings can produce unsupported full types, which still fail projection. Unknown keys,
connectivity options and settings ineffective for the selected source/options are rejected.

For trusted local XML use an ordered root/overlay list; this runnable fixture example retains
the custom record, aliases and extra field without manually editing JSON:

```json
["--source","xml","--output","C:/qd-cxx/.work/schema-export/custom",
 "--input-root","C:/qd-cxx/tests/fixtures/schema/export/inputs",
 "--xml","root.xml","--xml","overlay.xml"]
```

Alternatively select `--source xml --resource dxlink --record Quote --record Profile`.
The pinned resource and its dependencies precede any local overlays. XML property options
`--xml-sysprops true|false` and `--xml-dxprops true|false` default false. Startup settings require
sysprops; explicit file/overrides require dxprops. With both enabled, explicit XML settings override
startup values. XML rejects `dxscheme.shares`, which that source does not consume.

Preflight snapshots only bounded local imports inside `--input-root` or the explicit pinned resource
closure. Paths use 1–240 ASCII letters/digits, `_`, `.`, space, `/`, `-`; internal `..` is allowed only
within the root. Remote/absolute imports, escapes, cycles, DTD/entities, XInclude and `xml:base` fail.
Only the exact pinned `dxfeed-schema.xsd` declaration is allowed. Hardened preflight denies external
DTD/schema access; upstream JAXP permits schema `file` access only to support the validated snapshot
declaration and copied pinned XSD. This is a trusted-local-input bridge, not a hostile-XML sandbox.
Input defaults/hard maxima are 64 files, 1048576 bytes and import depth 16, counting the properties
file and pinned XSD; overrides may lower them. Output/count/name budgets are listed in the contract.

Without `--record`, **all** records must be supported. Full default/dxlink schemes include unsupported
records; selecting Quote/Profile is explicit scope. Unknown or repeated names fail. Requested order
does not reorder source records; exported IDs are contiguous and recorded in the sidecar. Only exact
ordinary `DefaultRecord`, full types 2/8/10/40/56/72/88/120 and verified physical VOID tails are supported.
All actual retained-field lookup keys survive; omitted-tail keys are accounted for separately.
Indexed schemas, standalone VOID, full24/full137, arbitrary subclasses and partial lookup guesses fail.

## Consume and regenerate

Build with `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON` and
`ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON` as in [CPP-BUILD](../../docs/CPP-BUILD.md).
The resulting `schema.json` is consumed directly:

```powershell
& .work/schema-json-msvc/Release/qdcxx-tape-inspect.exe --schema .work/schema-export/events/schema.json `
  --input tests/fixtures/schema/export/expected/set-before/Quote-stream.qtp
& .work/schema-json-msvc/Release/qdcxx-tape-inspect.exe --schema .work/schema-export/events/schema.json `
  --tape-mode raw-message-v1 --input tests/fixtures/schema/export/expected/set-before/Quote-raw.qtp
```

The inspector keeps generic version-6 output. Applications call `loadJsonSchema` and then
`PreparedQuote::prepare` / `PreparedProfile::prepare` with explicit exported IDs and the same owned
schema context; they do not infer those IDs from Java storage slots. Generic custom/extra fields
remain accessible. The production-capture tests exercise both tape modes and generic/event values;
JSON serialization alone is not the compatibility evidence.

Regenerate into new scratch directories, compare two fresh runs, then compare with committed
expectations. These commands never replace those expectations:

```powershell
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DRUN=$repo/.work/schema-export/capture-one" -P tools/schema-export/tests/Capture.cmake
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DRUN=$repo/.work/schema-export/capture-two" -P tools/schema-export/tests/Capture.cmake
cmake "-DACTUAL=$repo/.work/schema-export/capture-two/expected" `
  "-DEXPECTED=$repo/.work/schema-export/capture-one/expected" -P tools/schema-export/tests/Verify.cmake
cmake "-DACTUAL=$repo/.work/schema-export/capture-one/expected" `
  "-DEXPECTED=$repo/tests/fixtures/schema/export/expected" -P tools/schema-export/tests/Verify.cmake
```

The opt-in Java/command suites use the same verified inputs and new scratch directories:

```powershell
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DRUN=$repo/.work/schema-export/commands" -P tools/schema-export/tests/CommandTests.cmake
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DCLASSPATH_RECEIPT=$receipt" `
  "-DRUN=$repo/.work/schema-export/launcher" -P tools/schema-export/tests/LauncherTests.cmake
```

Capture uses a separate 120-second JVM budget for production writer/reader observations; it does
not change the export command's 30-second budget. Git source verification has a 180-second timeout.
On WSL, use a native Linux checkout/classpath if mounted Windows storage exceeds process budgets.

Source/classpath hashes belong to the actual capture. A changed tool requires fresh verification evidence,
not relabeling an old hash. [Validation](../../tests/conformance/schema-export/VALIDATION.md)
records executed checks, platform/provenance limits and open independent acceptance. The normal
`schema_export_consumption` and `schema_export_cli` CTests consume committed observations only.
Native XML import, general settings, new identities/conversions, live replacement, transport and
private tape remain outside scope; historical fixtures are unchanged.
