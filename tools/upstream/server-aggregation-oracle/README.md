# Server aggregation production oracle

[Русская версия](README.ru.md) · [Developer tools](../../dev/README.md)

Invoke-Oracle.ps1 captures/verifies QDS 3.355 classes pinned by upstream.lock. The 68 raw rows include
the public DXEndpoint setter/validation chain, actual AgentAdapter/DistributorAdapter binary QTP
full and delta descriptors, reset, server clamping, malformed and omitted information, legacy
unavailability, STREAM and a real SocketConnector stop/start reconnect on an owned loopback listener.
TimePeriodInfo parsing records exact rounding, saturation and accepted extension syntax.

aggregationPeriodInfo is a separate peer report and may still describe an earlier request.
Server clamping and an unknown legacy peer are recorded separately. A setter after close changes
Java local intent without restarting connectivity. No acknowledgement message is invented.

~~~powershell
pwsh -NoProfile -File tools/upstream/server-aggregation-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/server-aggregation-oracle/Invoke-Oracle.ps1 -HelpRu
$inputs = @{
    Upstream = 'C:/reference/QD'
    JavaHome = 'C:/tools/jdk8'
    Logger = 'C:/tools/qdcxx-run-utc.exe'
    RunDirectory = '.work/server-aggregation-verify'
}
& tools/upstream/server-aggregation-oracle/Invoke-Oracle.ps1 -Mode Verify @inputs
~~~

Capture uses the same explicit inputs and a fresh RunDirectory, replacing only
tests/fixtures/server-aggregation/oracle.tsv and provenance.json. Verify compares exact bytes and
harness/launcher/upstream bindings. No arguments, -Help, -HelpRu and Get-Help start no Java and need
no operational paths. Exit0 means success/help; exit1 means invalid input, mismatch, timeout or process
failure. Each own child has a 60-second limit and Java has a 128 MiB heap. UTC and Pacific/Honolulu
execute sequentially. Existing production class/resource inventories must remain unchanged.
Full commands, exits, streams and inventories stay in the explicit private run directory.
Only harness-owned sockets and children are stopped. No external network or upstream/SDK build occurs.

The [native contract](../../../spec/transport/SERVER-AGGREGATION.md) describes bounded parsing,
local immutable configuration and asynchronous request timing. Java production semantics are the oracle.
