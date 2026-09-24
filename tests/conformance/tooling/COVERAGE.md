# Portable developer-tool coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

## Frozen expectations

The implementation starts at `9b4f8ddf5786f71ac5f6d2305a0215a84b86b4b4`.
Baseline metadata records the initial clean worktree/index and protected
inventory. The canonical plan
defines R1–R10 and U1–U5. The [machine-readable map](coverage.json) retains the initial 157 named obligations and adds the stalled-open-console, stalled-log-fifo, stalled-stderr-diagnostic and same-session-process-group R5 regressions
and the caller-temp-cache-preserved R6 regression with their checking owners (162 total). It is an acceptance inventory, not a claim that each check has run.

[coverage.cmake](coverage.cmake) compares exact identities, groups and uniqueness against
an independent required list and requires a nonempty owner. It rejects removal of every entry, including every existing
17 offline and 9 human mutation labels. A count alone cannot accept substitution of a scenario.
Run `cmake -DRUN=<private-test-directory> -P tests/conformance/tooling/coverage.cmake`.
The validator owns a uniquely named temporary child and removes only that child on normal completion.

## Ownership and retained behavior

| Family | Cases | Contract and owner |
| --- | ---: | --- |
| Bootstrap | 7 | Independent configure/build, success/failure logging, exact native argv, raw streams, bounded probe lifetimes and negative coverage validation: `tools/dev/Bootstrap.cmake`, `process_probe.cpp`, `coverage.cmake`. |
| Logger | 50 | Awkward arguments, actual Windows ACP 1252, child-only TZ and inherited cwd/environment; incremental UTF-8 replacement, retained BOM bytes, universal newlines, final fragments, dual-stream ordering; UTC/LF/log parity; normal and failure exits, destination failures and five-second owned cleanup; CLI/signal behavior and three mutation families: `logger_tests.cpp` and `logger.cmake`. |
| Portable formatter | 30 | Exact real-tool pipeline, read-only Check/idempotence; missing/wrong tools/configs, diagnostics/exits/interruption; per-file publication and temporary cleanup without removing caller-owned directories on early validation or late Check failures; tracked/new developer files and awkward paths; protected scopes/aliases/CR/LF and valid executable symlinks: `format.cmake`. |
| Interpreter denial | 21 | Direct/absolute/descendant Python and PowerShell controls, swallowed failures, restored baseline; exact trace audit negatives; existing tests/inner cases/mutations, eight production combinations, no production doctest, guarded bootstrap/formatter/logger and malicious formatter descendant: `guard.cmake` and `audit.cmake`. |
| Existing offline mutations | 17 | Unchanged `inspection_cli_negatives.cmake`: exact comparison, expected TSV admission, empty selection and Java observation controls. |
| Existing human mutations | 9 | Unchanged `inspection_cli_negatives.cmake`: label/value/escape/completion/trailing-output/frame-offset controls. |
| Optional Windows setup | 28 | Unchanged `python-migration/Test-SetupCpp.ps1`: integrity/download/archive/path/link/publication controls, real locked cold/warm acquisition, wrong versions and real formatter checks. This historical convenience is outside the portable interpreter-denied tree. |

The full existing 36 CTest suite names and inner schedules remain required. The 17+9 mutation labels
are preserved exactly; restored baselines remain mandatory. The historical Q027 49/42 trace counts
do not define this task's evidence inventory. New matrix evidence must derive its own exact inventory.

## Characterization before replacement

Windows logger and
Linux logger characterize the existing PowerShell logger;
the Linux run includes `/dev/full`. Windows formatter retry
records the 28 existing setup/formatter checks. These runs intentionally use the old PowerShell path
outside the new interpreter-denied execution tree; they make no interpreter-independence claim.
The first formatter attempt's network failure is retained separately rather than rewritten as success.

R5 adds a stricter requirement than historical logger evidence: root exit independently triggers
owned descendant cleanup within five seconds even when inherited streams stay open. Historical
success does not establish that new behavior. The native probe uses no production QD libraries.
A full, unread stderr pipe must not block launcher failure reporting after owned cleanup. Diagnostics use a best-effort native write with a 100-millisecond wait, immediately followed by process exit; the writer owns its message and takes no stdio locks.
The same-session process-group regression verifies that changing a descendant process group without leaving the owned session does not escape cleanup.
Its `hold`/`running` loops stop after 30 seconds, but an output write can block; outer contract tests
own containment, impose shorter deadlines and verify cleanup rather than relying on the probe timeout.

## Initial bootstrap evidence and limits

Successful Windows bootstrap contains UTC phase bounds,
length-delimited commands, configure/build exits and raw stream sidecars. It initially built only
the probe before logger implementation was ready. No root CMake, doctest or prebuilt logger was used.
Initial attempt records the sandbox-denied Windows SDK lookup;
the retry used the installed Visual Studio 2022 toolchain with permitted SDK access.
Intentional missing-source failure records configure exit 1;
build does not start after this failure. Coverage negative checks
record all 157 missing-entry rejections. These are author checks only.

Initial Linux bootstrap configured and built the native probes
and available logger sources with GCC. This initial source-stage run reports `u8path` deprecation
warnings; later profile runs validate the final sources. It is not the final interpreter-denial matrix.

Bootstrap is the explicit first-build logging exception: it records raw stdout/stderr and UTC start/end
instead of line timestamps. Raw sidecars preserve bytes even if CMake's combined text view differs.
Subsequent acceptance uses the built native logger. Final Windows/Linux profile evidence belongs in
the paired validation report. macOS remains unverified; source inspection is not a build/runtime claim.
Independent acceptance remains open.
