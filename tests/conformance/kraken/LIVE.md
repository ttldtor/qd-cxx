# Verify live Kraken quote delivery

[Русская версия](LIVE.ru.md).

This check runs a finite public BTC/USD session and compares the gateway output with two independent native Feed client processes. It verifies the bid and ask prices and sizes by their exact raw wide-decimal values. For each client it checks the symbol and the zero Quote metadata defined by the gateway profile. TICKER can combine intermediate updates. A shared set of quotes proves delivery during this run.

## Run the check

Use the [gateway build](../../../tools/kraken/README.md), a native Quote client built as described in the [offline qualification guide](README.md), and explicit PEM trust for the current Kraken chain. For the native client alone, configure its standalone test project with `-DORG_TTLDTOR_QDCXX_KRAKEN_TEST_PEER=OFF`. This build requires only the installed Feed SDK. Use PowerShell 7.2 or later.

```powershell
./tests/conformance/kraken/Test-Live.ps1 -Help
./tests/conformance/kraken/Test-Live.ps1 `
  -Gateway ./stage/kraken/bin/qdcxx-kraken-gateway.exe `
  -Client ./build/kraken-native/qdcxx_kraken_quote_client.exe `
  -TrustPem ./kraken-ca.pem -Output ./.work/kraken-live
```

Choose a new output directory for each run. The check starts the gateway on an ephemeral IPv4 loopback port and starts the two clients only after the gateway reports its bound port. All three processes must exit cleanly. The acceptance receipt records an admitted matching quote and per-client counts. Complete stdout and stderr stay in the output directory.

## Observed run

On 15 November 2026 at 13:02 UTC, an MSVC Release gateway and two external Feed processes completed the check. The installed gateway code is revision `5854e27a` and the install rules are revision `939affef`. The native client is revision `5451d3fe`, built against that relocated SDK. Client A received 152 quotes and client B received 138. There were 127 distinct BBO tuples shared by both clients and the gateway admissions. The 13:02:55.074309 UTC source observation was bid 65533.8 and ask 65533.9, with sizes 0.00057223 and 5.32993112. All three processes exited successfully and the gateway reported zero pending publications. The raw tuple was `167766657 167766913 14649224 136446236808`.
