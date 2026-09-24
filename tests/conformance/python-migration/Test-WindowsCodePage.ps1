#Requires -Version 7.4
<#
.SYNOPSIS
Run the affected CLI suites with isolated Windows-1252 executable copies.
.DESCRIPTION
Requires Windows 11 locale-specific activeCodePage manifests and the Windows SDK mt.exe.
Only fresh ignored executable copies receive manifests; source binaries and system locale stay unchanged.
Run this script under qdcxx_no_python_guard to retain the complete process trace.
#>
param(
    [Parameter(Mandatory)][string]$BinaryDirectory,
    [Parameter(Mandatory)][string]$ManifestTool,
    [Parameter(Mandatory)][string]$EvidenceRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (!$IsWindows) { throw 'This control requires Windows 11' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$BinaryDirectory = (Resolve-Path -LiteralPath $BinaryDirectory).Path
$ManifestTool = (Resolve-Path -LiteralPath $ManifestTool).Path
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
[IO.Directory]::CreateDirectory($EvidenceRoot) | Out-Null
$work = Join-Path $repo ('.work/python-migration-cp1252-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$cmake = (Get-Command cmake -CommandType Application | Select-Object -First 1).Source
$sources = @{}
$copies = @{}
$manifest = Join-Path $work 'code-page.manifest'
[IO.File]::WriteAllText($manifest, @'
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0" xmlns:asmv3="urn:schemas-microsoft-com:asm.v3">
  <asmv3:application>
    <asmv3:windowsSettings xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">
      <activeCodePage>en-US</activeCodePage>
    </asmv3:windowsSettings>
  </asmv3:application>
</assembly>
'@)
foreach ($name in @('qdcxx-qtp-inspect','qdcxx_inspection_cli_tests','qdcxx_inspection_process_tests')) {
    $source = Join-Path $BinaryDirectory "$name.exe"
    $copy = Join-Path $work "$name.exe"
    $sources[$source] = (Get-FileHash -LiteralPath $source).Hash
    Copy-Item -LiteralPath $source -Destination $copy
    $originalManifest = Join-Path $work "$name.original.manifest"
    & $ManifestTool '-nologo' "-inputresource:$copy;#1" "-out:$originalManifest"
    if ($LASTEXITCODE -ne 0) { throw "Cannot extract manifest: $name" }
    & $ManifestTool '-nologo' '-manifest' $originalManifest $manifest "-outputresource:$copy;#1"
    if ($LASTEXITCODE -ne 0) { throw "Cannot embed code page: $name" }
    & $ManifestTool '-nologo' "-inputresource:$copy;#1" "-out:$(Join-Path $EvidenceRoot "$name.manifest.txt")"
    if ($LASTEXITCODE -ne 0) { throw "Cannot retain manifest: $name" }
    $copies[$name] = @{path=$copy;sha256=(Get-FileHash -LiteralPath $copy).Hash}
}
$helper = $copies['qdcxx_inspection_process_tests'].path
$codePage = & $helper --code-page
if ($LASTEXITCODE -ne 0 -or $codePage -cne '1252') { throw "Expected actual GetACP()=1252, received $codePage" }
Write-Output "Observed GetACP()=$codePage in manifested helper"
$program = $copies['qdcxx-qtp-inspect'].path
$checker = $copies['qdcxx_inspection_cli_tests'].path
$fixtures = Join-Path $repo 'tests/fixtures/protocol/offline-inspection'
# Demonstrate the unchanged production boundary with the same immutable input.
$unicodeInput = Join-Path $work 'space Юникод;literal.qtp'
Copy-Item -LiteralPath (Join-Path $fixtures 'expected/composed.qtp') -Destination $unicodeInput
& $program --profile oracle-sre-v1 --input $unicodeInput 1> (Join-Path $work 'unicode.out') 2> (Join-Path $work 'unicode.err')
if ($LASTEXITCODE -ne 3 -or (Get-Item -LiteralPath (Join-Path $work 'unicode.out')).Length -ne 0) {
    throw 'Expected unchanged narrow-argv Unicode path limitation at Windows-1252'
}
Write-Output 'Unchanged narrow-argv production boundary: Cyrillic input path rejected with exit 3'
foreach ($suite in @('offline_cli','human_cli','inspection_cli_negatives')) {
    $fixtureRoot = if ($suite -eq 'human_cli') { Join-Path $repo 'tests/fixtures/protocol/human-inspection' } else { $fixtures }
    & $cmake "-DPROGRAM=$program" "-DCHECKER=$checker" "-DPROCESS_HELPER=$helper" "-DFIXTURES=$fixtureRoot" `
        "-DRUN_DIR=$(Join-Path $work $suite)" -P (Join-Path $repo "tests/conformance/$suite.cmake")
    if ($LASTEXITCODE -ne 0) { throw "Windows-1252 suite failed: $suite" }
}
& $helper $program (Join-Path $fixtures 'expected/composed.qtp') (Join-Path $work 'process')
if ($LASTEXITCODE -ne 0) { throw 'Windows-1252 process contract failed' }
foreach ($source in $sources.Keys) {
    if ((Get-FileHash -LiteralPath $source).Hash -ne $sources[$source]) { throw "Original binary changed: $source" }
}
@{utc=[DateTime]::UtcNow.ToString('o');os=[Environment]::OSVersion.VersionString;codePage=[int]$codePage;
    manifestTool=$ManifestTool;originalHashes=$sources;isolatedCopies=$copies;productionUnicodePathExit=3;
    suites=@('offline_inspection_cli','human_inspection_cli','inspection_cli_negatives','inspection_process_contract');
    result='passed';systemLocale='unchanged';originalBinaries='unchanged'} |
    ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8NoBOM (Join-Path $EvidenceRoot 'result.json')
Write-Output 'PASS Windows-1252: complete offline/human/negative/process suites; original binaries unchanged'
