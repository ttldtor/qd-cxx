# Synthetic peer EndpointId witnesses

These complete type1 envelopes use deterministic synthetic UID bytes and names. They contain no live
endpoint identity, credentials or market data. Their descriptor property is `witness=synthetic-endpoint`.
The base descriptor payload is 34 bytes. `files.json` records exact immutable binary identities.

QD 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, produced the envelopes with
`ProtocolDescriptor.composeTo`, `EndpointId.readEndpointId`/`writeEndpointId` and `ByteArrayOutput`.
The official parser accepted nine complete cases, rejected seven malformed cases and left one
unconsumed byte for each of the two extra-tail cases. The selected native profile rejects such
remaining bytes explicitly. These are factual protocol witnesses, not a claim of Java test-suite coverage.

| Case | Official outcome | Selected native expectation |
| --- | --- | --- |
| absent | Complete, no endpoint | PROTOCOL event, unchanged old grammar |
| null | Complete, explicit null endpoint | PROTOCOL event |
| present | Complete, UID 16/name `synthetic-peer`/counter 0 | PROTOCOL event |
| empty-uid-name-min | Complete, UID 0/empty name/INT64_MIN | PROTOCOL event |
| null-name-max | Complete, UID 16/null name/INT64_MAX | PROTOCOL event |
| utf16-name | Complete, synthetic name including NUL and an unpaired surrogate | PROTOCOL event |
| long-uid | Complete, UID 256/name `synthetic`/counter -1 | PROTOCOL event within frame/work limits |
| long-name | Complete, name of 64 `n` units | PROTOCOL event, or explicit text RESOURCE_LIMIT |
| frame-excess | Complete, UID 8192 | RESOURCE_LIMIT with the 8192-byte test frame cap |
| invalid-uid-negative | IOException, UID length -2 | MALFORMED |
| invalid-uid-overflow | IOException, UID length INT_MAX+1 | MALFORMED |
| truncated-uid-length | EOFException | MALFORMED |
| truncated-uid | EOFException | MALFORMED |
| truncated-empty-uid | EOFException after UID length 0 | MALFORMED |
| truncated-name | EOFException | MALFORMED |
| truncated-counter | EOFException | MALFORMED |
| extra-after-present | One trailing byte | UNSUPPORTED |
| extra-after-null | One trailing byte | UNSUPPORTED |

The null read context uses ordinary byte-array lengths -1..INT_MAX. It permits zero-length UID;
the separate JVMId ReadContext/reference restrictions do not apply. A nonnull UID is followed by
nullable UTFString and a signed compact-long counter. No endpoint field is a routing guarantee.
