# Kraken ticker decoder

The optional `qdcxx_kraken_decoder` component decodes bounded Kraken Spot WebSocket v2 ticker JSON for the configured first gateway profile. The caller owns connection lifecycle, verifies subscription requests, and supplies a single complete text message.

The decoder reports acknowledgement, heartbeat, system status, snapshot, and update. A data message contains one or more BTC/USD rows. Prices and sizes are parsed directly from decimal JSON tokens into QD wide decimal values. Values outside the exact representation are rejected. The provider timestamp is retained as text. Quote timestamps, sequence and exchange codes are zero.

Set `ORG_TTLDTOR_QDCXX_BUILD_KRAKEN_DECODER=ON` to build the internal component and enable its test when `BUILD_TESTING=ON`. The default is OFF. Acknowledgements carry an optional request ID and rejected requests carry their error text. Status messages carry the provider system state.

The maximum input is 64 KiB with at most 64 rows, 128 fields in each inspected object, and 32 nested levels. Missing fields, unexpected symbols, duplicate fields, malformed JSON and non-finite or unrepresentable numeric values are errors. A message is returned only if all its rows validate. Results own their data. This component does not open sockets.
