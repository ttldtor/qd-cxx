# Production feed-filter oracle

[Русская версия](README.ru.md). English is authoritative.

These launchers execute the existing Java QD production parser, delegate/category paths,
IPF filter and tracker at the revision in `upstream.lock`. They do not rebuild the reference
SDK. Each child has a128MiB Java heap and a60-second deadline. Processes run sequentially;
only a launcher's own timed-out child tree is stopped.

| Launcher | Captured behavior | Fixture |
| --- | --- | --- |
| `Invoke-Oracle.ps1` | QD glob/composite grammar, standard factories, wildcard projection, first category across real delegates, subscription/data routing, typed selectors | `oracle.tsv` |
| `Invoke-IpfOracle.ps1` | Complete local-file loading, attributes, immutable old values, lazy tracker catchup, equal/failed updates, throwing and removed listeners, stop/restart, endpoint retirement | `ipf-oracle.tsv` |
| `Invoke-ConfigOracle.ps1` | Actual QD TimePeriod, QDConfig properties and IPF config validation | `config-oracle.tsv` |

No arguments, `-Help`, `-HelpRu` and `Get-Help <launcher> -Full` require no inputs and start
no Java, filesystem output or source work. Operational modes require all four explicit paths:

```powershell
pwsh -NoProfile -File tools/upstream/feed-filter-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/filter-check-01
```

Use the corresponding launcher for the IPF and config fixtures. `Verify` is default and
compares actual output with its fixture/provenance. `Capture` replaces only the named TSV
and its paired provenance JSON; prepare and review those candidates before publication.
The IPF harness also creates and removes finite local fixture files within RunDirectory;
it starts no network connection. Initial incomplete-file warnings and deliberate listener
exceptions remain in private raw stderr. A clean EOF and a semantic COMPLETE marker are
separate observations.

RunDirectory must be fresh, inside this checkout's `.work`, with no redirecting ancestor.
Previous evidence is never overwritten. Every run retains complete UTC argv/exit logs,
raw stdout/stderr, source hashes, harness/launcher hashes, actual Java version and separate
counts of production `.class` files and resources. Pre/post inventories of every used
classpath root must match. UTC and Pacific/Honolulu TSV results must match. Source-class
inventory is not an assertion that every class was loaded. No operational private path
is included in public provenance or TSV output.

Exit0 means success or help. Exit1 means invalid input, missing/mismatched pinned reference,
child failure/timeout, class mutation, timezone difference or fixture/provenance mismatch.
Errors point to translated help. Compile-time syntax failures are observations, not skipped
cases. Existing raw captures are retained when the harness is corrected or extended.

The [native contract](../../../spec/subscriptions/FEED-FILTERS.md) distinguishes a QD
wildcard query from ordinary predicate evaluation and limits advertised types to the actual
native registry. Java source TODO comments are not treated as confirmed defects without a
behavioral witness. Native C++ tests consume the captured expression fixture; Java execution
alone does not prove native runtime lifetime, resource or packaging guarantees.
