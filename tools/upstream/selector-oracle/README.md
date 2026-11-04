# Production selector oracle

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` captures or verifies actual pinned Java8 selector equality/hash/construction, real delegate regional/source/wildcard selection and raw time-bound conversion. It invokes existing production classes; no copied equality, generated diagnostic mapping, network, endpoint or SDK rebuild is used. Raw asymmetric Java results remain unchanged. [ADR-0012](../../../docs/decisions/ADR-0012-decorated-subscription-identity.md) classifies the specific native correction; the capture itself makes no real Feed-container bug claim.

No arguments, `-Help`, `-HelpRu` and `Get-Help ./Invoke-Oracle.ps1 -Full` require no paths and start no Java or output work. Operational modes require four explicit paths:

```powershell
pwsh -NoProfile -File tools/upstream/selector-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/selector-check-01
```

`Verify` is default. `Capture` replaces only `tests/fixtures/selectors/oracle.tsv` and `provenance.json`; use it to prepare reviewed candidates. RunDirectory must be fresh, contained within this checkout's `.work`, without redirecting ancestors. Existing evidence is never overwritten. Production source hashes, exact revision, complete class inventories, real JDK version and executable/harness/launcher hashes bind the result. Public provenance contains no operational private paths.

Each subprocess is logged with UTC command/exit and bounded to60s; Java/javac heap is128MiB. Processes run sequentially; only this launcher's own timeout tree is stopped. UTC and Pacific/Honolulu observations must match exactly. Exit0 means success/help; exit1 means invalid inputs, missing/mismatched pinned reference, process failure, timeout or observation/provenance mismatch. Errors point to help; private raw stdout/stderr/command logs and receipts remain in RunDirectory.

The90-row corpus contains20 pair comparisons, constructor/value/collection observations, current Quote regional/source/time-series/wildcard delegate selections,13 signed time bounds and7 source dispatch cases. It includes source -1/-2/MIN/default/known/absent, MIN/MAX milliseconds, fromTime-excluded identity and explicit wildcard versus literal-star distinction. A HashSet insertion example is separate from untested real DXFeed subscription-container behavior. Typed source/time event receiving and arbitrary provider compatibility remain outside this oracle. [Native contract](../../../spec/subscriptions/DECORATED-SELECTORS.md) and separate deterministic installed Feed consumer cover the advertised native boundary.
