# Subscription connection tool

[Русская версия](README.ru.md). English is authoritative.

`qdcxx-connect` receives binary QTP records from an existing compatible source. Its default route
retains the controlled plain STREAM driver. Explicit `--mode recovery` composes the existing
TCP/TLS recovery owner and receiver for one STREAM, TICKER or HISTORY record/client per invocation.
Start with the [receiving tutorial](../../docs/SUBSCRIPTIONS.md); the
[console C++ example](../../examples/subscriptions/README.md) uses the public application API.

## Build and help

Enable CLIENT, RUNTIME and CONNECT; enable RECOVERY for the new route. External JSON needs
`ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON`; TLS needs `ORG_TTLDTOR_QDCXX_BUILD_TLS=ON` and its exact
qualified static SDK. The [build instructions](../../docs/CPP-BUILD.md#optional-subscription-tool)
describe supported compilers, explicit SDK inputs and installed targets. Target `qdcxx_connect`
produces `qdcxx-connect`. Disabled features reject before files, signals/bootstrap or networking.
Short command names below require the executable on `PATH`; otherwise use its full path.
PowerShell requires `./qdcxx-connect.exe` when invoking it from the executable directory.

No arguments and standalone `--help` print identical English help; standalone `--help-ru` prints
Russian help. They require no endpoint or input files and perform no application activation.
Mixed help/working arguments are usage errors. A help-output failure is visible.

## Recovery invocation and options

```text
qdcxx-connect --mode recovery --host 127.0.0.1 --port 19123 --symbol IBM --family stream --schema-profile quote --output quote --duration-ms 5000
```

The example requires an already running compatible source; the tool starts no service at that port.
Options are separate name/value pairs. Only `--symbol` may repeat; unknown, incomplete, duplicate
or contradictory options refuse. There are no stdin commands, implicit files or ambient defaults.

| Option | Recovery meaning, range and default |
| --- | --- |
| `--mode recovery` | Required explicit route selection |
| `--host HOST`, `--port N` | Required numeric IP or ASCII/A-label DNS name without URI/zone; port 1–65535 |
| `--symbol SYMBOL` | Required; 1–128 printable ASCII units, first not `*`; up to 16 distinct symbols |
| `--family stream\|ticker\|history` | Default `stream`; one family/record/client per run |
| `--schema-profile quote\|profile` | Complete built-in schema; default `quote`, local record ID 0 |
| `--schema-json PATH` | Explicit native binary JSON file, at most 1 MiB; excludes `--schema-profile` |
| `--record-id N` | Local ID 0–2147483647; mandatory with an external schema |
| `--history-from-raw N` | Required only for HISTORY with an external indexed schema; complete signed int64 decimal |
| `--selectors exact-only\|canonical-exact-v1` | Default `exact-only`; neither permits All/wildcards |
| `--output generic\|quote\|profile` | Default `generic`; selected prepared rows supplement all generic fields |
| `--dns-server IPv4`, `--dns-port N` | Explicit numeric resolver for named hosts; port 1–65535, default 53 |
| `--dns-timeout-ms N` | 1–2147483647 ms; default connect timeout |
| `--attempts N` | Positive uint32, including the initial attempt; default 1, also bounded by the 1 MiB epoch-label table |
| `--retry-delay-ms N` | 1–2147483647 ms for attempts > 1; default 250; rejected with one attempt |
| `--duration-ms N` | Admission interval 1–600000 ms, default 5000, including negotiation |
| `--remove-after-ms N` | One removal of the initial keys; strictly positive and less than duration |
| `--max-records N` | Optional completed-record target 1–1000000; no default target |
| `--drain-ms N` | Local retirement allowance 1–60000 ms, default 1000 |
| `--connect-timeout-ms N` | 1–2147483647 ms, default 1000 |
| `--read-timeout-ms N`, `--write-timeout-ms N` | 1–2147483647 ms; default admission duration |
| `--poll-interval-ms N` | Caller cadence 1–2147483647 ms, default 1 |
| `--security plain\|tls` | Default `plain`; TLS needs the separate build/trust/reference profile |
| `--tls-trust-pem PATH` | Required for TLS; explicit ordinary CERTIFICATE PEM, no system-trust fallback |
| `--tls-reference NAME` | Required for TLS; verified numeric IP or ASCII/A-label identity independent of route |
| `--tls-handshake-ms N`, `--tls-close-ms N` | TLS deadlines 1–2147483647 ms, each default 1000 |
| `--auth-basic-token-file PATH` | Optional TLS-only complete ASCII Basic token, at most 4096 bytes, no newline |
| `--auth-timeout-ms N` | With token file only; 1–2147483647 ms, default 1000 |

Explicit timeout values win independently of argument order. Paths are bounded to 4096 bytes;
native Windows UTF-16 arguments become UTF-8 without changing process codepages. Only explicit
files are read, once, after feature/argument checks. Errors do not echo paths, identities or tokens.

The fixed input profile permits at most 16 schema records, 128 logical fields, 256 lookups,
128 UTF-16 units per name/16384 total name units and 64 records per frame. Incompatible prepared
mappings and profile excess reject before activation. Live data can exhaust fixed queues/caches;
this is a visible failure without hidden budget growth. Numeric routes bypass DNS; named routes
require an explicit resolver and use the documented extra 80 MiB logical requested-payload reserve.
Both legacy and recovery typed output allow 128 declared exact-type alternatives in their finite
preparation profile; the complete current Quote and Profile mappings require 72 and 108 respectively.

## Schemas, TLS and output

The [tutorial](../../docs/SUBSCRIPTIONS.md#use-the-recovery-cli) gives Quote, Profile and HISTORY
commands. HISTORY requires an explicit indexed/time layout; its signed raw bound is not Unix or
business-event time. See [native binary JSON](../../spec/schema/NATIVE-BINARY-JSON.md) and
[indexed schemas](../../spec/schema/INDEXED-SCHEMAS.md). Schema selection does not create peer support.

TLS verifies the explicit trust/reference before optional credential use and never downgrades to
plaintext. Ordinary trust input is limited to 524288 bytes/256 certificate blocks. A Basic file
contains the exact `Basic ` prefix plus canonical base64, without newline, trimming or normalization.
Credentials are file input, not argv. TLS is restricted to the
[closed host profile](../../spec/cli/TLS-HOST-PROFILE.md), including the exact static image, serialized
single SDK owner, fresh process and absent `OPENSSL_ia32cap`; arbitrary foreign SDK use is outside it.

Recovery stdout starts `qdcxx-subscriptions<TAB>1`. Generic schema/record/field values and original
metadata are always retained; prepared rows add selected business values. `null` and an empty
engaged value remain distinct. UTF16HEX/BYTEHEX preserve exact units/bytes, and raw numeric bits
are not formatted as prices or timestamps. Actual commands, reset/epoch transitions, HISTORY
observations and local terminal evidence go to stderr. Use the exact
[version 1 row/encoding contract](../../spec/cli/RECOVERY-OUTPUT.md) for parsers.

Finite retries recreate attempts and replay the current desired set. Reset publication precedes
its acknowledgement, with visible gaps and epoch changes; delivery is not exactly-once. HISTORY
range evidence is local AS-OF; refusal/ineligibility means unknown. Duration/record target requests
bounded local drain. A whole callback batch can cross the target; its suffix is retained. Drain is
limited to the admitted local prefix and proves neither peer ACK nor whole-network flush. SIGINT/
SIGTERM requests abort; normal progress and retirement use the same work profile. A blocked sink
or OS call has no hard preemption deadline. See the [recovery contract](../../spec/transport/RECOVERY.md).

| Recovery exit | Meaning |
| --- | --- |
| `0` | Actual DRAINED without expiry, with readiness/command/target requirements satisfied |
| `1` | Setup, protocol, transport, output or retirement failure |
| `2` | Usage error or unmet readiness/target |
| `130` | Handled signal followed by actual ABORTED; failure takes precedence |

<a id="controlled-stream-connection-tool"></a>
<a id="tool-subscribe-legacy-stream-en"></a>

## Legacy route without mode selection

The sections below apply only when `--mode recovery` is absent and retain the original
`qdcxx-stream`/summary output and controlled STREAM shutdown semantics.

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
