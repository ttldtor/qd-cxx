# Windows NetTest bundle

[Русская версия](README.ru.md).

The bundle runs a finite synthetic QTP workload and records the output of each
Java QD 3.357 receiver. Run it on an otherwise idle Windows x64 host. The bundle
contains a Java 21 runtime, the QD verifier jars, both native producers, their
Microsoft C++ runtime dependencies and the schema for Record mode. It can also
include PowerShell 7. Without it, install PowerShell 7.2 or later and make
`pwsh.exe` available on PATH.

Open PowerShell in the extracted directory and run:

```powershell
.\run.cmd -OutputDirectory .\results\run-1 -ProducerKind Event -Contract stream -Clients 4 -Rates 1000,10000,100000 -Duration 10
```

`run.cmd -Help` lists the parameters without starting a measurement. Each run
uses one source and the configured number of Java receiver processes. The
default endpoint is IPv4 loopback. Event mode supports STREAM and TICKER.
Record mode currently supports one STREAM receiver. Batch size is one. The
workload is synthetic and counts each receiver delivery in aggregate throughput.

The source rate is configured in records per second. It does not configure a
network link rate. The offered count for a single run must fit the 22-bit
sequence space. At high offered rates the source can report fewer admissions
than the nominal rate. Check individual producer and receiver fields alongside
the aggregate delivery rate.

For a capacity sweep use `-Pacing yield` in Event mode. It yields to the
scheduler while waiting for the next publication or retry.
It can consume an additional CPU core. Use the default `sleep` mode for
comparisons with paced baseline runs.
The retry count depends on how frequently the producer checks for capacity.

Every output directory must be new. It contains `baseline.json` with CPU load
sampled before the runs, available memory and adapter link speeds, one folder
per run, and `summary.csv`. Each run retains `result.json`, raw process
and thread samples, `sampling.json`, `metrics.prom`, and stdout/stderr. `-SampleIntervalMs 100` controls the nominal sampling interval (10..10000 ms). The JSON retains separate last observed live active memory, sampled peaks, OS lifetime peaks and observed-thread CPU intervals. Names are optional and thread roles are not inferred. Refer to `result.json` for per-receiver latency,
delivery, CPU and memory. STREAM validates every expected record for each
receiver. TICKER retains the latest value per subscribed key, so intermediate
updates may be conflated and the final value is checked.

Timestamp precision is reported in the individual results. Event records
carry nanoseconds where the schema supports them. Accuracy depends on both
platform clocks and their resolution. Measurements across machines require
independently verified clock synchronization.
The latency histogram uses 1 microsecond buckets from -10 ms to 100 ms and
1 millisecond buckets outside that range. Nanosecond units do not imply
nanosecond timing resolution.

## Monitoring

Docker with Compose can run the optional dashboard. From the bundle directory:

```powershell
docker compose -f .\dashboard\docker-compose.yml up -d
.\run.cmd -OutputDirectory .\results\with-metrics -ProducerKind Event -Contract stream -Clients 4 -Rates 1000,10000 -Duration 30 -PushgatewayUrl http://127.0.0.1:9091
```

Grafana is at <http://127.0.0.1:3000> with the provisioned **qd-cxx NetTest**
dashboard. Prometheus is at <http://127.0.0.1:9090>. The runner publishes a
snapshot when each run completes. Prometheus scrapes it every five seconds.
Use the retained JSON and CSV as the detailed per-run results. `metrics.prom` is exactly the posted snapshot; CPU gauges contain sampled-window deltas, so the dashboard uses them directly. Thread intervals can differ, and missing observations are NaN rather than zero. Repeated runs replace the job snapshot. Docker pulls
the pinned images if they are absent. Stop the services with:

```powershell
docker compose -f .\dashboard\docker-compose.yml down
```

Prometheus and Grafana data remain in named Docker volumes after this command.

## Physical network

Loopback tests cover the QTP application path on one host. To measure the
physical network, run a separately obtained `iperf3` on each host, record the
adapter speeds and collect both host results. For example:

```powershell
# Receiving host
iperf3.exe -s
# Sending host, in separate runs
.\link.cmd -Iperf C:\path\iperf3.exe -Server RECEIVER_ADDRESS -OutputDirectory .\results\link -TargetsMbps 100,1000,0 -Duration 30 -Adapter Ethernet
```

The wrapper exercises one TCP connection by default. Each positive target
sets an offered bit rate, and `0` uses an uncapped TCP source. Supply
`-Protocol udp` with positive targets for a UDP sweep. `-Parallel` sets the
number of connections and each target applies to each connection. The optional
adapter captures its counters before and after each run. Traffic selection
follows the operating system routing table. Confirm the installed iperf3 version
supports `-b` and emits sender and receiver summaries under `end.sum_sent` and
`end.sum_received` in JSON. UDP additionally requires receiver loss and jitter
in `end.sum_received`. Controlled
bandwidth and latency experiments require a network emulator or shaping
configuration external to the bundle. Keep its settings with the results.
The QTP producer/receiver sweep and the physical network test report different
units and must be interpreted separately. A single synthetic record does not
represent a measured OPRA message.
The current source accepts at most 1,000,000 offered records/s and fewer than
2^22 records in one run. OPRA's April 2026 notice describes a July test target
of 625,000 messages in 100 ms on an output line, equivalent to 6.25 million/s
in that interval. This bundle cannot reproduce that line target with its
current source limits. [OPRA capacity notice](https://cdn.opraplan.com/documents/notices/OPRA_Capacity_Test%20Reminder_041126.pdf)

## Bundle assembly

The developer runs `Create-Bundle.ps1 -Help` from the repository and supplies
the built executables, matching schema and provenance, QD 3.357 verifier jars,
QD license, Microsoft C++ redistributable DLLs, and JDK 21. JDK `jlink` creates
the smaller Java image, including `javac`, which the runner uses to compile
the bundled verifier. Pass `-PowerShellDirectory` to include PowerShell 7.
The manifest lists SHA-256 hashes of benchmark inputs. The destination is
created once and is never overwritten.
