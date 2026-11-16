# Native verification of TimeAndSale/STREAM

[Русская версия](NATIVE-VERIFIER.ru.md).

The Record NetTest consumer accepts --verify-stream for one IBM subscription
with the default physical TimeAndSale layout. Start the source before the
consumer. Both use the same schema bundle.

~~~powershell
# Source host. Set the bind address to the source test NIC.
qdcxx-nettest p --bundle C:/path/to/default --address SOURCE_IPV4 --port 7301 --record TimeAndSale --contract stream --symbol IBM --instances 1 --duration 10 --rate 100000 --batch 32
# Receiver host. Use the reachable source address and a longer timeout.
qdcxx-nettest c --bundle C:/path/to/default --address SOURCE_IPV4 --port 7301 --record TimeAndSale --contract stream --symbol IBM --instances 1 --duration 30 --verify-stream
~~~

ACTIVE marks the first verified record. After the source closes successfully,
the receiver prints RESULT and a separate decimal CHECKSUM. Accept the run if
both processes exit successfully, source written equals receiver delivered,
and the run logs contain the checksum. The verifier validates the fields and
sequence as it reads the records. It excludes timestamps from the checksum
so that repeated runs with the same delivered prefix can be compared.
Use the Java QD verifier on the same controlled Record NetTest scenario as
a separate compatibility check.

On two idle hosts, record the build and schema hashes, OS, CPU, NIC link
speeds, route and TCP round-trip time. Run the physical [link test](windows/README.md)
separately. Preserve both outputs including generated, composed, written,
wireRecords and delivered, CPU and memory per process, and adapter counters.
To report end-to-end latency between hosts, measure clock offset and
uncertainty using a verified synchronization method. The CLI timeout and
RESULT window have different meanings from one-way transport latency.

The verifier follows the 22-bit sequence across wrap using the full record
ordinal, with a limit of 3,600,000,000 records per run. Verified mode omits
consumer heartbeats and targets the synthetic Record NetTest source.

## Producer stage measurements

Add --profile-producer to print a separate PROFILE line. generationNs,
composeNs and writeNs are monotonic elapsed nanoseconds summed over DATA
batch generation, frame composition and individual socket write calls.
The line counts generated batches, composed frames, write attempts,
would-block results and partial writes. The timers include call overhead
and may influence throughput at small batch sizes. For absolute throughput,
repeat the same offer with the default producer and compare both exact
delivery receipts. Idle time, subscription traffic and time in the receiver
are outside the producer stage totals.
