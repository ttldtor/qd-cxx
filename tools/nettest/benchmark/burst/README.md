# Synthetic burst corpus

[Русская версия](README.ru.md).

`Corpus.ps1` generates a reproducible workload for a single offered burst. It
uses synthetic messages and payloads. The default profile schedules 625,000
messages across 100 ms and limits each UDP payload to 1,472 bytes. It describes
the intended workload independently of any transmitter's actual send times.
It creates data before the measurement and performs no network operations.

```powershell
pwsh -NoProfile -File tools/nettest/benchmark/burst/Corpus.ps1 -Mode Generate -Directory .work/burst-input
pwsh -NoProfile -File tools/nettest/benchmark/burst/Corpus.ps1 -Mode Validate -Directory .work/burst-input
```

`-Help` and `-HelpRu` list parameters, default values, limits, exit behavior and
file side effects. Generate requires a new directory. Validate reads the
parameters from its manifest, verifies the hash and reproduces every header,
payload and scheduled offset. Generation or validation errors exit nonzero.

`Measure-Clock.ps1 -Directory .work/burst-input -Output .work/burst-clock.json`
validates and preloads the corpus before timing. It reports scheduled datagrams
and messages within the window and overall, with p99 and maximum lateness in
nanoseconds. The scheduler actively waits and uses CPU. This measurement covers
the local clock and scheduler and performs no network or QTP transmission.

## Binary format

`manifest.json` identifies `qdcxx-synthetic-burst-v1` and records the message
count, window in milliseconds, payload limit, seed, size cycle, schedule,
message and datagram counts, byte totals, minimum and maximum payload sizes,
file size and SHA-256 of `corpus.bin`.

All integers in `corpus.bin` are unsigned little-endian. Its 16-byte file header
contains the ASCII magic `QBURST01`, a 32-bit message count and a 32-bit UDP
payload limit. It is followed by datagram records to end of file. Each record
contains an eight-byte scheduled offset in nanoseconds, a four-byte UDP payload
length, a four-byte message count and the indicated number of payload bytes.

Each payload starts with ASCII `QDP1`, a 32-bit zero-based datagram number and
an eight-byte zero-based ordinal of its first message. Message sizes cycle
through 24, 40, 64 and 80 bytes using `(ordinal + seed) % 4`. A message starts
with its two-byte total length and eight-byte ordinal. For remaining bytes,
initialize a 32-bit unsigned state with `seed ^ ordinalLow32 ^ ordinalHigh32 ^
0x9e3779b9`. For each byte update the state with `state ^= state << 13`,
`state ^= state >> 17`, and `state ^= state << 5`, truncating to 32 bits after
each update. The low byte is the next message byte.

Messages are greedily packed in order until the next one exceeds the configured
payload limit. A datagram's offset is the floor of the first message ordinal
multiplied by the window length in nanoseconds and divided by the total number
of messages. All messages in that datagram have the same offered send time.
The final datagram can be shorter. The corpus contains one burst. A replayer
can repeat it with a separately specified period and must report actual send
times and delayed or unsent datagrams.

For network runs, see [synthetic multicast burst replay](../multicast/README.md).
