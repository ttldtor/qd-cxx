# External JSON schema author validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Date: 2026-09-24 UTC. Scope: QDCXX-029 bounded native JSON schema loading.
Actual clean starting HEAD: `3bc0e5ee37cf1dd68306d8016a5b65f68c2da2e0`.
Implementation and author self-review are complete;
independent review/acceptance remains open.

<a id="binary-json-profile-qualification"></a>

## Binary JSON profile qualification

This separate section specifies qualification of the
[binary profile](../../../spec/schema/NATIVE-BINARY-JSON.md). Historical ordinary-profile results
elsewhere on this page retain their date/scope and do not establish final-source binary acceptance.
The following binary-addition checks were executed on 2026-10-31 UTC. Windows used MSVC 19.42
x64 Release; Linux used GCC 13.3 x86-64 Debug with ASan+UBSan and LSan disabled.

| Executed check | Windows | Linux |
| --- | --- | --- |
| Full module suite, JSON ON, CLIENT/RUNTIME OFF | 69/69 passed | 69/69 passed |
| Full module suite, JSON OFF, CLIENT/RUNTIME OFF | 65/65 passed | 65/65 passed |
| Corrected allocation/resource contract | 19 cases, 1221 assertions passed | 19 cases, 1221 assertions passed |
| Clean production JSON ON, CLIENT/RUNTIME OFF; moved-prefix HISTORY consumer | 1/1 passed | 1/1 passed |
| Clean production JSON/CLIENT/RUNTIME ON; moved-prefix schema, endpoint and controlled-TCP consumers | Each of three consumers passed 1/1 | Each of three consumers passed 1/1 |

Production profiles used BUILD_TESTING=OFF and disabled command-line tools. Earlier test-enabled
module packages remain validation artifacts, with their testing extras outside the clean SDK claim.
The full suites precede the test-allocator alignment correction; production behavior was unchanged,
and the corrected final allocation/failure/backend contract was rerun separately on both compilers.
The ordinary request measurements exclude instrumentation headers and unobserved process allocations.

An official production Java QD control capture and verification against both native outputs passed.
The 344-byte control stream and 4778-byte callback projection remained identical; only CMake source
provenance and its manifest changed. This is unchanged control-session evidence, not a Java HISTORY
producer qualification or a full standard event adapter claim.

Source tests cover nineteen full identities, pair/single/ordinary layouts, malformed members/numbers,
independent provenance, zero/equality/one-short/overflow budget edges, tiny input with huge configured
maxima, default intersection, long-prefix name comparisons, field coverage and allocation cleanup.
Qualification inspects all nine pinned backends; source bounds and enabled-backend sizeof/request
observations remain separate. MSVC 19.42 x64/STL and GCC 13.3 x64/libstdc++ capacity observations are
not portable heap limits. Ordinary allocation instrumentation must preserve default-new alignment;
its headers and unintercepted/aligned/process allocations are outside observed payload requests.

Required native profiles are Windows x64 Release and Ubuntu 24.04 GCC 13.3 Debug ASan+UBSan with
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. LSan is excluded. JSON ON/OFF module checks use
CLIENT/RUNTIME OFF and do not qualify a combined runtime/client profile. Package checks install,
physically relocate and rebuild independent ON and pure OFF consumers. OFF must avoid optional
dependency acquisition/import/source access; changed complete SDK hashes must fail ON configuration.

The [installed consumer](../schema_json_consumer/main.cpp) loads pair/single JSON, overwrites and
destroys input before schema use, then composes existing real `LiveSession` and `HistoryCollector`.
The source session lives during admission and `stage`; it dies before view opening and
`front`/reading/`pop`. Checks retain raw time, NUL/unpaired UTF-16 payload and symbol units, null/empty
payloads, opaque bytes, extra fields, source context and rejection of an independent foreign load.
Staging after source destruction is not demonstrated. Native constructed frames establish neither
new Java HISTORY production captures nor full wire equivalence.

CLI gates cover paired selectors, usage before I/O, OFF before missing-file I/O, help/no-argument
equivalence without input, static rows and failed flush. Ordinary goldens/API/errors and tape modes
remain regression gates. EN/RU links/examples, Doxygen and changed Markdown whitespace require
verification alongside final-source native evidence.

## Implementation and coverage

- U1: [native version-1 contract](../../../spec/schema/NATIVE-JSON.md) and three small examples define
  resolved ordinary schemas, explicit aliases, exact full types, Unicode, budgets and errors separately
  from the unchanged XML-captured profile. External generic output has identity `external-schema-v1`,
  version 6, in both tape modes and both formats.
- U2: optional `qdcxx_schema_json` returns owned `LocalSchema` with `EXTENDED_FIELDS_V1`, including
  empty/all-int schemas. simdjson 4.6.4 is pinned by archive SHA-256 in CMake; only its amalgamation is
  built, with private linking and retained MIT attribution. Public headers contain no parser types.
  [Loader tests](../schema_json_tests.cpp) cover decoded duplicate keys at every object level,
  missing/unknown/wrong-kind members, malformed UTF-8/escapes, exact supplementary/NUL names,
  numeric tokens/ranges, unsupported full IDs/indexed/profile/version requests, aliases/local
  invariants, input/parser lifetime, independent identities and zero/exact/exceeded budgets.
- U3: `--schema` and seven finite `--max-schema-*` options validate/load before tape IO, with bounded
  stream acquisition, no fallback and fixed schema diagnostics. [Integration tests](../schema_json_integration_tests.cpp)
  load mixed custom/reordered Quote/Profile records, prepare IDs 1/2, preserve optional/default/extra
  values and reject a preparation from another context. Unchanged Java-derived STREAM/RAW tapes and
  oracle event expectations check production wire/event behavior; deterministic constructed mixed
  tapes check the designed local layout. Generic typed TSV/human output, null/empty, chunking,
  output bounds/write/flush errors and a growing input stream are covered.
  [CLI tests](../schema_json_cli.cmake) cover argument conflicts/duplicates, help equivalence,
  unreadable/missing/malformed/oversized schemas, error precedence, disabled loading, both diagnostic
  versions, both modes/formats, quoted paths, output limits and empty/empty-field/all-int schemas.
- U4: build/API and CLI instructions, schema indexes, documentation indexes are updated
  in EN/RU. The documented preparation function was extracted, compiled and run on the mixed example.

Core/protocol/event APIs and wire semantics are unchanged. DOM validates syntax; a nonallocating
quote-aware preflight bounds all container nesting, including empty containers. Fixed member arrays
avoid attacker-sized duplicate-key sets. Logical budgets precede proportional owned allocations;
`LocalSchema::create` remains authoritative. Parser-unrepresentable numbers are `MALFORMED_JSON`;
wrong representable kinds/ranges are `INVALID_SCHEMA`, both exit 5. This distinction is specified.

## Executed verification

All current logs and generated mutations/builds remain ignored under `.work/schema-json-029/`.
Native developer tools were bootstrapped on both hosts; subsequent build/test logs used
`qdcxx-run-utc --log PATH -- EXECUTABLE ARGUMENTS`. Logs retain exact argument vectors and UTC bounds.

| Profile/check | Result |
| --- | --- |
| Windows x64, VS 2022 MSVC 19.42.34444, Release, loader ON, both inspectors ON | Build passed; complete configured CTest 43/43 passed |
| WSL Ubuntu 24.04, GCC 13.3.0, Ninja Debug, ASan+UBSan, same options | Build passed; complete configured CTest 43/43 passed |
| Production-only Windows Release, loader ON / OFF, both inspectors ON | Both configure/build runs and CLI scripts passed |
| Production-only Linux GCC Release, loader ON / OFF, both inspectors ON | Both configure/build runs and CLI scripts passed |
| Final expanded `schema_json_cli` after full-suite start | Targeted rerun passed on Windows and Linux |
| Documentation preparation function | Compiled/ran on Windows MSVC 19.51.36257 and Linux GCC 13.3.0; both mappings prepared |
| Changed C++ files | Exact Uncrustify 0.83.0 then clang-format 20.1.8 Format and final Check passed, 10 files |

Full-suite commands were `ctest --test-dir .work/schema-json-029/windows -C Release --output-on-failure`
and `ctest --test-dir .work/schema-json-029/linux --output-on-failure`, following
`cmake --build BUILD --config Release --parallel 6` on Windows and `cmake --build BUILD --parallel 6`
on Linux. Linux Debug used `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie`, linker
`-no-pie`, `ASAN_OPTIONS=detect_leaks=0` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
Focused final reruns added `-R ^schema_json_cli$`. Production-only configurations used
`BUILD_TESTING=OFF`, `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON` or `OFF`, and both
`ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON` and `ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON`.
Their CLI script command was `cmake -DPROGRAM=EXE -DROOT=ROOT -DENABLED=ON_OR_OFF -DRUN=SCRATCH
-P tests/conformance/schema_json_cli.cmake`.

The Windows ON production configure fetched and hash-verified the pinned archive. Other ON runs
used its extracted source via `FETCHCONTENT_SOURCE_DIR_SIMDJSON`; test builds reused the existing
pinned doctest source. Both clean OFF directories have no `_deps` and no simdjson/doctest cache
entries; all production-only caches have no doctest entries. The OFF CLI script verified built-ins,
help and `LOADER_DISABLED` before missing-file access. No normal command added a Python/PowerShell
requirement; host shells only launched native CMake/compiler/CTest/logger processes.

Initial focused runs exposed missing explicit wire aliases in the example and the parser's fixed
minimum capacity for tiny malformed documents; both were corrected before full runs. A new CLI
edge-case test initially used a CMake variable instead of a quoted literal; its corrected reruns
passed. MSVC also required moving one raw-string expression out of a doctest macro. The old CLI
passed its original argument suite and failed the new missing-schema-file expectation (usage 2
instead of IO 3), demonstrating the new route's test discrimination. No broad mutation campaign ran.

## Review, limits and handoff

No errors were detected in the final executed checks. LSan was disabled; no leak-coverage claim is
made. macOS and forced allocator exhaustion were not tested. Linux Release emitted an existing
`wide_decimal.cpp` `-Wstringop-overflow` warning outside changed code; no warning-free claim is made.
No new Java capture, private-tape access, XML/property engine, new type/conversion, transport or
Python migration was performed. There was no scope deviation.

Author self-review included all changed/new files and focused reuse, quality and efficiency checks;
it does not close independent acceptance. EN/RU claims and relative links, changed/untracked Markdown
whitespace, `git diff --check`, the empty staged diff and the complete inventory were checked.
Scoped Git comparisons confirmed historical fixtures, upstream locks/tooling and old schema examples
unchanged. No new evidence archive was created.
