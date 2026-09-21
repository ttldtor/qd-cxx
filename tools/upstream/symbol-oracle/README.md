# Symbol oracle

[Русская версия](README.ru.md). English is authoritative.

The independently authored SymbolOracle calls pinned PentaCodec Reader/Writer directly, with
EMPTY_SET and no resolver. Inputs are bounded UTF-16 hex or wire hex, never platform Unicode
conversion. Writer bytes, additional reader forms, strict prefixes, observable state after failures,
reset and a following sentinel are separate rows. After-error probes use separate ByteArrayInput
buffers: they observe primitive state, not production-parser recovery.

The production integration uses BinaryQTPComposer and BinaryQTPParser with an ordinary empty R
record and zero event flags. Composer options are EMPTY_SET; the production parser internally resets
with SUPPORTED_SET (PentaCodec ignores reader options). Seven delivered symbols in two messages
exercise repetition and message-local reset. No diagnostic decoder substitutes for these callbacks.

Invoke-Oracle.ps1 is an additive scoped copy of the binding launcher. It verifies locked clean
upstream origin/commit/tag/version, performs a fresh sequential native JDK 8/Maven reactor build
with upstream tests and build cache disabled, hashes tools/inputs/classes/dependencies, and preserves
raw stdout/stderr, environment and exact commands. Existing helper sources remain unchanged.

```powershell
pwsh -NoProfile -File tools/upstream/symbol-oracle/Invoke-Oracle.ps1 -Mode Capture -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
pwsh -NoProfile -File tools/upstream/symbol-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
```

Capture never promotes goldens. Review normalized/symbols.tsv before copying to
[expected](../../../tests/fixtures/encoding/symbols/expected/symbols.tsv). Verify compares exact file
sets and hashes; a temporary changed expectation must fail, then the unchanged baseline must pass.
See paired [coverage](../../../tests/fixtures/encoding/symbols/COVERAGE.md) and
[validation](../../../tests/fixtures/encoding/symbols/VALIDATION.md). Huge allocations, resolver caches,
other buffers, negotiation, event semantics, transport and upstream test-suite execution are excluded.
