# Bounded binary tape reader

## Explicit ordinary discovery

[WIRE-INVENTORY](../schema/WIRE-INVENTORY.md) defines the separate opt-in
`TapeReader(limits, mode)` constructor, initially empty owned schema, borrowed
`schema()`/`descriptions()` views and atomic late description snapshots.
`TapeReader(schema, limits, mode)` retains the caller-identity behavior below.
Discovery creates fresh provenance per successful description, including an empty
batch; failed updates retain the old complete state. Local IDs/field IDs are
append-only, while sender IDs and wire field order remain exact source metadata.
No event adapter, hasTime inference, conversion or new container mode is added.

## Identity-preserving local schema snapshot

The reader constructs its owned local schema with `LocalSchema::boundedCopy`, retaining the caller's
opaque identity under the existing schema budgets. Its append-only wire-description rebuilds therefore
produce plans and completed records compatible with prepared mappings made from that caller schema.
Earlier records and mappings remain valid across rebuilds and after the reader or source schema dies.
A separately created local schema has a fresh identity even if every declaration matches. No mutable
local schema replacement is introduced. See [EVENT-MAPPING](../events/EVENT-MAPPING.md) for the exact
ownership/context API; constructor exceptions, construction charges and terminal-prefix rules stay unchanged.

## Explicit RAW/MESSAGE extension

[RAW-TAPE](RAW-TAPE.md) defines the independent opt-in `TapeMode::RAW_MESSAGE_V1` policy:
actual type-5 RAW_DATA, measured MESSAGE timing, absent/empty/hs options, exact owning heartbeat
and cursor timing, and an additional finite metadata budget. `TapeMode::LEGACY` is the default;
all descriptor, dispatch and output statements below describe that preserved mode. Local schema
selection remains independent. No broader types or full private-file compatibility follows.

## Explicit default Quote extension

QDCXX-022 adds [default-quote-v1](DEFAULT-QUOTE.md) with exact field identities 2/8/56/120 through
the explicitly selected local schema. Transport/descriptor acceptance below is unchanged. The
reader copies the local profile, rebuilds one typed default shape per local record within existing
schema/construction charges, and accumulates typed output values across messages. All snapshot,
terminal, prefix and offset rules remain. TapeDiagnostic adds bounded static refusal reasons;
legacy diagnostic fields and serialized version 1 output remain unchanged. The type-8-only text
below describes the preserved legacy profile, not the new explicitly selected Quote profile.

[Русская версия](TAPE-READER.ru.md). English is authoritative.

QDCXX-021 author contract, specified after Java probes and before C++ implementation.
[Decision](../../docs/decisions/ADR-0002-bounded-tape-reader.md). This is explicit uncompressed
BINARY, time NONE, EMPTY_SET, STREAM_DATA, raw flags zero, ordinary exact type 8, hasTime=false.
Caller supplies validated immutable local schema; no importer, transport or inferred schema.

## Evidence and differences

Locked Java QD from upstream.lock is the oracle. New production FileReader observations are in
[fixtures](../../tests/fixtures/protocol/tape-reader/expected). The unchanged original constrained
file delivers seven records; late S/R/E descriptions remap to local S/R/E identities. Two clean
upstream builds produced identical 102 artifacts (51 cases). Java accepts missing/repeated headers,
repeated/redefined IDs, unknown properties, duplicate property keys (last value wins), missing opt,
null opt and empty time in these probes. Missing footer and empty input return normally. A malformed
complete description can install its completed prefix and deliver later data; malformed complete
messages can report an error and continue. Truncated files report stream errors and previously
completed messages may be delivered. These are Java observations, not C++ policies.

## Descriptor grammar and restrictive policy

Payload begins with four ASCII bytes DXP3. Property count is compact-long constrained to 0..INT_MAX
(Java BufferedInput.readProperties), followed by UTFString key/value pairs. Send and receive lists
use compact-int counts (signed low 32-bit projection), each followed by that many message entries:
compact-int ID, UTFString name, property map. These distinctions also apply to noncanonical encodings.

Require one initial type-1 message, before even empty heartbeat envelopes. Require type=tape and
present opt with an empty non-null value. Time may be absent or exactly NONE; empty/null is rejected.
Version may be absent or any non-null exact UTF-16 string, including empty: diagnostic text only.
Reject null/empty keys, null values, repeated keys, unknown keys and unsupported property values.
Receive list must be empty. Send list must be empty or a single entry with ID 15, name STREAM_DATA,
and no message properties. Reject negative counts/IDs, null/empty message names, inconsistent names,
additional entries/properties and all trailing extensions. Require complete payload consumption.
Unsupported policy is distinct from malformed primitive/count/field errors; finite resource failure
is distinct from both. No support is inferred from version. Text preserves exact UTF-16 units.

Descriptor examination order is magic, global property count/pairs, send count/entries, receive
count/entries, trailing-byte check, required-property check. Count validity precedes count budget;
primitive errors precede validation of their result. A pair is fully read before key/value policy.
Every successful string charges aggregate units, including a later-rejected pair. Before each string,
limit units to the smaller per-string and remaining aggregate allowance. No untrusted count reserve.

## State, ownership and resources

Single-threaded incremental reader composes FrameReader, parseDescribeRecords, StreamDataContext
and decodeStreamData. Feed returns at most one owning frame event plus completed records; resubmit
unaccepted suffix. Empty feed is not EOF. Reader owns a bounded copy of local schema, retained raw
accepted descriptions, and one immutable decoding snapshot. Outputs own their text/values and outlive
input and reader. No public mutable plan, reset, replacement, follow or recovery API is provided.

For each description frame, parse the whole payload, then build a private candidate of old plus new
raw descriptions and rebuild its immutable context. Duplicate IDs anywhere fail, including identical
repeats. Publish candidate descriptions/context only after full success; failed updates publish nothing.
Unknown local records are successful skip plans after exact type validation. Old published snapshot
and earlier output remain valid. Each data message starts fresh symbol state.

Mandatory finite limits bound accepted input bytes, body/pending storage, frames including markers,
descriptor properties/entries/per-string/aggregate units, retained descriptions/actions/names, schema,
cumulative construction units, data record/field attempts including skips/repeated targets, emitted
values and owned symbol copies. Zero is a real bound. All cumulative charges use subtraction before
addition. Each rebuild charges the full candidate description+action+name counts and local schema
record/field/lookup/name counts before copying/building. This bounds repeated construction, not just
retained size; quadratic duplicate/lookup scans are additionally bounded by finite description/schema
limits. No performance or exact heap-byte claim. Old and candidate snapshots temporarily coexist.

Data receives remaining aggregate record/field/value/symbol limits; subtract actual charged record/
field attempts and completed output units. Frame byte bound limits each payload. Constructor invalid
configuration and allocation may throw before input. Subsequent allocation failure is terminal, retains
earlier data and old snapshot, and exposes no partial current record. No allocator-injection claim.

## Lifecycle, precedence and positions

At a new frame check frame budget, then input budget and existing framing rules. A complete frame
is charged before dispatch. Header/sequencing precedes message policy. Description syntax precedes
candidate construction/binding. Data error precedence is unchanged from STREAM-DATA. Stop on first
failure; no subsequent byte is processed. Previously completed records, including a prefix of the
current data payload, remain available.

Empty envelopes after the header are no-op markers. They are not authenticated footer/end markers;
continue parsing all trailing bytes. Explicit type zero and all other message types are unsupported.
Finish accepts only clean envelope EOF with an accepted header. Header-only and no-footer files are
valid; empty/missing-header and truncated input fail. Completion proves parsing, not completeness of
an intended recording. Track marker presence separately from completion.

Frame start/end use actual FrameReader boundaries. Payload start is end minus payload size, including
noncanonical length/type widths. Payload consumed/error are separate from accepted file bytes: the
complete frame was accepted before dispatch. Primitive errors point to primitive start, validation to
its checkpoint; description/data delegate their exact relative positions. Binding/duplicate/construction
checks occur at description payload end. Absolute payload positions are payload start plus relative
positions. Framing failures have frame start/accepted file position, no invented payload location.

Public API remains provisional. Designed tests must cover whole/bytewise/multiple chunks, ownership,
atomic updates, prefix failures, all limits at zero/exact/excess, malformed descriptors and noncanonical
positions. Historical fixtures and qdcxx-qtp-inspect behavior remain unchanged. Default Quote with
56/2/120 is still the next real-event milestone, outside this profile.

## Complexity and failure checkpoints

Let D/A/N be candidate descriptions/actions/name units, S the local schema records/fields/lookups/name
units, R the local record bound, L the maximum record lookup count and U the maximum compared name length.
A rebuild charges C=D+A+N+S before copying. Context construction costs O(D² + D*R*U + A*L*U + N + S)
with the reused linear searches. Sum C is bounded by constructionUnits; D/R/L/U are independently bounded,
so repeated construction is bounded by their product with constructionUnits, without unchecked products
in the accounting implementation. Initial schema validation/copy is separately bounded by SchemaLimits.
Even empty description updates charge S and the existing candidate. This is an analytical logical-work
bound, not a benchmark or exact allocation count.

Descriptor allocation failures retain the last committed primitive consumed/error checkpoint (a string
read that throws has not committed new consumption); no descriptor is published. Description syntax
errors preserve raw-parser positions but publish no part of that message. Candidate copy/binding failures
use the description payload end and retain the previous context. Returned frames and completed data own
all their storage. Context views are borrowed only until the next successful update or reader destruction;
a failed update does not invalidate them. Caller-retained frames/results remain the caller's responsibility.

## Explicit additional field profile

[FIELD-TYPES](../encoding/FIELD-TYPES.md) defines EXTENDED_FIELDS_V1 with exact 2/8/10/40/56/72/88/120,
owned nullable UTF-16, logical field positions independent of Java slots, and cumulative string work/ownership bounds.
Both existing tape modes keep their descriptor, flags, timing, terminal-error and append-only policies.
Earlier type exclusions below describe the older profiles; only this explicit profile extends them.
No Profile event adapter, indexed schema, importer or implicit discovery is added.

## Partial catalog mode with indexed generations

The supplied-schema constructor now explicitly accepts BINARY_CATALOG_V1 only with
BINARY_FIELDS_V1 and finite snapshotUnits. Its current header/data grammar is the same
restricted BINARY/NONE/STREAM_DATA subset, accepting absent/empty/exact `hs` options and
all known data flag subsets of 0x5f through the explicit catalog event policy. Old LEGACY/RAW_MESSAGE_V1
behavior and ordinary discovery remain unchanged; the discovery constructor refuses the
new mode. Other families, headerless input, other options/flags, FIELD and source chains remain pending.

Unknown/duplicate/whitespace option tokens are UNSUPPORTED/OPTIONS; absent and empty remain distinct
in the owned descriptor. Nonzero known flags are accepted even without hs advertisement, matching
the production reader rather than the composer mask. Unknown raw bits fail before sender ID with
UNSUPPORTED/OPTIONS. [STREAM-DATA](STREAM-DATA.md#catalog-event-metadata) specifies owned removalTime,
resolved time words, default allocation admission, counters and the appended REMOVAL_TIME stage.
No new reset, callback, collector or implicit discovery policy follows from this extension.

[INDEXED-SCHEMAS](../schema/INDEXED-SCHEMAS.md) defines pair 32/single 64 local metadata,
owned resolved plan positions and exact S/D/C work/peak charges. Active sender descriptions
are replaced in stable first-seen order. Every successful nonempty batch publishes fresh
immutable provenance, even for equal shapes; an empty batch is a no-op. All input entries
are validated, including overwritten intermediates. Failed parse/type/admission/allocation
retains old schema, context, descriptions and borrowed addresses. Publication uses only
nonthrowing operations without user callbacks. Returned owned records/contexts survive it.

New-reader construction is the explicit boundary for an independent closed source. No
header/footer resets state or work counters. Typed causes, framing/payload coordinates,
completed-prefix behavior and separate payload ownership budgets retain their contracts.
[Validation](../../tests/conformance/protocol/field-types/INDEXED-VALIDATION.md) records
both production pins and the deliberately stronger C++ rollback policy.


