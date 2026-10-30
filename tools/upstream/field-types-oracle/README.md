# Exact field-type production oracle

[Русская версия](README.ru.md). English is authoritative.

This oracle runs unchanged locked QD through production `StringField`/`IOUtil`, integer field
methods, `BinaryQTPComposer`/`BinaryQTPParser`, and `FileWriter`/`FileReader`. It retains original
uncompressed BINARY files in STREAM/NONE and RAW/MESSAGE modes with EMPTY_SET options and zero
flags. It never reads the private tape. Full Profile event mapping is not exercised or implemented.

`FieldTypes` is a diagnostic ordinary record. Its logical order is Text(10), Short(40),
Sequence(72), Date(88), Count(8), Char(2), Seconds(56), Wide(120), Tail(10). Java stores Text/Tail
at object indices 0/1; the seven scalar fields occupy integer indices 0..6, followed by a VOID
tail at 7. `schema.tsv` records this projection and every physical slot/local/property name of
the unchanged default Profile. Profile has 24 physical int slots, two objects, no time indexing,
and 16 logical fields; no field or upstream scheme option is removed.

Three string-reader surfaces are measured separately: ordinary ByteArrayInput (`buffered`),
DataInputStream (`data-input`), and production message-boundary BufferedInputPart (`message-part`).
The last checks character count against remaining message bytes before allocation. Valid INT_MAX
count is tested only on that boundary; counts outside the Java int range are safe primitive probes.
The harness uses a 128 MiB heap and 30-second process limit. Timeout is failure. It does not allocate
an enormous array merely to demonstrate exhaustion.

Each launcher run requires a new output directory and first invokes the existing tape baseline
Verify: locked clean upstream validation, clean reactor build, classpath/provider/dependency hashes,
and byte-exact old tape expectations. PowerShell 7.4+, JDK 8, Maven and the old inspector are required.
The formatter uses a separate JDK 21+.

```powershell
$fieldArgs = @{
  Upstream = 'C:/src/qd-reference'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/tools/apache-maven-3.9.16/bin/mvn.cmd'
  Inspector = 'C:/src/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome C:/tools/jdk-23
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome C:/tools/jdk-23
& tools/upstream/field-types-oracle/Invoke-Oracle.ps1 @fieldArgs -RunDirectory .work/field-types-a
& tools/upstream/field-types-oracle/Invoke-Oracle.ps1 @fieldArgs -RunDirectory .work/field-types-b
& tools/upstream/field-types-oracle/Compare-FieldTypes.ps1 -Actual .work/field-types-a/field-types -Expected .work/field-types-b/field-types
& tools/upstream/field-types-oracle/Test-Negatives.ps1 -RunDirectory .work/field-types-negatives
& tools/upstream/field-types-oracle/Invoke-Oracle.ps1 @fieldArgs -Mode Verify -RunDirectory .work/field-types-verify
```

The comparator enforces the finite artifact set, row grammar, canonical integers, UTF-16 units,
nullable/scalar invariants, full tags, ordering and completion, then compares all bytes. Capture
validates structure against itself; independent cross-capture and Verify establish equality.
Verify never replaces expectations. Negative checks cover bytes, tags, values, units, nullability,
order, identity, missing/duplicate/unknown rows, arity, completion and primitive consumption.

## Binary scalar extension

The same invocation also writes `field-types/scalars`: 25 additional artifacts,
with 1106 production parser samples in STREAM and RAW per pinned producer. The
82 historical root artifacts remain byte-identical. Verify validates the finite
scalar grammar and all bytes; Test-Negatives includes six scalar mutations, for
23 rejected mutations total. Artifact metadata now includes child paths.

BYTE/SHORT use requested `BinaryRecordDesc` instances and the production composer
with CompactIntField consumers (wire1/3, consumer8). Their default factories return
null. Other new non-VOID types use actual PlainIntField/DecimalField/LongField/
TimeMillisField consumers. Ten original FileWriter/FileReader recordings cover
0/4/24/104/152 in STREAM/NONE and RAW/MESSAGE. VOID is elided by the producer;
explicit wire VOID acceptance/default-zero is a separate parser observation.
Physical long tails are never enumerated as logical fields.

For an already built, clean pinned classpath, the internal harness also accepts
`FieldTypesOracle <new-directory> scalar-only`. This runs only the scalar capture;
it does not build upstream or verify expectations. Compile the same source against
the pinned JDK8 classpath first, preserve class/source/runtime hashes, then compare
the complete 25-file set with the reviewed pin-specific directory. The ordinary
launcher retains its complete clean baseline Verify workflow. The second pin's
scalar capture/repeat and C++ consumption are recorded in the
[scalar validation](../../../tests/conformance/protocol/field-types/SCALAR-VALIDATION.md).
No new general-purpose CLI or default inspector profile is introduced.

## Owned byte/string/object extension

The governed invocation additionally writes `field-types/objects`:49 deterministic
artifacts and803 observation rows per pin. `ObjectFieldsOracle.java` is an internal
helper compiled/invoked by the same launcher, not a separate tool. It captures
requested composer and FileWriter/FileReader null/empty/content in STREAM and RAW,
actual parser known/unknown-field/unknown-record/repeated-target cases, direct
production FieldReader over BufferedInputPart, and supporting bounded primitives.
The unchanged82 root and25 scalar artifacts are still verified first.

ByteArrayField is the local9/233 consumer; only requested composer establishes
wire233, while its FileWriter uses actual type9. STRING uses StringField(true),
SERIAL_OBJECT uses MarshalledObjField and getBytes only. No getObject/unwrap or
Java deserialization is called. RAW callback fanout is distinct from wire count.
Explicit timestamped cursors make RAW files deterministic; observation rows use
explicit LF, so fixture text attributes do not alter captured bytes. Captures are
not normalized afterward. Known249 uses projected INT; Java unknown object skips use
LONG bytes and bypass137 UTF validation. The generic C++ exact grammar is fixed
independently of mapping, as specified in [FIELD-TYPES](../../../spec/encoding/FIELD-TYPES.md).

The existing internal entry also accepts `FieldTypesOracle <new-directory>
object-only` for both-pin prebuilt-classpath captures. This neither rebuilds nor
verifies upstream; the ordinary governed launcher retains complete clean Verify.
Compare checks finite row/artifact grammars and all bytes; Test-Negatives adds five
object mutations, for28 rejected mutations in total. Historical receipts remain
unchanged. [Object validation](../../../tests/conformance/protocol/field-types/OBJECT-VALIDATION.md)
records the new evidence separately.

## Indexed schema and active-description observations

The same governed invocation also writes `field-types/indexed`:26 artifacts and572
canonical LF rows per pin. `IndexedSchemasOracle.java` is compiled by the existing
launcher with the unchanged native-v1 exporter as an observation helper; it is not a
separate CLI. Eleven original FileWriter/FileReader cases retain five arbitrary signed 64
times each, including standard TimeAndSale with its complete actual source schema.
Slot/lookup projection, actual descriptor binding, omitted tails, aliases, missing/reordered
fields, equal/new/redefined senders and production reset are distinct observations.

Four successful single-message repeated-sender batches prove last-wins. Assembled late/
middle-invalid description batches demonstrate actual Java prefix mutation; following
data bytes come unchanged from production composer. They are not FileWriter originals.
The native-v1 exporter still reports UNSUPPORTED_INDEXED. C++ chooses exact types and
whole-batch rollback, as [INDEXED-SCHEMAS](../../../spec/schema/INDEXED-SCHEMAS.md) specifies.

The internal entry `FieldTypesOracle <new-directory> indexed-only` supports separately
qualified prebuilt-classpath captures. It does not rebuild/verify upstream. The ordinary
launcher still performs full clean baseline Verify; its metadata records all four Java
sources, original artifacts and actual runtime/classpath provenance. The strict comparator
checks26 indexed files after unchanged82/25/49 sets; six indexed mutations bring the
negative total to34. [Indexed validation](../../../tests/conformance/protocol/field-types/INDEXED-VALIDATION.md)
keeps baseline clean-build and current prebuilt qualifications separate.

See [coverage](../../../tests/fixtures/encoding/field-types/COVERAGE.md),
[captures](../../../tests/fixtures/encoding/field-types/CAPTURES.md),
[validation](../../../tests/fixtures/encoding/field-types/VALIDATION.md) and the
[field contract](../../../spec/encoding/FIELD-TYPES.md). Metadata/log timestamps and paths are
run-specific provenance; observed artifact bytes are never normalized.

## Event flags and removal-time extension

The same governed invocation now writes `field-types/flags`:105 additional artifacts. FlagsOracle
is an internal FieldTypesOracle companion, not another launcher. Its source and class hashes are
included in the ordinary provenance receipt. `FieldTypesOracle <new-directory> flags-only` supports
an explicitly qualified prebuilt classpath without rebuilding upstream or verifying expectations.
The normal invocation retains the complete clean baseline Verify workflow.

The child has 104 original binary artifacts and 4300 canonical observation rows. It separately labels
actual FileWriter/FileReader files, requested composer inputs/masks, designed message mutations,
production parser observations and primitive Penta reads. Every known subset of 0x5f is observed in
all four data families. RAW's three callbacks are compared before selecting one STREAM projection.
Exact descriptor absence/empty/hs and unknown/duplicate options are recorded; omitted FileWriter
settings actually select hs. REMOVE_EVENT covers raw signed 64 values, ordinary/indexed/unknown/
zero-field consumers, omitted defaults, following sentinels and incomplete time payloads.

Compare-FieldTypes checks the exact 105-file child set, finite row grammar/counts, raw values,
projection multiplicity and every artifact byte. Test-Negatives adds eight flags mutations to
34 earlier cases:42 rejections total. The historical 182 artifacts remain unchanged; the full
reviewed set is 287. [Flags validation](../../../tests/conformance/protocol/field-types/FLAGS-VALIDATION.md)
separates Java behavior from selected C++ profile refusals, completed-prefix policy and remaining scope.




## Finite dispatch companion

DescriptorOracle extends this same Capture/Verify workflow to609 artifacts, retaining all287 earlier
files byte-for-byte. Normal capture has305 artifacts (303 raw tapes,668 observations,57-range UID map);
a separate bounded child with explicit noHeaderAndFooter has17 artifacts (16 raw tapes,25 observations).
Actual FileWriter/FileReader covers all four families with NONE/MESSAGE; controlled descriptor/control/EOF
mutations are separate. Raw random synthetic endpoint UIDs remain intact with per-run hashes. Only57
structurally source-identified UID ranges are normalized in memory during comparison; every other byte
and the map shape must match. Headerless16 tapes are byte-exact. No new maintained launcher/dependency.
The existing negative workflow now checks50 mutations (42 prior and8 dispatch). See
[dispatch validation](../../../tests/conformance/protocol/field-types/DISPATCH-VALIDATION.md) and
[manifest](../../../tests/fixtures/encoding/field-types/dispatch-captures.json). Historical sections
above retain their original subset counts and source hashes.
