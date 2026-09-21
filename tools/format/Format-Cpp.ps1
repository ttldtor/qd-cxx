[CmdletBinding()]
param(
    [ValidateSet('Format', 'Check')][string]$Mode = 'Check',
    [string]$Uncrustify,
    [string]$ClangFormat,
    [string[]]$Files
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$lock = Get-Content (Join-Path $PSScriptRoot 'cpp-tools.lock.json') -Raw | ConvertFrom-Json
if (!$Uncrustify) { $Uncrustify = Join-Path $PSScriptRoot ('.work/' + $lock[0].executable) }
if (!$ClangFormat) { $ClangFormat = Join-Path $PSScriptRoot ('.work/' + $lock[1].executable) }
foreach ($tool in @(@($Uncrustify, 'Uncrustify-0\.83\.0(?:\D|$)'), @($ClangFormat, 'clang-format version 20\.1\.8(?:\D|$)'))) {
    if (!(Test-Path -LiteralPath $tool[0] -PathType Leaf)) { throw "Missing formatter: $($tool[0]). See tools/format/CPP.md." }
    $version = & $tool[0] --version 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or $version -cnotmatch $tool[1]) { throw "Wrong formatter version: $version" }
}
foreach ($config in @('.uncrustify.cfg', '.clang-format')) {
    if (!(Test-Path -LiteralPath (Join-Path $repo $config))) { throw "Missing configuration: $config" }
}
if (!$Files) {
    $Files = @(& git -C $repo ls-files --cached --others --exclude-standard -- 'include/*.hpp' 'src/*.cpp' 'tests/conformance/*.cpp' 'tools/qtp-inspect/*.cpp' 'tools/qtp-inspect/*.hpp')
    if ($LASTEXITCODE -ne 0) { throw 'Source discovery failed' }
}
if ($Files.Count -eq 0) { throw 'No C++ source files selected' }
$tempRoot = Join-Path $repo ('.work/format-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$mismatches = @()
try {
    foreach ($file in $Files) {
        $source = [IO.Path]::GetFullPath($file, $repo)
        $relative = [IO.Path]::GetRelativePath($repo, $source).Replace('\', '/')
        if ($relative -notmatch '^(include/|src/|tests/conformance/|tools/qtp-inspect/|\.work/)' -or $relative -match '(^|/)(upstream|generated|fixtures)/' -or [IO.Path]::GetExtension($source) -notin @('.cpp', '.hpp')) {
            throw "Outside formatting scope: $source"
        }
        $copy = Join-Path $tempRoot ([IO.Path]::GetFileName($source))
        Copy-Item -LiteralPath $source -Destination $copy
        $diagnostic = & $Uncrustify -c (Join-Path $repo '.uncrustify.cfg') --no-backup -l CPP $copy 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $diagnostic -match '(?i)(warning|error|unknown|invalid)') { throw "Uncrustify failed: $diagnostic" }
        $diagnostic = & $ClangFormat --Werror "--style=file:$repo/.clang-format" -i $copy 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $diagnostic.Trim()) { throw "clang-format failed: $diagnostic" }
        if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $copy).Hash) {
            if ($Mode -eq 'Format') { Copy-Item -LiteralPath $copy -Destination $source }
            else { $mismatches += $relative }
        }
    }
} finally {
    # Only delete the verified, newly created task-local temporary directory.
    $resolved = [IO.Path]::GetFullPath($tempRoot)
    if (!$resolved.StartsWith((Join-Path $repo '.work/format-'), [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe temporary directory' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
if ($mismatches.Count) { throw ('Formatting mismatch: ' + ($mismatches -join ', ')) }
Write-Output "$Mode passed: $($Files.Count) C++ files (Uncrustify 0.83.0 -> clang-format 20.1.8)"
