# Bounded STREAM_DATA decoder

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
