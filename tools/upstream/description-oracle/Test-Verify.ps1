#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Upstream,
    [Parameter(Mandatory)][string]$JavaHome,
    [Parameter(Mandatory)][string]$Maven,
    [Parameter(Mandatory)][string]$TestDirectory
)
$ErrorActionPreference='Stop'
$TestDirectory=[IO.Path]::GetFullPath($TestDirectory)
if (Test-Path $TestDirectory) {throw 'TestDirectory must be new'}
$expected=(Resolve-Path "$PSScriptRoot/../../../tests/fixtures/protocol/p08/expected/descriptions.tsv").Path
$before=(Get-FileHash $expected).Hash
New-Item -ItemType Directory "$TestDirectory/changed-expected" -Force | Out-Null
$changed="$TestDirectory/changed-expected/descriptions.tsv"
$text=[IO.File]::ReadAllText($expected)
$mutation=$text.Replace('0/0052/0078:8','0/0052/0078:9')
if ($mutation -ceq $text) {throw 'Mutation target missing'}
[IO.File]::WriteAllText($changed,$mutation,[Text.UTF8Encoding]::new($false))
$changedBefore=(Get-FileHash $changed).Hash
$argsForRun=@{Upstream=$Upstream;JavaHome=$JavaHome;Maven=$Maven;Mode='Verify'}
$caught=''
try {
    & "$PSScriptRoot/Invoke-Oracle.ps1" @argsForRun -RunDirectory "$TestDirectory/mismatch" -ExpectedDirectory "$TestDirectory/changed-expected"
} catch {$caught=$_.Exception.Message}
if ($caught -cne 'VERIFY FAILED: descriptions.tsv') {throw "Unexpected mismatch result: $caught"}
if ((Get-FileHash $changed).Hash -cne $changedBefore) {throw 'Temporary expectations changed'}
& "$PSScriptRoot/Invoke-Oracle.ps1" @argsForRun -RunDirectory "$TestDirectory/baseline"
if ((Get-FileHash $expected).Hash -cne $before) {throw 'Baseline changed'}
@{negativeDetected=$true;temporaryUnchanged=$true;baselineUnchanged=$true;baselineVerifyPassed=$true} |
    ConvertTo-Json | Set-Content "$TestDirectory/proof.json"
