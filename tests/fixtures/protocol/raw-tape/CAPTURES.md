# RAW tape captures

[Русская версия](CAPTURES.ru.md). English is authoritative.

The [284 expected artifacts](expected/) were copied unchanged from capture A; clean capture B
then reproduced them byte-for-byte. No timestamp/path/byte normalization or upstream.lock change
was made. Six original producer files and 88 diagnostic wire files retain full headers and bodies.
The production `raw-quote.qtp` and `connector-quote.qtp` each contain 296 bytes; default-options
`connector-default.qtp` contains 298 bytes with hs. The effective schema remains 331 records;
selected Quote has 12 integer slots, no object fields, and 8 logical fields with types 56/2/120.

- [Capture A metadata](evidence/a/raw-metadata.json), [clean build/environment A](evidence/a/metadata.json).
- [Capture B metadata](evidence/b/raw-metadata.json), [clean build/environment B](evidence/b/metadata.json).
- [Exact cross-capture comparison](evidence/compare.txt).
- [Ten strict verifier mutations and unchanged baseline](evidence/negative-results.tsv).
- [Full negative Verify metadata](evidence/negative/raw-metadata.json), [rejection](evidence/negative/raw-verify.stderr).
- [Full baseline Verify metadata](evidence/verify/raw-metadata.json), [result](evidence/verify/raw-result.txt).

Adjacent logs and metadata retain actual UTC times, run paths, source/tool/harness SHA-256 values,
artifact hashes, Java/Maven commands, classpath content, provider observations, dependency hashes
and old-inspector identity. Captured stderr deliberately includes production corruption diagnostics;
these are measured negative outcomes, not a claim that every input is accepted. Earlier captures
and hashes remain historical; they are not relabeled as the final verification.

The first development run used already-built locked classes and established the behavior before
[contract](../../../../spec/protocol/RAW-TAPE.md) and C++ implementation. Clean builds reproduced
the final observations. Independent C++ review is separate. The owner sample is not present in
this evidence; structural inventory and its limitations are documented separately.
