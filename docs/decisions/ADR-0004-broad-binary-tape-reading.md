# ADR-0004 — Finite generic binary tape reading

[Русская версия](ADR-0004-broad-binary-tape-reading.ru.md). English is authoritative.

Status: selected technical direction for final contract review. Implementation and full catalog
acceptance are pending. This decision extends generic wire reading, not Java business semantics.

## Context

The existing ordinary profiles deliberately accept exact field identities and a small message,
flag and descriptor subset. Production tapes can contain additional physical field encodings,
indexed local layouts, nonzero flags and other data-message families. A structural walk reaching
EOF cannot establish value/order/time parity or justify a claim of support for all Java QD inputs.

The reference set is QD3.355 at `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and QD3.356 at
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. The separate
[producer fixtures](../../tests/fixtures/protocol/real-tape/producer/README.md) and
[validation](../../tests/conformance/protocol/real-tape/producer/VALIDATION.md) characterize three
named production cases and a derived truncation at each pin. They do not prove the pending broader rows.
The original lock and historical fixtures remain unchanged.

## Decision

1. Define a closed, versioned generic-wire target in
   [BINARY-TAPE-COVERAGE](../../spec/protocol/BINARY-TAPE-COVERAGE.md) and its
   [support matrix](../../tests/fixtures/protocol/real-tape/support-matrix.tsv).
   Preserve exact source identities, canonical raw values, UTF-16 units/opaque bytes, symbols,
   flags, source family and named time metadata. Preserve generic custom records and extra fields.
   Each applicable row needs pinned source and actual production/C++ positive and negative evidence.
2. Add one explicit core field profile `BINARY_FIELDS_V1` and a separate protocol mode
   `BINARY_CATALOG_V1`; final API spelling is reviewed before implementation. Existing profiles,
   ordinary discovery, admission tokens, default modes and serialized output remain unchanged.
   Core remains independent of protocol modes, transport, file I/O and ambient configuration.
3. Enumerate 19 candidate full field identities and reserved TIME_NANOS168 separately. Physical
   widths, compact-int projection, long storage and length/UTF grammar require implementation.
   Sharing serialization bits does not authorize an unknown full identity. Do not add Java's
   decimal/int/wide/shares/time-unit conversions merely to make a local consumer fit.
4. Extend owned values with nullable bytes for BYTE_ARRAY9, CUSTOM_OBJECT233 and SERIAL_OBJECT249;
   STRING137 uses exact nullable UTF-16 units. Retain marshalled serialization bytes without
   executing objects or loading Java classes. Preserve the distinct compact-int marshalled length
   and compact-long byte-array length rules. Unknown object contents are opaque, not a claim of
   semantic deserialization support. Legacy PlainObjField execution is excluded.
5. Require explicit caller/source-proven indexed layout. DESCRIBE_RECORDS carries no hasTime bit.
   Characterize both pair-int32 and single-int64 time layouts through production consumers before
   acceptance; never infer indexed/business semantics from record names. Physical VOID tails of
   a long are not logical values; only an actual explicit wire VOID field can project raw zero.
6. The new mode supports reviewed immutable generations for active sender-description replacement.
   Build and validate the entire sparse candidate before atomic publication. Retained records and
   context identities survive later generations; borrowed views have explicit invalidation points.
   Retain only bounded current state, not unbounded description history. Failures keep the previous
   generation; admitted construction work is never refunded. Ordinary discovery keeps its strict
   repeated-ID refusal. Equal/redefined IDs and failed replacements remain pending QDCXX-043 tests.
7. Use a new explicitly constructed reader for an independent closed source. In-place reset is
   pending only if production requirements need it and its complete state/budget contract is reviewed.
   No header/footer resets state implicitly, and reset/source changes cannot evade cumulative bounds.
8. Decode RAW/TICKER/STREAM/HISTORY data as generic wire records, preserving family and wire order.
   Preserve the six known data flag bits and REMOVE_EVENT's special compact-long record-time payload.
   Existing SymbolValue owns flags; add only an optional owned removalTime to the record. This is
   separate from future FIELD event-time metadata. No collector or transaction/snapshot scheduler
   follows from parsing those fields or flags. PART, subscription and RMI payload operations are
   explicit unsupported features of this generic data target.
9. Characterize descriptor tails/properties/deltas and headerless self-described binary before
   enabling their exact bounded policy in QDCXX-045. Headerless requires an actual FileWriter
   positive case, not parser permissiveness alone. Reject schemeKnown/readAs/resync/ignore/continue/
   cycle alternatives explicitly. A valid Java alternative outside this profile is unsupported,
   while malformed input is terminal with the documented completed prefix and coordinates.
10. Keep FIELD time, sidecars, GZIP/ZIP, split sources and broader operational streaming pending
    their named contracts. This packet adds no dependency, download, callback framework or source API.
    Compression dependency/license/platform/OFF acquisition and member/entry/CRC/inflation policy
    remain a future gate. Future callbacks need a separate lifetime/stop/error/close/EOF contract.

## Alternatives

- Extending existing profiles silently would break established refusal/output contracts and confuse
  local field support with container or business compatibility.
- Selecting width from low bits or applying Java conversions automatically would discard exact
  identity and source representation, and can misread long or object-length payloads.
- Deserializing arbitrary Java objects would introduce a different execution, dependency and
  compatibility problem. Opaque bytes satisfy the selected generic-wire boundary.
- Copying Java's mutable partial remapping and sender-indexed allocation would not supply the required
  C++ snapshot lifetime, atomicity or finite-resource guarantees.
- Declaring an unnamed full-QD target would hide unsupported versions, message families and source modes.

## Consequences and delivery

QDCXX-041 handles missing scalar identities; 042 handles owned byte/string/object payloads; 043 handles
indexed layout and generations; 044 handles options/flags/symbol state; 045 handles data dispatch and
descriptor policy. Their test/API gates are in
[CONTRACTS](../../tests/conformance/protocol/real-tape/CONTRACTS.md).
New profile support remains pending until all applicable rows have evidence. Future046–050 rows are
not closed or implemented by this decision.

The producing harness has Windows qualification only. Portable fixture consumption still requires
the C++ client Windows MSVC Release and Linux GCC Debug ASan/UBSan matrix; producer-host limits do not
waive that requirement. Independent final-source ownership/state/resource review is required before
critical contracts close. Documentation and source characterization alone do not establish runtime
compatibility, completion, a full catalog pass or a released version.
