# Configuration ownership and precedence

[Русский](CONFIGURATION.ru.md) · [Settings reference](CONFIGURATION-REFERENCE.md) · [Endpoint properties](ENDPOINT-CONFIGURATION.md)

English is authoritative.

Configuration belongs to the component that consumes it. Resolve sources at an explicit boundary,
validate typed values, then pass them to that component. Core, codecs and event mappings do not
discover application files, environment variables or mutable process properties.

## Choose the configuration boundary

| Boundary | Current behavior | Reading path |
| --- | --- | --- |
| Public Feed API | `api::EndpointOptions` supplies typed policy; `api::EndpointConfiguration` resolves the supported text layers before endpoint construction. Typed verified TLS is supported by qualified TLS-enabled builds. | [Endpoint configuration](ENDPOINT-CONFIGURATION.md), [Feed API](FEED-API.md) |
| Low-level controlled STREAM example | `client::EndpointSettings` becomes an immutable `client::EndpointOptions` snapshot. Its example driver accepts numeric-host plain TCP with one attempt. | The low-level snapshot sections below |
| Schema acquisition | Explicit native JSON or wire discovery produces a validated owning schema. Consumers receive that schema separately from source loading. | [Ordinary JSON](../spec/schema/NATIVE-JSON.md), [binary JSON](../spec/schema/NATIVE-BINARY-JSON.md), [wire inventory](../spec/schema/WIRE-INVENTORY.md) |
| Other components and tools | Typed settings, nested capacities and explicit work credits have their own owners. A field's presence does not enable another subsystem. | [Per-field reference](CONFIGURATION-REFERENCE.md), [developer tools](../tools/dev/README.md) |

The two `EndpointOptions` types are different APIs in different namespaces. The plain-only limits
of the low-level example do not describe the high-level Feed API. There is no general Java-properties
loader, process-wide priority chain or universal live-update mechanism.

## Resolve sources before activation

For `api::EndpointConfiguration::create`, the priority from lowest to highest is:

1. Typed `api::EndpointOptions` fallback, whether supplied explicitly or default constructed.
2. The caller's explicit defaults layer.
3. The caller's explicit properties layer.
4. Supported properties of the selected address, in block order.

The last duplicate wins within a layer. Only the selected address is parsed. Every supplied entry,
including a shadowed duplicate, consumes the input envelope and must use a supported key and bounded
text; selected values undergo typed semantic validation. False, zero, absence and empty text retain
their per-key meaning. For example, an empty native name is explicit, while empty inline
`requestedAggregationPeriod` resets the request. This order is local to this resolver; it does not
establish Java precedence for every property family.

Construction resolves and validates before publishing an endpoint slot or admitting a connection.
It performs no DNS, network, file discovery or application callback. `Endpoint::connect()` admits
activation separately. Inline properties in a later `connect(address)` are rejected; they do not
replace immutable construction policy. See the [supported keys and exact errors](ENDPOINT-CONFIGURATION.md).

## Ownership, timing and observations

Borrowed property strings live through `create`; successful configuration owns the retained values.
`options()` is a borrowed const view while a retaining, non-moved-from handle lives. `snapshot()` owns
its copies and does not keep a runtime or connection alive. Independent handles may be read/copied
concurrently; replacement or destruction of the same handle requires synchronization. A moved-from
handle may only be reassigned or destroyed.

`getEffectiveConfiguration()` copies local policy and admitted connection intent under the endpoint
mutex. Requested and effective retry delay can differ: a one-attempt policy applies zero delay.
Property origins identify the winning layer, not a server acknowledgement. Connection and requested
aggregation revisions identify their respective local control changes. The original configuration
stays immutable when `setRequestedAggregationPeriod` changes the endpoint's current request.
Construction snapshots are not live views. Effective local policy does not prove DNS resolution,
negotiation, delivery or a server-effective aggregation period.

Settings containing a provider or dependency need their own lifetime contract. Shared ownership of a
credential provider retains its handle, not synchronized application state, bounded callback time or
secure erasure. The low-level snapshot contract below and the [verified Feed TLS contract](FEED-API.md)
describe their respective invocation and retirement rules. Configuration resolution never calls a
credential provider. Secrets must not enter ordinary labels, rejected-input diagnostics or fixtures.

## Reading the complete native inventory

The [settings reference](CONFIGURATION-REFERENCE.md) identifies the declaration, type, units, default,
read timing, mutability and failure behavior for each inventoried field. It separates field-specific
consumer contracts from declaration-only entries and marks missing information explicitly.
Nested settings are compositions; work credits are consumable inputs; requirement measurements and
fixed constants are not settings. A capacity bounds retained state; a work quantum bounds an operation
or turn. Neither silently becomes a lifetime record quota or a bound on process RSS.

Current defaults such as 64 input properties and 4/128 Feed record slots are configurable or
profile-specific policies. They are not QTP limits. A declared number alone supplies no performance
recommendation or proof that another value is supported. Reserved inactive fields, including the
attachment diagnostic-text budget, must not be presented as effective resource controls.

## Accepted evolution; implementation remains separate

The configuration contract for future loaders and component updates is:

- Each setting has an owning consumer, exact type/unit/default, validation, origin and revision.
  Requested and applied states are observable; secrets are redacted. A setting declares whether it
  applies immediately, on reconnect or after restart. Unsupported settings cannot appear applied.
- Applications attach file, launch-option or environment loaders explicitly. Acquisition, precedence,
  typed validation and application remain separate. Existing endpoint precedence is preserved;
  another property family requires its own evidence and declared priority. No CLI or file syntax is
  introduced here.
- Structural validation precedes activation. Unknown enum values reject; compatibility behavior for
  unknown properties must be explicitly selected. In particular, the accepted IPF completion-policy
  rule rejects unknown enum values before reading or publication; the current consumer's fallback
  behavior is documented separately in the reference.
- A budget reduction below occupied state and outstanding reservations rejects. A supported deferred
  reduction is a separate observable operation. Linked live changes require consistent application
  and explicit failure outcomes; snapshot construction alone does not implement that machinery.
- Admission evolves toward byte budgets accounting for payload ownership and metadata. Scheduling
  quanta, semantic query limits and wire-format bounds stay distinct. Existing 4/128 and symbol-size
  guards remain until a qualified replacement preserves their guarantees.
- The qualified TLS Certificate/CertificateRequest profile remains bounded at 65,536 bytes. Expanding
  it requires dependency-owner and resource qualification; raising a field is insufficient. Normal
  recovery-tool operation is intended to use the common byte profile, with any retained old count
  profile named explicitly and documented in help.

These are accepted requirements, not claims that general loaders, atomic updates, byte-budget
replacement or recovery-tool migration are implemented. Source-specific unknowns in the reference
remain unknown until the owning contract and its checks establish them.

## Java compatibility evidence

The [configured-event baseline](../spec/schema/CONFIGURED-EVENTS.md) records fresh-JVM production
observations for selected startup, endpoint file/API, mutation and XML inputs. Explicit field
selection, nano-time shortcuts, XML defaults and programmatic defaults have distinct rules.
The [endpoint oracle](../tools/upstream/endpoint-configuration-oracle/README.md) records another
bounded family. Neither establishes universal Java precedence or universal live mutability.

Java names, aliases, defaults, units, deprecation and read timing must trace to locked consumers and
captures under `upstream.lock`. The [project settings wiki](https://github.com/dxFeed/dxfeed-graal-cxx-api/wiki/QD-and-dxFeed-Java-API-Project-Settings)
and release notes are discovery aids. [Pinned release notes](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/ReleaseNotes.txt)
describe endpoint-file scheme properties overriding JVM properties for that family; the rule does
not transfer automatically to connectors, logging, monitoring or collectors. Unimplemented Java
families remain explicit compatibility work rather than silently accepted native properties.

## Explicit schema sources

`loadBinaryJsonSchema` selects the [binary profile](../spec/schema/NATIVE-BINARY-JSON.md) with explicit
logical fields, lookups and indexed/time contributors. Success owns a `BINARY_FIELDS_V1` schema with
independent provenance; copies retain identity, separate loads do not. Ordinary `loadJsonSchema`
retains its v1 profile and limits. `BinaryJsonSchemaLimits` intersects input/count/name bounds with
parser-request bytes, logical work credits and two-copy schema-model bytes; model bytes are not an
exact heap/RSS cap. Caller input, retained schemas and downstream copies remain separate.

First pinned simdjson backend selection may read `SIMDJSON_FORCE_IMPLEMENTATION`. That dependency
selection does not resolve application properties; foreign SDK-global mutation remains outside
resource qualification. No XML imports, inferred units/event mappings or live schema replacement
is added. [Validation mode](TAPE-INSPECT.md#binary-json-configuration-validation) finishes without
tape/network work and adds no HISTORY subscription facade.

The inspector's `--wire-schema ordinary-v1` is mutually exclusive with `--schema PATH` and
`--profile NAME`. It resolves at the CLI boundary before tape I/O. Existing record/field/lookup/name options
bound complete union preflight; JSON document/depth options remain loader-only. Wire discovery
remains available without JSON. [WIRE-INVENTORY](../spec/schema/WIRE-INVENTORY.md) bounds discovery;
[TAPE-INSPECT](TAPE-INSPECT.md) specifies version-7 source metadata. No external loader or event
adapter is selected implicitly.

## Selected native endpoint API (QDCXX-073)

These sections describe the low-level `client::EndpointOptions` and controlled example driver.
The public `api::EndpointOptions` and its working TLS/reconnect support have a separate
[Feed contract](FEED-API.md). Statements about a future consumer below concern this low-level handoff.

The selected `client::EndpointOptions` value is created from a validated `core::LocalSchema` and explicit `client::EndpointSettings`. Creation copies the schema and caller-provided strings/settings into an immutable snapshot. The snapshot preserves schema provenance and does not retain the caller's schema or settings objects. Mutating or destroying those inputs after successful creation does not change the snapshot.

The API is declared in [`qd/client/endpoint_options.hpp`](../include/qd/client/endpoint_options.hpp), as part of the optional `qdcxx_client` facility. Its header is usable with the client enabled and runtime disabled; it does not pull in TCP runtime, TLS SDK or JSON-loader headers. See the existing [C++ build guide](CPP-BUILD.md) for component build switches and package setup. This configuration layer performs no environment, file, JSON, Java-properties or global setting lookup. Each endpoint receives its own explicit value snapshot; there is no precedence merge or live reconfiguration.

`EndpointOptions::settings()` and `schema()` return borrowed const views valid while a retaining, non-moved-from snapshot remains alive. Copy a snapshot before releasing the object that currently retains it. Distinct snapshot handles may be read, copied and released on different threads; do not concurrently mutate or destroy the same handle. A moved-from handle may only be reassigned or destroyed.

Schema choice stays explicit: native callers supply a validated `LocalSchema`; the CLI continues to select its existing Quote or Profile schema at its boundary. This API does not infer a schema or add JSON loading.

## Settings and bounds

`EndpointSettings` groups endpoint identity, runtime/security policy, optional TLS handoff data, retry policy, timeouts and `EndpointBudgets`. The component capacities and work limits remain separate typed values; the configuration does not combine them into one byte total or reinterpret their units.

| Setting group | Contract |
| --- | --- |
| Host and port | Explicit, copied values. Host is nonempty, at most 254 ASCII bytes, and rejects NUL, `%`, URI/user-info syntax, brackets, zone IDs and address-property syntax. The configuration shape accepts numeric hosts and bounded ASCII/A-label DNS names. Port is 1–65535. The current STREAM driver still accepts numeric hosts only. |
| Runtime and security | `CALLER_DRIVEN_TCP` and `CREDENTIAL_FREE_PLAIN` are the selected caller-driven defaults. `TLS` is a requested policy value only; it does not mean a TLS runtime exists or has verified a peer. |
| Operation duration | The controlled helper uses the current 5000 ms default; the accepted operation range is 1–600000 ms. This is the stream's finite operation interval, not a hard network deadline. |
| Timeouts | Separate DNS, connect, read, write and poll durations. Current controlled defaults are 1000 ms for DNS/connect, the operation duration for read/write and 1 ms for poll. Present values must be positive and fit `INT_MAX` milliseconds. DNS settings are handoff-only for this numeric-endpoint driver. |
| Retry | Current defaults are one attempt and zero delay, so no reconnect. More than one attempt is handoff-only and requires a positive delay that fits checked duration arithmetic; the current driver rejects retry requests beyond one attempt. |
| TCP capacities | `inputBytes` and `writeBytes` are byte capacities; `addresses` is a count; `cnameDepth` is a CNAME-hop count; `contextBytes`, `handlerBytes` and `residentBytes` are logical byte capacities. The current controlled example uses `{8192, 8192, 16, 4, 65536, 8192, 8388608}` respectively. |
| Runtime-turn work (`EndpointRuntimeWork`) | The seven separate axes are `steps` and `handlers` (work-item counts), `socketBytes` (bytes), `dnsCalls` (calls), `dnsPackets` (packets), `dnsBytes` (bytes) and `dnsUnits` (DNS work units). The current numeric-only controlled example uses `{128, 128, 10000000, 0, 0, 0, 0}` in that order; zero DNS credits do not enable DNS. |
| Other bounded scopes | `EndpointBudgets` contains the existing schema, layout, writer, session, desired-state, collector, client and TCP capacities plus writer-preparation, submit, protocol, dispatch and runtime-turn work. Their meanings and units remain those of the corresponding component contracts. |

`controlledStreamSettings()` supplies the current controlled example's populated defaults, but returns unported settings. The caller must provide a valid host and port and a schema before creating an endpoint snapshot. These current C++ defaults are not Java compatibility promises. The ordinary OS socket send-buffer setting remains with the OS; it is not a new endpoint option.

A snapshot captures settings at creation. The driver reads immutable limits when it opens an owner turn. Work credits replenish only at the existing explicit application-turn boundary, not on each poll or dispatch call. The stream has no lifetime byte or record quota. Configuration/schema storage is a separate logical scope from runtime/session storage and process RSS.

## Using a snapshot with the controlled driver

The new snapshot overload of `runStream` uses the snapshot's schema, host and port. Existing operation-selection fields in `StreamOptions`—record/symbol selection, duration, removal, target and signal—remain in that type. The legacy `runStream` signature, `StreamOptions` aggregate layout, defaults and legacy validation/error behavior stay unchanged.

In the new overload, an empty `StreamOptions::host` and port zero mean that the caller omitted those duplicate values. If supplied, host and port must exactly match the snapshot. A conflict fails configuration validation before this driver touches its borrowed bootstrap or performs its own I/O; this says nothing about activity the caller started earlier. The old overload continues to require and use its legacy host/port inputs.

The snapshot's dispatch-turn operation credit is capped by the unchanged `StreamOptions::ownerOperationsPerTurn` value, whose current controlled range is 1–10000 and default is 10000. The effective operations credit is the smaller of the two. This legacy cap cannot raise snapshot credit. Every other snapshot work axis and nested desired/collector credit remains as configured, including applicable stop, drain and failure paths. There is no second owner-operation budget setting and no silent fallback to helper defaults. If a required initial credit cannot admit preparation, the new path must fail before its own I/O; small later credits remain observable component outcomes, not a reason to increase them.


The current driver’s sufficient retirement profile also requires outer commands >= 1; outer records >= max(1, client.commandEntries, collector.frames + collector.records + schema.records().size() + desired.entries), with checked addition; outer bytes >= max(collector.residentBytes, session.residentBytes); nested collector operations >= 1, visits >= collector.records + collector.frames, and bytes >= collector.frames * session.residentBytes, with checked multiplication; runtime steps and handlers >= 1; and outer operations >= 1. For a full runtime quantum, socketBytes >= max(inputBytes, writeBytes) is a sufficient driver guard. The write floor is conservative for the largest permitted write quantum, not a universal TCP minimum. These checks do not invalidate generic handoff/partial-progress snapshots.

The selected record also requires checked dispatch field credit >= 2 * selectedLocalRecord.fields.size() before this driver’s I/O. On the first dispatcher.progress call of a fresh turn only, an actual WORK_LIMIT with no earlier successful progress is reported visibly and requests ABORT using unchanged credits. A later residual WORK_LIMIT, IDLE or native YIELD retains its normal meaning.

The new path accepts only the existing controlled profile. DNS endpoints, TLS, credential handoff and retry counts above one are rejected by `requirePlainStream()` before this driver's bootstrap/I/O setup. Setting these values does not activate a HISTORY collector, MUX delivery, reconnect, TLS or any other source integration.

## TLS and credential handoff boundary

These settings can own TLS handoff data without implementing TLS. When TLS is selected, the caller supplies a reference identity and PEM trust bytes; the configuration layer checks only their bounded shape and size. The identity is 1–254 ASCII bytes using the bounded numeric-IP or ASCII/A-label DNS presentation; it rejects NUL, wildcard, URI/user-info syntax, brackets, zone IDs and address-property syntax. The PEM input is at most 512 KiB. Configuration does not open files, parse certificates, build a chain or prove SAN/identity/cryptographic verification. Those obligations remain with the future TLS adapter under the [accepted runtime and TLS policy](decisions/ADR-0006-live-runtime.md); no certificate-validation result follows from configuration acceptance.

There is no implicit TLS timeout or token limit. A TLS request must provide positive handshake and close timeouts. If a credential provider is supplied, it must also provide a positive authentication timeout and token capacity of at least seven UTF-16 units, within the configured session text/output envelope. Those authentication settings are invalid without a provider. TLS without Basic authentication remains possible as a handoff shape, but neither TLS nor authentication is executable in the current driver. Credential eligibility may be established only after a future adapter has actually verified TLS; a configuration value never marks a session `VERIFIED_TLS_BASIC`.

The provider writes into a fixed-size borrowed UTF-16 destination and returns a produced-unit count or a static error. The result must be nonempty, no larger than the destination and begin with the exact `Basic ` prefix. The provider must not retain the destination. The configuration factory never calls the provider, allocates a token buffer or materializes credentials. A future runtime invokes it on its serialized owner.

The provider handle is retained by shared ownership, but the provider's state remains caller-owned and caller-synchronized when shared between endpoints. Configuration bounds include the finite shared handle, not arbitrary provider state, captures, retained resources, callback duration or destructor work. Releasing the last snapshot may run the caller's provider destructor on the releasing thread; its destructor must be valid there. No secret erasure guarantee is made for ordinary C++ memory.

## CLI and diagnostics

The existing `qdcxx-connect` CLI keeps its schema/output choices and existing help, parsing, no-argument and exit-status behavior. The CLI additions are the singleton positive-decimal options `--connect-timeout-ms`, `--read-timeout-ms`, `--write-timeout-ms`, `--poll-interval-ms` (each at most `INT_MAX`) and `--security plain|tls`. The current executable remains plain-only. `--security tls` is recognized so it can report an unsupported profile before schema, signal, bootstrap or network setup; it does not accept trust or credential secrets and does not start I/O. No retry flag or duplicate owner-operation budget is added.

Configuration errors and diagnostics use static error/stage labels. They do not include host, PEM, token, provider state or raw argument values. There is no automatic settings dump, serialization or secret-bearing CLI option.

## Installed consumer example

The exact tracked consumer source is [tests/conformance/endpoint_options_consumer/main.cpp](../tests/conformance/endpoint_options_consumer/main.cpp). Its [CMake project](../tests/conformance/endpoint_options_consumer/CMakeLists.txt) links qdcxx_client and qdcxx_quote. It creates two snapshots, changes the input between creations, and verifies that the first keeps its port and timeout. It starts no runtime and performs no network I/O.

```cpp
// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Native installed-package example: explicit immutable configuration only, without runtime/JSON or I/O activation.
#include "qd/client/endpoint_options.hpp"
#include "qd/events/quote.hpp"

#include <iostream>

/** Construct two independent immutable snapshots without any runtime or ambient configuration.
 * @return Zero for preserved independent effective values; nonzero for preparation/output failure.
 */
int main() {
    namespace qdcxx = org::ttldtor::qdcxx;
    auto schema = qdcxx::events::defaultQuoteSchema({16, 128, 256, 128, 16384});
    auto input = qdcxx::client::controlledStreamSettings(std::chrono::milliseconds{2000});

    if (!schema || !input) {
        return 1;
    }

    input->host = "127.0.0.1";
    input->port = 7301;
    auto first = qdcxx::client::EndpointOptions::create(*schema, *input);
    input->port = 7302;
    input->timeouts.read = std::chrono::milliseconds{750};
    auto second = qdcxx::client::EndpointOptions::create(*schema, *input);

    if (!first || !second || !first->requirePlainStream() || !second->requirePlainStream() ||
        first->settings().port != 7301 || second->settings().port != 7302 ||
        first->settings().timeouts.read != std::chrono::milliseconds{2000}) {
        return 1;
    }

    std::cout << "Independent endpoint snapshots: " << first->settings().port << ", " << second->settings().port
              << '\n';
    std::cout.flush();

    return std::cout ? 0 : 1;
}
```

Expected stdout: Independent endpoint snapshots: 7301, 7302. The program exits 0. The installed consumer passed on Windows and Linux.
