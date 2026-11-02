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
| Recording and replay | The bounded in-memory RAW_DATA/MESSAGE v1 writer, whole-file strict admission, and optional offline create-only tape rewriter are implemented ([writer](../spec/protocol/TAPE-WRITER.md), [admission validation](../tests/conformance/protocol/tape-write/ADMISSION-VALIDATION.md), [rewrite contract](TAPE-WRITE.md)). The optional synchronous replay library is implemented ([contract](../spec/protocol/OFFLINE-REPLAY.md), [validation](../tests/conformance/replay/VALIDATION.md)). Remaining work includes a real-clock adapter, subscription/data composition, draining and failure/shutdown contracts before equivalents of Connect/Post. Preserve file/profile boundaries and distinguish replay timing from event/business time. |
| Feeds and multiplexing | The receiving side provides shared STREAM/TICKER/HISTORY queues, per-client isolation, bounded slow-consumer handling, local subscription changes and recovery: [application guide](SUBSCRIPTION-RECEIVER.md), [CLI guide](SUBSCRIPTIONS.md). Publishing, a downstream server, exchange adapters, multiple transport channels and event-route provenance remain separate extensions; preserve the existing protocol when designing them. |
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
