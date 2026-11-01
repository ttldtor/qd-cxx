# RAW tape oracle validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Starting repository HEAD: `c1654f2dbb5163917b848d1b84b17dfb9d0999fc`. Executed on 2026-09-22 UTC,
with locked QD `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, Zulu JDK 8 and the pinned upstream
build path. Formatter used the separate OpenJDK 23.0.2 and google-java-format 1.36.1. No errors were
detected in the successful checks below. These are initial runs, not independent review acceptance.

## Executed checks

| Check | Actual result |
| --- | --- |
| Java Format then Check | PASS, 15 discovered sources; final inventory source included |
| Clean capture A (.work/raw-clean-a2) | PASS, old tape baseline Verify plus 94 new cases / 284 structurally checked artifacts |
| Clean capture B (.work/raw-clean-b) | PASS, same clean build and capture; all 284 artifact bytes equal A |
| Test-Negatives (.work/raw-negatives-final) | All ten byte/value/time/order/arity/missing/extra/completion/noncanonical/full-type mutations rejected; unchanged baseline passed |
| Full negative Verify (.work/raw-negative-verify) | Expected exit 1, `artifact mismatch: raw-quote.tsv`, against separate timestamp-mutated expectation copy |
| Full baseline Verify (.work/raw-baseline-verify) | PASS against unchanged repository expectations after negative run |

Actual clean launch arguments are in each linked metadata file in [CAPTURES](CAPTURES.md).
Reproduction commands and explicit paths are in the [README](../../../../tools/upstream/raw-tape-oracle/README.md).
For the full negative run the normal Verify command additionally used
`-ExpectedDirectory .work/raw-negatives-final/time`; all other settings stayed the same. Its error
is the intended verifier rejection, not a Java process crash. The final baseline did not regenerate
or replace expected files. Original expectations were copied from A and independently checked by B.

An initial development launcher attempt collided with the baseline launcher's existing `raw` log
directory; it failed before new capture. The output directory was corrected to `raw-tape`, then the
entire clean capture was rerun as A2. No failed run is reported as a successful capture.

The production path was exercised for both explicit empty and default hs connector options,
real default Quote and focused synthetic records. Runtime descriptor/record/heartbeat/error rows,
raw input bytes and typed strict verification are mandatory evidence; diagnostic envelope scans
alone are insufficient. Java accepts broader options/time/heartbeat variants than the implemented
bounded policy, and may continue after malformed complete messages. These observations are retained,
not converted into implied C++ support.

This report does not assert C++ compiler/sanitizer/CLI results, independent review acceptance,
private-file completeness, general tape support, scheduling, nonzero flags or broader types.
Those are separate evidence/remaining-scope questions. The semantic learning is captured in the
EN/RU RAW-TAPE contract and coverage.
