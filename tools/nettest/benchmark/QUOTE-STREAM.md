# Synthetic Quote STREAM benchmark

[Русская версия](QUOTE-STREAM.ru.md).

Build `qdcxx_quote_stream_nettest` with the public Feed API enabled. The tool
publishes physical Quote records through the STREAM collector. Its inputs are
port, total records, offered records per second, and batch size (1–8). Rate
zero submits as fast as local admission permits. Start the publisher first.

~~~sh
qdcxx-quote-stream-nettest 7300 250000 50000 8
javac -cp "$QD_CLASSPATH" -d classes tools/nettest/benchmark/StreamReceiver.java
java -cp "classes:$QD_CLASSPATH" StreamReceiver 127.0.0.1:7300 IBM quote-stream ns probe
~~~

On Windows use a semicolon as the classpath separator. Supply the QD 3.357
classpath. After `READY` from the publisher and `PROBE_RECEIVED` from the
receiver, send `GO` to the publisher on stdin. After the publisher prints
`RESULT`, write the decimal expected count followed by a newline to the
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

The receiver's `--self-test-quote-stream` checks positive and damaged records
through a local Java QD STREAM collector. Count only exact deliveries when
estimating capacity. Compare both the publisher's active rate and the received
rate, and record the batch size and per-process CPU and memory.
