# Explicit FIELD event-time metadata

[Русская версия](TAPE-TIME.ru.md). English is authoritative.

The protocol library separates physical FIELD words, ordinary business fields, indexed record time,
and MESSAGE heartbeat state. This contract extends the finite
[binary message policy](BINARY-MESSAGES.md). It does not select a new inspector format or replay policy.

## Selection and compatibility

Existing constructors and `StreamDataContext::create` select `FieldTimePolicy::DISABLED`.
Their NONE/MESSAGE binding, removal handling and output bytes retain their previous meanings;
FIELD descriptor text remains `UNSUPPORTED/TIME`.

Select READ explicitly through
`TapeReader(schema, limits, TapeMode::BINARY_CATALOG_V1, FieldTimePolicy::READ)`,
`TapeReader(limits, TapeDiscovery::BINARY_CATALOG_V1, FieldTimePolicy::READ)`, or
`StreamDataContext::createWithFieldTimePolicy(schema, descriptions, limits, FieldTimePolicy::READ)`.
The old three-argument factory remains unambiguous, including taking its function address.
READ requires BINARY_FIELDS_V1 and, for TapeReader, BINARY_CATALOG_V1. Invalid policy values and
old-mode/READ combinations fail before input and before schema construction. Diagnostic exception
construction can itself allocate. Context construction returns `UNSUPPORTED_PROFILE` for invalid policy.

READ admits exact FIELD/field descriptor values alongside existing NONE/none/MESSAGE/message values.
It recognizes prefixes independently of descriptor text, including headerless, NONE and MESSAGE input.
This corresponds to explicitly enabling production parser `readEventTimeSequence`; descriptor text
does not silently select the feature. No ambient configuration or broader case folding is added.

## Exact recognition and binding

Recognition examines raw physical descriptions before zero-width VOID elision:

1. The first field must be named exactly EventTime with full type56.
2. Only the immediately following raw EventSequence/full72 is a second metadata word.

A first-only prefix is valid. Wrong full types, aliases, case changes, reordered fields and later
occurrences stay ordinary exact-type actions. VOID before EventTime prevents recognition; VOID between
EventTime and EventSequence leaves only the first metadata word. No names imply indexed layouts.

The shared planner applies virtual-prefix precedence before business lookup, including every
intermediate replacement that is later overwritten in the same batch. A supplied business target with
the same name remains at its typed raw-zero default unless another ordinary action assigns it.
Discovery retains exact raw descriptions and actions, omits only recognized metadata from ordinary
local declarations, and preserves its append-only name/full-type registry across generations.
Standalone `bindRecord` retains its existing ordinary behavior.

`StreamRecordPlan::eventTimeFieldCount` is0..2.
`removalPrefixFieldCount` is a separate exclusive action position: after effective VOID elision,
every leading name beginning with Event belongs to the REMOVE prefix. Explicit VOID actions retain
their zero-width field-work charges and do not stop that prefix. REMOVE reads the whole prefix using
its existing exact actions, then the compact-long removalTime. Leading business EventPrice or
wrong-type EventSequence retains its transmitted value; ordinary fields after the prefix keep
defaults. Caller-proven indexed layout receives removalTime through its existing separate mapping.

## Owned values and projections

`StreamDataRecord::fieldTime` is an owned optional `FieldEventTime`. Absent prefix, engaged zero,
absent sequence and explicit zero sequence remain distinct. `rawSeconds` and optional `rawSequence`
retain full signed compact-long values, before production Java casts them to signed32 words.
No source pointer, callback or additional shared ownership is retained.

`packed()` combines low32(rawSeconds) as the high word and low32(rawSequence or0) as the low word,
using unsigned arithmetic and bit-casting. `eventTimeMs()` widens signed32 seconds before multiplying
by1000 and adds unsigned sequence bits31..22. Millisecond subfields1000..1023 remain exact; there is
no guessed normalization, saturation or rejection. Signed64 extrema are valid physical words.

`sourceOrdinal` is the zero-based source wire-record attempt, including described unknown records
that are skipped. It continues across all four data families and failed attempts, independently from
callback fanout or emitted record indices. Standalone decoding starts at0; TapeReader adds checked
prior work before returning completed records. Counters and ordinal arithmetic never wrap.

`TapeFeedResult::timing` remains the separate owned MESSAGE/heartbeat state and retains its existing
saturation rules. Production delivered cursor time uses a nonzero externally configured MESSAGE
packed value in preference to every FIELD value; zero leaves FIELD packing in effect. Raw FIELD
metadata remains intact in C++. Applications can apply that projection without storing another
duplicate time in every record. No replay scheduling change follows.

## Bounds, errors and publication

The existing primitive, wire-field, processed-record and payload budgets apply even to skipped
metadata. READ adds two logical snapshot/construction units per plan, including every intermediate
validation, with old/candidate/temporary peak accounting. Rejected publication preserves old
schema, descriptions, context and borrowed addresses; admitted construction work is not refunded.
Successful publication retains the established view invalidation boundary.

`StreamDataLimits::ownedFieldTimeRecords`, default0, bounds completed plus current metadata owners.
An engaged output costs one fixed unit; mapped output checks it at OUTPUT before dense defaults or
output ownership. Unknown skipped records consume primitive/work allowances without output metadata
charges. `StreamDataResult::ownedFieldTimeRecords` reports completed owners; TapeReader accumulates
them across returned families without refunds. Empty/header frames need no metadata owner.

Incomplete current records publish neither partial metadata nor partial fields. Completed prefix
records survive subsequent primitive, budget and allocation failures. Field failures retain existing
FIELD stage, offset and codec diagnostics; removal time retains its separate REMOVAL_TIME stage.
Owned contexts and outputs survive source/schema/input/reader destruction, moves and replacement.
Empty timed messages and heartbeat events do not authenticate EOF.

Actual production Java throws NullPointerException for the measured unknown consumer with recognized
FIELD actions. C++ deliberately preserves its existing bounded safe skip guarantee. This stronger
guarantee is not described as Java callback parity.

## Presentation and evidence

Every existing formatter entrypoint refuses any engaged FIELD metadata, including zero, before
row construction or output-budget handling. No old output version silently drops these words.
The existing tape-write path uses DISABLED; its record/view bridge also refuses engaged FIELD
metadata. No OutgoingRecordView field, writer grammar or CLI FIELD selection is added.

The maintained [production oracle](../../tools/upstream/field-types-oracle/README.md) records raw
designed inputs separately from actual FileWriter/FileReader/parser/composer observations. The
[validation report](../../tests/conformance/protocol/field-types/FIELD-TIME-VALIDATION.md) distinguishes
executed producer pins, canonical source records, callback projections and native checks. Existing
NONE/MESSAGE captures remain immutable.
