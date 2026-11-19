# Variable payload byte accounting

[Русская версия](VARIABLE-PAYLOAD-BYTES.ru.md). English is authoritative.

This reference describes how the native decoder and receiver account for variable
payloads within their existing bounded scopes. The limits are operational budgets,
not a single process-wide memory limit. The
[field type specification](FIELD-TYPES.md) defines status precedence and wire types;
[UTFString](UTFSTRING.md) defines the separate primitive interface.

## Units and ownership

A borrowed input span is valid for the call and is not retained by reference. A
successful decoded value owns its storage. Null and empty are distinct: null has
no value, while empty has a present value of length zero. Opaque byte arrays and
serialized objects retain exact bytes; retaining them does not deserialize an
object. A UTF-16 unit is charged as two logical bytes for type 137. Surrogates
are preserved as units, including isolated surrogates; a supplementary character
uses two units. No normalization or replacement is implied.

Logical counts describe the scope named by each limit. They exclude container
capacity, allocator bookkeeping and unrelated allocations. Allocator storage
may exceed logical size; RSS also includes other process memory and operating
system effects. Neither can be inferred from these counters. A move transfers
ownership; it does not make a second logical payload copy. Ordinary caller
copies own independent values and may allocate and throw.

## Data field budgets

The finite, zero-default `StreamDataLimits` budgets are independent:

| Scope | Limit | Charge |
| --- | --- | --- |
| Type 10 character array | `fieldString.encodedBytes`, `fieldBytes` | Per value and aggregate examined bytes, including prefix. |
| Type 10 and 137 text | `fieldString.codeUnits`, `fieldUnits`, `ownedFieldUnits` | Per-value reserved-unit limit, aggregate reserved work and peak owned UTF-16 units for both. |
| Types 9, 137, 233, 249 | `fieldPayloadEncodedBytes`, `payloadEncodedBytes` | Per value and aggregate prefix and body bytes actually examined. |
| Same types | `fieldPayloadBytes`, `payloadWorkBytes` | Per-value declared body and aggregate admitted body work. |
| Same types | `ownedPayloadBytes` | Completed output plus current record plus one temporary payload, in logical bytes. |

Zero is an actual allowance. A null or empty body costs zero body work and
storage, but its prefix still costs encoded bytes. Scalars consume neither
payload nor string budgets. Symbol, frame, record and field limits remain
separate. Type 10 does not consume the new payload-byte budgets.

After a valid length and complete-message remaining-byte check, a type 9, 233
or 249 body of length B admits B work bytes and needs B temporary owned bytes.
The [UTFString length rule](UTFSTRING.md#errors-positions-and-resource-policy) permits
-1 for null and body lengths through 8589934588 bytes (INT32_MAX * 4). A declared
length must also fit the message and the caller's budgets. For type 137, the body
admits B work bytes, reserves
U = min(B, INT32_MAX) text work units, and needs U available owned units and
checked 2 * U temporary payload bytes before allocation. Retained storage is
2 * actual decoded units. Type 10 reserves its declared character count in
units. It charges bytes as they are examined.

Completed output and the current record coexist with the new temporary value.
Replacement must fit before the old value is released; skipped values need
temporary allowance too. Skips, replacement and failure do not refund examined
bytes or admitted work. A failed current record is discarded; earlier complete
records survive. Result ownership counters describe completed output only.
TapeReader accumulates work and completed ownership across frames, including a
terminal failure, even if the caller releases previous returned records.

All additions and products must be representable and checked before use; bounds
are checked by subtraction before addition. Rejection before admission does not
charge the rejected work. For type 137, a decoded point beyond the admitted
unit reservation fails before append, after examination of its complete tail.
A pair is never partially appended. Allocation or length failure while building
a field becomes `ALLOCATION_FAILURE`, retaining already incurred charges.
Length-domain and remaining-message checks precede body admission. An actual
truncation, malformed input and artificial budget cutoff have distinct outcomes;
see [field checkpoints](FIELD-TYPES.md#new-payload-budgets-and-failure-checkpoints).

## Input assembly and projection

`TapeReader::feed` borrows each supplied span, accepts a prefix and owns bounded
pending frame storage; the caller resubmits the unaccepted suffix. Empty input
is not EOF. Frame/pending limits and field budgets are separate: during
assembly or decoding, retained frame bytes and decoded values can coexist.
Caller-retained results remain the caller's responsibility.
A LiveEvent separately reports its requested State and full arena backing, including
spare capacity. Its release-work allowance also includes shared epoch reservations;
these scopes differ from field logical-byte counts and physical-memory measurements. See
[TapeReader](../protocol/TAPE-READER.md#state-ownership-and-resources).

The receiver's callback borrows its batch facade. `retain()` preserves the
existing generic backing, not the facade or mapping plan; an owning projection
can outlive the callback. Projection work is a separate prepaid budget:
`ReceiverProjectionWork` counts visits, text units and conservative result/text
copy bytes. It is based on declared route allowances, so actual copies may be
shorter. Attachment routes also include their configured serialized-body copy
allowance. The complete attempted result is prepaid before its allocation;
work already spent is not a measure of retained storage. See
[receiver lifetime and limits](../../docs/SUBSCRIPTION-RECEIVER.md).

## Shared record blocks

The internal schema-bound block stores exact nullable payloads and source metadata
in one packed array with a shared control and immutable binding. Copy admission
reserves the complete requested block capacity before allocation. Rejection
publishes no partial owner and preserves the input. The physical domain charges
the binding once; each explicit range retain separately reserves the full block
and associated binding against its consumer domain. Retaining a small range
therefore may charge the whole block. A second retain adds a consumer charge,
while sharing the physical backing. Moving a lease transfers its charge; final
release returns reservations under the existing lifetime rules.

Block copy counters count each symbol and nonempty engaged payload copied.
Field and row descriptors, source metadata and alignment are accounted separately.
The packed backing and shared control report allocator requests; the schema uses
the existing capacity model. Allocator overhead and RSS remain separate
observations. Independent range copying creates a new, potentially smaller block
and reuses the binding. See [record ownership](../schema/RECORD-OWNERSHIP.md#internal-schema-bound-storage-candidate)
and [the storage decision](../../docs/decisions/ADR-0015-shared-record-blocks.md)
for admission errors, lifetime and accounting boundaries. This is an internal
storage candidate exercised through the local STREAM path; existing public
queue retention and LiveEvent contracts continue to apply.
## Reproducible boundaries

With other budgets sufficient, a type 9 field containing three bytes requires
four encoded bytes (one-byte prefix plus body), three work bytes and three
temporary owned bytes. Allowances 0, required minus one, exact, and exact plus
one are checked for each budget, including a skipped field. For type 137 with
three ASCII bytes, the same encoded/work thresholds apply and peak temporary
payload is six bytes. Null and empty type 9 values each examine one prefix
byte and need zero body work and owned bytes.

For a three-byte value followed by a skipped three-byte value and replacement
with another three-byte value, peak ownership needs six bytes while total work
is nine. With ownership five, admission fails; with six, it succeeds and the
completed output owns three. With work allowance eight, the third body is
rejected and six work bytes remain charged. A malformed `01 c2 80` UTFString
examines three bytes before rejection despite a declared body of one; it never
crosses the supplied message or encoded allowance.

These are small deterministic boundary cases, not giant allocation or RSS
measurements. See [object field tests](../../tests/conformance/object_field_value_tests.cpp),
[field tape tests](../../tests/conformance/field_type_tape_tests.cpp), and
[primitive evidence](PRIMITIVES.md). The source tests define their complete
fixtures and expected statuses; they do not establish general Java equivalence
for untested cases.