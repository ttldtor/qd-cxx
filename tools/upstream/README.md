# Upstream analysis tools

This directory is reserved for reproducible tools that inspect the locked Java QD checkout or capture oracle output for fixtures.

The [schedule oracle](schedule-oracle/README.md) observes complete constructor/default grammar and accepted timezone identities through the actual pinned Java 8 production classes. Capture/Verify uses explicit inputs, fresh bounded run directories and unchanged class inventories. Input-free EN/RU help starts no Java.

Tools must:

- require or verify the expected upstream commit;
- avoid mutating the upstream checkout by default;
- produce deterministic output where possible;
- record invocation and relevant environment details;
- keep generated artifacts separate from hand-maintained specifications.

[schema-oracle](schema-oracle/README.md) implements QDCXX-005: verified locked-source Java builds, 17 bounded schema subcases, raw/normalized captures, provenance metadata and read-only golden verification. QTP tooling and production C++ remain outside this task.

QDCXX-006 adds the [protocol oracle](protocol-oracle/README.md), reusing schema-oracle build/provenance conventions. Never run clean-build harnesses concurrently on one upstream checkout.

The [container oracle](tape-container-oracle/README.md) captures production FileWriter/FileReader
and StreamCompression observations at either accepted baseline/current pin using existing read-only
class directories. Capture/Verify, explicit native UTC logging and built-in help require no upstream
rebuild. The [developer catalog](../dev/README.md#optional-tape-containers-and-production-oracle)
documents the separate manual help regression and optional native compression component.

- [Event-source oracle](event-source-oracle/README.md): exact source identities and invalid-input policy.

- [Candle-period oracle](candle-period-oracle/README.md): exact aggregation types, multipliers and short names.
- [Candle-symbol oracle](candle-symbol-oracle/README.md): production canonical identity, getters, explicit attributes and equality.

- [Event-value oracle](event-values-oracle/README.md): raw-preserving market codes, bit fields, numeric time/day-ID and existing SHORT_STRING business conversions.

The [schedule query oracle](schedule-query-oracle/README.md) captures actual Day/Session/calendar/raw-reference behavior with separate source-derived TZDB2026f data expectations and input-free EN/RU help.

The [serialized attachment oracle](attachment-oracle/README.md) witnesses real Message/Configuration delegates, serialized Object bytes and explicit Java getter failures, with input-free EN/RU help.

- [Time-series oracle](time-series-oracle/README.md): pinned Java8 identity/range/capability and inheritance, bounded Capture/Verify and input-free EN/RU help.
- The [OptionSale oracle](option-sale-oracle/README.md) captures indexed HISTORY events for pinned Java QD schema variants.

- [NuamTrade oracle](nuam-trade-oracle/README.md): actual pinned mapper/delegate statistics, physical variants and regional symbols.
