# Bounded outgoing live DATA

[Русская версия](LIVE-DATA-WRITE.ru.md).

`SubscriptionWriter::composeData` composes one nonempty, uncompressed plain binary
`TICKER_DATA` (10), `STREAM_DATA` (15) or `HISTORY_DATA` (20) envelope.
`LiveSession::publishData` requires the peer's matching receive capability and
allows publication after the session's authentication gate permits it. The
caller orders the corresponding type-2 descriptions before these bytes on its
connection. Outgoing sender IDs and field order come from this session's
immutable outgoing layouts. The peer's sending layouts have no role here.

Each occurrence supplies an exact nonnull symbol and either all tagged values
in local logical order or, for removal, an empty value span and a raw signed64
removal time. The wire walks the complete outgoing field order and preserves
full field types, raw scalars and null versus empty payloads. The writer
rejects removal if the first non-VOID outgoing field starts with `Event`.
Java consumes such a prefix ahead of the removal time even with its
event-time option disabled. Records without indexed time use zero as the
removal time.

Penta symbols, CESU-8 fallback and repeat markers use fresh state per envelope.
The known six raw flag bits (mask `0x5f`) require the history snapshot option
(`hs`) for all three families. Java's binary writer masks unsupported flags.
The native writer rejects these inputs so the caller cannot accidentally
substitute a value for a removal. The first MRU flag value is 1. Zero leaves
the MRU unchanged. Nonzero flags use F8 for the current MRU or F9 plus a
compact integer for a new one, preceding the symbol or its repeat marker.

Inputs are borrowed for the call. The complete envelope is published through
existing bounded output ownership and partial-write accounting. A rejection
returns no output, releases candidate storage, retains spent turn credits and
leaves the session usable. Composition performs no network I/O.

The Java byte comparison uses the
[field-types flags captures](../../tests/conformance/protocol/field-types/FLAGS-VALIDATION.md)
made by `FlagsOracle` through production `BinaryQTPComposer` and
`BinaryQTPParser`. The native test compares the type-2 and DATA envelopes
for the three families with all 64 known flag combinations under `hs`, and
the Java projections of those same inputs with its option disabled.
