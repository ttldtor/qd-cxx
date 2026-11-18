# NetTest metric definitions

[Русская версия](METRICS.ru.md).

Use these definitions when comparing record publishers, public event publishers
and Java or .NET implementations. A result identifies its workload, observation
point and interval. Keep the raw counts alongside calculated rates so that a
reader can reproduce the calculation.

The rules below define the comparison contract. The final section maps the
existing tools to it and identifies observations that they do not yet export.
Existing output field names and wire formats remain unchanged.

## Counts and rates

A source record is one generated update before distribution to clients. A
delivery is one update observed by one client. Declare the event-to-record
mapping when comparing an event API with physical records; one event may require
several records. Count records, events, QTP frames and bytes separately.

| Observation | Count | Completion boundary |
| --- | --- | --- |
| Offered | Unique updates presented to the submission API | First submission attempt; retries retain the same identity |
| Admitted | Updates accepted into the publisher's responsibility | Successful admission; asynchronous APIs must name the accepting completion |
| Encoded | Record occurrences in completed DATA frames | Successful frame composition, before socket writes |
| Written | Record occurrences in fully written DATA frames | The local transport has accepted every byte of that frame |
| Delivered | Updates observed at a named receiver boundary | The verifier or application callback has inspected the update |

The configured rate is a requested workload intensity, in source records/s.
At rate zero in an unpaced profile, report the actual offered and admitted
rates. Zero denotes unpaced submission in that profile.

For each measured rate, store the count, start and end observations, duration in
nanoseconds, and whether startup or drain is included. Calculate
`rate = count * 1,000,000,000 / durationNs` without integer truncation. A missing
or nonpositive duration makes the rate unavailable. Retain the measured duration
even if a rounded rate is printed. Configure durations and record actual durations
as separate values.

Report source throughput and delivered throughput per client. An aggregate
delivery rate sums client deliveries over one common observation interval.
If client intervals differ, report each interval; adding their rates does not
describe a single measured aggregate interval. Encoding and writes may include
one occurrence per destination. Shared encoding may instead count one frame
reused by several destinations. State which convention applies.

Declare whether the timer covers submission, network completion or final receiver
observation. A count that includes drain divided by the generator interval is a
generator-normalized delivery rate. End-to-end completion throughput uses the
interval from the first measured generation to the final verified delivery.
An acknowledgement or process exit can provide a later completion bound; label
that boundary explicitly if the actual receiver timestamp is unavailable.

For example, 1,000 admitted records in one second, delivered to two clients over
1.2 seconds from generation start, yield 1,000 source records/s, approximately
833.33 deliveries/s per client and 1,666.67 aggregate deliveries/s over that
1.2-second interval. Dividing 2,000 deliveries by the one-second generator window
gives 2,000 generator-normalized deliveries/s. Both calculations retain their
different denominators.

## Correctness and capacity

Remove subscription probes and warmup from every measured count and retain their
counts separately. A STREAM run qualifies for exact-delivery comparison only
after every client verifies the expected count, order, fields and final state,
including deliveries received during drain. Report duplicates, gaps, malformed
records, rejected admissions, abandoned updates and drain timeouts explicitly.
An update retried successfully counts once as offered and once as admitted.

TICKER may combine intermediate updates. Report admitted generations, observed
generations and the final generation per key. Apply the collector's ordering and
final-state contract to TICKER; its observed update count has different meaning
from the exact STREAM delivery count. HISTORY also needs the declared snapshot,
transaction and retention rules before record counts can be compared.

Call a result a maximum sustained rate only after testing increasing offered
loads, reconciling delivery and recording backlog at the start and end of the
measurement interval. Specify the allowed backlog and latency criteria. A finite
test followed by successful drain demonstrates completed delivery at that load;
it leaves sustained capacity unestablished when backlog growth was not measured.
Report the highest passing tested load and the first failing load, if observed.

Keep schema, data values, pacing, generator batch size, client count, queue and
frame byte limits, transports, runtime options and receiver build fixed within
each comparison. Alternate implementation order and keep individual run results.
Record CPU topology, clock setup and background load. A virtual network result
identifies that network; physical-link capacity needs a physical-link run.

## Latency and clocks

Define each timestamp's observation point. For generation-to-receipt age, stamp
the update once when generated and preserve the timestamp across admission
retries. This includes time spent waiting for admission. An admission-to-receipt
measurement needs a separate admission timestamp.

Use monotonic clocks for local elapsed intervals. Cross-process latency requires
a demonstrated common clock origin, unit and conversion. Cross-machine one-way
latency also records synchronization method and uncertainty. A language API's
nanosecond unit alone does not establish clock compatibility or precision.
Wall-clock adjustments can produce negative ages. Preserve negative samples and
their count; investigate the clock relationship before ranking implementations.

Each latency result states sample count, minimum, maximum, percentile rank rule,
histogram range and bucket widths. Keep out-of-range counts and make a percentile
unavailable when its rank falls outside the represented range. Calculate
percentiles per client. The largest client p99 is labelled worst-client p99.
An aggregate percentile requires merging compatible histograms or raw samples;
averaging client percentiles is not defined by this contract.

The current Java `StreamReceiver` uses rank `ceil(N * p / 100)` for integer
percentiles. Its p50, p95 and p99 values are the **lower edge** of the bucket
containing that rank:

| Age range | Bucket width |
| --- | --- |
| -1 s through values below -10 ms | 1 ms |
| -10 ms through values below 100 ms | 1 microsecond |
| 100 ms through 30 s inclusive | 1 ms; 30 s has an endpoint bucket |

A reported central-bucket p99 of 12,345,000 ns places the represented quantile
in `[12,345,000, 12,346,000)` ns. It has not established a 12,345,000 ns upper
bound. The millisecond fields use mathematical floor division, including for
negative values. Minimum and maximum nanosecond fields retain the actual sample
extrema, including out-of-range samples. With zero samples, percentiles are
unavailable. The current receiver does not export p99.9.

TimeAndSale uses epoch timestamps; the nanosecond mode includes `TimeNanoPart`.
Synthetic Quote/STREAM uses the shared monotonic clock encoded to microseconds
in the Quote fields. Its `ns` result unit does not add precision to that encoding.
Verify the common clock with the selected C++, Java and .NET runtimes on the
measurement host before comparing their Quote age results.

## CPU and memory

CPU measurements state the process or thread, user and system CPU deltas, and the
wall interval between the same two samples. Define
`averageCpuCores = (userCpuSeconds + systemCpuSeconds) / elapsedSeconds`.
One fully busy logical processor gives 1.0 average CPU cores, or 100% on a
one-core scale. Host-normalized percentage is
`100 * averageCpuCores / availableLogicalProcessors`; record the divisor,
affinity and any quota that limits the process. For example, two CPU seconds in
one wall second give 2.0 cores, 200% on the one-core scale, and 25% of eight
available logical processors. Average cores do not establish per-thread load or
where copying and encoding consumed CPU; those claims require thread attribution.

Memory measurements include the platform API and sampling interval:

- Working set or resident set: resident pages reported for the process at a sample.
- Private bytes: the platform's private-memory measure, named as such.
- Sampled peak: the largest observation in the declared interval; intervening
  allocation spikes can be missed.
- OS-reported peak: the operating system's peak counter with its reset/lifetime
  scope recorded.
- Final memory: the last observation at a named boundary, such as drained but
  still running. Record its timestamp before process exit.

Keep these values separate from each other and from allocator reservations or
queue payload bytes. The sum of individual process peaks is a sum of maxima
which may have occurred at different times. A simultaneous aggregate peak needs
aligned samples. Mark unsupported counters unavailable, with a reason.

## Existing tool observations

These mappings describe the checked-in implementations. Missing observations
remain unavailable until instrumentation supplies them.

| Tool or field | Meaning and limits |
| --- | --- |
| Record NetTest `generated`, `composed`, `written` | Generated physical rows, rows in successfully composed DATA frames, and rows in fully written DATA frames. The producer generates per connection; totals across connections contain destination occurrences. There is no distinct public publisher-admission counter in this path. |
| Record NetTest producer `seconds`, `rps` | `seconds` is configured production duration. `rps` truncates `written / seconds` to an integer and may include writes during drain. Recalculate a labelled rate from raw fields for comparisons. |
| Event NetTest `generated`, `admitted` | Generated source events and successfully admitted events, with retries reported separately. The synthetic TimeAndSale mapping is one event to one record. |
| Event NetTest `ingressRecords`, `sentFrames`, `drainMs` | Publisher collector ingress, fully sent frame count and local queue-drain duration. They cannot establish receiver delivery or a record count per frame without additional observations. Startup probes are subtracted from the first two fields. |
| Event NetTest `seconds` | Configured generation window. Outstanding retries and queue drain can extend completion beyond it. |
| Quote publisher `publishNs`; Java publisher `elapsedNs` | Measured local submission-loop duration. Receiver completion is checked separately. Compare these as admission/submission throughput using each API's acceptance contract. |
| `Measure-Stream.ps1` `offeredRps`, `nominalRecords` | Configured intensity and nominal count. Actual distinct offered submissions are not exported. |
| `sourceRecordsPerSecond` | Record mode uses fully written occurrences; Event mode uses admitted source events. Both divide by the producer's reported `seconds`. The two fields therefore have different completion boundaries. |
| `deliveredIncludingDrainPerProducerSecond` | Sum of verified client deliveries, including drain, divided by the producer's reported `seconds`. Receiver completion duration is not its denominator. |
| `Measure-Stream.ps1` CPU | User/system CPU deltas between first and last active samples; `averageCpuCores` uses that interval. Per-process active intervals may differ. |
| Memory from `Measure-Stream.ps1` | First and median working set, sampled peak working set and sampled peak private bytes. Sampling is nominally 100 ms and includes scheduling jitter. Final memory and OS peak are not exported. |

The existing .NET comparison uses the same receiver and synthetic Quote payload.
Record the exact SDK/native runtime versions, submission timer and acceptance
boundary from the publisher used for each run. The repository does not provide
a maintained .NET publisher in this benchmark directory; do not infer its
metrics from Java or C++ field names.

## Result checklist

A comparable result retains the workload identity, versions and build settings;
configured and observed load; boundary-specific counts and intervals; every
client's delivery verdict and latency histogram; process CPU and memory samples;
probe/warmup/drain accounting; and unavailable observations with reasons.
Use the [fan-out procedure](FANOUT.md) or
[Quote/STREAM procedure](QUOTE-STREAM.md) for execution commands. This contract
adds no performance result by itself.
