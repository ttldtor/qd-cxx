# Synthetic Quote STREAM benchmark

[Русская версия](QUOTE-STREAM.ru.md).

The [metric definitions](METRICS.md) specify submission and delivery boundaries,
clock requirements, histogram rounding and process resource measurements.

Build `qdcxx_quote_stream_nettest` with the public Feed API enabled. The tool
publishes physical Quote records through the STREAM collector. Its inputs are
port, total records, offered records per second, and batch size (1–8). Rate
zero submits as fast as local admission permits. Start the publisher first.
Pass `--stream-bytes TARGET HARD` after these four arguments to enable the
byte-limited STREAM profile. TARGET is the encoded payload threshold and HARD
is the complete QTP frame limit in bytes. Omitting the option retains the
existing profile. The generator's BATCH remains independent of both limits.
`--clients N` selects 1 to 8 consumers. `--pending-records N` sets the input
slots and `--delivery-bytes N` sets the per-peer byte-mode delivery window.

~~~sh
qdcxx-quote-stream-nettest 7300 250000 50000 8
javac -cp "$QD_CLASSPATH" -d classes tools/nettest/benchmark/StreamReceiver.java
java -cp "classes:$QD_CLASSPATH" StreamReceiver 127.0.0.1:7300 IBM quote-stream ns probe
~~~

For example, use `qdcxx-quote-stream-nettest 7300 250000 50000 8
--stream-bytes 8000 65536` to compare byte-limited output with the same
eight-record generator calls.

On Windows use a semicolon as the classpath separator. Supply the QD 3.357
classpath. After `READY` from the publisher, wait for `PROBE_RECEIVED` from
every receiver. Send `PROBE` to repeat the probe if one has yet to confirm
demand. Send one `GO` to start measurement. After the publisher prints
`RESULT`, write the expected count, followed by a newline, to each
receiver's stdin. Another publisher can stay connected until
`COUNT_REACHED N` and then close. The receiver checks the final count after
disconnect, including any extra deliveries.

The synthetic record uses IBM, bid price equal to the ordinal starting at 1,
ask price one higher, bid exchange B, ask exchange A, and ask size 2. Bid and
ask time both hold seconds on the shared monotonic clock; bid size holds the
microsecond remainder plus 1. Ordinal zero is reserved for the startup probe.
This encoding requires a common machine clock, such as QPC with the verified
Windows toolchains. The latency histogram measures generation to Java receipt.
Record negative samples and verify the clock domain before comparing p99.
The receiver verifies all eight Quote fields and the exact order.
The publisher's `sourceIngressFrames` and `sourceIngressRecords` count complete
source events and records admitted to the STREAM collector, including probes.
They are read after drain, outside the publication timer. Comparing their ratio
with the generator batch size shows how ready publications are combined before
collector processing; these counts describe input, independently of network frames.

The publisher's `streamFrames` result counts completely sent STREAM DATA
frames across clients, including startup probe frames. Subtract the reported
`probeFrames` for the measured stream. `probeRecords` counts probe records
published before `GO`. Byte mode also reports
`maxFrameRecords` and `maxFrameBytes` for completed STREAM DATA frames, and
`failedFrames` for frame composition failures that retire a peer.
These publisher counters are separate from the receiver's delivery checks.

The receiver's `--self-test-quote-stream` checks positive and damaged records
through a local Java QD STREAM collector. Count only exact deliveries when
estimating capacity. Compare both the publisher's active rate and the received
rate, and record the batch size and per-process CPU and memory. Verify each
receiver's count, fields, order and p99 before comparing fan-out rates.

## Java QD reference publisher

Compile the publisher and use the same receiver with Java QD 3.357 on the
classpath. Run the two commands in separate terminals.

~~~sh
javac -cp "$QD_CLASSPATH" -d classes tools/nettest/benchmark/QuoteStreamPublisher.java tools/nettest/benchmark/StreamReceiver.java
java -cp "classes:$QD_CLASSPATH" QuoteStreamPublisher 7300 250000 50000 8
java -cp "classes:$QD_CLASSPATH" StreamReceiver 127.0.0.1:7300 IBM quote-stream ns probe
~~~

The optional `--clients N` sets the Java server maximum connections (default 1);
N must be a positive integer. Start that many receivers. The Java publisher
binds loopback, waits for the exact Quote subscription,
and submits physical records with a reused `RecordBuffer` through
`QDDistributor.process`. After `READY`, send `PROBE` as needed until every
receiver reports `PROBE_RECEIVED`, then send `GO` to the publisher. After
`RESULT`, send the expected count to each receiver. After every receiver
reports `COUNT_REACHED N`, send `CLOSE` to the publisher. Wait for all
processes to exit and check every receiver's final result.

For a comparison, alternate the publisher order and keep the receiver build,
record count, symbol, pacing and API batch size fixed in each round. An API
batch limits a submission. The STREAM collector and QTP writer can use
different outbound frame sizes. At rate zero report the observed exact-delivery
throughput. At a specified rate verify whether the publisher sustained that
rate in addition to checking delivery.
