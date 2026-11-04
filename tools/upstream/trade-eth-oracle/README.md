# TradeETH production oracle

[Русская версия](README.ru.md).

`TradeETHOracle.java` observes actual generated `TradeETHMapping`, `TradeETHDelegate` and `TradeBase` getters
from the revision in `upstream.lock`. It constructs complete composite and regional physical
variants from production `DXFeedScheme`, preserving two-slot fields and VOID tails. It does not
replace production numeric, time, flags or symbol algorithms. `TradeId` one-slot neighbor reads are
captured as evidence for the native adapter's explicit incompatible-type boundary.

Run `Get-Help ./Invoke-Oracle.ps1 -Full`, or invoke the script without arguments, with `-Help`, or
with `-HelpRu`. Help needs no operational paths and starts no Java, SDK or network work.

```powershell
pwsh -File tools/upstream/trade-eth-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/qd-reference -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/trade-eth-verify
```

`Mode` defaults to Verify, which compares raw bytes and provenance without replacing fixtures.
Capture replaces only `tests/fixtures/trade-eth/oracle.tsv` and `provenance.json`. The four operational
paths are mandatory: the exact locked checkout with existing built production classes, JDK8,
the UTC logger, and a **new** evidence directory contained in this checkout's `.work`. Redirecting
ancestors and reused run directories are refused. No upstream source or SDK is rebuilt or changed.

Each subprocess is sequential, uses at most a128MiB Java heap and has a60-second timeout. Only the
launcher's own timed-out process tree is stopped. Logs retain full commands, exits, UTC intervals,
source/class inventories and failures in that evidence directory. Java version is extracted from
the actual bounded JDK8 version line; logger service lines are not version data. The fixed capture
uses `dxscheme.nanoTime=true` and explicit TradeETH exchanges Q/z in UTC and Pacific/Honolulu. Q/z are
fixture inputs, not implementation constants. Both timezone outputs must have identical bytes.

Exit0 means successful help or Capture/Verify; exit1 means invalid input, unavailable or mismatched
reference, subprocess failure/timeout, observation mismatch or I/O failure. Capture writes candidate
fixture/provenance and run artifacts; Verify writes only new run artifacts. Neither mode opens a
public feed connection. Preserve failed runs when repeating with a fresh directory.

Public provenance contains revision, source/harness/launcher/JDK hashes, actual Java version,
production-class inventory digest and raw fixture digest; it excludes local paths. Store raw TSV
without Git text conversion. Format the owned harness with the pinned Java formatter before Capture,
then run Verify after the final source freeze. See [TradeETH](../../../spec/events/TRADE-ETH.md) and
[capture layout](../../../tests/fixtures/trade-eth/CAPTURES.md).
