# Bounded STREAM_DATA decoder

## Explicit FIELD prefix policy

[TAPE-TIME](TAPE-TIME.md) defines `createWithFieldTimePolicy` and owned `FieldEventTime`.
READ applies exact raw-prefix precedence before local lookup, preserves complete signed compact-long
metadata separately from ordinary low32 fields, and admits one fixed output owner before defaults.
REMOVE reads the separate effective leading Event* physical prefix before removalTime.
Existing context creation and decoding signatures retain their default policy and charges.

[Русская версия](STREAM-DATA.ru.md). English is authoritative.

The rules below describe the unchanged three-argument zero-flags entry points unless explicitly
qualified by the [catalog event policy](#catalog-event-metadata).

## Additive local record identity

The [prepared mapping contract](../events/EVENT-MAPPING.md) adds `core::RecordContext` to each
`StreamRecordPlan` and appends it to `StreamDataRecord`. Known plans retain the source local schema's
opaque identity and local record ID; both STREAM and RAW copy that context into completed records.
Unknown plans have invalid contexts and still emit no record. Old aggregate initializers remain valid
and create invalid contexts; they do not authenticate values for the prepared event API.
The identity owns no schema declarations, source names or protocol state and survives their destruction.
No wire bytes, output values, work counters, failure positions or existing error precedence change.

## Explicit RAW payload entry point

QDCXX-023 adds `decodeRawData(payload, context, limits)` for a complete type-5 payload under
[RAW-TAPE](RAW-TAPE.md). Production FileReader observations establish the same exact field layout,
fresh symbols and zero-flags value projection for this bounded subset. Both public entry points
share a private payload implementation and retain the same result, charging, positions and terminal
prefix rules. `decodeStreamData` remains the type-15 entry point. Neither API dispatches arbitrary
message kinds, applies heartbeat time, nor implements Java's ticker/stream/history callback fanout;
the tape layer applies timing once per actual RAW frame.

## Explicit typed profile

QDCXX-022 adds the opt-in [Quote field contract](../encoding/QUOTE-VALUES.md). The snapshot copies
one bounded tagged raw-zero shape per local record and keeps the validated profile. Exact incoming
types select UTF_CHAR or compact-long reads even for skips. UTF_CHAR malformed input reports the
additive INVALID_FIELD status at FIELD with the primitive start and actual consumed count; existing
enum ordinals are preserved. Extended records populate typedValues only; legacy records populate
values only. emittedValues still counts logical fields once. Default context, symbols, flags, error
precedence and all legacy behavior below remain unchanged; no type coercion or dynamic session added.

[Русская версия](STREAM-DATA.ru.md). English is authoritative.

QDCXX-016 accepts one complete type-15 payload without its outer envelope. Only ordinary exact
COMPACT_INT (8), EMPTY_SET, raw eventFlags=0, no time indexes, metadata or replacers are supported.
No transport, writer, dynamic session, descriptor dispatch, importer or event API is provided.

## Evidence

The additive [production oracle](../../tools/upstream/stream-data-oracle/StreamDataOracle.java)
uses the locked QD 3.355 BinaryQTPParser with a real S/R/E scheme and preceding complete descriptions.
[Inputs](../../tests/fixtures/protocol/stream-data/inputs.tsv) are hand-authored stimuli, not expected
outputs. Every strict prefix gets its own complete type-15 frame. Callbacks provide local IDs,
lossless UTF-16 hex, ordered int values, flags, batch counts and errors. P01/P03/P04 and the existing
[binding evidence](../../tests/fixtures/protocol/binding/COVERAGE.md) remain unchanged.

BinaryQTPParser.parseData resets symbols per message, reads symbol then readCompactInt sender ID.
BinaryRecordDesc.readFields uses readCompactLong for SER_COMPACT_INT, including skip actions;
FLD_INT casts to signed low 32 bits. C++ reproduces that projection without out-of-range signed casts.
Repeated targets apply in wire order. Fresh missing ordinary int slots contain raw zero; this is
not an application default. readRecord selects readRemoveEventFields for REMOVE_EVENT and reads a
compact-long time instead of ordinary fields. C++ rejects every nonzero raw flag before sender ID,
including flags Java accepts; explicit zero flags remain allowed. Composer writeMessageHeader resets
its writer, and writeRecordPayload delegates to BinaryRecordDesc.writeRecord. Sources are pinned in
[SOURCES](SOURCES.md). First capture preceded C++ implementation; see paired fixture reports.

## Immutable context

StreamDataContext::create accepts validated LocalSchema and unique raw descriptions, never mutable
RecordBinding plans. It bounds schema records/fields/lookups/per-name/aggregate names using supplied
SchemaLimits, then descriptions and aggregate actions/names with ContextLimits and bindRecord.
Schema storage is inspected but not copied: snapshot owns only plans and each local output field count.
No pointer or name is retained, no array is indexed by sender ID. Empty context is valid. Negative or
duplicate sender IDs and malformed descriptions fail INVALID_DESCRIPTION; non-8 descriptions fail
UNSUPPORTED_PROFILE. Failed construction publishes nothing. Duplicate rejection is an API limitation,
not Java replacement semantics. Context cannot be default constructed; moving preserves context invariants. Read-only lookups borrow a plan only for the context lifetime.

Validation order: schema budgets in record order, description count, then for each description duplicate
ID, bindRecord with remaining aggregate actions/names, charge validated names/actions, append complete
plan. Binding errors retain their categories. bad_alloc/length_error become ALLOCATION_FAILURE.
No new schema semantics or loader is introduced.

## Results, precedence and resources

StreamDataLimits requires finite payloadBytes, processedRecords (including skipped/zero-field),
wireFields, emittedRecords, emittedValues, ownedSymbolUnits and SymbolLimits. Zero is valid.
SymbolLimits.inputBytes means remaining payload span, not encoded symbol length. Logical counts bound
owned elements, not allocator overhead/capacity. Aggregate symbol units count every emitted copy,
including repeats; temporary current symbol and reader state are bounded separately by SymbolLimits.

1. Payload size excess fails at PAYLOAD, position zero. Context validity is guaranteed by construction.
2. At each nonempty record start, charge processedRecords at RECORD before reading its symbol.
3. Read symbol at SYMBOL; preserve SymbolStatus cause. Truncation becomes TRUNCATED_PAYLOAD,
   malformed/invalid length becomes INVALID_SYMBOL, resource/allocation preserve their categories.
4. Reject nonzero flags at FLAGS after the symbol and before any record-specific data.
5. Read compact-int sender ID at RECORD_ID. Negative becomes INVALID_ID; absent mapping becomes
   UNDESCRIBED_ID, both after the complete ID. Primitive truncation consumes the available prefix.
6. For an emitting plan, check remaining emitted record/value/symbol budgets at OUTPUT before allocating
   zero-initialized local values. Unknown records allocate no output values and consume every field.
7. Before each field check/charge wireFields at FIELD; readCompactLong and assign low 32 bits to any
   target. Skipped fields use the same primitive and truncation behavior. No guessed widths.
8. Append only the complete current record at OUTPUT; charge output totals after successful append.
   On success stage END and consumed/errorOffset equal payload.size(). Empty input succeeds.

On first failure return status, stage, payload-relative consumed, errorOffset and completed records.
Primitive errorOffset is primitive start; validation/resource checkpoints use current consumed position.
Symbol cause is populated only for symbol reads; other primitive truncations use the stage to identify
the compact primitive. Allocation exceptions caught at the active checkpoint preserve earlier records;
no partial current record escapes. SymbolReader internally reports committed examination positions.
Vector allocation failure reports its checkpoint without further byte consumption. No allocation-failure
injection or exact allocator-memory guarantee is claimed.

Each call creates fresh SymbolReader; no continuation/recovery state or cross-frame borrowing exists.
Outputs own local ID, exact SymbolValue and local-order int32 values and outlive input/context/calls.
C++ completed-prefix retention is an explicit task-authorized policy: Java drops the current malformed
message batch (late-failure), while prior valid messages can be delivered (reset-repeat). Java callbacks,
outer frame positions and exceptions are observations, not C++ position/batching expectations.

## Validation

[Fixture validation](../../tests/fixtures/protocol/stream-data/VALIDATION.md) and
[conformance validation](../../tests/conformance/protocol/stream-data/VALIDATION.md) record executed checks
and remaining limitations. No full
QTP compatibility, event semantics, huge allocations, other architectures or transport is implied.

<a id="aggregate-accounting"></a>

## Charged work accounting

The aggregate accounting contract adds zero-initialized
`processedRecords` and `wireFields` to StreamDataResult. They count budget charges, not only
completed records/fields: increment after a successful budget check at the existing decrement,
before reading the symbol or field. Skips, repeated targets and later-failing attempts count.
Rejected budget checks do not count. All returns, including allocation errors, retain accumulated
charges; each is bounded by the original limit. Existing check order, wire semantics, completed
results and consumed/error positions are unchanged. A caller may carry file budgets across messages
by passing min(message limit, remaining total) and subtracting the returned actual charges.

## Explicit additional field profile

[FIELD-TYPES](../encoding/FIELD-TYPES.md) defines EXTENDED_FIELDS_V1 with exact 2/8/10/40/56/72/88/120,
owned nullable UTF-16, logical field positions independent of Java slots, and cumulative string work/ownership bounds.
Both existing tape modes keep their descriptor, flags, timing, terminal-error and append-only policies.
Earlier type exclusions below describe the older profiles; only this explicit profile extends them.
No Profile event adapter, indexed schema, importer or implicit discovery is added.

<a id="catalog-event-metadata"></a>

## Explicit catalog event metadata

`decodeStreamDataWithEventPolicy(payload, context, limits, DataEventPolicy::BINARY_CATALOG_V1)`
adds the catalog payload grammar without builtin FIELD metadata, with an explicitly validated BINARY_FIELDS_V1 context.
The old `decodeStreamData` and `decodeRawData` names/signatures remain unoverloaded and zero-flags
only, preserving existing function-address use. DataEventPolicy::ZERO_FLAGS selects that same
strict grammar through the new entry point. Invalid policy/profile combinations fail with
UNSUPPORTED_PROFILE at PAYLOAD before payload-limit or record work; old entry-point precedence
is unchanged. No FIELD event metadata is declared or autodetected by this API.

The broad policy accepts every subset of projected low 32 mask 0x5f: TX_PENDING1, REMOVE_EVENT2,
SNAPSHOT_BEGIN4, SNAPSHOT_END8, SNAPSHOT_SNIP16 and SNAPSHOT_MODE 64. Descriptor options do not mask
reader flags. Unknown projected bits, reserved0x20, REMOVE_SYMBOL0x80 and negative raw masks are
UNSUPPORTED_PROFILE at FLAGS after a complete symbol and before the sender ID. Compact-int flags
retain the existing signed low 32 projection; physical high bits are discarded before this check.
SymbolValue already owns exact flags and flag-byte counts; no second flags member is added.
These are generic metadata, with no collector, transaction or snapshot scheduler.

REMOVE_EVENT replaces ordinary field payloads with exactly one compact-long removal time in
the NONE grammar. `StreamDataRecord::removalTime` owns that signed 64 value. Disengaged means no
removal; engaged0 means a serialized removal time of zero. Unknown described records consume and
validate it without emission. Undescribed IDs still fail before time decoding. Source Java reads
configured builtin event fields before the time; explicit FIELD declarations remain refused at
the tape boundary until their separate contract is implemented.

Known records first admit emittedRecords, every dense local emittedValue and owned symbol units,
then allocate fresh defaults. Omitted scalar fields stay raw0; nullable strings/bytes stay null.
Only the explicit resolved [indexed layout](../schema/INDEXED-SCHEMAS.md) changes those defaults:
PAIR32 writes signed high/low words at the selected logical positions, SINGLE64 writes one raw 64.
No physical VOID tail, field-name heuristic, time-unit conversion or second indexed-time value
is introduced. The owned removal time is retained even for nonindexed records.

ProcessedRecords counts the attempt before symbol decoding. The special compact-long is bounded
by this record count and payload/input limits; it does not charge wireFields, which counts
descriptor-field attempts. Ordinary omitted fields charge no field/string/payload decoding work,
but dense local values still require output allowance before allocation. Existing work never refunds.
Truncation returns TRUNCATED_PAYLOAD at the appended REMOVAL_TIME stage, primitive-start
errorOffset and actual examined consumed. No incomplete record is published; owned completed
prefix/counters survive later parse or allocation failure. Values, symbols, removalTime and
RecordContext survive input/context/schema/reader destruction, moves and later generations.

[Production validation](../../tests/conformance/protocol/field-types/FLAGS-VALIDATION.md) separates
composer masks, primitive observations, designed messages and original FileWriter/FileReader
recordings at both pins. Java discards the buffered current-message prefix for the captured removal
truncations; C++ deliberately preserves its existing completed-prefix guarantee. Production RAW
fanout is three labelled callbacks, with equality checked before selecting one STREAM projection.
The four-family container dispatch policy is defined in [BINARY-MESSAGES](BINARY-MESSAGES.md);
this complete-payload API does not select collectors or a time source.

Existing formatter record/profileRecord entry points cannot represent raw flags or removalTime.
They throw invalid_argument before building output for nonzero eventFlags or any engaged removalTime,
including0. An explicit zero prefix with eventFlagsBytes>0 preserves old output. The automatic
tape-write record projection likewise refuses engaged removalTime; writer input/API is unchanged.




## Broad tape dispatch boundary

[BINARY-MESSAGES](BINARY-MESSAGES.md) applies the explicit policy decoder to all four generic data
families, with source family on the owning frame and no collector fanout. Existing three-argument
decoders stay unchanged. Direct policy payload decoding assumes no builtin FIELD prefix; declared
FIELD descriptors remain refused. The broad reader owns descriptor/session budgets and generations.
