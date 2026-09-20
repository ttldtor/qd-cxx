# Local DataScheme and QTP record descriptions

QDCXX-004, Java QD 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Evidence labels are defined in the [protocol inventory](README.md); citations use the [locked source index](SOURCES.md).

## What crosses the wire

**S:** Local DataScheme is supplied to parser/composer; DESCRIBE_RECORDS does not load XML, construct a DefaultScheme or change local IDs. QDCXX-003 runtime descriptors feed this mapping. XML templates, enums, tags, docs, visibility rules, Java mapping factories, alias tables and scheme digest are not serialized here. [BinaryQTPParser.parseDescribeRecords; BinaryQTPComposer.describeRecord](SOURCES.md#framing-and-records), [local schema](../schema/README.md)

**S:** Type 2 payload repeats descriptions until frame end, with no record-description count:

```text
compactInt(senderRecordId), UTFString(recordName), compactInt(fieldCount),
    [UTFString(fieldName), compactInt(serialTypeId)] repeated fieldCount times
```

No numeric field ID is transmitted. Description position determines data byte order. Default writer layout takes integer fields then object fields, omits VOID slots, uses **getPropertyName()**, and includes full SerialFieldType IDs (serialization and representation bits). Optional EventTime/EventSequence precede ordinary fields when enabled. Long storage tails/synthetic VOID time slots are not wire fields; hasTime is not a separate descriptor flag. [BinaryRecordDesc; BuiltinFields](SOURCES.md#framing-and-records)

**S:** With describe-records enabled, first use of a record in data **or subscription** emits a description; subsequent uses are remembered. Connection composer enables this mode. Layout can reflect requested fields instead of all local fields. This is not local DataScheme serialized verbatim. [AbstractQTPComposer.beginRecord; BinaryQTPComposer](SOURCES.md#framing-and-records)

## Two directions, two ID spaces

**S:** Receiver reads sender ID/name, calls its own scheme.findRecordByName, builds a reading descriptor with local field lookups and installs recordMap[senderId]. Later data/subscription IDs select this map. Field lookup calls record.findFieldByName(wireName), including available local aliases, but no alias list is negotiated. Record lookup uses record name; no analogous record-alias exchange is established. [BinaryQTPParser; BinaryRecordDesc](SOURCES.md#framing-and-records)

**I — Example, not captured bytes:** A has Quote ID 0, B has Quote ID 7. A describes (0, Quote, ...); B maps incoming 0 to local 7. B sends its own description/data with ID 7; A independently maps incoming 7 to local 0. Neither side renumbers DataScheme. Equal numeric IDs alone do not establish equivalence.

**S:** Default BinaryQTPParser requires descriptions. Protected isSchemeKnown can be overridden to synthesize a descriptor from local ID/order. Legacy/test paths explicitly assume this; normal connection parser does not. [BinaryQTPParser.getOrCreateRecordDesc](SOURCES.md#framing-and-records), [MessageAdaptersTest](SOURCES.md#upstream-tests)

## Subscriber description influences outgoing fields

**S:** ConnectionQTPParser also stores descriptions in schemeMap[localRecordId]. ConnectionQTPComposer accesses them through MessageAdapterConnection.getRequestedRecordDesc. On first construction of its cached writer descriptor:

1. Nonempty peer description supplies requested names/order/types. DIR_WRITE matches local fields and chooses conversion/write operations; missing/unwritable fields are omitted.
2. An **originally empty** description means all known local fields (documented schema-less receiver signal).
3. Absent peer description also selects local default layout, subject to wide-decimal version fallback.
4. Sender describes its actual chosen layout and its own local record ID before values; it does not blindly echo peer ID or fields.

Sources: [ConnectionQTPParser/Composer](SOURCES.md#connection-and-transport), [BinaryQTPComposer.getRecordDesc; BinaryRecordDesc.isEmpty](SOURCES.md#framing-and-records).

**S/U:** Empty *intersection* differs from originally zero fields: empty remembers original count. Later descriptions update parser maps, but a cached writer layout returns before consulting new requests; composer resetSession also retains that cache. Do not promise dynamic renegotiation without capture. Source comments mention typical subscription-to-data synchronization, not a universal concurrency proof.

## Schema differences

| Difference | Observed behavior and limit |
| --- | --- |
| Record ID/order differs | **S:** record-name mapping resolves IDs; no local scheme.getRecord(wireId) assumption except explicit known-scheme mode. |
| Field order/index differs | **S:** descriptor sequence governs reading; name lookup selects local storage index. |
| Sender field absent locally | **S:** supported serial type is decoded/consumed by skip operation, preserving following fields. Skipping may allocate/read objects, not simply seek bytes. |
| Local field absent on wire | **S:** no assignment for that slot; normal fresh/cleared RecordBuffer storage is zero/null. **I:** raw storage initialization is not an application value/default promise (decimal zero bits can mean something else), XML default or mapping getter behavior. |
| Record name unknown locally | **S:** logs once per name, retains null-record descriptor, consumes supported data and skips delivery; subscriptions are skipped. **U:** optional built-in event metadata/field replacement paths can dereference null cursor; do not generalize normal skip behavior to every option. |
| ID never described | **S:** default parser reports unknown record as corrupted message; distinct from a described but locally unknown name. |
| Numeric representation differs | **S:** converters cover int/long, tiny/wide decimal, seconds/millis and legacy Profile.Shares scaling. Failed converter lookup retries with local representation bits but wire serialization bits; mismatch can mean raw reinterpretation instead of rejection. **D:** FieldAdaptationTest asserts conversion, truncation, NaN/infinity/time cases, not run here. |
| Object versus numeric mismatch | **S/U:** selection depends on wire serial type and local field class; some combinations skip, others produce questionable value paths. No generic coercion/rejection guarantee. |
| Invalid description | **S:** negative record ID/count, empty/null record/field name, out-of-range type produce corruption. Unsupported serialization code within accepted type range can instead throw InvalidDescException, logged without installing that ID (or replacing its old mapping). |
| Repeated ID/field target | **S:** successful description replaces ID mapping; field loop has no duplicate-target rejection. **I:** repeated target writes occur in wire order. **U:** capture duplicate aliases/remaps before specifying compatibility. |

Sources: [BinaryQTPParser; BinaryRecordDesc](SOURCES.md#framing-and-records), [RecordBuffer](SOURCES.md#encoding-and-core), [FieldAdaptationTest](SOURCES.md#upstream-tests).

**I:** Future protocol state needs incoming ID/layout mappings and chosen outgoing layouts separately from the local scheme. This does not prescribe Java arrays, cache policy, locks or C++ types. See P03–P08/P12 [capture scenarios](../../tests/fixtures/protocol/README.md).

## Executed subset (QDCXX-006)

**C:** [Measured evidence](CAPTURES.md) now confirms bounded P03 directional ID mapping, P04 reordering/alias-property lookup/raw-zero slots, and P05 absent/zero/x/absent-z requested layouts through the actual connection path. The example above remains illustrative; exact R/S captures are separately linked. Originally empty and empty-intersection requests are observably different. No generic default-value, numeric conversion, dynamic layout or session-reset rule is inferred from these nine cases.
