# QDCXX-030 author validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

## Independent acceptance and integration

Current status: accepted and integrated by the owner in master at
`e386808c49f82a837e15dd670c40a26db5a26991` (implementation `1a9ea87`, diagnostic fix `e386808`).
This record summarizes the completed independent review and follow-up; it does not claim new runs.
The historical author sections below retain their original handoff status and evidence.

- Initial review: Windows MSVC Release build and 45/45 CTest; Linux GCC Debug ASan+UBSan,
  two schema-export tests 2/2 with `detect_leaks=0`; fresh JDK 8 command tests and C++ Format Check.
- Follow-up: fresh JDK 8 compilation and full Java command tests passed. Both new endpoint/XML
  regressions produced exit 5, empty stdout, exactly one expected stderr row and no published bundle.
  Upstream logging finding #2 is closed. Reusable exporter code was unchanged.
- The raw-fixture whitespace issue is closed: worktree and range checks passed; `-text` and fixture
  SHA-256 stayed unchanged. Finding #1 remains a nonblocking, unspecified error-precedence question,
  not a confirmed defect; this acceptance does not choose a policy for it.
- Author formatter/capture Verify were not repeated in the follow-up. Full Linux/production-only
  matrices were not repeated by the reviewer; macOS and LeakSanitizer remain unverified.
  No private tape was used, and full schema/tape compatibility is not claimed.

## Historical author handoff

Date: 2026-09-24 UTC. U1–U4 of the assigned plan
are implemented and author-verified. **Independent acceptance remains open.** Starting HEAD was
`2ac1a85ea29c4553dd26db880e4e1fe0d2e03f50`; branch `task/qdcxx-030-java-schema-export`.
No staging, commit, push or merge was performed. The [complete file manifest](changed-files.txt)
includes tracked edits and all new deliverables; temporary work remains under ignored
`.work/schema-export-030/`. All evidence paths below are relative to that scratch directory unless
they are links. Transient logs are not committed and must be regenerated on another machine.

## Implemented boundary and evidence

- U1: [JAVA-EXPORT](../../../spec/schema/JAVA-EXPORT.md) defines exact types, sources, lookup relation,
  logical projection, budgets and publication. `ProjectionCharacterization.java` observes locked
  production objects independently of the exporter: accumulated aliases, scoped keys, wide/VOID/int/
  object slots, source-order ID remapping and explicit rejection boundaries.
- U2: a reusable read-only exporter plus a fresh-JVM command uses actual `DXFeedScheme`, disconnected
  endpoint and `DXScheme.Loader` paths. The complete checked `DefaultRecord.fieldsByName` relation
  supplies lookups; verified physical tails are documented separately. XML input closure is validated
  and snapshotted before upstream construction. Publication writes both files before moving a new
  directory and preserves existing destinations. No production C++ code or native grammar changed.
- U3: [reusable inputs and new expectations](../../fixtures/schema/export/) cover configured
  Quote/Profile, a mixed XML custom record with an extra field, scalar/control/supplementary names
  and an empty scheme. The unchanged `EventMappingOracle` supplies configured production
  `FileWriter`/`FileReader` and event observations. The additive capture supplies custom production
  writer/reader observations. JSON generation, raw field observations and event projections are
  distinct evidence; Java RAW callback fanout is not a C++ delivery contract.
- U4: [portable usage](../../../tools/schema-export/README.md), build/status/task documents and this
  report have synchronized EN/RU versions. Existing C++ JSON loading, generic inspection and
  prepared Quote/Profile APIs consume committed captures without regenerating Java data.

JDK 8 and the locked origin/tag/commit were verified by the launchers. Windows preparation ran the
clean upstream Maven reactor with Zulu 8u422 and Maven 3.9.16; `prepare/classpath-receipt.json` records
the exact 14-module class/resource trees. Every export/capture rehashed those trees. Linux command
tests used Zulu 8u504, a clean native Linux checkout at the same locked commit and identical verified
portable class/resource bytes from that Windows build. The exporter was compiled and executed on
Linux; a separate Linux Maven reactor build was **not** performed. The committed small
[capture provenance](../../fixtures/schema/export/expected/provenance.json) records actual source,
input and classpath hashes. Scoped `.gitattributes` preserves new source/capture bytes across hosts.

## Executed checks

| Check | Executed profile and result | Scratch evidence |
| --- | --- | --- |
| Java projection, exporter, sources and command | Windows JDK 8u422 and Linux JDK 8u504: passed; eight source scenarios, plus Linux symlink containment | `commands-windows-settled/`, `commands-linux-native/` |
| Launcher boundary | Windows: 12 negative boundary cases, no-argument/help stdout and empty stderr equivalence, empty argument preserved as usage error: passed | `launcher-tests-final/` |
| Two fresh production captures | Windows: 19:16:43Z–19:16:49Z and 19:16:52Z–19:16:57Z; exact inventory/byte Verify passed between runs and against new fixtures | `capture-settled-a/`, `capture-settled-b/` |
| Deliberate expectation mutation | Altered `custom/stream.tsv` rejected with `Capture mismatch`; expected nonzero result observed | `mutated-settled/` |
| Complete CTest | Windows MSVC 19.42 Release, JSON and both inspectors ON: **45/45 passed**, 461.94 seconds | `windows-{configure,build,ctest}.log` |
| Complete CTest | WSL Ubuntu 24.04, GCC 13.3 Debug ASan+UBSan, JSON and both inspectors ON: **45/45 passed**, 565.48 seconds | `linux-{configure,build,ctest}.log` |
| Production builds | Windows Release and Linux Release; `BUILD_TESTING=OFF`, JSON ON/OFF, both inspectors ON: all four passed | `windows-prod-{ON,OFF}-{configure,build}.log`, `linux-prod-{ON,OFF}-{configure,build}-final.log` |
| Process/dependency audit | 14 nonempty configure/build/CTest traces: no Java, javac, Maven, Python or PowerShell process; no doctest in production caches; JSON OFF has no simdjson; no production `_deps` acquisition directory | matching `*.exec.tsv`, `trace-audit.txt`, four production `CMakeCache.txt` files |
| Formatting | Java pinned google-java-format 1.36.1 Format then Check with JDK 21.0.4: 24 files passed; native Uncrustify 0.83.0 then clang-format 20.1.8 Format on added C++ test, Check: 84 files passed | formatter output and `cpp-format-final.log` |
| Final inventory | Authored documentation links and whitespace, new-file manifest, unstaged/staged diff checks and unchanged historical/production paths checked | author final audit |

The source/command checks cover exact and one-over projection/output/input limits, malformed and
unsupported records/types/maps, unknown/duplicate selection, properties precedence, inactive/unknown
settings, XML imports/overlays and pinned dxlink, remote/escaping/cyclic inputs, DTD/entities/XInclude,
schema-location restrictions and failed publication. Windows and Linux both exercised existing file,
empty/nonempty directory preservation and staging cleanup. Linux additionally exercised symlink escape
rejection; Windows symlink creation was unavailable to the test account and is not claimed.

The two new CTests exercise production tapes in both existing modes, whole-file and one-byte chunks,
TSV and human inspection, explicit exported event IDs, retained prepared mappings after original
schema/JSON destruction, custom/extra values, empty/scalar-name round trips, and discriminating
full-type/alias-target/value mutations. Existing test helpers supply the independent event comparator.

Ordinary checks used the existing native `qdcxx_no_python_guard --deny-interpreters`, wrapping the
bootstrapped `qdcxx-run-utc --log ... -- <executable> ...`. Executable basenames in all 14 traces were
also audited for Java/javac/Maven. ASan used `detect_leaks=0`; UBSan used
`halt_on_error=1:print_stacktrace=1`; GCC used `-fno-pie`/`-no-pie`. This is not LeakSanitizer coverage.
Production JSON ON used the already pinned local simdjson 4.6.4 source; OFF did not acquire it.
Opt-in Java checks and the optional PowerShell formatter are separate from ordinary C++ commands.

## Decisions, limitations and review status

No scope deviation requiring a changed native contract, upstream lock or new production component
was necessary. The exact JDK 8 local XSD declaration requires upstream JAXP schema access `file`;
DTD/stylesheet access remains denied. Hardened preflight accepts only that declaration and copies
the pinned XSD into the private snapshot. This qualified trusted-local-input boundary is documented;
neither arbitrary hostile XML isolation nor a strict whole-process memory bound is claimed.
XML ignores `dxscheme.shares`, so the command rejects it; selectors accept only the documented `*`
subset. Whole default/dxlink schemas remain unsupported: selection is explicit, never silent filtering.

An initial Windows bootstrap lacked SDK access in the restricted runner; the authorized rerun passed.
The initial WSL classpath on `/mnt/c` exceeded the unchanged 30-second endpoint-command budget;
the complete command suite passed after moving the identical verified upstream classpath to native
Linux storage. A temporary Linux matrix-script variable incorrectly changed one build-directory
argument; only the production phases were corrected and rerun. Full CTest was not repeated without
a code change. Final source simplifications were followed by affected Java checks and fresh captures;
only new capture provenance changed, not production observation bytes.

No errors were detected in the successful checks above. GCC Release emitted an existing
`-Wstringop-overflow` warning in unchanged `src/core/wide_decimal.cpp:68`; it was not resolved or
independently diagnosed in this task. macOS, forced JVM exhaustion/crash during publication and
concurrent hostile filesystem mutation were not tested. No private tape was read. Historical
fixtures, upstream Java tools, `upstream.lock`, production `src/` and `include/` remain unchanged.

The author reviewed the complete diff and new files for correctness, scope, style/braces and
unnecessary complexity; focused simplification feedback was applied and rechecked. This does not
close independent acceptance. `ce-compound` was considered and skipped: the reusable reasoning is
already captured in the authoritative export contract and focused tests, so a second learning would
duplicate them. No additional task was started.

Suggested commit scope: exactly [changed-files.txt](changed-files.txt), after independent acceptance.
Suggested message: `feat(schema): export resolved Java schemes to native JSON`.

## Review correction #2 on 1a9ea87

2026-09-24 UTC; starting tree clean at `1a9ea87`. This correction addresses upstream log leakage,
not finding #1 or configuration-versus-file-I/O precedence. Before endpoint/XML initialization,
the command-only `SchemaSource` resets JUL handlers and disables the root logger. The fixed classpath
uses JUL; locked `com.devexperts.logging.DefaultLogging.configure` leaves a root with no handlers
unchanged. `SchemaExport` retains its own stderr diagnostic; `ResolvedSchemaExporter` is unchanged.
No stream redirection, log buffer, native/C++ contract change or private-tape access was introduced.

Fresh-JVM regressions in `CommandTests.cmake` first failed on both cases: endpoint with an unknown
record produced six stderr lines including a snapshot path; well-formed XML with `name="Private Value"`
passed preflight and reached XSD validation (`cvc-pattern-valid`), producing three stderr lines
including that value and a path. After the fix, both return exit 5, empty stdout, no published
bundle, and exactly one LF-terminated stderr row: `schema-export-error<TAB>1<TAB>INVALID_SCHEMA`
for endpoint and `schema-export-error<TAB>1<TAB>INVALID_INPUT` for XML. Exact byte comparison rejects
any extra value, path, stack trace or line. Scratch runs: `review2-red/` and `review2-green/`.

Windows Zulu 8u422 Java command tests passed; pinned Java Format and Check passed (24 files).
Affected production capture ran at 20:36:05Z–20:36:10Z in `review2-capture/`. Its full file inventory
matched; `Verify.cmake` passed for `custom`, `empty`, `names` and `set-before`, preserving every
schema, projection, source observation and tape byte. The root source-provenance hash changed as
expected; the old fixture was not rewritten. Actual new metadata is retained here:

```json
{"revision":"0ddc0a09779fb61d74cc0b37c6449ff47dcb715f","sourcesSha256":"16766a5986ccabed1c66563fbe1811192841cab50f2b91a0340744e74f3242f1","classpathSha256":"6fefdf02950d4603e178d7d752abbbb94d9577c96e42e1e4925b900f32ab8af2","inputsSha256":"b70a2c19961c42daa2eafb7d2ef1ae1df1462654900f7ba1ba2b5b5222f41cad"}
```

The only attribute change is `whitespace=cr-at-eol` for
`tests/fixtures/schema/export/expected/provenance.json`; its inherited `-text` raw-byte policy stays.
Its before/after SHA-256 is `184994cab64901576827c04f7db803a945d3e6f1ef44fe52d61d5cd00664df0e`.
`git diff 2ac1a85ea29c4553dd26db880e4e1fe0d2e03f50 --check` failed before this attribute and passed
after it. Fixture bytes were not normalized. Final diff, documentation links and whitespace were
reviewed. No errors were detected in these successful checks; C++/Linux suites were not rerun for
this command-only correction. Independent acceptance remains open; no staging or commit occurred.
Additional simplification/independent review was not expanded beyond this requested narrow fix.
`ce-compound` was skipped because the code comment, regression and this evidence capture the lesson.

Exact correction files: `.gitattributes`, `tools/schema-export/SchemaSource.java`,
`tools/schema-export/tests/CommandTests.cmake`, `tests/conformance/schema-export/VALIDATION.md` and
`tests/conformance/schema-export/VALIDATION.ru.md`. Suggested later commands (not executed):

```powershell
$files = @('.gitattributes', 'tools/schema-export/SchemaSource.java',
  'tools/schema-export/tests/CommandTests.cmake', 'tests/conformance/schema-export/VALIDATION.md',
  'tests/conformance/schema-export/VALIDATION.ru.md')
git add -- $files
git diff --cached --check
git commit --only -m "fix(schema): isolate upstream export diagnostics" -- $files
```
