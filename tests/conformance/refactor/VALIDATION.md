# QDCXX-026 author validation and handoff

[Русская версия](VALIDATION.ru.md). English is authoritative.

Starting HEAD: `758303a66d5ed181d7123a2c665d858aba2e8834`, initially clean worktree and index.
Scope and per-helper decisions: audit, QDCXX-026.
This is author verification and self-review; independent acceptance remains pending.

## Proof before extraction

U1 rebuilt the original production/checker sources in MSVC Release with both inspectors enabled.
Baseline test log records all 31 suites and exit 0;
suite audit preserves every name and identifies the two added CLI argument suites.
Initial hashes precede authored-source edits.

Before U2/U3 production changes, new descriptor and schema/event characterizations passed alongside
the existing compact/symbol tests: build,
6-suite run. They pin long versus low-32-bit descriptor reads,
noncanonical/truncated prefixes, primitive-start versus validation-end offsets, malformed-before-budget
ordering, NUL/paired/lone/reversed UTF-16 keys, differing schema/mapping error precedence, literal
int32 min/zero/max decimal representations, absent Profile raw zero and unchanged wide raw bits.
Existing exact/excess/zero budget and context-identity tests were retained.

Existing UTFString/symbol cases already pin full-tail examination, overlong BMP, lone surrogates,
supplementary acceptance versus FD rejection, `01c280`, `01c2`, sentinel preservation and terminal/reset
behavior. The 16-suite U2/U3 run passed before U4 and also ran
the new TSV/UTF-16/canonical-number characterizations on the old checker helpers. The
10-suite U4 run passed after UTFChar reuse. No writer changes required new
capacity permutations; existing atomic-write tests remained active.

The original CLI mains passed the new argument checks before C07:
QTP, tape.
These distinguish empty/signed/whitespace/suffixed/overflow operands, accepted leading zeroes,
zero chunk rejection, help, invalid/duplicate arguments and literal paths with spaces and semicolons.
Both checks are now CTest suites. U5 full run passed all 33 suites.

## Final compiler matrix

Every test configuration enables both inspectors. MSVC 19.42.34444 and ClangCL 19.1.5 use Visual
Studio 17 2022 x64 and Windows SDK 10.0.26100.0. GCC 13.3.0 runs in Ubuntu-24.04 WSL.
CMake is 4.4.2 on Windows and 4.4.3 in WSL. The existing doctest 2.4.12 source caches are reused;
no dependency or tool installation was introduced. Configure/build/test logs retain actual commands
and ISO-8601 UTC line timestamps, using unchanged `tools/run_utc.py`.

| Configuration | Result | Evidence |
| --- | --- | --- |
| MSVC Debug | 33/33; exit 0 | CTest |
| MSVC Release | 33/33; exit 0 | CTest |
| ClangCL Debug | 33/33; exit 0 | CTest |
| ClangCL Release | 33/33; exit 0 | CTest |
| GCC Debug | 33/33; exit 0 | CTest |
| GCC Release | 33/33; exit 0 | CTest |
| GCC Debug ASan+UBSan | 33/33; exit 0 | CTest |

ASan+UBSan uses `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie` and linker `-no-pie`.
Separate directories prevent configuration overlap. `PYTHONDONTWRITEBYTECODE=1` is set explicitly
in Windows and WSL child environments; existing Python checks run unchanged.

All four configure/build combinations completed with exit 0.
The four clean GCC Release `BUILD_TESTING=OFF` combinations cover both inspector options ON/OFF.
Production audit checks no `_deps` directory and no Python/doctest
cache entries; `prod-<QTP>-<TAPE>-configure.log` and corresponding build logs retain the commands.

## Exact output and negative controls

CLI snapshots and final snapshot retain exact
stdout/stderr as base64 and every exit/argument vector for 264 invocations. They cover both tools,
four profiles, legacy/RAW modes, TSV/human output versions, diagnostics 1/2, chunks 1/7/137, zero
budgets, unsupported inputs and invalid arguments. They contain no malformed/exit-5 case;
finding #1 adds the separate comparison below without rewriting these historical snapshots. The
final comparison reports byte-identical results after simplification.
These snapshots supplement independent Java-derived comparisons; they do not certify pre-existing behavior.
Read/write/flush, partial-output and multiple-flush checks remain in the full suites.

The final CTest matrix retains configured-event, field-type and CLI mutation families, including
malformed TSV/UTF-16 and null/empty distinctions. The unchanged additional negative runners were rerun:
compact, UTFString,
framing, descriptions,
binding, symbols,
STREAM_DATA, offline CLI.
Each runner's adjacent log records exit 0 and its restored baseline check; compact also exercises
the pinned formatter's negative control. No old expected file was regenerated.

[Invoke-Mutations.ps1](Invoke-Mutations.ps1) copies authored build inputs into `.work/` and exercises
three independent witnesses: representation selector `0x20 → 0x28` (full type still composes to 40),
the `01c280` consumed expectation `3 → 2`, and present compact zero decimal expectation `128 → 0`.
Mutation log records exit 1 with the intended assertion for each
mutation and exit 0 before and after restoring each copy. Protected originals are never mutated.

## Review, preservation and limitations

The author reviewed the full diff/new helpers, ownership and exception boundaries, ordered budget
charges, named widths, exact outputs and dependency direction. Read-only verification covered reuse, quality and efficiency. Two improvements were applied: remove the
redundant CLI digit scan before unsigned `from_chars`, retaining the empty guard/full-consumption
check; reuse the small help assertion in the new argument suite. No efficiency findings required a
change. The final matrix covers those edits. This does not close independent review.

Pinned Uncrustify 0.83.0 then clang-format 20.1.8
Format/Check passed 67 discovered files.
Seven private/test headers were additionally formatted and checked explicitly.
New private helpers are not public include/API additions. No production target or dependency was added.

Protected audit checks 3611 pre-existing protected files against the
initial hashes: fixtures/captures/historical evidence, public headers, locks and Java/Python/oracle
tooling remain unchanged. The initial inventory also records editable checker sources; their changes
are reported separately. Source hashes identify the resulting authored code.
Document audit checks changed/untracked Markdown whitespace,
reciprocal language links, local targets and the empty index. Raw historical evidence was not normalized.

Initial configuration attempts failed because the sandbox denied Windows SDK discovery, and then
because a guessed cache path did not exist. The corrected, authorized invocation used the existing
cache; all attempts remain logged. The first snapshot script printed asynchronous task return objects
alongside result rows; this was corrected before the authoritative 264-case capture. These failed or
superseded attempts are not counted as passing gates. Compiler/CMake warnings are not suppressed;
GCC retains the existing WideDecimal text-buffer warning. No warning-free claim is made.

No errors were detected in the successful checks listed here. Other architectures, exhaustive inputs,
allocation-failure schedules and complete Java/QTP compatibility were not verified. Java sources and
oracle workflows did not change, so no new Java capture/Verify was required or claimed. The private
tape was not read. UTFString writer reuse and broader oracle-tool sharing were deliberately retained
or deferred, as allowed by the plan. No behavioral bug was fixed under the refactoring label.

## Finding #1: malformed CLI comparison follow-up

Only the snapshot runner, this EN/RU report, the exact manifest and new
`evidence/finding-1/` files changed in this follow-up. The earlier malformed-coverage claim above
was corrected. [Invoke-CliSnapshot.ps1](Invoke-CliSnapshot.ps1) now has `Historical` (the original
264 rows), `Malformed` (160 additional rows), and default `All` (424 rows). Every additional case
must return exit 5; equal unsupported/budget errors cannot satisfy that gate.

The ten existing fixtures cover QTP malformed descriptions, payloads, frame/prefix truncation,
legacy tape malformed headers/data and truncation, and RAW timestamp truncation. The full cross
product is TSV/human and default/1/7/137 chunks; tape additionally uses default/1/2 diagnostics.
QTP has no diagnostics-version option. Counts are QTP 40, tape legacy 72 and tape RAW 48, all
using `oracle-sre-v1`. Coverage lists exact fixture paths and counts.
No fixture was generated or modified.

The preserved `.work/refactor-026-baseline-bin` executables were used directly for the
additional baseline. Their PE link timestamps
18:23:12Z (QTP) and 18:23:09Z (tape), on 2026-09-23, match the original
build log. A fresh historical replay
reproduced all 264 original rows exactly. To check provenance beyond timestamps, `git archive`
exported the recorded starting HEAD `758303a66d5ed181d7123a2c665d858aba2e8834` into the isolated
`.work/refactor-026-finding-1/source`; configure
and build used MSVC Release with both tools and testing OFF.
Binary provenance records SHA-256 of each full EXE and
its `.text` section: saved and rebuilt code sections are identical for both tools; current code
sections differ. The rebuilt malformed snapshot also
matches the saved executables exactly. The fresh build corroborates the saved baseline; it is not
substituted for it. No pre-refactor full-EXE hash was recorded, and full rebuilt EXE hashes differ;
this is a corroborated provenance chain, not a claim of reproducible whole binaries or signed custody.

After the current build, all 160
current results matched saved stdout/stderr bytes
(base64), exit values, tool names and argument vectors exactly; see the
comparison log. All 160 exits are 5.
The default-suite check additionally compares 424 current
rows against the concatenation of the original 264 and the saved 160. Original `cli-before.json`,
`cli-after.json`, fixtures and all earlier evidence remain byte-for-byte unchanged.

Affected MSVC Release CLI/CTest checks passed 9/9;
Format and Check passed 67
files using the pinned formatter pipeline, without changing C++ bytes. The first
CTest attempt passed seven but could not start two existing
Python checks because of sandbox permissions; the authorized retry ran all nine. An initial
fixture selection attempt rejected
`heartbeat-truncated-1.qtp` because it returns unsupported/exit 4; it was replaced by the existing
`timestamp-truncated-1.qtp`. Those failed attempts are retained and not counted as passing checks.

The historical follow-up audit rechecked provenance, exact snapshots,
exit-5 counts, the pre-follow-up hashes, and the original
hash/link/manifest audit with outputs redirected into this new evidence directory. The
audit log records preservation and the empty index. Historical audit
outputs and source-hash records are preserved; fresh source hashes
identify the updated scripts. The script/report diff is finding-1.patch.

Recheck from the repository root with the saved binaries still present (use fresh output names):

```powershell
$runner = 'tests/conformance/refactor/Invoke-CliSnapshot.ps1'
$evidence = 'tests/conformance/refactor/evidence/finding-1'
pwsh -NoProfile -File $runner -Bin .work/refactor-026-baseline-bin -Suite Malformed -Output .work/recheck-before.json
pwsh -NoProfile -File $runner -Bin .work/refactor-026-msvc/Release -Suite Malformed -Output .work/recheck-after.json -Compare .work/recheck-before.json
pwsh -NoProfile -File $runner -Bin .work/refactor-026-baseline-bin -Suite Historical -Output .work/recheck-historical.json -Compare tests/conformance/refactor/evidence/cli-before.json
$env:PYTHONDONTWRITEBYTECODE = '1'
ctest --test-dir .work/refactor-026-msvc -C Release --output-on-failure -R '(inspection_cli|argument_contract)$'
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Format
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
```

No errors were detected in the successful checks above. This follow-up did not rerun the full
seven-configuration matrix; it changes no production code. Independent re-review of finding #1
remains pending. Java/Python, private tape and following tasks were untouched;

## P2: preserve raw evidence in Git

The owner reports that independent re-review closed finding #1. The historical audit enforced
the earlier scope guard; its recorded results were not rewritten. This separate follow-up addresses
Git preservation only; it does not reopen CLI behavior or repeat the build matrix.

At the time of this historical audit, [root attributes](../../../.gitattributes) disabled text conversion only for `.log`, `.json`, `.sha256`,
`.txt` and `.patch` below `tests/conformance/refactor/evidence/`. Clean raw files keep whitespace
checks, with CRLF explicitly accepted. Exactly 37 existing raw paths with captured trailing spaces
or patch context have individual `-whitespace` overrides. There is no directory-wide exception;
ordinary `.ps1`, `.md`, `.cpp` and `.hpp` retained their existing policies, including the PowerShell
script inside the evidence directory. Historical captures were neither trimmed nor rewritten.

The historical raw-evidence audit used disposable repositories and compared raw working-file Git object IDs with staged blobs and SHA-256 with extracted files, separately with `core.autocrlf=true` and `false`. This was a baseline preservation check, not a current executable runtime check.
The recorded result includes attributes for every manifest
path. All 159 paths were added and passed whitespace checking; all 108 raw blobs and extracted
files matched their source bytes in both configurations. Four intentional trailing-space defects
were rejected in each isolated copy: `.ps1`, `.md`, C++, and a clean raw JSON; restored checks passed.

The historical pre-follow-up hash audit preserved every pre-existing file except the explicitly edited `.gitattributes`, manifest and EN/RU reports. The owner index SHA-256 was unchanged before and after; the audit did not stage anything in that index. Its report was written outside the checked manifest.

The manifest includes the attributes, verifier and new evidence. The final
documents and manifest were rechecked after the report was written. No errors were detected in
these checks. Production code, Java/Python, fixtures and all pre-existing raw evidence remain
unchanged. Independent acceptance of this P2
fix remains separate.
