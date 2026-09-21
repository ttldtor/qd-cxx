# QDCXX-006 executed validation

[Русская версия](VALIDATION.ru.md). This English document is authoritative.

Executed 2026-09-20 on Windows 11 amd64, PowerShell 7.6.6, Zulu JDK 1.8.0_422-b05, Maven 3.9.16. Oracle: QD 3.355, locked commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. All builds ran sequentially on the external checkout; upstream tests and build-cache reuse were disabled and the JDK enforcer remained enabled.

## Results

| Check | Result |
| --- | --- |
| Origin, commit, peeled tag, POM/release identity and clean source | Passed before/after all four final runs. |
| Capture A and independent clean-build capture B | All nine cases complete; all 18 exact/semantic expectation files identical. Raw case stdout also matches byte-for-byte. |
| P01 fragmentation | Whole, all 30 two-chunk splits and bytewise (32 schedules) preserve ordered records. Exact callbacks/partial-input states retained separately. |
| P03/P04/P05 | Both directional data/subscription maps; reordered/alias-property/absent field observations; four actual connection requested-layout variants complete. |
| Negative Verify | Temporary P04-order semantic receiver slot 0 changed from 22 to 999. Rejected exactly `P04-order.semantic.tsv`, not an infrastructure error. |
| Baseline Verify | All nine cases / 18 files pass with original expectations. |
| Immutability | Both checked-in baseline and temporary changed expectation set retain their pre-verify hashes. |

All case stderr streams are empty. No parser corruption, unexpected connection close or restart occurred; P05 teardown close is explicit and retained. Capture interpretation and source mapping are in [CAPTURES.md](../../../spec/protocol/CAPTURES.md).

## Commands and preserved evidence

Run the exact PowerShell commands in the [driver README](../../../tools/upstream/protocol-oracle/README.md). Final runs use `.work/final-a`, `.work/final-b`, and `Test-Verify.ps1 -TestDirectory .work/final-verify` (mismatch, then baseline). Metadata preserves actual native executable/argument arrays, cwd and paths. Development trials in `.work/dev` and `.work/run-a` are not promoted evidence; the final driver/launcher were captured twice and verified twice.

- [Capture A metadata](captures/final-a/metadata.json) and [capture B metadata](captures/final-b/metadata.json): unchanged raw stdout/stderr, environment/provider inventory, Java/Maven/compiler/build logs, result and provenance. No generated class files or upstream source are checked in.
- [Negative verify metadata](captures/mismatch/metadata.json), [positive verify metadata](captures/baseline/metadata.json): full provenance/environment/build logs and result messages. Duplicate regenerated raw/normalized outputs remain in ignored work directories; their hashes are retained in [validation.json](captures/validation.json).
- [Self-test proof](captures/verify-proof.json): exact mutation/failure and both expectation sets' before/after hash checks.
- [validation.json](captures/validation.json): per-run raw/normalized hashes, baseline comparisons, raw reproduction, classpath comparisons and current tool/input identity checks.
- [expected](expected/): explicit promotion from reviewed final capture A. Each exact `.tsv` retains callback/chunk evidence; each `.semantic.tsv` separately retains ordered record values. P01's extra schedules are checked against its whole-input sequence in Java rather than treating grouping as semantic equality.

Normalization only changes CRLF to LF; semantic projection is separately labeled, not silent deletion of callback evidence. The runtime provider/classpath inventory reuses the schema-oracle inspector, with its content hash recorded. Programmatic inputs live in ProtocolOracle.java and are identified by tool hashes; no XML or released QD binary is substituted. Build-cache jar inventory is a superset of reactor dependencies, not a runtime jar claim. Per-directory classpath hashes identify each actual build even if generated resources vary.

## Review and limits

Reviewed exact descriptors, values and frame offsets against BinaryQTPParser/Composer, BinaryRecordDesc, AbstractQTPParser, PentaCodec, DefaultRecord/AbstractDataField and the connection classes at the lock. In P04, addFieldAlias's property-name update is preserved, not normalized away. P05's nonempty absent-z request produces zero outgoing fields and a delivered R/ABC, distinct from the all-fields zero-request signal. No unexplained discrepancy remains within this scope.

P05 uses the real connection parser/composer and in-memory transport. Only heartbeat scheduling and protocol-descriptor generation are suppressed as documented; clock/EndpointId bytes are not removed after capture. This validates first-use layout selection on fresh connections, not connection timing, negotiation, auth or dynamic cache/reset semantics.

The [coverage manifest](COVERAGE.md) leaves P02, P06–P13 and unlisted portions pending. There is no independent primitive implementation, upstream test-suite result, complete Java wire-compatibility claim, C++ conformance or QDCXX-007 work.

## Later Google Java Style migration

The original captures and their source hashes above remain historical evidence. The current harness and launcher were subsequently migrated to Google Java Style; a fresh JDK 8 clean-build Verify passed against these unchanged expectations. Current-source metadata and checks are in [style migration validation](../../../tools/format/VALIDATION.md). No original capture or expected file was rewritten.

## QDCXX-012: separate P02 baseline

Added 17 P02 subcases / 117 schedules with exact bytes, positions, callbacks and observational hooks.
[Coverage](p02/COVERAGE.md) and [validation/provenance](p02/VALIDATION.md) record two independent captures
and immutable negative/baseline Verify. Historical captures/expectations are unchanged. Incomplete outer
input waits; incomplete type inside a complete body is stream corruption. Unknown type and malformed
payload are distinct from framing, and adapter closure is distinct from returning-consumer policy.
Java invalid-length diagnostics read extra bytes; that position is not C++ accepted. Earlier pending-P02
statements above describe QDCXX-006; the remaining payload/session/transport matrix stays deferred.
