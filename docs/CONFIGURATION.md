# Configuration architecture requirements

## Explicit binary JSON configuration

The separately selected [binary profile](../spec/schema/NATIVE-BINARY-JSON.md) is loaded through
`loadBinaryJsonSchema` with explicit logical fields, lookups and indexed/time contributors. Success
owns a `BINARY_FIELDS_V1` schema with independent provenance. Copies retain identity; separate loads
do not. Ordinary `loadJsonSchema` retains its v1 profile and limits. No core seam is added.

`BinaryJsonSchemaLimits` intersects original input/count/name bounds with parser-request bytes,
logical work credits and logical two-copy schema-model bytes. These are separate scopes; model bytes
are not an exact heap/RSS cap. Caller input, retained schemas and downstream copies remain external.
No XML/imports, Java properties, inferred units/event mappings or live replacement is added.
First backend selection by pinned simdjson may read `SIMDJSON_FORCE_IMPLEMENTATION`; the loader
does not resolve application properties or mutate SDK globals. Foreign SDK-global mutation is outside
resource qualification. [Validation mode](TAPE-INSPECT.md#binary-json-configuration-validation)
finishes without tape/network work and adds no HISTORY subscription facade.

## Explicit wire-schema source

The tape inspector's `--wire-schema ordinary-v1` is an explicit source choice,
mutually exclusive with `--schema PATH` and `--profile NAME`. It resolves at the
CLI boundary before tape I/O; core/protocol read no environment, properties or
schema files. [WIRE-INVENTORY](../spec/schema/WIRE-INVENTORY.md) bounds discovery
and [TAPE-INSPECT](TAPE-INSPECT.md) specifies version-7 source metadata. Existing
schema record/field/lookup/name options configure the complete union preflight;
JSON document byte/depth options remain loader-only. No external loader or event
adapter is implicitly selected, and JSON-disabled builds retain wire discovery.

[Русская версия](CONFIGURATION.ru.md). English is authoritative.

The project needs counterparts for relevant Java QD/dxFeed settings across subsystems, not only event schemas.
This remains a broader requirements and investigation effort: there is no general configuration loader or complete compatibility contract. The selected native endpoint API below accepts explicit typed snapshots within a defined boundary.

## Sources and baseline

The implemented QDCXX-025 [schema settings subset](../spec/schema/CONFIGURED-EVENTS.md) adds fresh-JVM
production observations for selected startup, endpoint file/API, mutation and XML inputs. It records
the measured precedence and read timing per setting, including explicit field selection versus nanoTime.
These observations supplement the source facts below; they do not establish a universal precedence
or implement a C++ settings loader. Prepared event mappings consume only explicit validated schemas.

- [QD and dxFeed Java API Project Settings](https://github.com/dxFeed/dxfeed-graal-cxx-api/wiki/QD-and-dxFeed-Java-API-Project-Settings), read 2026-09-23: discovery list, not an exhaustive or pinned specification.
- [Current release notes](https://github.com/devexperts/QD/blob/master/ReleaseNotes.txt): discovery and evolution history; master is mutable.
- [Pinned release notes](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/ReleaseNotes.txt), source consumers and production observations define the baseline under `upstream.lock`.

The pinned QDS 3.353 notes describe renamed scheme properties, retained deprecated aliases, and endpoint-file scheme properties overriding JVM system properties.
That is a documented rule for those settings, not proof of one precedence order for all components.
The pinned `SchemeProperties` source distinguishes explicit field selections from the nano-time shortcut and XML defaults from programmatic defaults.
These are source/document observations, not new runtime captures.

## Architectural requirements

1. Separate source acquisition, precedence resolution, typed validation and component application. Programmatic properties, properties files and launch-time options need a documented route into explicit configuration; environment-variable support and exact C++ CLI syntax require separate decisions.
2. Allow compatible external property names where meaningful, while exposing typed values to consumers. Record deliberate renames/deviations; do not silently accept ineffective settings or imply JVM-only controls work in C++.
3. Define scope and read timing per setting: process default, endpoint, connector, schema or another component; initialization snapshot, component rebuild or explicitly supported live update. A setter call after initialization does not automatically change an existing Java or C++ component.
4. Allow independently configured instances to coexist. Global compatibility defaults may be an explicit resolution input; core/codecs/event mappings do not consult ambient mutable state. Snapshot semantics cannot silently replace an upstream live-update contract; investigate and decide such cases separately.
5. Preserve absent, empty and explicit false/zero distinctions where relevant. Define types, units, ranges, parsing, invalid values, duplicate keys, aliases and source conflicts from evidence. Do not invent a universal priority such as command line over file.
6. Record effective configuration and value origins for diagnostics, with secrets redacted and bounded output. Never put credentials, authenticated addresses or private configuration files into fixtures, manifests or logs.
7. Keep configuration independent of transport implementation. Schema resolution produces the artifact from [ADR-0003](decisions/ADR-0003-configurable-schemas-and-event-mappings.md); network, collector and logging settings are consumed by their owning components, not the schema builder.

## Inventory and compatibility ledger

Create a project-wide ledger as a separately scoped follow-up before general configuration loading and operational-tool configuration are implemented.
For each key or key family record: Java name and aliases; consuming source and pinned revision; subsystem; grammar/type/units/default/range; accepted sources and precedence; read timing/cache behavior; version/deprecation history; sensitivity; C++ disposition and proving tests.
Dispositions are: implemented and verified, planned, unsupported, JVM-specific/not applicable, or unresolved. A discovered key is not automatically supported.
Compare wiki entries with actual consumers and search generated keys as well as literal property calls; release notes explain changes but do not establish completeness.

Cover schema/provider options, endpoint feed/publisher settings, connector/address properties, heartbeat/reconnect, file/replay, subscriptions/collectors/buffers, threading, logging/monitoring and test-only settings.
Prioritize by implemented components and the [qds-tools roadmap](upstream/QDS-TOOLS.md), not by reproducing every internal Java tuning knob.

## Delivery boundary

QDCXX-025 takes the schema-related slice: fresh-JVM source/precedence/timing probes, effective configuration provenance, and explicit resolved schemas for Quote/Profile.
Use the ledger fields above rather than creating a competing property policy in each event adapter.
A selected native [EndpointOptions API](CONFIGURATION.md#selected-native-endpoint-api-qdcxx-073) now provides explicit per-endpoint snapshots. The general C++ properties-file loader, complete settings catalogue, operational-tool properties and live-update policies remain follow-up work in [the roadmap](NEXT-STEPS.md).
Properties requiring unsupported types or subsystems become explicit unsupported/deferred cases; do not coerce their values or change `upstream.lock` to make a case pass.

## Selected native endpoint API (QDCXX-073)

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


[Endpoint configuration](ENDPOINT-CONFIGURATION.md): explicit addresses/properties, immutable snapshots, effective local values and Java compatibility boundaries.
