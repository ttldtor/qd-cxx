# OTC and imbalance binary oracle

[Русская версия](README.ru.md).

The harness observes original physical cursor values and typed fields using the
`BinaryQTPComposer`, `BinaryQTPParser`, `OtcMarketsOrderDelegate` and
`OrderImbalanceDelegate` from the revision in [upstream.lock](../../../upstream.lock).
The default, tiny-decimal, integer and full-order-book profiles use real standard records.
One additional case omits all optional imbalance fields. Six malformed definitions each
omit one required event-specific field and must be refused by the factory.

The harness covers unsigned OTC index bits, indexed removal and snapshot flags, signed
business time and quote access payment, decimal boundaries, reserved flag codes, exact
source names and nullable maker text. The binary fixtures use the `hs` protocol option.
FIELD time is not negotiated in these binary observations, so its Java event time is zero.

Use `Invoke-Oracle.ps1 -Help` or `-HelpRu` without working inputs. To verify the
[checked-in fixture provenance](../../../tests/fixtures/auction-order/provenance.json), supply an already built pinned
QD checkout, an existing JDK 8 and the project's UTC logger.

```powershell
pwsh -File tools/upstream/auction-order-oracle/Invoke-Oracle.ps1 -Mode Verify `
    -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe `
    -RunDirectory .work/auction-order-verify
```

The launcher runs offline with a fresh evidence directory. It compares TSV and QTP bytes
across UTC and Pacific/Honolulu. Capture mode replaces the fixture candidates and
provenance. Verify mode compares the fixtures and records the result in the evidence directory.
