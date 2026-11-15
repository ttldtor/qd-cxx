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
