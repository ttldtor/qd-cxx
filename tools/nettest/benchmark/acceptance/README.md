# Burst receipt acceptance

[Русская версия](README.ru.md).

Use the corpus, sender receipt and receiver receipt from one multicast burst.
The result identifies its inputs by SHA-256 and records each acceptance check.

~~~powershell
pwsh -File tools/nettest/benchmark/multicast/Test-BurstReceipts.ps1 -Directory C:/bench/input -SendReceipt C:/bench/send.json -ReceiveReceipt C:/bench/receive.json -Output C:/bench/acceptance.json -MaxSendP99Ns 1000000
~~~

Supply a new output path. The optional p99 bound is in nanoseconds and applies
to sender-local scheduled-to-send-completion lateness. Omit it when no
threshold has been selected. It does not describe end-to-end delivery latency.
The command validates the corpus and checks receipt formats, roles, identity,
multicast group and port, counts, payload bytes, loss, duplicates, ordering
and corruption. Sender and receiver must each account for all scheduled
messages and datagrams within their own windows. The receiver's window starts
with its first valid unique datagram.

Valid comparisons write a JSON result even if checks fail. Exit 0 requires all
enabled checks to pass. Exit 1 indicates a failed check or invalid inputs. A
passed result covers the supplied synthetic receipts. It does not certify a
physical network, OPRA compatibility or cross-host one-way latency.

For an actual QTP fanout acceptance, establish the target record mix, burst
schedule, client count and required per-client throughput before the run.
Record each source and receiver build, schema and corpus hash, route, link
speed, OS, CPU and network adapter details. Run the physical link baseline
separately. Verify STREAM sequence and content for every receiver, correlate
each receiver's final count with the source, preserve errors and run logs,
and measure queue drain. TICKER requires separate latest-value and conflation
criteria. Report sender, receivers and throughput units independently.

For cross-host one-way p99, record verified clock synchronization and its
uncertainty. Use a one-clock setup if this evidence is unavailable. The
multicast receipt check alone does not evaluate QTP fanout or perform those
hardware, clock and end-to-end checks.
Pair the sender and receiver receipts from the same physical run. The corpus hash and multicast endpoint alone cannot establish that two receipts belong to one run.
