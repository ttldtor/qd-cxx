# NetTest reference points for record and event publication

## Purpose

Use these tools to compare publication paths at distinct layers. A result is meaningful only when the record or event type, collector contract, symbols, batch size, offered rate, connection count, duration, and consumer load are aligned. The tools do not measure the same work by default.

## QD NetTest

QD NetTest publishes physical QD records. Its producer fills a `RecordBuffer` with generated records and passes the buffer to each selected collector's `QDDistributor.processData`. It selects one record for a run through the `com.devexperts.qd.tools.NetTest.record` system property, which defaults to `TimeAndSale`. This is the record-level compatibility reference. A run covers one record type, not the full schema catalog. [QD 3.355 producer source](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestProducerDistributorThread.java), [QD 3.357 producer source](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestProducerDistributorThread.java), [QD 3.357 record selection](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestSide.java)

The common command supports producer and consumer modes (`p` or `c`), an address, instance count (`-C`), symbol configuration (`-S`), collector selection (`-c`), wildcard subscription (`-w`), rate limiting (`-r`), statistics period (`-s`), and output name (`-n`). The record batch defaults to 100 and can be set with the `com.devexperts.qd.tools.NetTest.batchSize` system property. The default collector is ticker and the default statistics period is 10 seconds. [QD 3.357 CLI](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTest.java)

QD 3.357 preserves the 3.355 NetTest producer, CLI, record selection, and latency-field discovery in the compared core files. Its consumer adds default-on object-field unwrapping, controlled by `com.devexperts.qd.tools.NetTest.unwrapObjects` and defaulting to `true`. This adds object decoding work on the consumer when records contain object fields. It does not change the producer's record encoding path. [QD 3.355 consumer source](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestConsumerAgentThread.java), [QD 3.357 consumer source](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestConsumerAgentThread.java)

The consumer computes lag from a record's time and sequence fields when it recognizes that layout. Its `TimeAndSale` path combines whole seconds with the high bits of `Sequence`, giving millisecond resolution. A record without recognized time fields reports zero lag. Keep the selected record and collector the same when comparing consumer lag. [QD 3.357 latency calculation](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestConsumerAgentThread.java)

The producer lag timer includes rate-limit waiting and record generation. Measure publication-call time separately. [QD 3.357 producer](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestProducerDistributorThread.java)

## dxFeed event publication

The dxFeed Graal Native SDK exposes the dxFeed API through a native interface. Measure it as an event-facing API path. The public SDK description does not define a benchmark command line, so CLI details belong in the implementation's own documentation when that implementation is public. [Graal Native SDK](https://github.com/dxFeed/dxfeed-graal-native-sdk)

The Java dxFeed API publishes event objects through `DXPublisher.publishEvents`. A publisher endpoint can use the `PUBLISHER` or `STREAM_PUBLISHER` role. This is a Java event-facing API path, distinct from calling QD's record distributor directly. [DXPublisher API](https://docs.dxfeed.com/dxfeed/api/com/dxfeed/api/DXPublisher.html), [DXEndpoint API](https://docs.dxfeed.com/dxfeed/api/com/dxfeed/api/DXEndpoint.html)

These event API measurements do not establish byte-for-byte wire equivalence with QD NetTest. The Java event API and the C API have separate runtime and publication boundaries. Compare their throughput only after aligning workload and consumer contract, and report each API's own publish-call and consumer measurements separately. [Graal Native SDK](https://github.com/dxFeed/dxfeed-graal-native-sdk), [DXPublisher API](https://docs.dxfeed.com/dxfeed/api/com/dxfeed/api/DXPublisher.html)

## Compatibility and measurement checklist

- Keep QD 3.355 as the locked compatibility baseline. QD 3.357 is a source-verified additional reference at commit `052e916c842026cf23a8dcf2067b83cbcc929b88`; this comparison does not require changing the 3.355 lock. [QD 3.355 tag](https://github.com/devexperts/QD/tree/qd-qds-3.355), [QD 3.357 tag](https://github.com/devexperts/QD/tree/qd-qds-3.357)
- Use QD NetTest for direct record-path interoperation. Set the same record explicitly on producer and consumer, and choose a collector contract that carries that record.
- Treat QD's `batchSize` system property as a record count. For each event API, verify what its configured batch counts, then align the actual number of records produced per batch before comparing rates.
- Separate producer calls, records accepted by the API, bytes written, records delivered to the consumer, and consumer lag. A publisher's local rate alone does not show successful delivery.
- Ticker may conflate repeated updates. Use a stream-style contract when the comparison requires every publication to reach the consumer.
- QD NetTest consumer lag is millisecond-resolution for the default time/sequence layouts. Do not compare it as sub-millisecond end-to-end latency.
- A full-catalog record test requires a dedicated scenario that iterates every record and validates both directions. QD NetTest itself selects a single record per run.
