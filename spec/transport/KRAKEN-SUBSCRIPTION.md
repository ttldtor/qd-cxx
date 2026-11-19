# Kraken BTC/USD source subscription

[Русская версия](KRAKEN-SUBSCRIPTION.ru.md). English is authoritative.

The private source drives [one qualified WSS attempt](BEAST-WSS-SESSION.md) on its owner thread.
After verified upgrade it sends one Kraken Spot WebSocket v2 ticker subscription for BTC/USD
with `event_trigger=bbo` and `snapshot=true`. The public channel requires no account or key.
The route, DNS identity, trust certificate bytes and retry policy are explicit.

The request ID is the attempt generation, beginning at 1 and limited to positive signed
32-bit IDs. Each new attempt uses a new ID. The source admits delivery only after Kraken echoes
that ID in a successful ticker acknowledgement and sends the initial snapshot. Either can arrive
first. An early snapshot occupies one bounded decoder result; subsequent pre-admission updates
and duplicate snapshots are ignored. A rejected or mismatched acknowledgement ends the attempt.
Once admitted, each valid update refreshes quote freshness and can be read after the initial
snapshot. Status and heartbeat traffic carry no observations and do not refresh the quote.

The driver retains at most one decoded quote message for delivery. The caller takes owned rows
in message order and supplies work slices on the owner thread. While rows are pending after
admission, the source applies backpressure to WSS reads while continuing to enforce freshness.
The monotonic freshness deadline uses the time at which the source handled the complete WSS
message. Provider timestamps remain exact decoder text. Timeout, invalid input, disconnect and
stop cut off delivery and discard every undelivered row. A caller may retain previously transferred
rows and use the attached generation to distinguish them.

The source reads ACK and snapshot within the same 10-second default admission deadline.
The default freshness interval is 30 seconds. Each transport setup stage and the subscribe
write have a 5-second default deadline. Retry delays double from 250 ms up to 5 seconds by
default. At most eight consecutive failed attempts are allowed, with the delay and count reset
after successful admission. The corresponding settings accept positive durations up to
five minutes and a failure limit from 1 to 64. Invalid local inputs, policy and resource failures
can terminate without retry. The last failure retains its attempt ID and available native or
decoder diagnostic. Stop prevents retries and allows the owner to observe transport retirement.
A pending native resolver may delay retirement as explained in the WSS ownership contract.

To build the private source in the root project, set
`ORG_TTLDTOR_QDCXX_BUILD_KRAKEN_SOURCE=ON`, keep `ORG_TTLDTOR_QDCXX_BUILD_TLS=OFF`, and supply
the two explicit pinned source and OpenSSL paths from
[the Beast profile](../../cmake/beast-wss-profile/README.md). The Kraken WSS source owns the
process OpenSSL image, so enabling the QTP TLS runtime in the same root build is rejected.
The source build enables the existing optional Kraken decoder.
The source and its headers are private to the root build,
and the standalone Beast dependency package keeps its existing installation. With testing
enabled, run `ctest --test-dir <build> -R 'kraken_(subscription|ticker_decoder)'`. The subscription
check uses synthetic responses over a verified local TLS and WebSocket connection.

[Kraken WebSocket v2 ticker reference](https://docs.kraken.com/exchange/api-reference/spot-websocket-v2/ticker)

## BBO values and continuity

The [Kraken ticker reference](https://docs.kraken.com/exchange/api-reference/spot-websocket-v2/ticker)
defines level 1 data and the `bbo` trigger for changes at the best bid or offer. This source
requests only BTC/USD. For each accepted row, `bid`, `ask`, `bid_qty` and `ask_qty`
become Quote `bidPrice`, `askPrice`, `bidSize` and `askSize`, respectively, with symbol
`BTC/USD`. Bid and ask prices are in USD per BTC, and quantities are in BTC; the source units are preserved without conversion. Numeric JSON values must be nonnegative and exactly representable as finite QD
wide decimals. The decoder does not round them. The other ticker fields do not become Quote
values.

Kraken describes the row timestamp as a ticker-data timestamp. The decoder retains its
nonempty text, up to 64 bytes, without interpreting it as either side's update time.
The published Quote has zero bid and ask timestamps, sequence and exchange codes. The gateway's
optional trace can include the retained timestamp; it is not delivered as a Quote timestamp.
The freshness clock is local and monotonic and does not compare provider timestamps.

The decoder rejects malformed JSON, unsupported message shapes or symbols, missing or wrongly
typed required fields, duplicate checked keys, and invalid or inexact decimals. A failed decode
returns no partial rows and ends the source attempt.

The decoder uses fixed implementation bounds; they are not configuration settings or Kraken
protocol limits. The [decoder implementation](../../src/kraken/ticker_decoder.cpp) declares
them and the [decoder interface](../../src/kraken/ticker_decoder.hpp) defines the errors.

| Input or retained value | Bound | Rejection |
| --- | --- | --- |
| Complete JSON message | 65,536 bytes | `RESOURCE_LIMIT` |
| JSON parser nesting | Parser configured with depth 32 | Parse failure becomes `MALFORMED_JSON`; parser allocation failure returns `RESOURCE_LIMIT` |
| Ticker data rows | 1–64 rows per message | Empty: `INVALID_MESSAGE`; over 64: `RESOURCE_LIMIT` |
| Direct fields in each checked object | At most 128 fields in the root, acknowledgement result, status row or ticker row | `INVALID_MESSAGE` |
| Decoded ticker timestamp | 1–64 bytes per row | `INVALID_MESSAGE` |
| Decoded rejection error or status system text | 1–256 bytes | `INVALID_MESSAGE` |

These are decoder limits; the [WSS session profile](BEAST-WSS-SESSION.md) separately bounds
transport messages. The source sets each WSS receive operation deadline to five minutes.
A transport failure or that deadline ends the attempt. The source does not run an independent
heartbeat deadline: the admission deadline requires both the matching acknowledgement and a
snapshot, and the configurable quote-freshness deadline applies after admission.

A disconnect, decode error, rejection, acknowledgement mismatch or timeout cuts off delivery;
the source discards unconsumed rows and retries when its policy permits. A new attempt requests a
new snapshot and requires its own matching acknowledgement. The snapshot establishes current
values for that attempt; it does not recover updates missed during the interruption. The
generation identifies attempts inside the source and trace, but is not a Quote sequence number.
The source detects these local failures and freshness expiry; it cannot identify every missing
market update or prove continuity from ticker values or timestamps. A quiet market can also
expire the freshness deadline. Kraken's [heartbeat](https://docs.kraken.com/exchange/api-reference/spot-websocket-v2/heartbeat)
indicates connection activity, while heartbeat and status messages do not refresh quote freshness.

The gateway drains available source rows and publishes the latest one for a demanded symbol.
QTP TICKER may combine intermediate updates, so publication does not promise delivery of every
source observation to each client. Previously delivered values cannot be retracted on a source
cutoff; consumers must account for interrupted or stale data. This gateway does not publish a
gap marker or an end-to-end continuity guarantee.

BBO describes the best bid and offer and their quantities. It cannot reconstruct deeper levels,
individual orders or a complete order book. Kraken's separate [level 2 book channel](https://docs.kraken.com/exchange/api-reference/spot-websocket-v2/book)
provides aggregated price levels; this gateway does not subscribe to it.
