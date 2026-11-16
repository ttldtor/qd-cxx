# Event STREAM fan-out comparison

[Русская версия](FANOUT.ru.md).

`Measure-EventFanout.ps1` runs the Event producer and Java QD 3.357 receiver
with one IBM TimeAndSale/STREAM subscription per client. By default it compares
publication batches of one and eight with 1, 2, 4 and 8 clients. Runs are
sequential. Each client confirms a probe before measurement and validates
every admitted sequence and its fields.

```powershell
pwsh -NoProfile -File tools/nettest/benchmark/Measure-EventFanout.ps1 `
  -Program $eventNettest -Java $java -Classpath $qd357Classpath `
  -OutputDirectory $newLogDirectory -Rate 1000 -Duration 10 -Repeats 3
```

Use `-Help` for defaults and limits, or `-HelpRu` for Russian help. Supply a
new output directory. The command starts producer and receiver processes and
loopback TCP sockets by default, compiles the Java verifier with the JDK, and
retains each run's stdout, stderr, `result.json` and `process-samples.csv`.
`-ListenAddress` selects a concrete local IPv4 address.

Set `-Clients` to distinct values from 1 through 8, `-Batches` to 1 or 8, and
`-Pacing` to `sleep`, `yield`, or an array containing both. Sleep is the
default. Yield can consume an extra CPU core and is intended for capacity
measurements. Retries between pacing modes are not directly comparable because
they poll capacity at different rates. Repeats reverse batch order on alternate
passes. Rate denotes offered source records per second.

`summary.csv` has one row per run. `admittedRecords` counts source records,
`deliveredRecords` counts validated client deliveries, and `sentDataFrames`
counts completed DATA frames across clients. `dataFramesPerDeliveredRecord`
divides frames by delivered records. `result.json` retains each client result.
`worstClientP95Ns` is the largest individual-client p95, with 1 µs histogram
buckets in the central range. Record age uses wall clocks on one host and can
be negative. Hardware clock accuracy is not calibrated.

CPU time is sampled in the active phase and summed across receiver processes.
The sum of each receiver's sampled peak working set or private commit combines
individual peaks that may occur at different times. Run on an otherwise idle
host for comparable results. Shared-host tests verify correctness without a
throughput threshold.

`-Help` and `-HelpRu` start no workload. Successful verification exits zero;
configuration, compilation, process, network, validation and output errors exit
nonzero. The underlying runner terminates its child processes. The output
directory and its evidence remain available after errors.

Source throughput counts admitted source records per configured producer second. Delivery throughput includes drain deliveries and uses the same producer window as its denominator.
