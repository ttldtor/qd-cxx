# Owned HISTORY receiving and observations

[Русская версия](HISTORY-RECEIVING.ru.md).

`EndpointOptions::contract = DeliveryContract::HISTORY` selects the existing indexed HISTORY
collector, per-subscription views and delivery queue. The SDK worker owns registrations, desired
mapping, replay, transport progress, source reset and reset acknowledgements. Applications do not
create a collector or supply turn-work credits. TICKER remains the default; STREAM and HISTORY
are explicit, distinct contracts.

A business type must have a verified HISTORY route. The current nine built-in Feed types have no
such route and subscription creation rejects them with `UNSUPPORTED_PROFILE` at
`subscription-history-event-type`, before native registration. A generic indexed layout or a
caller-declared time-series capability does not grant business-mapper permission. The common
HISTORY receiver can retain complete custom schemas through the existing lower receiver API;
this does not add a public raw-record Feed subscription or a typed TimeAndSale facade.

## Passive owning observations

`Subscription<Event>::getHistorySnapshot()` copies an owning `HistorySubscriptionSnapshot`.
It performs no I/O, queue progress, callback wait or source query. It may allocate its bounded
metadata copy and reports allocation errors through the existing facade error boundary.

The worker enumerates the registration's authoritative applied interests and queries the actual
`QueueRangeStatus` for every physical record/symbol/selector. A logical selector may expand into
several physical routes. Entries own symbol text and immutable schema provenance; no payload,
callback, collector or native owner is retained. The vector has no public ordering guarantee.
An opaque owner identity, checked publication serial and monotonic observation time identify
one owner-local cut. They are not global clocks, sortable cross-owner IDs or peer acknowledgements.

`CURRENT` means that the last successful publication still matches the latest accepted logical,
connection and clear fences and the observed callback-retirement cut. It does not assert that
there is no unread network input or that the result will remain fresh. Source coverage alone is
insufficient: only each entry's `completeDeliveredRange` establishes its scoped, synchronized,
clean, delivered local range. A narrower requested range may complete while a wider range from
another subscription remains partial. Snapshot END and zero event flags are not substitutes for
these conditions.

Unknown mapping, inactive source, reset, pause/callback retirement, publication refusal and
closing/retired owners have explicit states. Every noncurrent returned cut suppresses
`completeDeliveredRange`. Before the first publication, the serial is zero. A non-HISTORY
subscription returns `UNSUPPORTED`. Retained copies remain historical AS-OF values after source
changes and native retirement; they do not update themselves. A final cut means that the logical
registration and selected callback captures have actually retired.

## Bounds, range units and reset

`HistoryReceivingOptions` independently bounds shared points, retained frames/bytes, collector
resident backing, per-view pending updates and the two observation publication banks. Defaults are
4096 points, 256 frames, 64MiB shared frame backing, 128MiB resident collector backing, 128 pending
updates and 4MiB whole-source-owner backing per subscription, and 8MiB observation backing. These are logical reservations, not process
RSS or a wall-clock latency promise. Checked arithmetic and existing finite runtime capacities
reject incompatible profiles before activation. The whole outer worker turn shares one observation
work seed across all subscriptions and keys; no per-key credit refill occurs. Failed complete
publication yields explicit unknown evidence, never a partial vector with stale completion.

`requestedFrom` and `rawTime` are signed raw indexed-record coordinates. They are not Unix
milliseconds. Standard TimeAndSale binds physical Time/Sequence slots explicitly; its verified
Java delegate converts milliseconds into that record coordinate. General indexed requests use
their delegate's full-index bound. Logical time-series identity/capability remains distinct from
receiving availability, and live range mutation remains a separate contract.

Accepted disconnect/clear and desired changes invalidate current completion immediately through
detached facade fences. Fresh source completeness requires the real reset/mapping/ACK path and
callback retirement. Old immutable batches remain valid independently. No getter performs an ACK
or advances reconnection. The actual pinned Java oracle and native controlled-peer test cover the
common standard-schema receiver; a positive typed public business route must be verified separately.

See [the oracle](../../tools/upstream/history-receiver-oracle/README.md),
[logical time-series identity](TIME-SERIES-IDENTITY.md) and [statistics](CLIENT-STATISTICS.md).
