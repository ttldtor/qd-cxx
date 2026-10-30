#requires -Version 7.4
<#
.SYNOPSIS
Checks the corpus launcher's side-effect-free help and incomplete-argument rejection.
.DESCRIPTION
Creates only a new result directory below repository .work. Calls help from an empty working
directory with no JDK, Maven, source or producer arguments. No build or Java process is started.
.PARAMETER RunDirectory
New directory for separate stdout/stderr observations. Required for test execution.
.PARAMETER Help
Show usage without creating files. No arguments has the same behavior.
.NOTES
Exit 0: help or checks passed. Exit 1: invalid input or failed check. No external processes or
network connections are created. Results contain help text and the expected usage errors.
#>
[CmdletBinding()]
param([string]$RunDirectory, [switch]$Help)
if ($Help -or $PSBoundParameters.Count -eq 0) {
    'Test-Help.ps1 -RunDirectory NEW_REPOSITORY_WORK_PATH; no arguments / -Help prints help only.'
    return
}
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$RunDirectory) { throw 'Missing -RunDirectory' }
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
$RunDirectory = [IO.Path]::GetFullPath($RunDirectory)
$workPrefix = (Join-Path $repo '.work') + [IO.Path]::DirectorySeparatorChar
if (!$RunDirectory.StartsWith($workPrefix, [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $RunDirectory)) {
    throw 'RunDirectory must be new and below repository .work'
}
$ancestor = $RunDirectory
while ($ancestor) {
    if ((Test-Path -LiteralPath $ancestor) -and ((Get-Item -LiteralPath $ancestor).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Reparse points are not allowed for working paths'
    }
    $parent = Split-Path $ancestor -Parent
    if (!$parent -or $parent -eq $ancestor) { break }
    $ancestor = $parent
}
New-Item -ItemType Directory -Path $RunDirectory | Out-Null
$empty = New-Item -ItemType Directory -Path "$RunDirectory/empty-cwd"
$launcher = Join-Path $PSScriptRoot 'Invoke-Corpus.ps1'
Push-Location $empty.FullName
try {
    & $launcher 1> "$RunDirectory/noargs.stdout" 2> "$RunDirectory/noargs.stderr"
    & $launcher -Help 1> "$RunDirectory/help.stdout" 2> "$RunDirectory/help.stderr"
    if ((Get-Content "$RunDirectory/noargs.stdout" -Raw) -cne (Get-Content "$RunDirectory/help.stdout" -Raw) -or
        (Get-Item "$RunDirectory/noargs.stderr").Length -ne 0 -or (Get-Item "$RunDirectory/help.stderr").Length -ne 0) {
        throw 'No-argument/help output contract failed'
    }
    foreach ($arguments in @(@{ Mode = 'Verify' }, @{ Unknown = 'invalid' })) {
        $rejected = $false
        try { & $launcher @arguments } catch { $rejected = $true; $_.Exception.Message | Add-Content "$RunDirectory/rejections.txt" }
        if (!$rejected) { throw 'Invalid/incomplete arguments were accepted' }
    }
    if (@(Get-ChildItem -Force).Count -ne 0) { throw 'Help or usage started file work' }
} finally { Pop-Location }
'PASS: no-argument/help equivalence, empty stderr, no working inputs/files, two usage rejections.'
