# Portable developer-tool validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

## Scope and status

QDCXX-028 implements portable native development tooling; the recorded checks follow below.
Actual starting HEAD: `9b4f8ddf5786f71ac5f6d2305a0215a84b86b4b4`, initially clean worktree and empty staged diff;
Baseline retains original file hashes.
This is verification and self-review. Independent review/acceptance remains open.

Final source rechecks and targeted correction checks are complete as detailed below; final handoff
preservation receipts are linked in the audit section. Earlier failures remain separate from corrected reruns.

## Implementation decisions

- Standalone `tools/dev/Bootstrap.cmake` builds native tools independently of root production targets,
  doctest and a prebuilt logger. Its first-build exception records UTC phase bounds and raw stream sidecars.
  Subsequent measured phases use `qdcxx-run-utc`.
- The logger owns processes before execution, passes native argv without a shell, overrides only child
  `TZ`, and separately decodes/drains both streams. A private observer keeps root exit independent of
  stream EOF and destination writes. One five-second cleanup deadline remains active through EXIT and
  log closure; cancellation begins after four seconds, reserving one second for cleanup confirmation.
  Native log writes avoid retrying buffered output from a destructor. Failure diagnostics use a private
  raw native writer with a 100 ms best-effort allowance, followed by immediate process exit with the
  authoritative status; a full stderr cannot restart an unbounded wait after watchdog shutdown.
  Linux cleanup inventories the owned session through `/proc` and uses pidfds for race-safe signalling
  and waiting while retaining the root PID as a stable session identity. These helpers remain private to tooling.
- The CMake formatter uses the unchanged exact 0.83.0 → 20.1.8 pipeline and configurations. Each input
  gets a separate temporary directory with its original basename: clang-format uses that name when
  prioritizing the corresponding header. JSON selection and Git filename decoding preserve awkward names.
- The existing image guard gains opt-in `--deny-interpreters`; its original interface stays Python-only.
  New exact inventories are derived from this matrix. Production-only builds do not enter `tools/dev`
  or acquire doctest; no production libraries link these tools.

## Characterization and regression evidence

[Coverage](COVERAGE.md) and [coverage.json](coverage.json) retain 157 initial obligations plus four R5
regressions and one formatter regression, 162 total. Validation rejects deletion of each named entry, including all 17 offline and
9 human mutation names. The old Windows and
Linux logger checks and all
28 optional Windows setup/formatter checks ran before
replacement. These intentional historical PowerShell characterizations are outside the new denied tree.

Seven findings were reproduced and corrected during source verification:

1. An open, unread console pipe could hide root exit: red.
   The observer remains active even while final EXIT is emitted.
2. A POSIX FIFO log could still block an `ofstream` flush: red.
   The log now uses owned native nonblocking POSIX writes; Windows uses cancellable native writes.
   The focused source-stage Windows and
   Linux suites rerun baseline, all three actual logger mutations
   with their required diagnostic, and restored baseline. Linux additionally exercises `/dev/full` and
   a real FIFO; Windows verifies actual ACP 1252 with native Unicode argv and paths.
3. Numbered temporary basenames changed corresponding-header sorting. The unchanged old formatter
   passed the original inspector, while a reconstructed
   faulty driver fails the new header-order assertion.
   Corrected Windows and Linux
   formatter suites pass real-tool checks, five actual driver mutations and restored baseline.
4. A full stderr could block failure reporting after exception handling had stopped the watchdog:
   Windows red, Linux red.
   A private worker now attempts raw native diagnostic output for at most 100 ms; immediate exit 1 or 2
   preserves the established failure/usage result without destructor or stream-flush retries.
   Focused follow-up Windows and
   Linux suites end in EXIT 0; subsequent full-profile results are below.
5. A Linux descendant using `setpgid` stayed in the owned session but survived an EXIT 0:
   red. Cleanup now scans `/proc` for that session and uses pidfd signal/wait
   operations with a stable root PID, covering descendants that move to another process group in the
   same session. Missing-capability rejection returns 1
   before executing the command; the focused Linux suite ends in
   EXIT 0. This focused check is complemented by the full-profile results below.
6. The terminal audits of otherwise passing Windows/Linux matrices failed on roughly 1 MB traces:
   Windows regex stack overflow and Linux segmentation fault. After removing the repeated regex,
   validation exposed CMake list splitting that lost semicolons in image paths. The auditor now checks
   UTF-8 scalars individually and reversibly encodes list metacharacters when walking trace records.
   Windows and Linux regressions
   pass large UTF-8/special-path, invalid-data and restored-baseline controls. Earlier failing logs remain.
   A further small trace exposed incorrect Windows-style splitting of a literal POSIX backslash;
   basename parsing now follows the image path syntax. Final guarded Windows
   and Linux checks include that case and literal percent/list metacharacters.
7. A caller-provided `-DtempRoot` cache value was visible to early formatter failure cleanup, allowing
   deletion of a caller-owned directory: Windows red,
   Linux red. The driver now shadows that cache value with empty
   private state and cleans only the temporary directory it creates. Corrected guarded formatter suites
   on Windows and Linux
   pass all five driver mutations and restored baselines. Final global Check passes all 77 C++ files on
   Windows and Linux.
   Coverage rejects all 162 missing-entry mutations on Windows
   and Linux; the corresponding three-trace audits and their
   negative controls pass on Windows and
   Linux. This final correction changes CMake tooling only;
   the C++ sources used by all seven compiler profiles remain unchanged.

The first Windows final-source recheck passed 39/40 suites: the new large-trace audit exceeded its
30-second child-process timeout under the Windows debugger. The separate guarded reproduction
failed the same way. That audit-process bound is now 180 seconds; its data, rejection requirements and
the logger's five-second cleanup contract are unchanged. The guarded retry passed, and full Windows
rechecks were restarted in `windows-recheck-b`. The earlier Linux profiles passed the stricter 30-second
bound; later profiles and the final trace audits use the revised harness. Logger C++ sources remain at
the same freeze.

Earlier source-stage logs are retained as such. They include a sandbox-denied initial Windows SDK lookup,
an initial optional-download failure, an incorrect doctest cache path, and command-quoting mistakes.
The latter split unquoted PowerShell `-D` values; generated test/build artifacts were identified and
removed or isolated under `.work`, including cleanup of three stray root files, and corrected runs were
recorded separately. They are not final passes.
Historical evidence, fixtures, formatter/dependency locks and Java sources were not rewritten.

The first guarded Windows Debug matrix passed 39/40 suites; the coverage deletion loop exceeded its
180-second outer harness timeout (retained failure).
Whole-suite bounds are now 600 seconds (180 seconds per logger-suite subprocess); the logger's R5
deadline assertions are unchanged. Coverage checks all group counts before walking identities, so
missing-entry mutations fail early while intact inventories still validate every identity/owner.
Guarded Windows and
Linux source-stage reruns reject all then-current 159 deletions.
Two later R5 additions brought the inventory to 161, followed by the formatter regression for 162;
the older evidence is not relabelled.
The Windows matrix was
restarted in a fresh `windows-final-b` directory; no failed evidence was overwritten.
That Windows CTest used four parallel workers; the Linux matrix was sequential.
Earlier passing Linux profiles and Windows Debug used the prior, equivalent coverage traversal.
The focused reruns and subsequent profiles validate the final traversal; exact commands/timeouts remain
in each profile's evidence. Matrix-stage source hashes preserve
that earlier snapshot. Pre-formatter-fix hashes
preserve the next snapshot. Final source hashes, recorded at
`2026-09-24T14:01:37.7650527Z`, identify the final native and acceptance sources after the formatter fix.
The earlier matrices are not evidence for every later source correction.

## Required compiler and option matrices

The Windows `windows-final-b` matrix passed 282/282 CTest executions, and Linux `linux-final-a` passed
242/242, before the terminal audit failures described above. These totals count repeated suite executions
across profiles/options, not unique tests or assertions. All matrix phases use
`GUARD --deny-interpreters trace LOGGER --log log -- command`.
No CTest name/label filters or reduced fixture selections are used. The old 36 full-profile suites remain,
with four new tooling suites, 40 total. Exact applicable inventories are checked before each CTest run.
[case_audit.cmake](case_audit.cmake) checks all 156 frozen human/TSV chunk schedules, 72 legacy TSV
parity rows, the old 17+9 mutations and the previous exit-only/Unicode harness controls in full QTP runs.

| Completed earlier matrix | Full profiles | Four `BUILD_TESTING=ON` option combinations | Four `BUILD_TESTING=OFF` combinations |
| --- | --- | --- | --- |
| Windows | MSVC Debug/Release and ClangCL Debug/Release: 4 × 40 | 21 + 32 + 29 + 40 | Configure/build passed; unaffected by later tooling corrections |
| Linux | GCC Debug/Release and GCC ASan+UBSan: 3 × 40 | 21 + 32 + 29 + 40 | Configure/build passed; unaffected by later tooling corrections |

[recheck.cmake](recheck.cmake) rebuilt and reran complete CTest inventories for all seven compiler profiles
and all four testing-enabled option combinations on each OS, with guarded standalone bootstrap and real
formatting. The Linux recheck passed 242/242 suite executions and its
23-trace audit, including every missing-trace negative.
The Windows recheck passed all four 40-suite compiler profiles and the
21/32/29-suite option combinations. Its last option run retained an actual 39/40 failure from the new
caller-temp control while the formatter was still unfixed: 281/282 raw suite executions passed.
The corrected full 40-suite option rerun passed 40/40;
inner-case audit confirms all 156 schedules, 72 legacy
cases and 17+9 mutations. The Windows 26-trace audit
and the supplement's two-trace audit pass their
exact inventories and all negative controls. This supplement and the targeted final formatter checks
close the observed failure; the earlier 281/282 evidence remains unmodified.

The Windows matrix transcript and
Linux matrix transcript identify each executed phase.
Every phase `.log` records full argv/cwd and UTC COMMAND/EXIT; `.exec.tsv` records process images;
`.tests.json` records exact CTest commands; `.cases.log` preserves full inner test output.
`standalone-bootstrap.log.*` are the raw first-build stream sidecars.

Native tool sources and archive/executable SHA-256 are in native-tools.json.
Linux preparation repeats native archive extraction and records
versions, hashes and runtime linkage; no conda, pip, Python or PowerShell is used. The unchanged Windows
lock remains the Windows acquisition authority. Windows and
Linux record environment versions; each configure log records its compiler.
The tested [instructions](../../../tools/dev/README.md) separate acquisition from execution.

## Denial and evidence audit

Independent guard controls are outside another debugger because tracers cannot nest. Thirty controls
per OS pass in Windows and Linux:
seven aliases via absolute/PATH/swallowing descendants, two real interpreter paths through the same
three routes, a PowerShell-launching fake formatter, allowed and restored baselines.
Each directory's `inventory.json` specifies exact roots and expected denials. Default-policy
`.python-only.tsv` deliberately remains outside the stricter inventory and verifies backward compatibility.
No forbidden interpreter is executed; deliberate attempts are recorded as DENIED.

Windows and Linux audit negatives
remove every required control trace and reject empty, extra, malformed, wrong-root, relative-image,
forbidden-image, missing-LF, extra-tab, CRLF, BOM and invalid-UTF-8/UTF-16 traces, then restore the baseline.
Each final matrix derives its own exact trace inventory and repeats every required-file deletion.
The old Q027 49/42 trace counts are not reused as expectations.

After the auditor fixes, Windows matrix audit passed the exact
42-trace inventory and every missing-file negative; Linux matrix audit
passed the exact 38-trace inventory and corresponding negatives. Both include content mutations and
restored baselines. These checks validate the retained earlier matrix traces; they do not substitute
for the later source rechecks or final handoff audit.

Final handoff preservation/filter receipts are
Windows and Linux.
The historical preservation audit checked the complete
changed inventory, protected baseline hashes, source whitespace and relative links. Raw evidence
uses scoped `-text`; only concrete raw paths with actual whitespace diagnostics receive `-whitespace`.
Clean/smudge roundtrips use `core.autocrlf=true` and an ignored isolated Git object directory, without
writing the owner's index/object database. The audit compares index bytes before/after and requires
an empty staged diff; it does not claim a captured start-of-task index byte hash.

The first Windows handoff attempt rejected an audit-time change in
index bytes despite an empty staged diff. Concurrent index metadata refresh is the inferred cause,
not a proven staging operation. Final audits run after document freeze with `GIT_OPTIONAL_LOCKS=0`;
the retained failed attempt is not a passing receipt.

## Limitations and handoff

- Windows x64 and native Linux x86_64 under Ubuntu WSL are the required environments. Native Linux
  executables are used, not Windows tools through WSL. Other Linux distributions/architectures are untested.
- ASan/UBSan and LSan are separate: ptrace runs set `ASAN_OPTIONS=detect_leaks=0` and
  `LSAN_OPTIONS=detect_leaks=0`; they do not establish LeakSanitizer coverage.
- Linux owned-session cleanup requires Linux 5.4+ with readable `/proc`, available pidfd open/signal/wait
  operations and waitable `SIGCHLD` behavior. Missing capabilities fail before child execution.
  The non-Linux POSIX fallback is not qualified by these Linux results.
- macOS received source inspection only: POSIX code isolates Linux subreaper/pidfd calls, but compiler,
  process-group cleanup/reaping and runtime behavior remain unverified and nonblocking.
- POSIX child signal exits map to `128 + signal`; handled launcher cancellation returns 1. Escaped
  POSIX sessions, forced launcher termination and uninterruptible kernel/device I/O are explicit limits.
  POSIX FIFO logs require an existing reader. Logs are text, not byte-preserving inspector captures.
  The implementation deadline is five seconds; elapsed-time assertions allow 0.5 seconds for scheduling
  (0.6 seconds for the deliberately delayed injected failure). Stalled-output probes confirm actual root
  exit before checking cleanup and verify that owned descendants no longer run.
- CR/LF filenames and literal POSIX backslash source filenames are rejected before formatting;
  the latter are unsupported by the CMake path grammar. Forced termination of CMake is outside handled cleanup.
- Image denial is workflow evidence, not a sandbox against renamed/embedded hostile interpreters.
  Developer-tool Unicode results do not expand inspector Unicode support. Java tooling, remaining manual
  Python audits/generators, production code/settings and tracked bytecode retain their previous scope.

No approved requirement was intentionally reduced.
