# Endpoint configuration production oracle

[Русская версия](README.ru.md) · [Developer tools](../../dev/README.md)

Invoke-Oracle.ps1 captures or verifies actual QDS 3.355 production consumers pinned by upstream.lock.
It uses existing production class directories and JDK8. It never rebuilds upstream or changes
its classes/resources. The 44-row fixture records:

- SocketUtil address-list parsing, including broader Java port/host-list acceptance.
- MessageConnectors construction, property order, case, millisecond reconnectDelay and inactive state.
- DXEndpoint builder property precedence, input ownership and implementation map mutation visibility.
- Explicit file defaults versus system defaults and explicit builder overrides.
- FEED auto-connect and inline application timing, with one harness-owned loopback listener.
  STREAM_FEED construction records zero auto-created connectors.

The dummy ApplicationConnectionFactory is only a guard for inactive parser cases. It rejects
unexpected activation; no application connection is fabricated. Parser exceptions are recorded
only around the production parse call. Harness/getter/I/O failures fail the process. Credentials
in synthetic parser cases are fixed sample markers; output records booleans without credentials,
dynamic ports or private file paths. This does not qualify authentication or TLS.

## Run and help

~~~powershell
pwsh -NoProfile -File tools/upstream/endpoint-configuration-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/endpoint-configuration-oracle/Invoke-Oracle.ps1 -HelpRu
$oracleInputs = @{
    Upstream = 'C:/reference/QD'
    JavaHome = 'C:/tools/jdk8'
    Logger = 'C:/tools/qdcxx-run-utc.exe'
    RunDirectory = '.work/endpoint-configuration-verify'
}
& tools/upstream/endpoint-configuration-oracle/Invoke-Oracle.ps1 -Mode Verify @oracleInputs
~~~

Capture uses the same explicit inputs with -Mode Capture and a different fresh directory.
Capture replaces only tests/fixtures/endpoint-configuration/oracle.tsv and provenance.json.
Verify compares exact fixture and harness/launcher/upstream hashes. Review candidates before
publishing. No arguments, -Help, -HelpRu and Get-Help require no work inputs and launch no Java.

The runner bounds each own child to 60 seconds and Java heap to 128 MiB. It runs UTC and
Pacific/Honolulu sequentially, compares exact TSV bytes and checks complete before/after class
inventories. RunDirectory must be new, inside this checkout's ignored .work tree, with no
redirecting ancestors. Full commands/UTC/exits, raw streams, inventories and receipt stay private.
It may stop only its own timed-out child. Exit0 means success/help; exit1 means invalid input,
reference drift, timeout, process/I/O failure or mismatch. No external endpoint is contacted.

## Native boundary

[Endpoint configuration](../../../docs/ENDPOINT-CONFIGURATION.md) specifies the supported native
profile and explicit Java deviations. Native conformance compares admitted flat reconnectDelay
aliases to fixture values and verifies selected intentional rejections. Java raw acceptance is
not fabricated into native support. Defaults/properties never introduce ambient lookup in native
core. Existing [recovery](../../../spec/transport/RECOVERY.md) resources and wire semantics remain in force.