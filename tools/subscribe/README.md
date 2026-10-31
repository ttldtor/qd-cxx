# Controlled STREAM connection tool

[Русская версия](README.ru.md). English is authoritative.

`qdcxx-connect` is a native, opt-in, credential-free TCP STREAM application over the
[qualified runtime](../../spec/transport/TCP-RUNTIME.md) and
[local dispatcher](../../spec/subscriptions/CLIENT-DISPATCH.md). It is the first bounded receiving
subset of the [Connect inventory entry](../../docs/upstream/QDS-TOOLS.md), with its own CLI and output;
Java command-line compatibility is not claimed.

## Build and help

Enable `ORG_TTLDTOR_QDCXX_BUILD_CONNECT=ON`, `ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON` and
`ORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON` explicitly. All three default to OFF. Supply the exact qualified
Asio and c-ares source roots using the [runtime build instructions](../../spec/transport/TCP-RUNTIME.md#build-boundary).
The CLI adds no dependency acquisition and needs no JSON loader; `ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF`
is supported. Build target `qdcxx_connect`; its filename is `qdcxx-connect` (`.exe` on Windows).

No arguments and standalone `--help` print identical English help to stdout, with empty stderr and
exit zero, before working argument parsing, schema creation, bootstrap, signal changes or network work.
Standalone `--help-ru` prints Russian help. Failed help publication returns nonzero. Help never reads stdin.

Example against an explicitly controlled IPv4 loopback peer:

```powershell
.\build\stream\qdcxx-connect.exe --host 127.0.0.1 --port 19000 --symbol IBM --schema-profile quote --output generic --duration-ms 5000
.\build\stream\qdcxx-connect.exe --host 127.0.0.1 --port 19000 --symbol IBM --schema-profile profile --output profile --max-records 1 --duration-ms 5000
```

Use the corresponding executable path without `.exe` on Linux. A peer must implement the selected
DXP3 readiness, complete record descriptions and STREAM data; a listening socket alone is insufficient.

## Native options and boundaries

| Option | Meaning and default |
| --- | --- |
| `--host IP` | Required exact numeric IPv4/IPv6 literal; no URI, zone identifier, hostname or resolver discovery. |
| `--port N` | Required numeric port, 1..65535. |
| `--symbol SYMBOL` | Required, repeatable up to 16 distinct selectors, each 1..128 printable ASCII units; no leading `*`. Exact matching, no glob/regex expansion. |
| `--schema-profile quote\|profile` | Full built-in eight-field Quote or sixteen-field Profile; default `quote`. |
| `--output generic\|quote\|profile` | Exact generic rows by default; prepared summary must match the selected schema. |
| `--duration-ms N` | Admission interval in 1..600000 milliseconds, default 5000. |
| `--remove-after-ms N` | Optional removal of the entire initial exact selection; positive and less than duration. |
| `--max-records N` | Optional positive callback record target, at most 1000000; stops further source admission after an admitted prefix reaches the target. |
| `--connect-timeout-ms N` | Connect deadline, 1..2147483647 ms; default 1000 |
| `--read-timeout-ms N` | Input inactivity deadline, 1..2147483647 ms; default duration |
| `--write-timeout-ms N` | Output inactivity deadline, 1..2147483647 ms; default duration |
| `--poll-interval-ms N` | Caller progress cadence, 1..2147483647 ms; default 1 |
| `--security plain\|tls` | Default `plain`; `tls` rejects before schema/signals/bootstrap/network work |

Only `--symbol` may repeat. Unknown, incomplete, duplicate or incompatible arguments fail before
application activation. There is no stdin command interface, ambient configuration or schema loading.
The [C++ example driver](../../examples/subscriptions/README.md) separately accepts a validated caller
schema and exact UTF-16 selectors, including empty/NUL/unpaired units; the CLI's ASCII rule does not
change library symbol semantics.

The driver uses one owner thread, one `TcpRuntime`-owned session/decoder and one dispatcher.
Every whole polled event enters the dispatcher before later ordinary mutation. Pending local work
retries before another source poll or removal. Add/remove application, output composition, local
socket acceptance and peer acknowledgement are distinct; only the first three have local evidence.
There is no general desired-to-sent reconciliation engine.

The admission interval is measured from driver start, including negotiation. The record target is
not a truncation of an already admitted whole frame: graceful local retirement can finish its
eligible prefix. A duration/target/interrupt cutoff stops source admission, explicitly aborts native
TCP/output, then retires the admitted local prefix. It can leave pending or uncertain native writes
and partial input. Neither local terminal nor exit zero certifies a whole-network drain or peer ACK.
`--remove-after-ms` may be preempted by an earlier target/interrupt cutoff.

SIGINT/SIGTERM request cooperative stop. The application temporarily installs/restores its own signal
handlers and ignores POSIX SIGPIPE so a broken working-output pipe becomes a checked sink failure.
Arbitrary blocking sinks/callbacks and native OS completion have no hard elapsed-time guarantee.
TLS, credentials/authentication, reconnect, wildcard, shared MUX, TICKER/HISTORY and full subscriptions
remain outside this milestone.

## Immutable native configuration

After parsing every argument, the CLI resolves the same [typed endpoint snapshot](../../docs/SUBSCRIPTIONS.md#endpoint-options)
as the native overload. Explicit timeout overrides win over duration-derived defaults
regardless of argument order. All five new options are singletons. TLS is recognized
for a static unsupported-profile error before working setup; no trust/secret argv,
configuration files, environment or Java resolution are added. Errors do not echo
caller secrets. Existing working exit codes and noargs/standalone help behavior remain.

The new overload permits independently omitted legacy host/port, rejects supplied
conflicts and applies the minimum of snapshot operations and the old cap1..10000.
[Native driver restrictions](../../docs/SUBSCRIPTIONS.md#endpoint-options) describe
sufficient retirement/full-quantum guards and first-pristine-call WORK_LIMIT handling;
valid smaller generic options/TCP partial-progress profiles remain representable.
TLS/auth, retry and DNS handoff does not enable execution.
See [options validation](../../tests/conformance/subscriptions/OPTIONS-VALIDATION.md).

## Output version 1

Stdout contains diagnostic rows scoped to one run/epoch, without a success footer. Decimal integers
use base ten. `UTF16HEX` means four uppercase hex digits per exact UTF-16 unit; `BYTEHEX` means two
per byte. Literal `null` represents absence; a non-null empty payload is an empty cell.

Generic output begins with `qdcxx-stream<TAB>1` and emits these tab-separated row shapes:

```text
schema  localId  recordNameUTF16HEX  fieldCount
layout  localId  fieldIndex  fullType  lookupNameUTF16HEX
record  ordinal  localId  senderId  revision  bindingGeneration  cipher  eventFlags  symbolUTF16HEX  removalTimeOrNull  fieldSecondsOrNull  fieldSequenceOrNull  fieldSourceOrdinalOrNull
field   ordinal  fieldIndex  fullType  raw  unitsNullOrUTF16HEX  bytesNullOrBYTEHEX
```

The displayed spaces separate columns; actual output uses tabs. Ordinals begin at one. Every lookup,
including aliases, and every extra typed field is preserved. Legacy compact-only values use full type 8
when no typed views exist. Callback provenance is validated against the original bounded schema identity;
local ID, sender ID, revision and original binding generation remain separate coordinates.
This is not a serialized complete `LocalSchema`/`DataScheme`, a wire/tape roundtrip format or a
cross-run opaque identity. Schema profile/indexed layout and peer descriptor metadata are not serialized.

Prepared summaries begin with `qdcxx-stream-summary<TAB>1` and contain a selected subset of the mapped
business event; use generic output to retain all original fields:

```text
quote    ordinal  symbolUTF16HEX  bidTimeMs  askTimeMs  bidPriceRaw  askPriceRaw  timeMs
profile  ordinal  symbolUTF16HEX  flags  descriptionNullOrUTF16HEX  statusReasonNullOrUTF16HEX
```

Stderr reports separate `application`, `local` and `transport` observations: applied/composed commands,
target/cutoff, local callback counts/outcome, native primary/secondary stage/code/protocol category,
abort/peer end, reported accepted write bytes, pending output, unknown write acceptance and discarded input.
A final sink-flush failure remains a separate visible failure after both owners have retired.

## Exit and verification scope

- `0`: intentional cutoff or reached data target, without an observed failure; inspect native pending/uncertain output.
- `1`: invalid input, setup, peer/protocol/callback/sink failure, including ambiguous native close failure.
- `2`: interval expired before initial add composition or before a requested record target; also an applied removal whose output was not composed.
- `130`: observed interrupt without an earlier failure.

On Windows, qualified native close can report `READ_FAILURE`, `socket-read`, native code 1236 after
an intentional abort. Its source is ambiguous; it stays visible and returns `1`, even with a drained
local prefix. It is not silently reclassified as cancellation or successful retirement.

`connect_stream`, `connect_cli_contract` and `connect_process` are focused native CTests. They cover
actual local TCP readiness/add/remove, exact selection, byte sends, forced local work retry, full
prepared Quote/Profile, peer loss/partial framing, sink failure and owned child retirement. The
[independent consumer](../../tests/conformance/connect_consumer/CMakeLists.txt) separately exercises
installed package linkage and the programmatic example. These scenario definitions are not a claim
that every platform/configuration has been executed; final qualification is recorded by the task's
published validation evidence.
