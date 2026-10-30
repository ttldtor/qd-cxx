# Exact field-type captures

[Русская версия](CAPTURES.ru.md). English is authoritative.

The additional [FIELD manifest](field-time-captures.json) records29 immutable artifacts per pin,
115 generic plus5 actual standard indexed source records,469 labelled projection deliveries,
final-source capture/repeat, full638 governed Verify and62 rejected verifier mutations. Existing609
expectations remain unchanged. See the paired
[FIELD validation](../../../conformance/protocol/field-types/FIELD-TIME-VALIDATION.md) for ownership,
configuration precedence, raw versus projected values and the intentional safe unknown-skip difference.

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

## Owned payload captures

The separate [object capture manifest](object-captures.json) records both pinned
revisions, final source/runtime/classpath-inventory hashes and all49 artifact
hashes per pin. [Locked expectations](expected/objects/) and
[current expectations](current-object-expected/) retain original requested composer
and FileWriter bytes plus803-row production observations. Two final-source
captures per pin match; complete locked clean-reactor Verify also preserves all82
historical root and25 scalar artifacts. Earlier receipts above remain unchanged.
The [object validation](../../../conformance/protocol/field-types/OBJECT-VALIDATION.md)
separates known/unknown Java readers, wire233 from its local9 consumer, UTF/length
errors, RAW callback fanout and C++ acceptance limits.

## Event flags and removal-time captures

The separate [flags manifest](flags-captures.json) records both pins, final helper/runtime/
production-classpath hashes and 105 original artifacts per pin. The [flags observations](expected/flags/observations.tsv)
separate six original FileWriter/FileReader files, requested composer masks, designed mutations,
Penta primitives and labelled RAW fanout. Both full captures repeat identically; all 287 reviewed
artifacts match across pins and the clean locked Verify preserves the historical 182 unchanged.
See [flags validation](../../../conformance/protocol/field-types/FLAGS-VALIDATION.md) for the
bounded STREAM/NONE C++ acceptance and intentional unsupported/error differences.


The separate [finite dispatch validation](../../../conformance/protocol/field-types/DISPATCH-VALIDATION.md)
adds322 synthetic artifacts and output8/family/descriptor checks while preserving all287 earlier
files. [Dispatch provenance](dispatch-captures.json) records each original raw hash, the exact57
UID-only comparison view and independent headerless bytes. Earlier subset results above stay historical.
