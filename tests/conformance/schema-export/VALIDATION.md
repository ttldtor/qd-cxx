# QDCXX-030 author validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

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
