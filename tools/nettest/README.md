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

Run `--help` or `--help-ru` for all limits, defaults and exit codes. Help does
not read the schema bundle or start the runtime.
