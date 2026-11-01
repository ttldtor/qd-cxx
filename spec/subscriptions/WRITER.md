# Live subscription composition

[Русская версия](WRITER.ru.md). English is authoritative.

`qd/protocol/subscription_writer.hpp` and `qdcxx_protocol` provide a bounded, I/O-free
composer. This implements subscription/record-description output, not a live session, transport,
negotiation, collector or peer acknowledgement. Existing recording `TapeWriter` profiles are unchanged.

## Layout and wire profile

`SubscriptionWriter::create` owns a bounded schema copy preserving RecordContext identity,
complete explicit `OutgoingRecord` declarations, limits and one fixed output credit owner.
Declarations map local IDs to unique sparse sender IDs in 0..INT32_MAX-1; record names and full
field types match the local schema and every logical field appears exactly once through an
explicit lookup. Equal-shaped foreign schemas are not interchangeable. Large legal sender IDs
do not guarantee that Java peers can afford their separate dense remapping allocations.

The live validator accepts indexed BINARY_FIELDS_V1 for **description composition only**.
It neither enables new recording data types nor widens the existing OutgoingLayouts factory.
`describe(senderIds, work)` emits one DESCRIBE_RECORDS/type 2 envelope in supplied order,
including repeated known IDs. It records no announced/sent/readiness state.

`compose(message, requests, work)` preserves tuple order and duplicates. The six identities
are TICKER_ADD (11), TICKER_REMOVE (12), STREAM_ADD (16), STREAM_REMOVE (17), HISTORY_ADD (21) and HISTORY_REMOVE (22).
Each tuple is exact symbol then compact sender ID. Only HISTORY_ADD requires/allows a signed64
raw compact-long bound. Both HISTORY forms require explicit indexed metadata; other messages
and HISTORY_REMOVE reject a supplied bound. No business-time conversion is inferred.

Symbols reset per message. The default EXACT_ONLY encoder emits raw FD CharArray symbols and message-local
FF repeats; it does not promise Java's shortest PentaCodec representation. Non-null empty,
embedded zero and unpaired UTF-16 units are preserved. Null data pointers and reserved leading
`*` exact selectors are rejected; this default enables no All/glob or implicit converter. Explicit profiles are described below.
Descriptions use the existing bounded UTFString/compact primitives.

## Output lifetime and partial writes

A successful call returns move-only `PendingSubscriptionOutput` owning one complete envelope.
`bytes()` is the immutable original; `pendingBytes()` is the current suffix.
`consume(n)` advances only a local cursor; overshoot returns INVALID_CONSUMPTION unchanged.
In a transport, advance by the actual completed write count. Consuming all bytes is not a peer ACK.

Output survives input/composer destruction and transfers through move. Whole-byte views follow
the allocation into its new owner; old destination views expire on replacement.
Retained backing remains charged even after full consumption, until destruction/replacement
actually frees it. The shared fixed credit owner outlives its composer while outputs remain.
Original bytes permit mechanical retransmission; connection epochs and retry eligibility must
be handled by the later session, not inferred from byte availability.

Composer calls, sibling output access **and output destruction** must all be serialized and
non-reentrant. Cross-thread release is not supported by this API. Limits are per originating
credit pool; this is not a global quota across independently created composers/connection epochs.

## Limits, work and rejection

All capacities are explicit; zero is zero. frameBytes limits body including message identity
(and Java INT_MAX); operationBytes limits the complete envelope; operationEntries counts input
IDs/tuples including duplicates. symbolUnits bounds one symbol and text bounds one description
UTFString. LayoutLimits separately bound owned schema/declaration metadata.

residentBytes and peakBytes include old retained backing plus the new candidate; outputs bounds
simultaneously retained result owners. Credits are reserved before allocation and rolled back
on failure. Byte storage is an exact requested array; allocator/exception overhead and fixed
identity bookkeeping are not a process RSS limit.

Reuse one SubscriptionWriterWork through a caller-defined turn. Creation and all sizing,
metadata/comparison, primitive and encoding passes consume its explicit credits; successfully
charged work is never refunded on failure. Fresh turn credits permit continued feed work
without an implicit cumulative lifetime ceiling.

Validation, arithmetic, work, allocation and encoding complete before output publication.
Rejections return typed SubscriptionWriterError, preserve earlier outputs/configuration and
do not poison a healthy composer. No partially encoded result escapes.

## Example

These capacities are illustrative inputs. This complete C++23 program links qdcxx_protocol;
it exercises cursor ownership without opening a socket:

```cpp
#include "qd/protocol/subscription_writer.hpp"

namespace core = org::ttldtor::qdcxx::core;
namespace protocol = org::ttldtor::qdcxx::protocol;

int main() {
    constexpr std::int32_t COMPACT_INT_FIELD_TYPE = 8;
    const core::LocalRecord record{
        .id = 0, .name = u"Alpha", .hasTime = false, .fields = {{COMPACT_INT_FIELD_TYPE}}, .lookups = {{u"x", 0}}};
    const core::SchemaLimits schemaLimits{.records = 1, .fields = 1, .lookups = 1, .nameUnits = 5, .totalNameUnits = 6};
    auto schema = core::LocalSchema::create(std::span(&record, 1), schemaLimits);

    if (!schema) {
        return 1;
    }

    const protocol::OutgoingRecord declarations[] = {{41, 0, u"Alpha", {{u"x", COMPACT_INT_FIELD_TYPE, 0}}}};
    const protocol::LayoutLimits layoutLimits{
        .schema = schemaLimits, .records = 1, .fields = 1, .nameUnits = 16, .totalNameUnits = 32};
    const protocol::SubscriptionWriterLimits limits{.frameBytes = 512,
                                                    .operationBytes = 1024,
                                                    .operationEntries = 8,
                                                    .symbolUnits = 64,
                                                    .text = {.bodyBytes = 64, .codeUnits = 32},
                                                    .residentBytes = 4096,
                                                    .peakBytes = 4096,
                                                    .outputs = 4};
    protocol::SubscriptionWriterWork turn{.operations = 16, .visits = 4096, .textUnits = 65536, .outputBytes = 65536};
    auto writer = protocol::SubscriptionWriter::create(*schema, declarations, layoutLimits, limits, turn);

    if (!writer) {
        return 2;
    }

    const std::int32_t senderIds[] = {41};
    auto description = writer->describe(senderIds, turn);
    const core::SubscriptionRequest interest[] = {{{schema->recordContext(0), u"AAPL"}, std::nullopt}};
    auto subscription = writer->compose(protocol::SubscriptionMessage::STREAM_ADD, interest, turn);

    if (!description || !subscription) {
        return 3;
    }

    const auto retained = writer->retainedBytes();

    // Cursor demonstration only: a real transport advances by its actual write result.
    if (!subscription->consume(1) || !subscription->consume(subscription->pendingBytes().size()) ||
        !subscription->pendingBytes().empty() || writer->retainedBytes() != retained) {
        return 4;
    }

    return 0;
}
```

[Captured production parsing](../../tests/fixtures/subscriptions/writer/README.md) verifies the
selected native output with locked Java. [Validation](../../tests/conformance/subscriptions/WRITER-VALIDATION.md)
separates executed profiles from remaining live-session/runtime qualification.

<a id="selector-profiles"></a>

## Canonical symbol profiles

An explicit immutable `SubscriptionSelectorPolicy` at creation selects the
[canonical symbol contract](SELECTORS.md). Both canonical profiles encode Exact in all six messages;
only `CANONICAL_STREAM_ALL_V1` permits explicit All in STREAM 16/17. Default `EXACT_ONLY` retains old
FD/repeat bytes. No descriptor or text spelling enables permission, and local output is not peer ACK.
Complete-envelope Java comparisons and the legacy FD/canonical collector-key limitation are recorded
in [selector validation](../../tests/conformance/subscriptions/WILDCARD-VALIDATION.md).
