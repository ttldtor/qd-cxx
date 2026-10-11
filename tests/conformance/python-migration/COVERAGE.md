# Python-free checking coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

## Frozen contract (U1)

Starting HEAD: `dcccd0242252cadb674f4970cc723fbf113bed53`. The working tree was clean.
Baseline metadata, all tracked-file SHA-256,
CTest inventory, and the named scenario map
were captured before replacement. The map enumerates all 72 offline and 84 human cases, their
actual deduplicated chunk sizes, exits, arguments and intended replacement owners. Counters alone
are not coverage: the historical scripts omit some process invocations from their reported counters.
Historical scripts, manifests, captures and tracked bytecode remain immutable.

## Replacement ownership

| Existing branch | Replacement and acceptance |
| --- | --- |
| Offline designed cases | `offline_cli.cmake` reads unchanged JSON; `inspection_cli_tests.cpp` checks expected and observed TSV, exact bytes, stderr and exit at every mapped chunk size. |
| Java composed tape | Same adapter runs sizes 1 through wire length + 1, then 4096; strict independent observation parsing, exact wire bytes, record cells and chunk-invariant output. |
| TSV rules | ASCII, no BOM/CR, terminal LF; exact header; no empty cells; known row kinds/arity; unsigned canonical uint64 and signed int32 grammar; lowercase UTF-16 groups; contiguous frame indices/offsets; payload bounds; records only after type 15; global record order; unique final completion, counters and success agreement. |
| Diagnostic rules | Empty on success; otherwise one ASCII LF row, 11 cells, exact exit, known stage grammar, canonical uint64 or dash, payload-relative/absolute offset equality. |
| Offline process branches | All 19 malformed argument vectors, standalone/no-argument help, preclosed help/data pipes, missing and directory inputs; 30-second deadline includes drain and termination. |
| Human designed cases | `human_cli.cmake` reads unchanged JSON; exact bytes/exit/diagnostic, strict UTF-8, no BOM, controls/C1/bidi/line-separator restrictions, terminal LF, exactly one final completion on success. |
| Human output budgets | Every successful no-extra-argument case: exact total byte limit and both limits at every output-row boundary, exact completed prefix and write diagnostic. |
| Legacy TSV | All 72 historical cases, default and explicit TSV at sizes 1, 7, 4096; composed tape equivalence, seven invalid format vectors and help content. |
| Human process branches | Real executable with closed stdout; existing injected-I/O suites remain registered. |
| Harness integrity | Missing executables/timeouts are harness failures; raw stdout/stderr/exit mutations must reject independently; temporary inputs and original fixture hashes stay unchanged. |
| Argument transport | The real inspector opens an ASCII path containing a space and semicolon. The same CMake launcher passes empty, space, Cyrillic, semicolon, quote and terminal-backslash arguments to the native-argv test helper (`wmain` on Windows). This tests forwarding, not production support for paths outside the Windows active code page. |

## Named mutations and rejection stages

`inspection_cli_negatives.cmake` owns temporary inputs and requires the intended rejection stage.
No unrelated process error counts as a successful negative.

- Offline exact comparison: `value`, `symbol`, `local-id`, `name`, `complete-count`.
- Offline expected TSV validation: `offset`, `row-type`, `row-arity`, `number-syntax`, `hex-syntax`,
  `missing-complete`, `double-complete`, `trailing-row`.
- Manifest admission: `empty-selection`.
- Java semantic comparison: `java-value`; Java observation structure: `java-unknown`, `java-completion`.
- Human exact comparison: `label`, `value`, `escape`, `escape-lookalike`, `completion-count`,
  `missing-completion`, `double-completion`, `trailing-output`, `frame-offset`.

Restored baselines must pass after every mutation family. This is the pre-cutover coverage contract;
executed results and limitations belong in [validation](VALIDATION.md), not in inferred counters.

## Developer tools and Python-denial ownership

- `Test-RunUtc.ps1`: success/exit 37/missing executable, empty/space/Unicode/semicolon/quote/backslash
  arguments, child-only TZ with same-process parent check, CR/LF/CRLF/invalid UTF-8/BOM/final fragments,
  5000 lines per stream, unwritable log, real closed console and bounded cleanup with a running or
  already-exited root process. Linux also tests `/dev/full`.
- `Test-SetupCpp.ps1`: 28 named checks including real locked cold/warm archives, damaged EXE repair,
  bad cached/download hash, interrupted download, missing executable, path/separator/drive/UNC/ADS/device
  attacks, ZIP links/reparse entries, duplicate/extraction conflict, filesystem junctions, preservation
  of existing installations, wrong versions and Format/Check/read-only mismatch/idempotence.
- `inspection_process_contract`: four real preclosed stdout cases (no arguments, help, TSV, human),
  strict exit/diagnostic checks, missing executable and a timeout child that writes 256 KiB stderr.
  Stderr is captured to a raw file; there is no blocking pipe read outside the deadline.
- `Verify-NoPython.ps1` and `qdcxx_no_python_guard`: fresh caches, disabled CMake Python discovery,
  preserved applicable CTest names, zero disabled tests, all option combinations and named compiler profiles.
  Windows debugger creation stops and Linux ptrace exec stops inspect resolved executable images before
  their code runs, including descendants and absolute paths. Deliberate aliases/absolute/descendant
  controls must return 97 and record DENIED. This is a scoped workflow check, not a general security sandbox
  for renamed or embedded runtimes. Production-only builds neither discover nor fetch doctest.
  The Windows `guard-orphan` control also requires prompt cleanup of a live descendant after its root exits.
- The historical trace audit required the exact existing matrix inventory: 49 Windows and 42 Linux traces,
  including every configure/build, applicable inventory/test phase and denial/developer-tool control.
  It rejects empty or malformed records, invalid UTF-8, relative images, wrong root images,
  executed Python and missing/incorrect denials. The historical negative audit mutated isolated copies;
  review-fix verification records results and the boundary of this retained-evidence audit.
- `Compare-Legacy.ps1` is explicitly parity-only: it invokes the unchanged historical Python checker
  on the replacement's exact temporary mutations, records all 17 rejection stages, and checks that
  the 9 replacement human mutations equal the historical expressions byte for byte. It is excluded
  from the migrated path by purpose; it does not replace the unchanged historical human baseline run.
