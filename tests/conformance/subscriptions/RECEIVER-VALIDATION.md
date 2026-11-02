# Subscription receiver validation

[Русская версия](RECEIVER-VALIDATION.ru.md). English is authoritative.

The [receiver contract](../../../spec/subscriptions/RECEIVER.md) is checked by
[receiver tests](../subscription_receiver_tests.cpp),
[borrowed mapping tests](../borrowed_event_view_tests.cpp), and actual TCP/TLS callback
lifetime cases in [plain recovery](../recovery_lifecycle_tests.cpp) and
[TLS recovery](../recovery_tls_tests.cpp). Final qualification of the complete
subscription product remains separate.

## Native profiles

Windows MSVC 19.42 x64 Release with the dynamic release CRT passed 72 tests with JSON
disabled and 73 with JSON enabled. Linux GCC 13.3 Debug with ASan/UBSan passed the same
72/73 tests; leak detection was disabled. Runtime, client, recovery and TLS were enabled
in all four profiles. No external network was used. Dependency-disabled profiles are
outside these four runs.

The receiver suite has ten cases covering generic/custom fields, Quote/Profile mappings,
schema provenance, atomic factory refusal, mutable memberships, retained generic batches,
owned projected results, per-client exception isolation and explicit work limits.
The minimal Profile projection accepts 20 visits and refuses 19. Seven borrowed-view
cases check parity with owning inputs, invalid ranges and compatibility of existing calls.
Real TCP/TLS cases stop, destroy or replace the public recovery owner during a callback
before retaining and projecting the admitted batch.

## Installed console application

The [standalone application](../../../examples/subscriptions/receiver/CMakeLists.txt)
was built against relocated installed packages on both platforms. Each platform ran
both a numeric loopback address and an explicit loopback DNS server. The controlled
[peer](../subscription_receiver_example_peer.cpp) and its
[standalone build](../subscription_receiver_consumer/CMakeLists.txt) advertise the full
eight-field built-in Quote schema. After the application reports ACTIVE, the controller
releases two canonical-symbol records, OTHER and IBM. The application subscribes to IBM,
prints exactly one IBM row and reaches DRAINED with no deadline expiry. DNS runs observe
two actual queries and resolve the fixture CNAME route; numeric runs observe none.

No arguments and `--help` produce identical English stdout with empty stderr;
`--help-ru` also succeeds. Seven invalid argument cases return 2. Standard input stays
open without data. Owned processes finish within finite controller deadlines.

[Captures](../../fixtures/subscriptions/receiver-example/README.md) preserve actual
application output, diagnostics and peer counters. Raw decimal zero means NaN.
Windows records native socket-read 1236 after cutoff; Linux records peer-end. A successful
local drain does not erase those diagnostics or prove wire flush, clean TLS shutdown,
peer acknowledgement or an upstream subscription acknowledgement. The peer reads one
client byte; it does not parse or certify the client's ADD message.

The same ten receiver cases also compiled and passed as separate installed consumers
on both platforms: 3193 assertions each, using relocated production headers/libraries.
A Linux run sent SIGTERM to the actual owned application after ACTIVE and observed
exit 130, ABORTED, no deadline expiry and clean peer retirement. External Windows
console-control injection was not checked; native callback cutoff coverage and normal
Windows console drain are separate checks.

## Limits

The console application uses plain TCP and one STREAM Quote subscription. TLS and all
three queue families have separate native test coverage; this application is not a
TLS/HISTORY tutorial. These checks do not establish public-provider interoperability,
complete business-event coverage, process RSS bounds or hardware-independent latency.
DNS reservations and logical work credits follow the
[qualified DNS profile](../../../spec/transport/DNS-RESOURCES.md).
