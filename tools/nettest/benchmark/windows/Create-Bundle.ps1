# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

[CmdletBinding()]
param(
    [string]$OutputDirectory,
    [string]$EventProgram,
    [string]$RecordProgram,
    [string]$SchemaDirectory,
    [string]$QdJarsDirectory,
    [string]$QdLicense,
    [string]$JdkDirectory,
    [string]$PowerShellDirectory,
    [string]$CrtDirectory,
    [switch]$Help
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Create a portable Windows x64 NetTest bundle in a new directory.'
    Write-Output 'Usage: Create-Bundle.ps1 -OutputDirectory DIR -EventProgram FILE -RecordProgram FILE'
    Write-Output '  -SchemaDirectory DIR -QdJarsDirectory DIR -QdLicense FILE -JdkDirectory DIR -CrtDirectory DIR'
    Write-Output '  [-PowerShellDirectory DIR]'
    Write-Output 'Requires JDK 21 with jlink and javac, QD 3.357 jars, Release executables and x64 CRT DLLs.'
    Write-Output 'With PowerShellDirectory the bundle includes PowerShell 7.2+. Otherwise it needs pwsh on PATH.'
    Write-Output 'Copies inputs, creates a Java runtime and writes a hash manifest. The destination must be new.'
    Write-Output 'Exit 0 on success, nonzero on invalid inputs or assembly failure.'
    exit 0
}
foreach ($name in 'OutputDirectory','EventProgram','RecordProgram','SchemaDirectory','QdJarsDirectory','QdLicense','JdkDirectory','CrtDirectory') {
    if (!$PSBoundParameters[$name]) { throw "Missing -$name. Use -Help for details." }
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be new.' }
$EventProgram = (Resolve-Path -LiteralPath $EventProgram).Path
$RecordProgram = (Resolve-Path -LiteralPath $RecordProgram).Path
$SchemaDirectory = (Resolve-Path -LiteralPath $SchemaDirectory).Path
$QdJarsDirectory = (Resolve-Path -LiteralPath $QdJarsDirectory).Path
$QdLicense = (Resolve-Path -LiteralPath $QdLicense).Path
$JdkDirectory = (Resolve-Path -LiteralPath $JdkDirectory).Path
$CrtDirectory = (Resolve-Path -LiteralPath $CrtDirectory).Path
$source = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$bench = Join-Path $source 'tools/nettest/benchmark'
$files = @(
    $EventProgram, $RecordProgram, $QdLicense,
    (Join-Path $SchemaDirectory 'schema.json'),
    (Join-Path $SchemaDirectory 'provenance.json'),
    (Join-Path $JdkDirectory 'bin/jlink.exe'),
    (Join-Path $JdkDirectory 'bin/javac.exe'),
    (Join-Path $CrtDirectory 'msvcp140.dll'),
    (Join-Path $CrtDirectory 'msvcp140_atomic_wait.dll'),
    (Join-Path $CrtDirectory 'vcruntime140.dll'),
    (Join-Path $CrtDirectory 'vcruntime140_1.dll'),
    (Join-Path $bench 'Measure-Stream.ps1'),
    (Join-Path $bench 'StreamReceiver.java')
) + @('qds.jar','dxlib.jar','dxfeed-api.jar' | ForEach-Object { Join-Path $QdJarsDirectory $_ })
foreach ($file in $files) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing input: $file" }
}
$version = (& (Join-Path $JdkDirectory 'bin/java.exe') -version 2>&1 | Out-String)
if ($version -notmatch '(?m)version "21[."]') { throw 'The JDK must be version 21.' }
if ($PowerShellDirectory) {
    $PowerShellDirectory = (Resolve-Path -LiteralPath $PowerShellDirectory).Path
    $pwshVersion = & (Join-Path $PowerShellDirectory 'pwsh.exe') -NoProfile -Command '$PSVersionTable.PSVersion.ToString()'
    if ([version]$pwshVersion -lt [version]'7.2') { throw 'PowerShell 7.2 or later is required.' }
}

$output = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
foreach ($dir in 'bin','schema','lib','tools','licenses') {
    $null = New-Item -ItemType Directory -Path (Join-Path $output $dir)
}
Copy-Item -LiteralPath $EventProgram -Destination (Join-Path $output 'bin/qdcxx-event-nettest.exe')
Copy-Item -LiteralPath $RecordProgram -Destination (Join-Path $output 'bin/qdcxx-nettest.exe')
foreach ($name in 'schema.json','provenance.json') {
    Copy-Item -LiteralPath (Join-Path $SchemaDirectory $name) -Destination (Join-Path $output 'schema')
}
foreach ($name in 'qds.jar','dxlib.jar','dxfeed-api.jar') {
    Copy-Item -LiteralPath (Join-Path $QdJarsDirectory $name) -Destination (Join-Path $output 'lib')
}
foreach ($name in 'msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll') {
    Copy-Item -LiteralPath (Join-Path $CrtDirectory $name) -Destination (Join-Path $output 'bin')
}
Copy-Item -LiteralPath (Join-Path $bench 'Measure-Stream.ps1'),(Join-Path $bench 'StreamReceiver.java') -Destination (Join-Path $output 'tools')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Run-Sweep.ps1'),(Join-Path $PSScriptRoot 'Test-Link.ps1'),(Join-Path $PSScriptRoot 'README.md'),(Join-Path $PSScriptRoot 'README.ru.md') -Destination $output
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'dashboard') -Destination $output -Recurse
Copy-Item -LiteralPath (Join-Path $source 'LICENSE') -Destination (Join-Path $output 'licenses/qd-cxx.txt')
Copy-Item -LiteralPath $QdLicense -Destination (Join-Path $output 'licenses/QD.txt')
if ($PowerShellDirectory) { Copy-Item -LiteralPath $PowerShellDirectory -Destination (Join-Path $output 'powershell') -Recurse }
$modules = 'java.se,jdk.compiler,jdk.crypto.ec,jdk.unsupported'
& (Join-Path $JdkDirectory 'bin/jlink.exe') --add-modules $modules --strip-debug --no-header-files --no-man-pages --compress=zip-6 --output (Join-Path $output 'java')
if (!(Test-Path (Join-Path $output 'java/bin/java.exe')) -or !(Test-Path (Join-Path $output 'java/bin/javac.exe'))) {
    throw 'The resulting Java image must include java.exe and javac.exe.'
}
@'
@echo off
if exist "%~dp0powershell\pwsh.exe" (
  "%~dp0powershell\pwsh.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run-Sweep.ps1" %*
) else (
  pwsh.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run-Sweep.ps1" %*
)
exit /b %errorlevel%
'@ | Set-Content -LiteralPath (Join-Path $output 'run.cmd') -Encoding ASCII
@'
@echo off
if exist "%~dp0powershell\pwsh.exe" (
  "%~dp0powershell\pwsh.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Test-Link.ps1" %*
) else (
  pwsh.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Test-Link.ps1" %*
)
exit /b %errorlevel%
'@ | Set-Content -LiteralPath (Join-Path $output 'link.cmd') -Encoding ASCII
Get-ChildItem -LiteralPath $output -Recurse -File |
    Where-Object {
        $relative = [IO.Path]::GetRelativePath($output, $_.FullName).Replace('\','/')
        $relative -notmatch '^(java|powershell)/'
    } |
    ForEach-Object { [pscustomobject]@{
        file = [IO.Path]::GetRelativePath($output, $_.FullName).Replace('\','/')
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }} | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $output 'manifest.json')
Write-Output "Bundle created: $output"
