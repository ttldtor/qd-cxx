# Production tape container oracle

[Русская версия](README.ru.md). English is authoritative.

`ContainerOracle.java` uses actual Java QD FileWriter, FileReader and StreamCompression with one
ordinary deterministic record. It records valid NONE/GZIP/ZIP payload observations, truncated
headers/trailers, CRC failure, trailing bytes, concatenated gzip members and multiple ZIP files.
The direct decompressor check also uses one-byte reads with available=0. Generated mutations are
diagnostic inputs, not a second implementation of the container parser. FileReader catches/logs
I/O errors, so canonical record counts are kept separate from direct decompressor outcomes.

The launcher uses an existing JDK 8 and exactly fourteen existing production class directories.
It compiles only this helper into a fresh output directory, never invokes Maven and compares full
production class inventories before/after. The supplied revision must be the accepted baseline
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` or separate current pin
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Revision identity is supplied by the caller's upstream
provenance; the launcher records actual class hashes and does not authenticate a Git checkout.

## Help and execution

Requires PowerShell 7.4, Windows JDK 8, existing native `qdcxx-run-utc`, and an UTF8 JSON array of
absolute production class directories in runtime order. Help and no arguments require no work
inputs and perform no capture or dependency acquisition:

```powershell
Get-Help ./tools/upstream/tape-container-oracle/Invoke-Oracle.ps1 -Full
pwsh -NoProfile -File tools/upstream/tape-container-oracle/Invoke-Oracle.ps1 -Help
```

With caller-provided paths, capture into a fresh private directory:

```powershell
$parameters = @{
    JavaHome = 'C:/jdk8'
    ClasspathJson = 'C:/private/production-classpath.json'
    Revision = '0ddc0a09779fb61d74cc0b37c6449ff47dcb715f'
    Logger = 'C:/private/dev/qdcxx-run-utc.exe'
}
& ./tools/upstream/tape-container-oracle/Invoke-Oracle.ps1 @parameters `
    -Mode Capture -OutputDirectory 'C:/private/container-capture'
& ./tools/upstream/tape-container-oracle/Invoke-Oracle.ps1 @parameters `
    -Mode Verify -ExpectedDirectory 'C:/private/container-capture' `
    -OutputDirectory 'C:/private/container-verify'
```

`Mode` defaults to Capture. Verify additionally compares exact canonical `results.tsv` bytes with
the expected directory. Output directories must be fresh; existing evidence is never removed or
rewritten. Each child has a30-second timeout; only that owned process tree may be stopped. Exit0
means help or successful capture/verification; exit1 means invalid arguments, timeout, child failure,
changed production inventory or canonical mismatch. Failed raw streams/logs are retained.

## Provenance and limits

Run the repository [Java formatter](../../format/README.md) before capture, then use Capture and
Verify for each changed source/launcher. Metadata records actual source/launcher/JDK/logger hashes,
revision, classpath inventories, argument vectors, UTC intervals and exit status. Raw UTC/stdout/
stderr and generated fixture hashes accompany canonical outcomes. No expectation or historical
hash is silently updated. Source changes require fresh metadata and actual Verify execution.

Evidence output contains local source/output paths and belongs in a private directory. Publish
only reviewed synthetic bytes, safe canonical rows and relative provenance summaries. ZIP writer
entry timestamps may change physical hashes between runs; the verifier compares decoded payload,
failure class and production callback observations, not regenerated archive identity. Original raw
fixture hashes still identify every retained capture exactly.

This tool proves selected production observations; it does not establish native C++ acceptance,
full ZIP compatibility, private-file acceptance, installed runtime behavior or final layered
COMPLETE. [Container contract](../../../spec/protocol/TAPE-CONTAINERS.md) defines the stricter
finite C++ profile. Linux native decoder tests are a separate check; this launcher is Windows-only.
