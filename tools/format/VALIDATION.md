# Google Java Style adoption validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed 2026-09-21 as the owner's style follow-up to QDCXX-009. The official
[guide](https://google.github.io/styleguide/javaguide.html) and
[formatter release](https://github.com/google/google-java-format/releases/tag/v1.36.1) were consulted.
The release jar was downloaded from its official asset URL; its SHA-256 is pinned in
[the lock](google-java-format.lock.json). No formatter jar or cache is committed.

## Formatting checks

- All three repository Java harnesses pass google-java-format 1.36.1 in default Google mode.
  The wrapper was exercised with Microsoft JDK 21.0.4; QD verification still uses Zulu JDK 8.
- Named packages, explicit imports and control-flow braces were applied along with automatic
  layout. SchemaOracle and EncodingOracle now have qualified names under `qdcxx.oracle`; their
  launchers and environment-inspector invocations were updated. ProtocolOracle retains upstream
  package access. All three compile against the source-built QD classpath with JDK 8.
- A temporary unformatted FormattingProbe.java was detected by Check with exit 1. Its hash and
  the hashes of all three real Java files remained unchanged. The probe was then removed and
  baseline Check passed. JDK 8 was rejected with the documented formatter-JDK diagnostic.
  format-check.json retains the results and Java source hashes.
- Format followed by Check establishes a formatter fixed point. The wrapper checks the pinned
  jar SHA-256 on every invocation. This is a formatting check, not a complete style or correctness
  checker; review of rules beyond formatter coverage remains required.

## Oracle regression and provenance

All six native clean builds ran sequentially on the shared locked checkout. The actual executable
arguments, working directories, upstream identity, tool/input/bytecode/classpath hashes and
environment/build logs are retained in metadata. Upstream source semantics and expected outputs
were not changed.

| Workflow | Result and evidence |
| --- | --- |
| Schema Verify | 17 cases pass; metadata, result. |
| Protocol Verify | 9 cases / 18 files pass; metadata, result. |
| Encoding capture A/B | Independent clean builds reproduce all three expectation files; [A](../../tests/fixtures/encoding/captures/capture-a/metadata.json), [B](../../tests/fixtures/encoding/captures/capture-b/metadata.json). |
| Encoding negative/baseline Verify | Temporary I-zero byte mutation rejected; real baseline passes; both expectation sets unchanged; [proof](../../tests/fixtures/encoding/captures/verify-proof.json). |
| Evidence audit | `python tests/conformance/check_encoding_evidence.py` passes current source provenance, all retained encoding observations and previous schema/protocol golden hashes. |

Schema/protocol runs use `.work/google-style` under their respective harness directories. Their
earlier task captures remain unchanged historical evidence for the earlier source hashes; the new
metadata above identifies the current sources. Encoding runs use `.work/google-a`, `.work/google-b`
and `.work/google-verify`; its current evidence was refreshed from those actual runs. No hashes in
historical metadata were manually substituted. oracle-check.json
retains normalized-output comparisons and the current source hashes.

The README and validation updates are paired EN/RU; relative file links and `git diff --check`
were checked. No C++ code, importer, production dependency, global Git hook or IDE setting was added.
