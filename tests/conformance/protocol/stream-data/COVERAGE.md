# STREAM_DATA conformance coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Tests](../../stream_data_tests.cpp), [negative checker](../../check_stream_data_negative.py),
[Java coverage](../../../fixtures/protocol/stream-data/COVERAGE.md), [contract](../../../../spec/protocol/STREAM-DATA.md).

Four doctest cases execute 85,549 assertions. All 35 Java observations are selected. Supported successful
messages compare actual local IDs, exact UTF-16, local value order and zero flags. Failed Java messages
are compared by corruption outcome; C++ retains completed records separately. Nonzero flags 1/2 are
explicit rejection cases with Java delivery checked separately. Direct checks define C++ error stage,
consumed/errorOffset and partial-prefix policy. No universal exception/callback equivalence is asserted.

Whole, every two-chunk split (including empty ends) and bytewise FrameReader feeds compare exact
complete description/data payloads and identical decoder results. The test driver uses a prebuilt
caller-supplied context, not a production session engine. Reset-repeat and a truncated symbol followed
by another complete valid frame prove fresh state and no cross-frame borrowing.

Snapshot tests cover duplicates, negative IDs, malformed names/types, unsupported types, invalid local
lookup targets, empty contexts, INT_MAX sparse IDs, aggregate action/name budgets and exact schema
record/field/lookup/name boundaries. Ownership checks destroy source schema/descriptions/input/context.
Decoder tests reduce each exact budget independently: payload, all processed records, wire fields,
emitted records/values/symbol units, symbol remaining-input/per-value/state limits. Repeats and skipped
or zero-field records count toward their appropriate budgets. Missing slots, duplicate targets and
unknown middle fields/records compare with Java. Primitive errors in targeted and skipped values,
resource-before-read precedence, undefined repeat, invalid lengths and nonzero flags have direct checks.

## Strict TSV schema

Accepted rows and exact arities: profile(7), case(2), descriptions(2), payload(3), wire(2), record(6),
callback(4), error(3), end(6), complete(2). Profile is exact; names are unique and all 35 required cases
must occur. Description/payload/wire order is enforced. Payload/record/callback indices are sequential;
hex/unit cells and signed int32 values are checked. Local IDs and value arity are validated. Callback
counts must match preceding records; errors must be message/type-15. End validates full-wire consumption,
zero pending bytes and exact record/callback/error counts. Exactly one completion must end the file;
unknown rows, blank/trailing data, duplicate completion, invalid arity or missing coverage fail.

All row types are either compared to decoder/frame behavior or checked as explicit Java observation
structure/profile/batching. No row is silently ignored. Fifteen temporary negative cases cover symbol,
value, ID, value order, failure row/outcome, unknown row, arity, duplicate/trailing/missing completion, missing
required case, out-of-range value, wire type and missing file. Immutable baseline passes afterward.

Not executed: allocation-failure injection, enormous allocations, 32-bit targets, exhaustive Unicode/
penta/compact inputs, optional metadata/replacers/conversions, event/time flags, dynamic session,
transport, writer, importer or performance checks. Five prior suites remain regressions.
