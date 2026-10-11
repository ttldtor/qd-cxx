# Product roadmap

[Русская версия](NEXT-STEPS.ru.md). English is authoritative.

The product combines a reusable C++23 library with operational utilities analogous in purpose to
the necessary Java qds-tools. The [locked utility inventory](upstream/QDS-TOOLS.md) establishes
actual commands and dependencies. Native command names, argument syntax, address strings and
configuration compatibility need explicit contracts; purpose equivalence does not promise Java CLI parity.

## Available foundation

[Current capabilities](CURRENT-STATE.md) include bounded tape inspection, exact supported field
representations, prepared Quote/Profile mappings, external native JSON and offline Java schema export.
Java currently resolves controlled XML/imports and supported settings; C++ consumes the saved schema.

## Remaining workstreams

| Workstream | Required outcome and prerequisites |
| --- | --- |
| Real-event coverage | Extend deterministic production Java writer/reader observations and C++ conformance for unchanged standard schemas. Inventory all identities, values, flags and timing first; do not simplify a real schema to fit current support. Offline data generation is sufficient; a demo endpoint is not required. |
| Recording and replay | Define bounded writing, subscription/data composition, stream timing, draining and failure/shutdown contracts before implementing equivalents of Connect/Post. Preserve file/profile boundaries and distinguish replay timing from event/business time. |
| Feeds and multiplexing | Define aggregation, per-client isolation, wildcard/ticker/stream semantics, changing subscriptions, reconnect and finite slow-consumer buffers. Network transport and threading require their own documented lifecycle and ownership contracts. |
| Configuration and sources | Continue the [configuration inventory](CONFIGURATION.md), verifying locked consumers, defaults, aliases, units, precedence and read timing. Native XML construction, broader provider/settings resolution, indexed schemas and live replacement remain distinct extensions. Preserve custom records and extra fields. |
| Operational diagnostics | Build Compare/FileAnalysis, subscription diagnostics and bounded load testing on established schema/file/transport capabilities. IPF remains a separate domain; RMI/JVM/Launcher equivalence is undecided. |
| Tooling and platforms | Preserve mandatory checks while replacing remaining manual Python workflows with equivalent positive/negative verification. See the [migration ledger](PYTHON-MIGRATION.md). macOS remains unverified; Windows/Linux evidence does not establish macOS behavior. |

The next implementation scope is not selected by this roadmap. Each extension needs a bounded
contract and conformance evidence. Additional types, conversions, session policies, compression,
flags and transport must not silently widen existing profiles.

## Compatibility gates

Use [open questions](upstream/OPEN-QUESTIONS.md) and [remaining tape gaps](REAL-TAPE-GAPS.md) to select
focused production-path probes. Source inspection, diagnostic walkers and private aggregate
observations do not replace end-to-end measurements. Update specifications, architecture decisions
and fixtures together when observable semantics change.
