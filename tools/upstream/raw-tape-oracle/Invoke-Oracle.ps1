#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunDirectory,
    [Parameter(Mandatory)][string]$Upstream,
    [Parameter(Mandatory)][string]$JavaHome,
    [Parameter(Mandatory)][string]$Maven,
    [Parameter(Mandatory)][string]$Inspector,
    [ValidateSet('Capture','Verify')][string]$Mode = 'Capture',
    [string]$ExpectedDirectory
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path "$PSScriptRoot/../../..").Path
$run=[IO.Path]::GetFullPath($RunDirectory)
# The existing launcher verifies the locked clean source, rebuilds the full reactor clean,
# records classpath/provider/build provenance and checks every unchanged tape baseline artifact.
& "$PSScriptRoot/../tape-oracle/Invoke-Oracle.ps1" -Mode Verify -Upstream $Upstream -JavaHome $JavaHome -Maven $Maven -Inspector $Inspector -RunDirectory $run
if ($LASTEXITCODE) { throw 'Baseline clean Verify failed' }
$meta=Get-Content "$run/metadata.json" -Raw | ConvertFrom-Json
$java=Join-Path $JavaHome 'bin/java.exe'
$javac=Join-Path $JavaHome 'bin/javac.exe'
& $javac -encoding UTF-8 -cp $meta.classpath -d "$run/harness" "$PSScriptRoot/RawTapeOracle.java" 1> "$run/raw-javac.stdout" 2> "$run/raw-javac.stderr"
if ($LASTEXITCODE) { throw 'Raw compile failed' }
$info=[Diagnostics.ProcessStartInfo]::new($java)
$info.UseShellExecute=$false
$info.CreateNoWindow=$true
$info.RedirectStandardOutput=$true
$info.RedirectStandardError=$true
foreach ($arg in @('-Dfile.encoding=UTF-8','-Duser.language=en','-Duser.country=US','-Duser.timezone=UTC','-cp',$meta.classpath,'qdcxx.oracle.RawTapeOracle',"$run/raw-tape")) { $info.ArgumentList.Add($arg) }
$started=[DateTime]::UtcNow.ToString('o')
$p=[Diagnostics.Process]::Start($info)
$o=$p.StandardOutput.ReadToEndAsync()
$e=$p.StandardError.ReadToEndAsync()
if (!$p.WaitForExit(30000)) { $p.Kill($true); throw 'Raw timeout is not EOF evidence' }
[IO.File]::WriteAllText("$run/raw.stdout",$o.GetAwaiter().GetResult())
[IO.File]::WriteAllText("$run/raw.stderr",$e.GetAwaiter().GetResult())
if ($p.ExitCode) { throw 'Raw run failed' }
$evidence=[ordered]@{
    format='qdcxx-raw-tape-oracle-v1'; mode=$Mode; startedUtc=$started; finishedUtc=[DateTime]::UtcNow.ToString('o')
    executable=$java; arguments=@($info.ArgumentList); exit=$p.ExitCode; baselineMetadata='metadata.json'
    sourceSha256=(Get-FileHash "$PSScriptRoot/RawTapeOracle.java").Hash
    harnessClasses=@(Get-ChildItem "$run/harness" -Recurse -File | ForEach-Object { @{path=[IO.Path]::GetRelativePath("$run/harness",$_.FullName);sha256=(Get-FileHash $_.FullName).Hash} })
    tools=@(Get-ChildItem $PSScriptRoot -File | Where-Object { $_.Name -in @('RawTapeOracle.java','Invoke-Oracle.ps1','Compare-RawTape.ps1','Test-Negatives.ps1') } | Sort-Object Name | ForEach-Object { @{name=$_.Name;sha256=(Get-FileHash $_.FullName).Hash} })
    artifacts=@(Get-ChildItem "$run/raw-tape" -File | Sort-Object Name | ForEach-Object { @{name=$_.Name; sha256=(Get-FileHash $_.FullName).Hash} })
}
$evidence | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 "$run/raw-metadata.json"
if (!$ExpectedDirectory) { $ExpectedDirectory=Join-Path $repo 'tests/fixtures/protocol/raw-tape/expected' }
& "$PSScriptRoot/Compare-RawTape.ps1" -Actual "$run/raw-tape" -Expected $(if ($Mode -eq 'Verify') { $ExpectedDirectory } else { "$run/raw-tape" }) 1> "$run/raw-verify.stdout" 2> "$run/raw-verify.stderr"
[IO.File]::WriteAllText("$run/raw-result.txt", "$Mode PASS`n")
Get-Content "$run/raw.stdout","$run/raw-verify.stdout"


