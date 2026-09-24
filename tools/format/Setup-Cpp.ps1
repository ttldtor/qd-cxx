#Requires -Version 7.4
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$IsWindows) { throw 'Setup-Cpp.ps1 acquires Windows x64 formatters only.' }
. (Join-Path $PSScriptRoot 'Setup-Cpp.Private.ps1')
$tools = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'cpp-tools.lock.json') -Raw | ConvertFrom-Json
foreach ($tool in $tools) {
    Install-LockedCppTool -Tool $tool -Cache (Join-Path $PSScriptRoot '.work')
}
