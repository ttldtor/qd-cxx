# Order-source capability oracle

[Русская версия](README.ru.md). English is authoritative.

Invoke-Oracle.ps1 captures or verifies actual pinned Java production OrderSource
capabilities and IndexedEventSubscriptionSymbol values. It does not rebuild the SDK,
contact a server or prove Feed subscription integration. See [the contract](../../../spec/events/EVENT-SOURCES.md).

~~~powershell
pwsh -NoProfile -File tools/upstream/order-source-capability-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/order-source-capability-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/order-capability-check
~~~

No arguments and -Help print identical EN help; -HelpRu prints RU help. All help is
available without operational paths, directory creation or Java work. Get-Help -Full
provides the same invocation contract. Mode defaults to Verify. Invalid/incomplete
operational invocations fail. Upstream, JavaHome, Logger and RunDirectory are required.
The reference revision must match upstream.lock, sources must match HEAD, production
classes must already exist, and JavaHome must be JDK8. RunDirectory must be inside
this checkout's .work; its own logs/observations may be replaced.

Every Java subprocess, including version, is sequential, bounded to 60 seconds and
128MiB heap. Only its own timed-out process tree is stopped. Full command streams,
source hashes, before/after production-class inventories and private receipts remain
in RunDirectory. Public provenance contains actual Java version and hashes without
command/private-path text. UTC and Pacific/Honolulu must match byte-for-byte.
Capture replaces only tests/fixtures/order-source-capabilities/oracle.tsv and
provenance.json with candidates. Verify changes neither. Exit0 means success/help;
exit1 means invalid inputs, reference/provenance mismatch, timeout, process or I/O failure.

503 rows cover all 64 built-ins and five custom sources across six event families
and full-book support; exact filtered list order; invalid unrelated/null event types;
arbitrary specific source IDs, ID-only symbol equality, bare-symbol distinction and
null-source rejection. Java's plain-symbol ALL semantics are documented in
IndexedEventSubscriptionSymbol; the pure C++ selector does not claim runtime Feed proof.
Raw TSV bytes remain capture output; do not normalize them with a source formatter.
