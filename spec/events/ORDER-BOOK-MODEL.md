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
