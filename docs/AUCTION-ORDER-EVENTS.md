# OTC orders and auction imbalance

[Русская версия](AUCTION-ORDER-EVENTS.ru.md).

`OtcMarketsOrder` is an owning indexed event in `qd/events/otc_markets_order.hpp`.
Its `order` member contains the complete common order properties. The indexed identity uses
the source ID from the record name as the high 32 bits and the unsigned wire `Index` as the
low 32 bits. The event preserves signed quote access payment and the complete OTC flags.
Positive payments indicate a rebate, negative payments indicate a fee and zero indicates neither.
The OTC accessor for price type maps reserved code 3 to `UNPRICED`. The indexed delivery
flags, common order flags and OTC flags are independent. The default logical schema contains
`OtcMarketsOrder` and `OtcMarketsOrder#pink`, with the normal HISTORY low-index layout.

`OrderImbalance` is an owning source-specific TICKER event in `qd/events/order_imbalance.hpp`.
Its source is resolved from the record name. It carries signed packed business time and sequence,
five numeric properties and complete auction flags. Reserved auction and side codes map to
`UNDEFINED` in the accessors. A missing time or sequence contributes zero to the corresponding
part of the packed value. Missing near and far prices are NaN. The default logical schema
contains the unsuffixed record and the `#NTV`, `#ARCA` and `#NUAM` variants. Each variant
can report the same underlying symbol independently.

Prepare either mapper against an explicit record ID in a validated local schema. The record
family and source suffix are verified during preparation. A custom schema can choose the field
order and supported numeric representations. Source publication capability does not restrict
mapping an explicitly present receive record. Both mappers validate the complete input context
and field shape before projecting an independently owned event. Both accept independent FIELD
event time in milliseconds, with zero as the default. They apply the supplied UTF-16 allowances
before retaining symbol or maker data.

The [pinned oracle](../tools/upstream/auction-order-oracle/README.md) describes the QTP and
delegate comparisons, including default, tiny-decimal, integer and full-order-book profiles.
