#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Upstream,
    [Parameter(Mandatory)][string]$JavaHome,
    [Parameter(Mandatory)][string]$Maven,
    [Parameter(Mandatory)][string]$TestDirectory
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$TestDirectory=[IO.Path]::GetFullPath($TestDirectory)
if (Test-Path $TestDirectory) { throw 'TestDirectory must be new' }
$expected=(Resolve-Path "$PSScriptRoot/../../../tests/fixtures/protocol/expected").Path
function Fingerprint([string]$Directory) {
    (@(Get-ChildItem $Directory -File | Sort-Object Name | ForEach-Object {
        "$($_.Name)`t$((Get-FileHash $_.FullName).Hash)"
    }) -join "`n")
}
$before=Fingerprint $expected
New-Item -ItemType Directory "$TestDirectory/changed-expected" -Force | Out-Null
Copy-Item "$expected/*" "$TestDirectory/changed-expected/"
$changed="$TestDirectory/changed-expected/P04-order.semantic.tsv"
$original=[IO.File]::ReadAllText($changed)
$modified=$original.Replace("0=22(22)", "0=999(999)")
if ($modified -ceq $original) { throw 'Mutation target not found' }
[IO.File]::WriteAllText($changed,$modified,[Text.UTF8Encoding]::new($false))
$changedBefore=Fingerprint "$TestDirectory/changed-expected"
$argsForRun=@{Upstream=$Upstream;JavaHome=$JavaHome;Maven=$Maven;Mode='Verify'}
$caught=$null
try {
    & "$PSScriptRoot/Invoke-Oracle.ps1" @argsForRun -RunDirectory "$TestDirectory/mismatch" `
        -ExpectedDirectory "$TestDirectory/changed-expected"
} catch { $caught=$_.Exception.Message }
if ($caught -cne 'VERIFY FAILED: P04-order.semantic.tsv') { throw "Unexpected mismatch-test outcome: $caught" }
if ((Get-Content "$TestDirectory/mismatch/result.txt" -Raw).Trim() -cne $caught) { throw 'Missing mismatch evidence' }
if ((Fingerprint "$TestDirectory/changed-expected") -cne $changedBefore) { throw 'Verify mutated temporary expectations' }
& "$PSScriptRoot/Invoke-Oracle.ps1" @argsForRun -RunDirectory "$TestDirectory/baseline"
if ((Fingerprint $expected) -cne $before) { throw 'Checked-in baseline changed' }
$proof=[ordered]@{
    changedFile='P04-order.semantic.tsv'; mutation='receiver raw slot 0: 22 -> 999'; observedFailure=$caught
    mismatchDetected=$true; temporaryExpectationsUnchanged=$true; baselinePassed=$true; baselineUnchanged=$true
    expectedFileHashes=$before; changedExpectedFileHashes=$changedBefore
}
[IO.File]::WriteAllText("$TestDirectory/proof.json",($proof | ConvertTo-Json -Depth 4)+"`n",[Text.UTF8Encoding]::new($false))
'VERIFY SELF-TEST PASS: deliberate mismatch detected; baseline passed; expectations unchanged.'


