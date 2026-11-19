# Symbol striping

[Русская версия](SYMBOL-STRIPES.ru.md). English is authoritative.

`qd/core/symbol_striper.hpp` provides immutable owned `SymbolStriper` partitions and
`SymbolStripeFilter` leaves. These values classify already resolved physical UTF-16 symbols.
A stripe is independent of a QTP channel, event source, connection epoch or executor domain.
Construction and queries perform no networking, callbacks or ambient configuration lookup.

## Grammar and selection

`SymbolStriper::parse` accepts an empty specification or `by1` for MONO, `byhashN` for HASH,
and `byrange-boundary-boundary-` for RANGE. Matching is case-sensitive and whitespace is
significant. A hash count is an unsigned decimal spelling within the positive Java-int domain,
a power of two and at least two. Leading zeroes are accepted; the name uses canonical decimal.
Thus `byhash0004` becomes `byhash4`, while the string `byhash1` is invalid. The separate Java
integer factory maps count one to MONO; that overloaded constructor is not a native API.

Range boundaries are nonempty, strictly increasing sequences of one to eight ASCII letters
or digits. The name preserves this valid range spelling. There is one more stripe than
boundaries; boundaries are lower-inclusive and upper-exclusive. MONO returns stripe zero.

HASH computes the Java string hash over exact UTF-16 code units, including empty text, NUL,
unpaired surrogates and both units of a supplementary character. Multiplication by 31 and then
by `0xB46394CD` wraps modulo 2^32. The high log2(N) bits select the stripe. No Unicode
normalization, case folding or event-symbol projection occurs.

RANGE skips an optional leading spread multiplier `=[+-]?[0-9]+(\.[0-9]*)?*`, then the
non-ASCII-letter/digit prefix. It uses up to the maximum boundary length, at most eight units,
clamping each encoded unit to `0x7F` and zero-padding a high-byte 64-bit key. The selected
index is the number of boundary codes less than or equal to that key. An empty significant
suffix belongs to the first stripe. The API follows the Java string route; it does not add a
raw cipher/string-pair or character-array-offset API.

Each exact string selects one stripe. Explicit ALL subscription admission is a separate
operation: it must reach every enabled stripe. A textual `*` is not an instruction to create
ALL in this API. The Java wildcard cipher is accepted by every stripe leaf; callers express
that admission explicitly through the `wildcard` argument.

## Static leaves and overlap

`stripeFilter(index)` creates an independently owned leaf. MONO yields `*`, HASH yields
`hashMofN` with M zero-padded to the decimal width of N, and RANGE yields
`range-left-right-`, with empty open ends. `SymbolStripeFilter::parse` accepts these static
leaf forms; `range---` becomes `*`. Arbitrary complete filter expressions return
`UNKNOWN_SPECIFICATION` and remain the responsibility of the upper filter boundary.

Compatible hash/hash and range/range overlap yields a proven contiguous `StripeIntersection`
with inclusive `first` and a `count`. MONO, ANY and different kinds yield `nullopt`, following
the production optimization contract. Unknown overlap requires considering every stripe;
it is never a known empty selection. A caller with an unrecognized complete expression must
likewise retain all stripes and continue applying the actual predicate.

These leaves do not install a subscription or event filter. `FeedFilter` already accepts hash
and range leaves and intersection syntax. An upper-layer record policy can combine its existing
expression with a generated stripe leaf; that policy must remain active after subset selection.
The pure core and collector targets do not depend on the event API.

## Ownership, errors and limits

Copies share immutable owned state and support concurrent const queries. Input views are
borrowed only during a call and must stay unmodified. Name views require an owning value or
copy to remain alive. A default or moved-from value is invalid; inspection returns an empty
name/zero count, while semantic queries return `INVALID_STATE`.

`SymbolStriperLimits` is required explicitly. It bounds input specification units, configured
stripe count, symbol query units and complete retained object/string/vector capacities. Zero
is a real zero. The backing count conservatively includes string character capacities as well
as object storage and excludes allocator bookkeeping and process RSS. Generated canonical
leaf text is bounded by retained storage, rather than by a caller-input spelling limit. Hash
metadata uses constant storage even for a large admitted count; constructing actual child
collectors requires separate per-child admission and backing.

Malformed recognized syntax, unknown factory formats, invalid stripe indices, finite resource
refusal and allocation failure have distinct detached errors. Candidate construction publishes
no partial value. Selection, leaf matching and overlap do not allocate. Neither an error nor
a completed classification implies wire emission, delivery, snapshot completion or reconnect.

## Compatibility evidence

The baseline is [`upstream.lock`](../../upstream.lock), Java QD 3.355 at
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Primary consumers are
[SymbolStriper](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SymbolStriper.java),
[factory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/SymbolStriperFactoryImpl.java),
[HashStriper](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/HashStriper.java),
[RangeStriper](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/RangeStriper.java),
[HashFilter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/HashFilter.java),
[RangeFilter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/RangeFilter.java)
and [RangeUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/RangeUtil.java).

The [production oracle](../../tools/upstream/stripe-oracle/README.md) captures
[raw observations](../../tests/fixtures/stripes/oracle.tsv) and
[source/class/runtime provenance](../../tests/fixtures/stripes/provenance.json).
Grammar, canonical names, ASCII and exact UTF-16 selection, cipher/string observations,
all generated stripe leaves, range edges and known/unknown overlaps are separate scenarios.
Pure classification fixtures do not establish striped collector delivery or parallel scaling.
[Striped local MUX composition](../mux/STRIPES.md) uses these values to route actual native collector input and multi-consumer demand. Its independent stripe hints, hard filter boundary, per-child quotas, resumable progress and native reset/snapshot contracts are specified separately.
