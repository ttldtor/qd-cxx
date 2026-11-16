# Synthetic multicast burst replay

[Русская версия](README.ru.md).

This tool transmits or verifies one validated synthetic burst corpus using an
IPv4 multicast group. Generate a corpus with the adjacent burst tool first.
Run both commands with the same corpus and use an administratively scoped
group, a free UDP port and a concrete IPv4 address for each interface.

~~~powershell
# Receiver host
pwsh -File tools/nettest/benchmark/multicast/Multicast.ps1 -Mode Receive -Directory C:/bench/input -Group 239.255.88.7 -Port 49872 -Interface RECEIVER_IPV4 -Output C:/bench/receive.json -TimeoutMs 30000
# Sender host, after the receiver prints READY
pwsh -File tools/nettest/benchmark/multicast/Multicast.ps1 -Mode Send -Directory C:/bench/input -Group 239.255.88.7 -Port 49872 -Interface SENDER_IPV4 -Output C:/bench/send.json
~~~

Both sides validate and preload the entire corpus. The receiver joins the
group on its chosen interface and reports READY. The sender uses TTL 1 and
counts a datagram only after the OS accepts the complete payload. P99 and
maximum lateness measure scheduled time to completion of the local send call.
They do not measure receiver delivery or physical-wire transmission time.

The receiver verifies each complete datagram against its preloaded corpus.
Its receipt distinguishes unique messages and datagrams, missing, duplicates,
out-of-order, unknown and corrupt datagrams. It listens for the full timeout
to collect late arrivals. Multicast transmission and kernel buffers can drop
datagrams, including on loopback. Process exit 0 means a receipt was
produced. Inspect Completed and the individual counters to judge delivery.
The sender's in-window counts include successful local calls before the
offered window closes.

Help and HelpRu show supported parameters. On one host, use 127.0.0.1 for
both interfaces if multicast loopback is available. For physical network
qualification use separate hosts, record NIC and switch configuration, run a
separate link-capacity baseline and retain both receipts. This synthetic
format contains neither OPRA packets nor QTP data.
Use a dedicated UDP port for the run. The receiver binds the port on all local addresses and compares any datagram delivered to that socket.
The receiver window begins when it validates its first unique datagram. Its in-window counts therefore use a receiver-local anchor independent of the sender clock. ReceiveSpanNanoseconds covers the first through last valid unique datagrams. SendTimeout is 100 ms per call and failed calls increment SendErrors.

Use the [acceptance checks](../acceptance/README.md) to reconcile paired receipts.
