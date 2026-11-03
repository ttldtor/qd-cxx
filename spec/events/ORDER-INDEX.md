# Order record sources and index packing

[Русская версия](ORDER-INDEX.ru.md). English is authoritative.

The independent C++23 target `qdcxx_order_index` exports
[order_index.hpp](../../include/qd/events/order_index.hpp). It depends on the
existing pure event mapping and source identity components. `qdcxx_feed` links it
publicly. No network, SDK, ambient setting or mutable cache is involved.

The authority is [upstream.lock](../../upstream.lock), pinned QD revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`: production `MarketEventMapping`,
`OrderMapping`, `OrderDelegate`, `OrderBase`, `OrderSource` and `MarketFactoryImpl`.
[Production observations](../../tests/fixtures/order-index/oracle.tsv) retain
actual calls, full field declarations and failures. The
[oracle launcher](../../tools/upstream/order-index-oracle/README.md) records
source hashes and an unchanged production class inventory.

## Explicit prepared mapping

`PreparedOrderSource::prepare(schema, localId, limits)` selects exactly one
explicit validated local record. Its last `#` separates the source name. Without
`#`, the source is `OrderSource::DEFAULT`. A suffix must contain one to four ASCII
alphanumeric UTF-16 units, preserving case, and resolves through `OrderSource`.
An empty suffix, `#DEFAULT`, `#COMPOSITE_BID`, punctuation or non-ASCII suffix is
invalid. For example, `Order#NTV#BZX` resolves BZX. The record family is chosen by
the caller; this component does not infer an event class from a name prefix.

Preparation binds the exact `Index` lookup with required full type
`COMPACT_INT` (8), through `EventMapping`. Aliases targeting the same local field
and unrelated fields are retained by the existing schema rules. Missing Index
returns `MISSING_REQUIRED`; another exact full type returns `INCOMPATIBLE_TYPE`.

This is an explicit standard-field profile. Java `MappingUtil.findIntField`
accepts a physical `DataIntField` and `OrderMapping.getIndex` reads its raw int
slot, including under INT, DECIMAL and SEQUENCE full identities. The C++ profile
does not claim that Java rejects those types. Broader field conversions remain
separate work; they can extend preparation without changing this source/index API.

A plan owns its source, full local field shape, selections and immutable record
context. It retains no original schema, names, fields or payload views. Copies
preserve the exact schema origin. Equal independently created schemas remain
foreign. Both projection methods first validate context, then every local field,
including extras. Projection allocates nothing and retains no values.

`mapIndex` / `mapIndexView` reproduce the production Order delegate:
`(uint64(sourceId) << 32) | uint32(wireIndex)`, returned as signed64 raw bits.
Negative signed32 wire indices preserve all low bits; they are not rejected on
receiving. This is distinct from publishing restrictions in Java `putEvent`.
A record source, regional exchange, business symbol and physical endpoint origin
are separate identities. No exchange or endpoint value enters this packing.

The original finite `MappingLimits` still apply. Record-name length must fit
`nameUnits`; its units additionally consume `totalNameUnits` and
`comparisonWork` before field preparation. Resolving a source retains at most
four logical name units, or seven for DEFAULT. String capacity and allocator
metadata are implementation selected; these logical bounds are not process RSS.
Invalid input, insufficient bounds and allocation failures publish no partial plan.
A moved-from plan may be destroyed or assigned; do not inspect its source.

## Raw order index helpers

`isSpecialOrderSourceId` is true exactly for IDs 1 through 9.
`orderIndexSourceId` reads the signed high16 word; when that word is special it
is the source ID, otherwise the signed high32 word is the source ID.
`orderIndexSource` resolves that ID and can throw `std::invalid_argument` for an
invalid encoded name, or `std::bad_alloc` while owning its name.

`changeOrderIndexSource` exactly reproduces `OrderBase.setSource`: a new special
source uses shift48, otherwise shift32. The old index selects the preservation
mask: old high16 special keeps low48 (exchange plus local index); otherwise keep
low32. The new source bits are ORed into that payload. In particular, changing an
old special index to a generic source can retain overlapping exchange bits; this
helper intentionally does not normalize or clear them. Arithmetic uses unsigned
bits and bit-preserving signed conversion, avoiding C++ signed-shift overflow.

Raw helpers accept all signed64 bit patterns. This does not make negative event
indices valid: Java's public `setIndex` rejects them. The oracle uses a clearly
labelled reflection probe only to observe actual `getSource` / `setSource` on
otherwise unreachable raw negative states. No C++ complete Order event or setter
is introduced here.

## Integration boundary

This component is ready for a complete typed Order adapter to consume. It does not
add `EventType::ORDER`, Order delivery through Feed, source subscription routing,
capability flags, publication or collector snapshot semantics. Those remain in the
full receiving-client plan. Existing Quote/Profile and generic record behavior is
unchanged. See [usage](../../docs/ORDER-INDEX.md) and
[source identity contracts](EVENT-SOURCES.md).
