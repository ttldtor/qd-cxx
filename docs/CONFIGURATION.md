# Configuration architecture requirements

[Русская версия](CONFIGURATION.ru.md). English is authoritative.

The owner requires counterparts of relevant Java QD/dxFeed settings across the project, not only event-schema options.
These are requirements and investigation work, not an implemented configuration subsystem or a complete compatibility contract.

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
The general C++ properties-file loader, complete catalogue, endpoint configuration API and live-update policies remain follow-up work in [the roadmap](NEXT-STEPS.md).
Properties requiring unsupported types or subsystems become explicit unsupported/deferred cases; do not coerce their values or change `upstream.lock` to make a case pass.
