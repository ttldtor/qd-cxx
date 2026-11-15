# Kraken ticker decoder

The optional `qdcxx_kraken_decoder` component decodes bounded Kraken Spot WebSocket v2 ticker JSON for the configured first gateway profile. The caller owns connection lifecycle, verifies subscription requests, and supplies a single complete text message.

The decoder reports acknowledgement, heartbeat, system status, snapshot, and update. A data message contains one or more BTC/USD rows. Prices and sizes are parsed directly from decimal JSON tokens into QD wide decimal values. Values outside the exact representation are rejected. The provider timestamp is retained as text. Quote timestamps, sequence and exchange codes are zero.

Set `ORG_TTLDTOR_QDCXX_BUILD_KRAKEN_DECODER=ON` to build the internal component and enable its test when `BUILD_TESTING=ON`. The default is OFF. Acknowledgements carry an optional request ID and rejected requests carry their error text. Status messages carry the provider system state.

The maximum input is 64 KiB with at most 64 rows, 128 fields in each inspected object, and 32 nested levels. Missing fields, unexpected symbols, duplicate fields, malformed JSON and non-finite or unrepresentable numeric values are errors. A message is returned only if all its rows validate. Results own their data. This component does not open sockets.

## Kraken Quote gateway

`qdcxx-kraken-gateway` bridges the fixed public BTC/USD BBO subscription to
TICKER Quote on `127.0.0.1`. Use the opt-in gateway and Kraken source profiles
with plain Feed. Disable the separate QTP TLS profile for this executable. The
source uses the qualified private Beast/OpenSSL process profile and a local
trusted PEM file containing explicit CA certificate anchors. No account is
required.

```sh
qdcxx-kraken-gateway --help
qdcxx-kraken-gateway --trust-pem /path/to/ca.pem --duration-ms 30000 --trace-quotes
```

The gateway prints `LISTEN 127.0.0.1 PORT`. Connect plain QTP clients to the
printed port and subscribe to composite Quote with the exact symbol `BTC/USD`.
`--listen-port` defaults to 0, which lets the OS select the port. The first
observed demand starts the source. `--source-host`, `--source-identity` and
`--source-port` configure the route separately from the TLS reference identity.
The live defaults are `ws.kraken.com`, `ws.kraken.com` and `443`.
The trust PEM must be a nonempty regular local file of at most 512 KiB.

The gateway admits the newest available BBO per owner turn. It holds one
current-generation quote while the source remains active and re-admits it on
an observed transition from zero to positive demand. Source cutoff discards
this application-held quote. While downstream interest persists, the TICKER
collector may still deliver its previously admitted value to a late subscriber.
The first new snapshot replaces that value. The gateway never re-admits its
discarded copy. TICKER may conflate pending values and delivery is independent
for each client.

The optional `--trace-quotes` prints this line after local Publisher admission. Its synchronous output needs a reader:

```text
PUBLISH GENERATION TIMESTAMP BID_RAW ASK_RAW BID_SIZE_RAW ASK_SIZE_RAW
```

The four raw fields are signed QD wide-decimal words. The timestamp is provider
text with only the characters used by its RFC 3339 representation, or `-` when
it cannot be represented on one trace line. It is separate from Quote times,
which remain zero. Local admission and final `sent_frames` report local and
socket progress. Compare received quotes in the clients to establish application
receipt.

`--duration-ms` bounds admission to 1 through 600000 ms from listener startup.
The default is 30000 ms. Source cleanup finishes before the QTP listener closes
and may wait for an outstanding native DNS worker. Ctrl+C requests interruption.
The final line is `STATS observed=N published=N sent_frames=N retired_peers=N pending_publications=N`.
Exit status 0 requires at least one admitted publication. Status 1 indicates a
diagnostic or no publications, 2 invalid arguments and 130 interruption.

For verification, use the [deterministic loopback qualification](../../tests/conformance/kraken/README.md) or the [live two-client check](../../tests/conformance/kraken/LIVE.md).
