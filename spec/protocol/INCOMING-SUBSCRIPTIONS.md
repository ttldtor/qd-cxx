# Incoming binary QTP subscriptions

[Русская версия](INCOMING-SUBSCRIPTIONS.ru.md). English is authoritative.

`FrameReader` accepts partial input and returns at most one owning frame per feed. Call
`decodeIncomingSubscription` for a ready subscription frame. On failure the decoder returns one
error and publishes no entries. An application can apply a successful batch only after decoding
has finished. Session negotiation, closing policy and demand application belong to the caller.
The feed-oriented `LiveSession` continues to use its own directional admission and storage rules.

| Frame type | Message | Tuple fields |
| --- | --- | --- |
| 11 | TICKER_ADD | Symbol, compact int sender ID |
| 12 | TICKER_REMOVE | Symbol, compact int sender ID |
| 16 | STREAM_ADD | Symbol, compact int sender ID |
| 17 | STREAM_REMOVE | Symbol, compact int sender ID |
| 21 | HISTORY_ADD | Symbol, compact int sender ID, compact long raw bound |
| 22 | HISTORY_REMOVE | Symbol, compact int sender ID |

Tuples repeat to the exact payload end. A payload may contain zero tuples. Each frame has fresh
Penta symbol and flag state. Flag prefixes participate in the symbol grammar. The outer type sets
the command action, as in the Java subscription parser. An exact symbol owns its decoded UTF-16
units, including empty text. The reserved leading `*` is rejected in this exact-selector profile.
HISTORY_ADD preserves the original signed 64-bit value and performs no unit conversion. Both
HISTORY variants require a known target to declare an indexed time layout.

The caller supplies the complete mapping for the incoming direction, in strictly increasing
nonnegative peer record ID order. The mapping can be derived from earlier peer record descriptions
or supplied explicitly for a shared scheme. An undeclared peer ID rejects the frame. An explicitly
described unknown local target is fully decoded and charged to the same limits as a known target.
Its symbol participates in subsequent repetition within the frame. Output keeps known tuples in
wire order, including duplicates. Local contexts own their record provenance and the batch can
outlive the frame, mapping, and original schema declarations.

The decoder limits payload bytes, local schema records, sender mappings, tuple count, per-symbol
UTF-16 units and aggregate decoded UTF-16 units. It debits caller-turn frame attempts, admitted
payload bytes, tuple and lookup visits, and symbol work. Before decoding each symbol it narrows the
allowed text to the remaining cumulative units and work. Failed operations retain prior charges.
Budgets describe one decoded frame and bounded primitive temporaries. A caller can independently
retain successful batches and must account for their lifetime in its own storage budget.

The [native conformance cases](../../tests/conformance/incoming_subscription_tests.cpp) reproduce
six byte sequences emitted by the Java QD `BinaryQTPComposer`, including HISTORY_ADD with
`INT64_MIN`, `-65`, `0`, `64`, `INT64_MAX` for `IBM` and sender `Order` ID `6`. The test supplies an
explicit map to a synthetic local schema, exercises every split before the last byte, and adds
synthetic rejection and resource cases. The composer evidence was captured in memory with existing
Java classes from the locked checkout. The malformed cases assert the C++ whole-frame contract.
See the locked [Java parser and composer sources](SOURCES.md#framing-and-records).
