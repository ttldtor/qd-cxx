# Default Quote captures

[Русская версия](CAPTURES.ru.md). English is authoritative.

The [86 expected artifacts](expected/) are copied unchanged from clean capture A after clean
capture B reproduced them byte-for-byte. No normalization or upstream-lock change occurred.
The original production [default-quote.qtp](expected/default-quote.qtp) is 326 bytes. It contains
the complete FileWriter output for five actual Quote objects in the unchanged composite record.
The 40 other QTP files are focused diagnostic inputs replayed by production FileReader.

- [Capture A](evidence/a/quote-metadata.json), [upstream build/environment A](evidence/a/metadata.json).
- [Capture B](evidence/b/quote-metadata.json), [upstream build/environment B](evidence/b/metadata.json).
- [Exact cross-capture comparison](evidence/compare.txt).
- [Negative Verify](evidence/negative/quote-metadata.json), [rejection](evidence/negative-rejection.txt).
- [Baseline Verify](evidence/verify/quote-metadata.json), [result](evidence/verify/quote-result.txt).
- [Nine verifier mutations](evidence/negatives/results.json), [unchanged baseline](evidence/negatives/baseline.log).

Each metadata file retains actual timestamps, source/tool/harness hashes and artifact hashes.
The linked baseline metadata records locked upstream source, actual Java/Maven commands,
classpath content/service observations, dependency JAR hashes and the old inspector hash.
Captured logs are adjacent. The raw stdout/stderr contain run-specific paths and UTC times;
they are provenance, not normalized goldens. Source and previous evidence hashes were not rewritten.

The first development observation used the already built locked classes and preceded the contract
and C++ implementation; clean A/B subsequently rebuilt the upstream reactor from scratch and
reproduced its Quote artifacts. The final verifier additionally requires exact equality of all
raw and mapped rows before/after production file serialization. The final Verify records its
actual current tool hashes; earlier captures retain their original metadata.

The schema inventory is a runtime dump, not a substitute scheme. Quote has 8 logical fields in
12 local integer slots; `VOID` tails are captured and never emitted as additional logical values.
`inputs.tsv` records the lossy input cases separately from Java's observed output. All names,
symbol UTF-16 units, numeric types/values, IEEE bits and scenario identifiers remain untranslated.
See [coverage](COVERAGE.md), [validation](VALIDATION.md) and
[reproduction commands](../../../../tools/upstream/quote-oracle/README.md).
