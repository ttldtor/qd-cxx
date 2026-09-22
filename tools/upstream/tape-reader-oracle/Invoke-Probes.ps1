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
$repo=(Resolve-Path "$PSScriptRoot/../../..").Path

$run=[IO.Path]::GetFullPath($RunDirectory)
& "$PSScriptRoot/../tape-oracle/Invoke-Oracle.ps1" -Mode Verify -Upstream $Upstream -JavaHome $JavaHome -Maven $Maven -Inspector $Inspector -RunDirectory $run
if ($LASTEXITCODE) { throw 'Baseline clean Verify failed' }
$meta=Get-Content "$run/metadata.json" -Raw | ConvertFrom-Json
$java=Join-Path $JavaHome 'bin/java.exe'
$javac=Join-Path $JavaHome 'bin/javac.exe'
& $javac -encoding UTF-8 -cp $meta.classpath -d "$run/harness" "$PSScriptRoot/TapeReaderOracle.java" 1> "$run/probe-javac.stdout" 2> "$run/probe-javac.stderr"
if ($LASTEXITCODE) { throw 'Probe compile failed' }
$info=[Diagnostics.ProcessStartInfo]::new($java)
$info.UseShellExecute=$false
$info.CreateNoWindow=$true
$info.RedirectStandardOutput=$true
$info.RedirectStandardError=$true
foreach ($arg in @('-Dfile.encoding=UTF-8','-Duser.language=en','-Duser.country=US','-Duser.timezone=UTC','-cp',$meta.classpath,'qdcxx.oracle.TapeReaderOracle',"$repo/tests/fixtures/protocol/tape/expected/constrained.qtp","$run/probes")) { $info.ArgumentList.Add($arg) }
$sourceHash=(Get-FileHash "$PSScriptRoot/TapeReaderOracle.java").Hash
$started=[DateTime]::UtcNow.ToString('o')
$p=[Diagnostics.Process]::Start($info)
$o=$p.StandardOutput.ReadToEndAsync()
$e=$p.StandardError.ReadToEndAsync()
if (!$p.WaitForExit(30000)) { $p.Kill($true); throw 'Probe timeout is not EOF evidence' }
[IO.File]::WriteAllText("$run/probe.stdout",$o.GetAwaiter().GetResult())
[IO.File]::WriteAllText("$run/probe.stderr",$e.GetAwaiter().GetResult())
if ($p.ExitCode) { throw 'Probe failed' }
$evidence=[ordered]@{ startedUtc=$started; finishedUtc=[DateTime]::UtcNow.ToString('o'); executable=$java; arguments=@($info.ArgumentList); exit=$p.ExitCode; baselineMetadata='metadata.json'; sourceSha256=$sourceHash; harnessClasses=@(Get-ChildItem "$run/harness" -Recurse -File | ForEach-Object { @{path=[IO.Path]::GetRelativePath("$run/harness",$_.FullName);sha256=(Get-FileHash $_.FullName).Hash} }); launcherSha256=(Get-FileHash $PSCommandPath).Hash; artifacts=@(Get-ChildItem "$run/probes" -File | Sort-Object Name | ForEach-Object { @{name=$_.Name; sha256=(Get-FileHash $_.FullName).Hash} }) }
$evidence | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 "$run/probe-metadata.json"
Get-Content "$run/probe.stdout"
if (!$ExpectedDirectory) { $ExpectedDirectory=Join-Path $repo 'tests/fixtures/protocol/tape-reader/expected' }
& "$PSScriptRoot/Compare-Probes.ps1" -Actual "$run/probes" -Expected $(if ($Mode -eq 'Verify') { $ExpectedDirectory } else { "$run/probes" }) 1> "$run/probe-verify.stdout" 2> "$run/probe-verify.stderr"
[IO.File]::WriteAllText("$run/probe-result.txt", "$Mode PASS`n")