# Exact field-type oracle author validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Starting HEAD: `a9299317c1d4e7f3202ed8136916fd3ee94b9f3f`. Executed 2026-09-22 UTC with locked QD
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, Zulu JDK 8 and the pinned upstream build path.
Formatting used OpenJDK 23.0.2 and google-java-format 1.36.1. These are author checks, not
independent review acceptance. No errors were detected in the successful checks below.

| Executed check | Result |
| --- | --- |
| Java Format then Check | PASS, 16 discovered sources |
| Clean capture A (`.work/field-types-clean-a`) | PASS, clean upstream build, unchanged old tape baseline Verify, 82 validated new artifacts |
| Clean capture B (`.work/field-types-clean-b`) | PASS, same clean procedure; all 82 artifacts byte-identical to A |
| Test-Negatives (`.work/field-types-negatives-final`) | All 17 mutations rejected, followed by unchanged baseline PASS |
| Full negative Verify (`.work/field-types-negative-verify`) | Expected exit 1: `artifact mismatch: mixed-stream.tsv` against separate unit-mutated expected copy |
| Full baseline Verify (`.work/field-types-baseline-verify`) | PASS against unchanged repository expectations after the negative run |

Actual commands and provenance are linked in [CAPTURES](CAPTURES.md). Reproduction commands are
in the [README](../../../../tools/upstream/field-types-oracle/README.md). The full negative command
adds `-ExpectedDirectory .work/field-types-negatives-final/unit`; the normal unchanged baseline
follows it and does not regenerate expectations. The negative exit is deliberate comparator
rejection, not a Java crash. The Java process has a 128 MiB heap and a 30-second timeout.

An early development composer run omitted automatic descriptions and correctly produced
unknown-record errors; the harness was corrected to use production automatic descriptions before
all retained clean captures. That failed development run is not reported as acceptance evidence.

All unchanged standard Profile fields, null/empty/content, complete primitive prefix probes,
production skips/defaults/repeats/mismatches and the two existing container modes were measured.
Source observations and direct primitives are distinguished from production file delivery.
Java conversion/recovery behavior does not enlarge the exact-match/terminal C++ contract.

This report makes no claim about C++ compiler/sanitizer/CLI results, independent review,
private-file completeness, additional types, indexed schemas or Profile business-event mapping.
The durable IOUtil/BufferedInputPart distinction is recorded in the EN/RU field contract and
coverage.
