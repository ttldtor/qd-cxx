# Symbol stripe production oracle

[Русская версия](README.ru.md). English is authoritative.

`StripeOracle.java` observes actual pinned Java `SymbolStriper`, MONO/HASH/RANGE factories,
stripe leaves and production filter intersections. It does not reimplement the selection algorithm.
The [symbol stripe contract](../../../spec/core/SYMBOL-STRIPES.md) defines the native string API.
`StripedCollectorOracle.java` additionally observes actual pinned qd-stripe production children, native agents and distributors. The [native MUX composition contract](../../../spec/mux/STRIPES.md) preserves the existing family boundaries; no fixture establishes parallel speedup.

Run from the repository root with PowerShell 7.4+, the explicit existing pinned QD checkout,
its built production classes, JDK8 and the native UTC logger:

```powershell
pwsh -NoProfile -File tools/upstream/stripe-oracle/Invoke-Oracle.ps1 --help
pwsh -NoProfile -File tools/upstream/stripe-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/stripe-check-01
```

No arguments, standalone `--help`/`-Help`, `--help-ru`/`-HelpRu` and `Get-Help -Full` require
no operational inputs and start no workload. `Verify` is the default and compares the reviewed
fixtures and provenance. `Capture` replaces only `tests/fixtures/stripes/oracle.tsv`, `collectors.tsv` and
`provenance.json`; use it to prepare reviewed candidates. All four working paths are required.
The run directory must be new, contained within this checkout's `.work`, with no redirecting
ancestor. Previous evidence is never overwritten. Exit0 means success/help; exit1 means invalid
input/reference, timeout, compilation/process or comparison failure.

The launcher compiles both repository harnesses and the nine exact pinned qd-stripe production sources into the selected private run classes. Existing production classes supply their dependencies; upstream is neither rebuilt nor changed. One child process runs at a time, with a128MiB Java heap and a60s
deadline. Only its own timed-out children are stopped. The two explicit timezones are UTC and
Pacific/Honolulu. No network or source connection starts, and no global properties are changed.

Production source hashes, the exact revision, full production/generated class inventories, actual JDK
version, executable/harness/launcher hashes and fixture hash bind each capture. Full UTC/raw
streams, inventories and a receipt stay in the private selected run directory; public provenance
contains no operational paths. New Java edits require the pinned Google Java Format with a
separate JDK21+, followed by capture/verification of the actual updated provenance.

The fixed cases cover accepted/invalid grammar, canonical names, numeric-factory distinctions,
all stripe leaves, ASCII characters, exact NUL/unpaired/supplementary UTF-16, symbols around
range boundaries, prefixes/spread multipliers, Java cipher/string observations and compatible
or unknown filter overlap. Java's raw wildcard cipher is tested as all-leaf admission separately
from a deterministic string stripe index. Full filters such as `A*` and `A,B` produce unknown
subset metadata and require real predicate evaluation at the upper layer.
The separate collectors.tsv corpus observes all three families with MONO/HASH/RANGE, overlapping independent agent demand, partial retrieval, snapshot providers, replacement, sizes, drops and child closure. HISTORY retained subscription sizes after close are recorded as observed, rather than coerced to zero. Its STREAM ALL scenario distinguishes a stripe hint from a real hard filter: fine hash hint within a coarser child admits both representative symbols; adding the independent predicate admits one.
