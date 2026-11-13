# OnDemand synthetic fixtures

[Русская версия](README.ru.md) · [Developer tools](../../dev/README.md)

OnDemandFixtureOracle uses the production `ReplayRequest`, `ReplayResponse`,
`Block`, `QuoteMapping` and `MDRQuote` classes from the Java QD revision
pinned by [upstream.lock](../../../upstream.lock). Build that revision first,
including the `dxfeed-ondemand` module. Use its original classes and service
resources. The generator accepts Java 8 or newer.

The commands below run from the qd-cxx root in PowerShell. Set `$qd` to the
verified QD checkout, and append any dependency entries required by its build
to `$qdEntries`. The build output directory must be writable. The generator
prints TSV with a label and lowercase hex per row. It opens no socket and
writes no file.

```powershell
$qd = 'C:/src/QD'
$pin = (Select-String -Path upstream.lock -Pattern '^commit').Line.Split('"')[1]
if ((git -C $qd rev-parse HEAD) -ne $pin) { throw 'Wrong QD revision' }
$qdEntries = @(Get-ChildItem -Path "$qd/*/target/classes" -Directory |
    ForEach-Object FullName)
$cp = $qdEntries -join [IO.Path]::PathSeparator
$out = '.work/ondemand-fixture-generator'
New-Item -ItemType Directory -Force $out | Out-Null
javac -encoding UTF-8 -cp $cp -d $out tools/upstream/ondemand-oracle/OnDemandFixtureOracle.java
if ($LASTEXITCODE -ne 0) { throw 'Fixture compilation failed' }
java -cp $out OnDemandFixtureOracle --help
java -cp $out OnDemandFixtureOracle --help-ru
java -cp ($out + [IO.Path]::PathSeparator + $cp) OnDemandFixtureOracle --generate
```

No arguments, `--help` and `--help-ru` require only the compiled generator.
The process returns 0 for success, 1 for generation failure and 2 for invalid
arguments. Compare decoded output with the digests below before replacing
[the C++ fixture](../../../tests/fixtures/ondemand/fixture.hpp).

| Row | Bytes | SHA-256 |
| --- | ---: | --- |
| `request` | 131 | `94dcbcea6beeed75595eebf2f628e161fc33cad086786081b0aa960c40a234b4` |
| `response-750ms` | 50 | `fe58185c9a771edd4309d230bd6c13d859349f1c443b070862679f39bb741337` |
| `response-2000ms` | 50 | `16ca0ef7c07020bcdd52daf2dcb057a8c377bcf435089c486183152624572b31` |

The request selects synthetic composite Quote for `MOCK` at
2024-02-12T14:30:05Z. Access fields are synthetic. Each response covers
five seconds before through 60 seconds after the cut. The first Quote is
at the cut, bid 12.25 and ask 12.50, sizes 10 and 20, exchange code Q.
The second Quote is at the indicated offset, bid 13.25 and ask 13.50.

## Local lifecycle verification

The CTest group `on_demand_feed` uses a test-owned IPv4 loopback HTTP peer
and, for return to live, a test-owned plain QTP peer. It verifies the exact
request, typed timestamps and values, access and framing refusal, coverage,
finite EOF, speed/pause, cache clear, live resume and reconnect with a fresh
epoch. It also checks shutdown while HTTP is pending, cancellation of an
unstarted historical callback, and deadline handling while a callback is
already running or before source activation.

```powershell
ctest --test-dir <feed-build> -R '^on_demand_feed$' --no-tests=error --output-on-failure
```

The tested service is the finite synthetic loopback profile. Deployed
OnDemand access, entitlement validation, encrypted HTTP and production
coverage require separate service qualification.
