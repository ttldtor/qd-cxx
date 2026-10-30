# Finite binary message policy

[Русская версия](BINARY-MESSAGES.ru.md). English is authoritative.

`TapeMode::BINARY_CATALOG_V1` explicitly selects this policy. The existing LEGACY,
RAW_MESSAGE_V1 and ordinary-v1 discovery policies retain their acceptance, defaults and output.
The [coverage catalog](BINARY-TAPE-COVERAGE.md) and
[production validation](../../tests/conformance/protocol/field-types/DISPATCH-VALIDATION.md)
state the finite evidence. This is generic wire reading; collectors and business conversions are excluded.

## Construction, identity and ownership

The supplied-schema constructor requires a validated BINARY_FIELDS_V1 schema and explicit finite
limits. It is the only route for caller-proven indexed layouts. The distinct discovery constructor is
`TapeReader(limits, TapeDiscovery::BINARY_CATALOG_V1)`. It starts with one admitted empty binary
schema identity; invalid discovery values or insufficient initial metadata bounds fail before
constructing that identity. Library caller limits have no implicit broad preset: zero stays zero.

Discovery creates ordinary records with hasTime=false. It cannot infer source slots, indexed layout,
business identity or time policy from names. Local record/field IDs and declared name/full-type pairs
remain retained and bounded after a sender is replaced. Reintroducing a retained name with a different
full type is UNSUPPORTED/TYPE, even when the old sender is no longer active. New names extend the
bounded registry. Active sparse sender replacements follow [INDEXED-SCHEMAS](../schema/INDEXED-SCHEMAS.md).

Every successful nonempty description batch gets a fresh immutable identity, including equal shapes.
An empty batch is a no-op: no identity, generation, schema rows or headerless authentication.
Failed batches retain published schema/context/descriptions and their borrowed addresses.
Owned records, fields, opaque bytes, symbols, removalTime and RecordContext survive subsequent
generations, input/reader/schema destruction and moves. The reader retains no snapshot history.

`descriptor()` and its text views expire on successful descriptor publication, move, assignment or
destruction. Failed descriptor candidates preserve them. Descriptor-only updates leave schema/context
views valid and do not reset descriptions, timing, symbol policy or budgets. Description publication
has its separately documented schema/context invalidation boundary. The reader is single-threaded;
feed returns owning events and introduces no callback or reentrancy contract.

## Dispatch and source order

| Message | Selected behavior |
| --- | --- |
| RAW_DATA5, TICKER_DATA10, STREAM_DATA15, HISTORY_DATA20 | One shared charged decoder; one record per wire record, original family in the owning frame |
| HEARTBEAT0 | Existing bounded empty/timestamp-only MESSAGE metadata, with finite timingEvents |
| DESCRIBE_PROTOCOL1 | Atomic known full/delta descriptor below |
| DESCRIBE_RECORDS2 | Atomic active-sender update and immutable generation |
| DESCRIBE_RESERVED3 | Bounded ignore event; retain frame, emit no data, reset nothing |
| PART4; subscriptions11/12/16/17/21/22; RMI49–56; TEXT61; ZIP75/GZIP139; other IDs | UNSUPPORTED/MESSAGE |

Families share exact fields, typed defaults, six known flag bits and REMOVE_EVENT semantics from
[STREAM-DATA](STREAM-DATA.md). Family is not duplicated in the record. The existing three-argument
decodeStreamData/decodeRawData APIs remain zero-only; the explicit policy entry point is
decodeStreamDataWithEventPolicy. No RAW fanout, ticker/history collector or scheduling is added.
Source order is wire order. Java pending family buffers can deliver wire15,20,15 as15,15,20;
this collector grouping is not the generic C++ delivery contract.

Each data frame starts fresh symbol/MRU state. All attempts, skipped/failed fields and owned output
share cumulative limits across families. Heartbeat and data-frame attempts charge timingEvents even
for NONE input. Prior accepted timestamp is carried forward; absent remains absent, with sequence0.
Descriptor changes, empty markers and no-op descriptions never clear accepted timing.

## Full descriptors and deltas

Parse DXP3 binary properties and message lists, with existing compact-long property counts,
projected compact-int list counts/IDs and bounded UTFString grammar. Text tokens are not a descriptor.
The first explicit descriptor requires type=tape, including after a valid headerless prefix.
Later descriptors merge known deltas: absent fields retain values; present fields replace them;
known sends union. Empty delta is a bounded no-op event. Candidate publication is nonthrowing only
after complete parsing, policy validation and resource admission.

| Property or extension | Selected acceptance/refusal |
| --- | --- |
| type | tape only; required in the first explicit descriptor |
| version | Optional exact UTF16, including empty; diagnostic text, never a compatibility/pin test |
| time | Absent, exact NONE/none/MESSAGE/message; other forms UNSUPPORTED/TIME |
| opt | Absent, empty or exact hs; other/duplicate/whitespace tokens UNSUPPORTED/OPTIONS |
| Unknown or duplicate top key | UNSUPPORTED/DESCRIPTOR after the examined pair; no arbitrary property map |
| Sends | Empty or any subset/order of exact numeric ID/name pairs for the four data families; no properties |
| Duplicate IDs, -1 named alias, mismatched ID/name, unknown message descriptor/properties | UNSUPPORTED/MESSAGE |
| Receive list | Empty only; other declarations UNSUPPORTED/MESSAGE |
| Optional EndpointId | Validate the finite structural tail, then UNSUPPORTED/DESCRIPTOR |

Sends are declarations, not authorization: an actual known data family need not have been declared.
Malformed counts, required pairs, magic and examined primitives fail MALFORMED. An unsupported
checkpoint can stop before later bytes; that refusal does not certify the unexamined suffix.
Java accepts several excluded alternatives, including unknown properties, aliases and some controls.
These are explicit finite-profile refusals, not claims of malformed Java grammar.

The ctx=null endpoint tail is a compact-long nullable byte-array JVM UID; for a nonnull UID it is
followed by nullable UTFString name and compact-long endpoint ID. Empty UID is valid; -1 is null,
other negative lengths invalid. Validate domain and remaining-frame bounds before skipping UID
bytes, without allocating/copying them. Bound the temporary name. Truncated/invalid primitives or
extra bytes after one complete tail are MALFORMED; a valid null/non-null tail is unsupported.
Do not import the different context-reference JVMId grammar or assume every valid UID has16 bytes.

## Headerless input and completion

The production FileWriter noHeaderAndFooter setting produces self-described binary: NONE starts
with descriptions2; MESSAGE can start with heartbeat0. Only this explicit broad mode accepts it.
Every data sender still needs a description before use, even with a supplied local schema.
There is no schemeKnown/readAs/resynchronization/ignore/cycle or numeric sender-to-local shortcut.

Finish requires actual physical EOF, complete framing and an accepted descriptor OR a committed
nonempty description batch. Empty files, marker-only, heartbeat-only and empty-description-only
inputs fail authentication. No footer is required. NEED_INPUT or an empty feed is not EOF.
An incomplete physical frame waits during feed and fails at framing finish. An incomplete primitive
inside a complete frame fails immediately with payload coordinates. Completed records are retained;
the current partial record is never exposed. Java can discard a buffered message prefix on error;
the C++ completed-prefix guarantee is deliberately stronger than that particular error delivery.

Native inspection borrows streams. Successful exit proves read/physical EOF/reader.finish/write/final
flush; it does not prove owner close-before-complete. Owner source lifecycle remains separate.
Already written complete-row bytes cannot be retracted after a flush failure: require both the row
and exit0. Java real-tape acceptance independently requires its instrumented physical EOF/open/close/
error/queue proof and literal FileReader digest parity; it does not supply native close evidence.

## Exact logical descriptor resource accounting

Use the same private finite ledger as schema/session construction. snapshotUnits bounds live metadata;
constructionUnits bounds cumulative admitted parse/copy/construction work. Old modes retain old checks.
Let B be all live S/D/C owners from INDEXED-SCHEMAS. For an owned effective descriptor d,
`H(d)=1+sum(1+UTF16 length)` for its engaged version/options/time; absent descriptor costs0.
An engaged empty text costs1. Fixed scalars/send booleans share the one descriptor header unit.

The simultaneous reservation is
`P=B+H(published)+H(candidate)+T(key)+T(value)+T(messageName)+T(endpointName)`.
Each independent temporary costs one header plus its live/reserved UTF16 units. Count every actual
owner; do not hide copies in a category. Fixed duplicate-key bits require no key-string history.
Frame bytes, encoded Row storage, allocator capacity and RSS have their separate bounds/qualifications.

Admit the complete descriptor payload size as work before parsing. Clone the entire old effective
header into an admitted candidate, charging H(old), or1 for a new empty candidate. A tiny/equal/no-op
delta still pays this clone. Before owned UTF decoding, reserve and charge one header plus
`U=0` for null/empty, otherwise `min(B+1, per-text units, remaining message units)`, where B is the
valid declared UTFString body length. Check addition/representation; Java's UTFString body domain
extends through8589934588 bytes. B+1 covers a malformed supplementary tail before its failure.
Use U as the decoder's unit cap before pushes. After success release unused live reservation,
retaining actual text units; cumulative admitted work is never refunded, including failure/skip.
Moves transfer admitted ownership; an actual copy requires its own prior peak/work admission.

Commit only after the complete candidate succeeds. Allocation failures translate at the current
checkpoint, preserving old addresses and cumulative work. UID skip owns no metadata; its examined
bytes remain covered by payload work. Empty deltas, repeated headers, failed candidates and source
finish never reset counters. Retired discovered names remain charged in S and final schema bounds.

Declared FIELD event metadata, sidecars, compression and source chains remain excluded. No new
dependency, network transport or source/callback framework follows from this finite policy.
