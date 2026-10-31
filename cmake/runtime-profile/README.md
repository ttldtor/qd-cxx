# Qualified runtime sources

[Русская версия](README.ru.md). English is authoritative.

Runtime builds use explicit source roots. Configuration never downloads or installs
dependencies. Runtime OFF does not inspect these roots or require their build tools.

| SDK | Source identity | Checked files |
| --- | --- | --- |
| Standalone Asio 1.38.2 | Commit `8806a6803cde7054c3049d3666d3ec36786568c5`; [source receipt](asio-1.38.2.sources.json) | 636 include files and the original BSL-1.0 license |
| c-ares 1.34.9+qdcxx2 | Upstream commit `bc91829cce983a35ec01f5dbc05465858a3df469`; [qualified receipt](c-ares-1.34.9-qdcxx2.sources.json) | 532 source/license files |

The receipts identify original archive hashes and every qualified file. A version
header alone cannot establish that the source used for resource qualification is
the source being compiled. Enabled configuration checks the complete relevant
file set and SHA-256 values before building the SDK.

## Prepare c-ares

Obtain the [original release archive](https://github.com/c-ares/c-ares/releases/download/v1.34.9/c-ares-1.34.9.tar.gz)
explicitly and keep its original MIT notices. Apply the
[cumulative patch](c-ares-1.34.9-qdcxx2.patch) to a fresh extraction of that archive.
It already includes the previous configuration restrictions; do not apply it on
top of qdcxx1. Stock c-ares and qdcxx1 are different, unsupported runtime inputs.

Example from the repository root in PowerShell, with GNU patch from Git for Windows
(adjust the explicitly supplied tool/archive paths for your installation):

```powershell
$runtimeArchive = (Resolve-Path -LiteralPath '.work/deps/c-ares-1.34.9.tar.gz').Path
$runtimeSource = Join-Path $PWD '.work/deps/c-ares-1.34.9-qdcxx2'
$runtimePatch = (Resolve-Path -LiteralPath 'cmake/runtime-profile/c-ares-1.34.9-qdcxx2.patch').Path
$runtimePatchTool = 'C:/Program Files/Git/usr/bin/patch.exe'
if ((Get-FileHash -LiteralPath $runtimeArchive).Hash -ne '9364b70a12eb59a06830fb488d9357f90100dff0d98b23ea3593a183d43e6f2f') {
    throw 'Unexpected upstream archive'
}
if (Test-Path -LiteralPath $runtimeSource) { throw 'Choose a fresh source directory' }
New-Item -ItemType Directory -Path $runtimeSource | Out-Null
tar -xzf $runtimeArchive -C $runtimeSource --strip-components 1
if ($LASTEXITCODE -ne 0) { throw 'Extraction failed' }
& $runtimePatchTool --batch --forward --fuzz=0 -p1 --directory $runtimeSource --input $runtimePatch
if ($LASTEXITCODE -ne 0) { throw 'Patch application failed' }
```

The native GNU patch invocation uses the same flags on Linux. The patch hash is
`65a92251cf4dd90933ec4df743e87d83ff01ac62d4d01d7ed6a9c9b79a020b7e`.
The archive-to-qualified-source procedure was checked on Windows and Linux with
zero fuzz and 532 matching file hashes.

The qdcxx2 correction makes deferred response adoption explicit: failed queue
admission frees the unqueued response, removes connection/timeout linkage and
propagates its error while retaining the query for orderly channel destruction.
No application callback is introduced on the failure branch. Default allocators,
threading restrictions and numeric DNS-server configuration are unchanged.

Four source-level ownership cases use valid objects and a supplied admission-error
status. They check cleanup, once-only destruction and borrowed callback lifetime.
They passed Windows Release and Linux Debug ASan/UBSan; a separate run of these
cases with LeakSanitizer enabled also passed. This is not an executed malloc-failure
reproduction or proof of the complete runtime memory/latency envelope.

## Prepare Asio

Use the [pinned archive](https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz)
with SHA-256 `9f2648fa483e58a6bf848d970ee0ea650ca19ed7769dfa520ed4f7b8d27af1db`.
Extract without modifying the headers and retain `LICENSE_1_0.txt` beside `include`.
Supply that `include` directory as `ORG_TTLDTOR_QDCXX_ASIO_INCLUDE` and the qualified
c-ares root as `ORG_TTLDTOR_QDCXX_CARES_SOURCE` when enabling the runtime.

The older [feasibility profile](../../tests/conformance/transport/runtime-feasibility/README.md)
retains its original qdcxx1 inputs and evidence. Its results are not relabelled as
qdcxx2 tests or complete production resource qualification.
