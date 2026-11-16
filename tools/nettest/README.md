# Record NetTest

[Русская версия](README.ru.md).

`qdcxx-nettest` runs a finite synthetic QTP workload using a physical record from a
binary-v2 schema bundle. It runs either a producer with a plain TCP listener or a
consumer with outbound plain TCP connections.

Build it with the TCP runtime and the native JSON loader. Supply a directory that
contains the paired `schema.json` and `provenance.json` from the same
[export](../schema-export/README.md). The installed SDK can contain the default and
dxlink bundles under `share/qdcxx/schemas`. The tool reads the files before opening
sockets. The limits are 1 MiB for the schema and 4 MiB for its provenance.

```sh
qdcxx-nettest p --bundle /path/to/default --port 7301 --duration 10 --rate 1000 --batch 10
qdcxx-nettest c --bundle /path/to/default --port 7301 --duration 9 --qd-text
```

The producer prints its listening port as `PORT NUMBER`; use port 0 to accept an
OS-assigned port. The consumer needs a nonzero port. Both use IPv4 loopback, the
physical record `TimeAndSale`, the STREAM contract and the symbol IBM by default.
`--address` accepts a numeric IPv4 or IPv6 address. `--record` and `--symbol`
accept exact UTF-8 selectors. Repeat `--symbol` to distribute offered load round
robin over up to 64 symbols, each at most 256 UTF-16 units. In producer mode only
symbols requested by a peer receive data. `--instances` is a limit on simultaneous
peers for a producer and the number of simultaneous connections for a consumer.

Producer `--rate` is the offered number of records per second per peer. `--batch`
caps records per DATA frame, subject to a layout-dependent frame budget. Use
`--batch 32` or less when using a relay that admits at most 32 rows per frame.
`--startup` limits startup separately from `--duration`; the producer's window begins
with its first eligible demand, and the consumer's window begins when all local ADD
writes have completed. The producer drains already composed DATA for up to two
seconds after the run. Native TCP write acceptance does not imply remote delivery.

Set `--contract ticker`, `--contract stream` or `--contract history` on both sides.
Use STREAM to compare individual occurrences with QD NetTest. HISTORY requires an
indexed record and uses a signed raw `--history-bound` on the consumer; its default
is zero. The producer applies the raw bound to the record's exported indexed positions
and marks the first eligible synthetic update as a one-row snapshot when `hs` is
negotiated. It does not retain a historical dataset. Synthetic numeric and text fields
use their exact wire types; Java serialization fields use null.

`RESULT` reports generated rows, rows composed into DATA, rows in fully written DATA
frames, validated wire occurrences at the consumer, and delivered selected records.
Producer byte counts include negotiation and the final drain. Consumer byte and
record counts cover its measurement window. Irrelevant counts are zero. Producer
`connections` counts accepted sockets cumulatively; consumer `connections` counts
established sockets. `rps` uses written records for the producer and delivered
records for the consumer. With `--qd-text`, an additional aggregate line follows
QD's `data … rps` wording and explicitly marks lag unavailable. Java producer lag
and consumer lag have different definitions. [NetTest comparison](../../docs/NETTEST-REFERENCE.md)

## Checks with Java QD

Run `Test-Loopback.ps1` with PowerShell 7, the built executable and a default
bundle. It creates a new log directory and runs native producer and consumer
processes over loopback for all three contracts. Pass an explicit JDK 8
executable and a classpath from the pinned QD 3.355 source build to add the two
Java interoperation directions. Java NetTest itself is long-running; the script
terminates its own Java process after the finite native counterpart completes.

```powershell
pwsh -NoProfile -File tools/nettest/Test-Loopback.ps1 -Program $nettest -Bundle $defaultBundle `
  -Java $java -Classpath $qd355Classpath -OutputDirectory $newLogDirectory
```

The Java overlap uses STREAM, one connection, `TimeAndSale`, a synthetic IBM IPF,
and batch size 10. Java's consumer reports observation rates, while local C++
producer counts remain exact frame-boundary counts. Full catalog wire round trips
are covered independently by [the schema exporter comparison](../schema-export/README.md).

## Measure verified STREAM delivery

`benchmark/Measure-Stream.ps1` runs the Record NetTest producer and a Java
QD 3.357 record consumer on IPv4 loopback. Build the producer in Release and
provide a [QD 3.357 classpath](https://github.com/devexperts/QD/tree/qd-qds-3.357)
with `qds.jar`, `dxlib.jar` and `dxfeed-api.jar`. Use a JDK with `javac` next to
`java`. The runner disables QD's monitoring endpoint builder, so measurement
uses the ordinary STREAM collector.

```powershell
pwsh -NoProfile -File tools/nettest/benchmark/Measure-Stream.ps1 -Program $nettest `
  -Bundle $defaultBundle -Java $java -Classpath $qd357Classpath -OutputDirectory $newLogDirectory
```

The fixed workload is one IBM `TimeAndSale` STREAM subscription. Defaults
are ten seconds, 1,000 offered records/s and one record per batch. The
receiver checks the physical record and generated sequence. The run succeeds
when the generated, composed, fully written and independently delivered
counts agree and the receiver reports no validation errors. Workloads must
fit the 22-bit sequence range so each record has a unique sequence number.

`result.json` includes counts, records delivered including drain divided by the producer window,
receiver latency quantiles and CPU and memory summaries for each process.
`process-samples.csv` includes the raw periodic samples. Latency is the
signed same-host wall-clock difference from the timestamp in the generated
record to its read by the QD verifier, at millisecond resolution. It includes
the QD decoding and delivery path. CPU is the
delta between the first and last live samples in the active window.
Memory peaks are sampled working set and private bytes. On Windows, private
bytes measure private committed memory and working set measures resident pages.
The report gives separate CPU and memory figures for the Record NetTest source
and the QD verifier.

The measurement runner accepts `-Help` and `-HelpRu` without starting a run.

For exact STREAM receipt checks with the native consumer, see the [native verifier](benchmark/NATIVE-VERIFIER.md).

### Run the event producer

Build `qdcxx_event_nettest` with the public Feed API enabled. The executable
`qdcxx-event-nettest` uses the built-in mixed publisher profile and sends one
`TimeAndSale` per `Publisher::publish()` call. The Java verifier checks
the same sequence and ten common record fields in both modes. The built-in
event profile also sends the sub-millisecond part in `TimeNanoPart`.
The wire schemas therefore differ.

```powershell
pwsh -NoProfile -File tools/nettest/benchmark/Measure-Stream.ps1 -Program $eventNettest `
  -ProducerKind Event -Java $java -Classpath $qd357Classpath -OutputDirectory $newLogDirectory
```

Event mode uses batch 1 and the built-in schema. The producer waits for exact
demand and reports local admissions and public publisher drain observations.
The `drainMs` counter measures queue drain after the last admission.
The Java verifier reconciles received records against the admission count.
Record mode reconciles against fully written records. Use `-Batch 1` for both
runs when comparing one publication per call.

When the publisher's input queue is full, the event producer retries the same
event with its original timestamp, through a bounded completion deadline.
No new events are generated after the configured window. The report shows
admission retries and the difference between nominal load and completed
publications separately from the receiver's exact-delivery check.

Pass `-Clients 2`, `-Clients 4`, or `-Clients 8` to start separate Java QD
receivers. A sequence-zero probe confirms each subscription before timing
starts. STREAM requires exact sequence and count at each receiver. Add
`-Contract ticker` for `Quote/TICKER`. This mode verifies increasing observed
sequences, validates fields, and requires the last admitted generation to
reach every receiver. It reports how many intermediate updates were observed.
The verifier serializes its TICKER ingress and observation calls because the
Java collector exposes a cursor over mutable record storage. TICKER age
includes this synchronization.

Event uses epoch timestamps with `TimeNanoPart` by default. The output includes
signed record age in nanoseconds and the histogram's 1 µs central bucket width.
Clock precision depends on the host. Record mode uses millisecond timestamps.
The multi-receiver result lists latency, CPU and memory per process and
distinguishes source records/s from aggregate delivered records/s.

For a portable Windows sweep, per-process receipts, and a separate physical
link test, see the [Windows NetTest bundle](benchmark/windows/README.md).

The [synthetic burst corpus](benchmark/burst/README.md) provides deterministic
payloads and a precomputed release schedule for burst measurements.

For `qdcxx-nettest`, run `--help` or `--help-ru` for all limits, defaults and exit codes. Help does
not read the schema bundle or start the runtime.
