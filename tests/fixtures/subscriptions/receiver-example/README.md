# Installed Quote receiver captures

[Русская версия](README.ru.md). English is authoritative.

Synthetic loopback runs captured on 2026-11-02; no external feed or system resolver.
The peer advertises the complete default Quote schema, then sends OTHER and IBM only
when the application reports ACTIVE. Raw decimal zero is NaN. DNS runs receive two
actual queries; numeric runs receive none. Each application prints one IBM row and
reports DRAINED without expiry. Native diagnostics remain unchanged after local cutoff.

| Profile | Application output | Diagnostics | Peer counters |
| --- | --- | --- | --- |
| MSVC 19.42 Release/MD numeric | [stdout](windows-numeric/live.stdout) | [stderr](windows-numeric/live.stderr) | [peer](windows-numeric/peer.stdout) |
| MSVC 19.42 Release/MD DNS | [stdout](windows-dns/live.stdout) | [stderr](windows-dns/live.stderr) | [peer](windows-dns/peer.stdout) |
| GCC 13.3 Debug ASan/UBSan numeric | [stdout](linux-numeric/live.stdout) | [stderr](linux-numeric/live.stderr) | [peer](linux-numeric/peer.stdout) |
| GCC 13.3 Debug ASan/UBSan DNS | [stdout](linux-dns/live.stdout) | [stderr](linux-dns/live.stderr) | [peer](linux-dns/peer.stdout) |

Linux leak detection was disabled. [SHA-256 inventory](hashes.json) covers the twelve
raw capture files, with original platform line endings. The
[validation report](../../../conformance/subscriptions/RECEIVER-VALIDATION.md) describes
source tests, installed builds and limits. The
[controlled peer](../../../conformance/subscription_receiver_example_peer.cpp) observes
one client byte; these captures do not certify ADD contents, peer acknowledgement,
wire flush, a clean transport shutdown or general public-provider compatibility.
