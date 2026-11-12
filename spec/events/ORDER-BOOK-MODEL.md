# Order-book snapshots

[Русская версия](ORDER-BOOK-MODEL.ru.md). English is authoritative.

Use `OrderBookModel` with ordinary `Order` events. Pass one exact symbol and
source ID per call, with events in arrival order. The model uses the
[indexed transaction contract](INDEXED-EVENT-MODEL.md) to stage unfinished
transactions and snapshots independently for each source. A committed update
from a second source does not publish pending changes from the first.

`getSnapshot(symbol)` returns owning bid and ask lists together. The snapshot
contains the currently committed values for that symbol across all sources.
Each order retains its full packed index and exact source. An absent symbol
produces two empty lists. Calls on one model must be serialized. The model
takes no callbacks and retains no subscriptions or input references. A saved
snapshot remains valid after later updates, `clear()`, or model destruction.

## Inclusion and order

The snapshot shows the sorted entries before any display filter suppresses a scope.

An indexed REMOVE erases only its symbol/source/index entry. Noncomposite
orders with zero or NaN size do not appear in the book. Composite orders
remain visible even with zero or NaN size. BUY orders appear on the bid side.
SELL and unspecified sides appear on the ask side, as in Java QD.

Bids sort by descending business price and asks by ascending business price.
At one price, nonindividual orders come before individual orders. The two
groups use these secondary keys:

- Nonindividual orders use descending business size, then ascending packed
  time/sequence, scope, exchange code, nullable market-maker text and signed
  packed index. Null text sorts before any string. Size comparison follows
  Java `Double.compare`, including signed zero and NaN.
- Individual orders use ascending packed time/sequence and signed packed
  index. The packed index already contains the source bits.

Price compares as binary64 using the typed Order accessors. Positive and
negative zero tie. NaN prices sort after numeric prices on both sides.
This gives C++ sorting a strict weak order when the reference model's price
comparison would treat a NaN as tied with every numeric price.

## Display options

Call `getFilteredSnapshot(symbol, options)` for an owning view of the same
committed data. The optional source selects one exact signed source ID. The
model then applies the requested scope filter separately to each side. The
seven reference filters select composite, regional, aggregate or individual
orders, composite with regional, all except individual, or all scopes.
`UNFILTERED` keeps eligible entries from every allowed scope. The original
`getSnapshot(symbol)` uses that unfiltered view.

Among eligible entries on one side, an individual order hides aggregates with
the same exchange code and nullable market maker. An individual or aggregate
order hides regional quotes for its exchange. Any visible noncomposite order
hides the composite quote on that side. Removing a detailed entry can expose
the less detailed one again. A depth of zero returns empty sides. Other
finite depths truncate each sorted side independently.

A positive lot size multiplies the returned business size of composite,
regional and aggregate orders. Individual sizes and raw source decimal bits
are preserved. Each snapshot derives the displayed size from the original
retained size, so changing the multiplier does not compound rounding.
An unrecognized filter or nonpositive lot size returns `INVALID_OPTIONS`.

## Aggregate corrections

The model enables corrections only when constructed with
`OrderBookCorrectionPolicy::SAME_SYMBOL`. The policy compares qualifying
aggregate orders from all sources of the exact symbol, even when a display
later selects only one source. Positive-size aggregates with finite prices
and a known BUY or SELL side qualify. Other order forms keep their usual
visibility. No symbol spelling or ambient property enables a policy.

A completed aggregate can suppress a same-side order at the same price with
an older or equal business time. A newer order can also suppress an older
unrelated aggregate when their business times differ by at least `keepAge`,
or an older crossing opposite-side aggregate at `flipAge`. A late value
strictly beyond a threshold is suppressed rather than evicting a newer one.
Only live, distinct entries participate in age and crossing comparisons.
This avoids the reference implementation's condition grouping admitting
the candidate itself or already suppressed entries into these comparisons.

A suppressed stamp prevents a repeated same-side, same-price value at the
same index and time or earlier from reappearing. A fresh change can restore
it. An explicit removal, zero size, or complete replacement of its source
snapshot retires the corresponding stamps. Removing a newer entry alone
does not revive previously suppressed values.

Corrections observe only completed indexed changes. When a call changes an
index several times, the final change for that index is considered. A single
indexed and correction commit publishes both sides together. The stamp
allowance counts live and suppressed entries across all symbols. Exceeding
it rejects the whole call, including any work needed to finish a pending
transaction. Ages are nonnegative and measured from event business time.
The model samples no wall clock. Construction rejects an invalid policy
or a negative age.

## Snapshot completion and bounds

Modern input uses SNAPSHOT_END or SNAPSHOT_SNIP. For older input, a wire index
of zero can finish an open snapshot. For a special source, the packed exchange
field must also be zero. TX_PENDING still delays publication until the
transaction completes. The original input event remains unchanged.

The indexed-model limits bound the number of symbol/source partitions, symbol
length, and counts of input, retained and pending orders. Combined snapshots
and normalization use additional owning copies bounded by those counts.
Limits count event objects and symbol units, and do not measure allocator
overhead. A bad input key, resource refusal or allocation failure leaves all
prior model state intact. `getSnapshot` reports a typed error if it cannot
make the complete detached copy.

The reference for finite-price ordering, size rules and legacy completion is
`OrderBookModel` and its `IndexedOrderModel` in the pinned Java QD source.
