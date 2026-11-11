# QDCXX-019 validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed on 2026-09-22 UTC. **Ready for independent review; no independent QDCXX-019 review is
claimed.** No errors were detected in the successful checks below;
the negative Verify and inspector unsupported exits are intentional rejection cases.
[Coverage](COVERAGE.md), [specification/recommendation](../../../../spec/protocol/TAPE-FILES.md),
[reproduction commands](../../../../tools/upstream/tape-oracle/README.md).

## Executed checks and evidence

| Check | Result / evidence |
| --- | --- |
| Two independent clean upstream builds and captures | [A metadata](captures/final-a/metadata.json), [B metadata](captures/final-b/metadata.json); sequential `qds-file -am clean compile`, build cache off, upstream tests skipped. A build, B build. Both production replay runs pass. |
| Exact reproducibility | [Comparison log](evidence/compare.log): all 22 artifacts match byte-for-byte, including original BINARY/TEXT files and inspector results; no normalization. Audit also compares harness-class and classpath-content hashes across all four runs. |
| Strict negative comparator checks | [19 mutations](evidence/negative.log): byte/value mutation, empty/missing/extra files, unknown row, missing completion, inspector stdout/stderr/exit, arity of all nine row kinds rejected; baseline passes. |
| Full negative Verify | [Metadata](captures/negative-verify/metadata.json), [failure](captures/negative-verify/verify.stderr): new clean build/capture against a temporary value-mutated expectation rejects `default.tsv`, Java exit 1 and launcher failure. Expected failure, not an unhandled baseline issue. |
| Full baseline Verify | [Metadata](captures/baseline-verify/metadata.json), [result](captures/baseline-verify/verify.stdout): another clean build/capture, all raw expectations unchanged. |
| Real file reader EOF | Each capture's raw log, e.g. [A](captures/final-a/raw/tape.stderr), records 13 unparsed bytes for the derived truncated file; TSV records four deliveries, one stream error and normal return. The 30-second process deadline did not expire. Complete files return with seven records and zero errors. |
| Existing inspector against unchanged files | Each run records four 10-second-bounded processes, all actual exit 4, with raw stdout/stderr/exit retained. Binary rejects type 1; TEXT bytes are interpreted as type 61. No header stripping or capability extension. [Case links](COVERAGE.md). |
| Inspector build | Initial [sandbox failure](evidence/inspector-build.log) reading the Windows SDK directory; [retry](evidence/inspector-build-retry.log) with access succeeds for MSVC Release `qdcxx_qtp_inspect`. No source changes were needed. |
| Java format | [Format](evidence/java-format.log), then [Check](evidence/java-check.log), google-java-format 1.36.1, JDK 21.0.4.7; 11 Java files pass. Final formatting changes no captured source hash. |
| Historical bytes, provenance, links, diff | Read-only audit, [log](evidence/audit.log), before hashes: 1530 tracked historical fixture/tool/production/test files including upstream.lock and the pre-existing modified cache retain their starting bytes. Current harness/input hashes match every capture. EN/RU reciprocal/local links and non-document evidence targets match; `git diff --check` passes. |

Environment: Windows 11 amd64, PowerShell 7.6.6, Zulu JDK 8u422 (8.80.0.17), Maven 3.9.16.
Java version, Maven version,
[environment observer](captures/final-a/environment.tsv). Harness JVM uses UTF-8, en/US and UTC;
Maven reports the host ru_RU/Cp1251 defaults separately. No locale equivalence outside this profile
is claimed. The source-built class-directory runtime's QDS-UNKNOWN is retained, not normalized or
replaced; commit, tag, version/POM/release notes, clean tree and classpath hashes identify the oracle.

Metadata retains exact command argument arrays, cwd, UTC start/end, exits, process deadlines,
source/input/harness-class/inspector hashes, classpath content and cached dependency JAR inventory.
Run paths, logs and execution timestamps differ and are preserved; only the 22 artifact set is
asserted byte-identical. The two clean capture directories and both Verify directories retain raw
artifacts separately. Compiled classes/build-work are reproducible intermediates, not committed.
The negative expectation copies remain under `.work/tape-019/`; the Java negative command recreates
them in a new directory. Neither production expectations nor historical evidence were rewritten.

## Review handoff and limits

The proposed minimum is explicit uncompressed BINARY/time NONE/EMPTY_SET/STREAM_DATA, with retained
protocol header/footer and bounded additive first-use descriptions. Descriptor policy, immutable
binding lifetimes and EOF policy need a scoped task/ADR and additional Java edge evidence before
implementation. No owner approval for implementation or runtime dependencies is inferred.

C++ sources, build/test sources, inspector behavior and upstream.lock are unchanged. The full C++
matrix was not rerun and is not required by this research task. Upstream tests were inspected as
source leads, not executed. CSV/BLOB/compression/sidecars, connector defaults in execution, nonzero
flags, follow/recovery, huge resources and real dxFeed schemas remain outside measured coverage.
There are no new Python scripts or Python dependencies; the existing `tools/run_utc.py` was used
unchanged for build/format/verification logs. New oracle generation and strict verification are
Java, launched through PowerShell. The existing tracked Python cache modification was left intact;
no evidence was removed as cleanup.

To reproduce the Java comparison from a fresh checkout, follow the
[launcher recipe](../../../../tools/upstream/tape-oracle/README.md). The original local audit and
its workspace-specific inputs are archived together.

The QDCXX-018 status correction in state/task index reflects **prior** independent review:
MSVC Release/GCC ASan+UBSan, ten suites each, format Check and hash/diff checks. Copies of the
original [MSVC](evidence/prior-review-018/msvc-test.log), [ASan/UBSan](evidence/prior-review-018/asan-test.log)
and [format](evidence/prior-review-018/format-check.log) logs retain their historical timestamps.
The task's supplied review record establishes the prior hash/diff checks. Migration review at
`4cb1d8c` reran only MSVC Release ten suites. Other QDCXX-018 checks retain their original scope;
none of these historical review runs is presented as a new QDCXX-019 check.
