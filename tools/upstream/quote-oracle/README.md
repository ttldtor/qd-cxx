# Default Quote production oracle

[Русская версия](README.ru.md). English is authoritative.

QDCXX-022 uses unchanged locked QD 3.355 and its complete default `DXFeedScheme`. The harness
rejects all scheme-property overrides, retains regional records and service providers, selects the
actual `QuoteDelegate` from `MarketFactoryImpl.createStreamOnlyDelegates`, and passes real `Quote`
objects through `putEvent`, production `FileWriter`, production `FileReader` and `createEvent`.
`EventDelegate.createEvent(cursor)` supplies the symbol and dispatches to `QuoteDelegate.getEvent`.
This is a direct production-file observation; it does not exercise a publisher endpoint or connector.

The file writer has exactly BINARY, compression NONE, time NONE, EMPTY_SET and STREAM_DATA enabled.
The complete original `default-quote.qtp`, including descriptor, actual record description and footer,
is retained. No header stripping or schema rewriting occurs. `inputs.tsv` records API inputs,
`put.tsv` records delegate output before serialization, and `default-quote.tsv` records delivered raw
fields and mapped events independently. `schema.tsv` inventories all 331 records and all their local
slots, full type identities, local/property names, plus the selected Quote's eight logical fields.
`primitives.tsv` contains focused field/utility probes. Other QTP files are explicitly diagnostic
inputs constructed by the harness, replayed through the unchanged production FileReader.

PowerShell 7.4+, JDK 8, Maven and the old QTP inspector are required. The original tape oracle runs
first: it checks upstream lock/cleanliness, executes a clean upstream reactor build, records actual
classpath/provider/tool hashes, and verifies every existing tape artifact against unchanged goldens.
No Python is added. Every run directory must be new; use fresh directories when repeating commands.

```powershell
$quoteArgs = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/profile/.m2/wrapper/dists/apache-maven-3.9.16/0daed3be3ebd1c706f0e69e8b07c6b73f5cc4ea3dfce72a8d0ec2e849ca2ddb0/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/naming-final-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome C:/path/to/profile/.jdks/openjdk-23.0.2
& tools/upstream/quote-oracle/Invoke-Oracle.ps1 @quoteArgs -RunDirectory .work/quote-clean-a
& tools/upstream/quote-oracle/Invoke-Oracle.ps1 @quoteArgs -RunDirectory .work/quote-clean-b
& tools/upstream/quote-oracle/Compare-Quote.ps1 -Actual .work/quote-clean-a/quote -Expected .work/quote-clean-b/quote
& tools/upstream/quote-oracle/Test-Negatives.ps1 -RunDirectory .work/quote-negatives
& tools/upstream/quote-oracle/Invoke-Oracle.ps1 @quoteArgs -Mode Verify -RunDirectory .work/quote-verify
```

`Compare-Quote.ps1` requires the exact finite artifact set, typed row arities, canonical integers,
UTF-16/byte/IEEE-bit grammars, full field identities, record and field order, mapping/metadata shape,
completion and record counts, then compares every artifact byte-for-byte. Type, value, order,
record identity, structure, decimal-bit and binary mutations must be rejected. `Capture` validates
its own structure; only `Verify` or cross-capture comparison establishes equality to another set.
No normalization of observations or original bytes is performed. Timestamps and paths in logs and
metadata are run-specific evidence and are not expected to be equal.

See [coverage](../../../tests/fixtures/protocol/quote/COVERAGE.md),
[captures](../../../tests/fixtures/protocol/quote/CAPTURES.md) and
[executed validation](../../../tests/fixtures/protocol/quote/VALIDATION.md).
RAW_DATA, MESSAGE timing, compressed tape, general field conversion, regional event mapping and
the owner's private tape are outside this oracle's acceptance profile.
