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
& $javac -encoding UTF-8 -cp $meta.classpath -d "$run/harness" "$PSScriptRoot/QuoteOracle.java" 1> "$run/quote-javac.stdout" 2> "$run/quote-javac.stderr"
if ($LASTEXITCODE) { throw 'Quote compile failed' }
$info=[Diagnostics.ProcessStartInfo]::new($java)
$info.UseShellExecute=$false
$info.CreateNoWindow=$true
$info.RedirectStandardOutput=$true
$info.RedirectStandardError=$true
foreach ($arg in @('-Dfile.encoding=UTF-8','-Duser.language=en','-Duser.country=US','-Duser.timezone=UTC','-cp',$meta.classpath,'qdcxx.oracle.QuoteOracle',"$run/quote")) { $info.ArgumentList.Add($arg) }
$started=[DateTime]::UtcNow.ToString('o')
$p=[Diagnostics.Process]::Start($info)
$o=$p.StandardOutput.ReadToEndAsync()
$e=$p.StandardError.ReadToEndAsync()
if (!$p.WaitForExit(30000)) { $p.Kill($true); throw 'Quote timeout is not EOF evidence' }
[IO.File]::WriteAllText("$run/quote.stdout",$o.GetAwaiter().GetResult())
[IO.File]::WriteAllText("$run/quote.stderr",$e.GetAwaiter().GetResult())
if ($p.ExitCode) { throw 'Quote run failed' }
$evidence=[ordered]@{
    format='qdcxx-quote-oracle-v1'; mode=$Mode; startedUtc=$started; finishedUtc=[DateTime]::UtcNow.ToString('o')
    executable=$java; arguments=@($info.ArgumentList); exit=$p.ExitCode; baselineMetadata='metadata.json'
    sourceSha256=(Get-FileHash "$PSScriptRoot/QuoteOracle.java").Hash
    harnessClasses=@(Get-ChildItem "$run/harness" -Recurse -File | ForEach-Object { @{path=[IO.Path]::GetRelativePath("$run/harness",$_.FullName);sha256=(Get-FileHash $_.FullName).Hash} })
    tools=@(Get-ChildItem $PSScriptRoot -File | Where-Object Extension -in '.java','.ps1' | Sort-Object Name | ForEach-Object { @{name=$_.Name;sha256=(Get-FileHash $_.FullName).Hash} })
    artifacts=@(Get-ChildItem "$run/quote" -File | Sort-Object Name | ForEach-Object { @{name=$_.Name; sha256=(Get-FileHash $_.FullName).Hash} })
}
$evidence | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 "$run/quote-metadata.json"
if (!$ExpectedDirectory) { $ExpectedDirectory=Join-Path $repo 'tests/fixtures/protocol/quote/expected' }
& "$PSScriptRoot/Compare-Quote.ps1" -Actual "$run/quote" -Expected $(if ($Mode -eq 'Verify') { $ExpectedDirectory } else { "$run/quote" }) 1> "$run/quote-verify.stdout" 2> "$run/quote-verify.stderr"
[IO.File]::WriteAllText("$run/quote-result.txt", "$Mode PASS`n")
Get-Content "$run/quote.stdout","$run/quote-verify.stdout"
