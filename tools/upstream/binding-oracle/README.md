# Binding production oracle

[Русская версия](README.ru.md). English is authoritative.

QDCXX-014 adds an independently authored probe around locked QD 3.355 BinaryQTPParser. No upstream
source is copied or patched. [Inputs](../../../tests/fixtures/protocol/binding/inputs.tsv) specify
13 fresh-parser scenarios; [coverage](../../../tests/fixtures/protocol/binding/COVERAGE.md) distinguishes
observable delivery from designed plans and source-supported repeated writes.

Invoke-Oracle.ps1 is a scoped copy of the description runner, preserving that runner and its provenance.
It checks origin/commit/tag/version/cleanliness, builds the native reactor with JDK 8, disabled build
cache and skipped upstream tests, and retains input/tool/environment/classpath hashes and commands.
ProtocolOracle's unchanged record/scheme/observer helpers and SchemaOracle's unchanged environment
inspector are compiled and hashed; FramingOracle is compiled as ProtocolOracle's existing dependency.
There is no diagnostic binding walk. Java primitive and PentaCodec writers generate descriptions/data;
the actual parser supplies delivery/error callbacks. No C++ event decoder is added.

Use PowerShell 7.4+, the existing JDK 8/Maven setup from [protocol-oracle](../protocol-oracle/README.md):

```powershell
pwsh -NoProfile -File tools/upstream/binding-oracle/Invoke-Oracle.ps1 -Mode Capture -Upstream <locked-checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
# Review normalized/binding.tsv before explicitly promoting it to binding/expected.
pwsh -NoProfile -File tools/upstream/binding-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <locked-checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <another-new-directory>
```

Shared checkout builds must run sequentially. Capture never promotes expectations; Verify compares
exact file sets and hashes without writing expectations. For a negative Verify, copy binding.tsv into
a temporary expected directory, change `1=101(101)` to `1=999(999)`, pass `-ExpectedDirectory`, require
`VERIFY FAILED: binding.tsv`, then Verify the unchanged baseline. Two clean captures and both Verify
results are retained in [validation](../../../tests/fixtures/protocol/binding/VALIDATION.md).

Optional built-in metadata, external event time, field replacers, XML and optional property overlays
are disabled. Artificial R/S/E records report mappings and type/slot names; service inventory remains
in environment.tsv. These are ordinary exact type-8 probes, not full provider/metadata/conversion parity.
Only CRLF becomes LF in normalized output; Java diagnostics remain raw. Capture timestamps are UTC.
