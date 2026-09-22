# Exact field-type captures

[Русская версия](CAPTURES.ru.md). English is authoritative.

The [82 expected artifacts](expected/) were copied unchanged from clean capture A and reproduced
byte-for-byte by clean capture B. Both runs rebuilt the locked upstream reactor and verified the
unchanged old tape baseline first. No timestamp/path/byte normalization or upstream.lock change
was made. Four original FileWriter files and one BinaryQTPComposer file are retained in full;
35 separately named diagnostic tapes exercise production parsing.

- [Capture A metadata](evidence/a/field-types-metadata.json), [clean build/environment A](evidence/a/metadata.json).
- [Capture B metadata](evidence/b/field-types-metadata.json), [clean build/environment B](evidence/b/metadata.json).
- [Exact cross-capture comparison](evidence/compare.txt).
- [17 strict verifier mutations](evidence/negative-results.json), [unchanged baseline](evidence/negative-baseline.txt).
- [Full negative Verify metadata](evidence/negative/field-types-metadata.json), [rejection](evidence/negative/field-types-verify.stderr).
- [Full baseline Verify metadata](evidence/verify/field-types-metadata.json), [result](evidence/verify/field-types-result.txt).

Adjacent logs retain actual UTC times, run paths, source/tool/harness hashes, artifact hashes,
Java/Maven commands, classpath content, provider observations, dependencies and old-inspector
identity. Production corruption diagnostics in stderr are intentional negative observations.
Development captures used already-built locked classes to establish behavior before the field
contract and C++ work; they are not counted as the clean reproduction runs.

The original standard Profile uses all 16 logical fields; schema.tsv additionally retains ten
physical VOID tails and local/property names. The new diagnostic record keeps independent Java
int/object indices explicit. Three singleton records occupy three data frames in each producer
file; the harness asserts the frame count. RAW callbacks 10/15/20 are retained separately,
with the STREAM callback furnishing the one-wire-record comparison projection.

No private tape was accessed. These captures prove the measured bounded cases; full event
adapters, arbitrary schemas and complete private-file compatibility remain unclaimed.
