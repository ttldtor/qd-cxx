# QDCXX-009 validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed 2026-09-21, Windows / PowerShell 7.6.6, Zulu JDK 1.8.0_422-b05, Maven 3.9.16, locked QD 3.355 commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Four final clean builds ran sequentially on the shared external checkout. Upstream tests and build-cache reuse were disabled; the JDK enforcer remained enabled.

## Checks

| Check | Result |
| --- | --- |
| Origin, HEAD, peeled tag, POM/release version and source cleanliness | Passed before/after every final run. |
| Independent capture A/B | All three files identical, including raw stdout before CRLF normalization. |
| Executed observations per run | 109 writer rows; 1037 reader rows, including 858 strict prefixes. All three families complete; oracle stderr empty. |
| Negative Verify | Temporary I-zero writer byte 00 → 01; exactly integers.tsv rejected. Infrastructure failure would fail the self-test. |
| Baseline Verify | All three real expectation files pass. |
| Immutability | Real and deliberately modified temporary expectation sets unchanged by verification. Existing schema/protocol goldens unchanged. |
| Evidence audit | `python tests/conformance/check_encoding_evidence.py` passes hashes, source/manifest provenance, transition IDs, positions, canonical round trips and strict-prefix completeness. It implements no codec. |
| Documentation | EN/RU results/coverage/contract synchronized; relative file links checked. |

## Reproduction and evidence

Use [launcher commands](../../../tools/upstream/encoding-oracle/README.md). Final directories: `.work/google-a`, `.work/google-b`, `.work/google-verify/mismatch`, `.work/google-verify/baseline`. Earlier `.work/dev` and `.work/final-a` were development trials and are not promoted evidence. Exact native executable/argument arrays and working directories are preserved in each metadata file. No expected bytes or decoded results were calculated by a harness codec or handwritten into the golden files. Promotion explicitly copied reviewed capture A.

- [Capture A metadata](captures/capture-a/metadata.json), [capture B metadata](captures/capture-b/metadata.json): unchanged raw stdout/stderr, build/compiler/version/environment logs and provenance. Generated classes and external upstream source are not checked in.
- [Negative Verify](captures/mismatch/metadata.json), [baseline Verify](captures/baseline/metadata.json): provenance, build/environment logs and exact result messages. Duplicate raw/normalized streams remain in ignored work directories; their hashes are retained below.
- [verify-proof.json](captures/verify-proof.json): precise mutation, required failure and before/after expectation fingerprints.
- [validation.json](captures/validation.json): all four runs' raw/normalized hashes, raw reproduction, classpath comparison, earlier schema/protocol golden hashes and unchanged status.
- [Expected files](expected/) preserve all reader positions, remaining byte counts, exception types/messages and raw UTF-16 values; [coverage](COVERAGE.md) maps the IDs and exclusions.

Normalization changes only CRLF to LF. Git attributes preserve exact captured/tool/input bytes for provenance; expected TSV uses LF. Java source hashes identify programmatic/hand-authored inputs; cases.json identifies the three JVM families. Tool hashes, compiled harness hashes and ordered source-built classpath fingerprints identify the implementation used. SchemaOracle's environment inspector is reused and hashed. Cache jar inventory is a build-dependency superset, not the runtime classpath. Environment metadata is not a semantic golden. Each runtime JVM explicitly selects en/US, UTC, UTF-8 and 64 MiB heap; individual native JVM invocations have a 30-second timeout, Maven 600 seconds. Maven retains its own recorded environment.

## Findings and limits

Source review and runtime outcomes agree within [PRIMITIVES](../../../spec/encoding/PRIMITIVES.md): shortest writer forms, additional reader forms, low-32-bit int truncation, isolated surrogate preservation, non-atomic partial reads, malformed UTF and safe length rejection. Consumed positions are measurements, not a rollback assumption. The direct ByteArrayInput profile does not replace QTP's complete-frame buffering contract.

No huge accepted allocation, other UTF/character codec, JSON Unicode policy, PentaCodec, full QTP/parser error policy, upstream test suite, C++ implementation/conformance or importer is validated. The harness limits are not proposed protocol limits. No upstream source semantics or previous oracle expectations were changed.

## Earlier manual formatting follow-up

Java formatting expands control-flow blocks, separates statements and methods, and wraps calls/arrays within 120 columns. Token comparison against the prior source found only whitespace and block-brace changes; inputs and observation order are unchanged. Two captures and negative/positive Verify were rerun to refresh actual source/bytecode provenance. All three expectation hashes remain identical to the pre-formatting baseline; expectations were not regenerated or overwritten. The earlier .work/capture-a, .work/capture-b and .work/final-verify directories validated the pre-formatting source; retained evidence now comes from the formatted source.

## Google Java Style adoption

The current source and evidence supersede the manual layout above: google-java-format 1.36.1 now applies Google Style (2-space blocks, 100 columns with guide exceptions). SchemaOracle and EncodingOracle use package qdcxx.oracle; launcher entry points were updated. All three harnesses were migrated, and encoding was captured twice and verified negatively/positively again with unchanged expectations. Exact migration validation, schema/protocol Verify metadata and persistent formatting rules are in [formatting validation](../../../tools/format/VALIDATION.md) and [instructions](../../../tools/format/README.md). Earlier captures identified earlier source hashes; current retained encoding metadata identifies the Google-formatted sources.
