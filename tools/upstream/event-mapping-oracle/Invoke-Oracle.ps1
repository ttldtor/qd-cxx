#requires -Version 7.4
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$RunDirectory,
  [Parameter(Mandatory)][string]$Upstream,
  [Parameter(Mandatory)][string]$JavaHome,
  [Parameter(Mandatory)][string]$Maven,
  [Parameter(Mandatory)][string]$Inspector,
  [ValidateSet('Capture','Verify')][string]$Mode='Capture',
  [string]$ExpectedDirectory
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path "$PSScriptRoot/../../..").Path
$run=[IO.Path]::GetFullPath($RunDirectory)
& "$PSScriptRoot/../tape-oracle/Invoke-Oracle.ps1" -Mode Verify -Upstream $Upstream -JavaHome $JavaHome -Maven $Maven -Inspector $Inspector -RunDirectory $run
if ($LASTEXITCODE) { throw 'Clean baseline Verify failed' }
$meta=Get-Content "$run/metadata.json" -Raw | ConvertFrom-Json
$java=Join-Path $JavaHome 'bin/java.exe'
$javac=Join-Path $JavaHome 'bin/javac.exe'
& $javac -encoding UTF-8 -cp $meta.classpath -d "$run/harness" "$PSScriptRoot/EventMappingOracle.java" 1> "$run/event-javac.stdout" 2> "$run/event-javac.stderr"
if ($LASTEXITCODE) { throw 'Event mapping compile failed' }
$cases=[ordered]@{
  default=@(); sequence=@('-Ddxscheme.enabled.Sequence=Quote'); nano=@('-Ddxscheme.enabled.TimeNanoPart=Quote');
  both=@('-Ddxscheme.nanoTime=true'); explicit=@('-Ddxscheme.nanoTime=true','-Ddxscheme.enabled.Sequence=Profile');
  'endpoint-file'=@('-Ddxscheme.nanoTime=true','-Ddxscheme.enabled.Sequence=Profile');
  'endpoint-api'=@('-Ddxscheme.nanoTime=true','-Ddxscheme.enabled.Sequence=Profile');
  mutation=@(); 'set-before'=@(); xml=@(); 'xml-system'=@('-Ddxscheme.nanoTime=true'); 'xml-properties'=@('-Ddxscheme.nanoTime=true','-Ddxscheme.enabled.Sequence=Profile'); 'xml-extension'=@(); 'xml-unsupported'=@();
  reorder=@('-Ddxscheme.nanoTime=true'); minimal=@(); rejected=@(); 'numeric-int'=@();
  'integer-representation'=@('-Ddxscheme.price=int','-Ddxscheme.size=int','-Ddxscheme.shares=int');
  'tiny-unsupported'=@('-Ddxscheme.wide=false'); 'invalid-boolean'=@('-Ddxscheme.nanoTime=not-a-boolean')
}
New-Item -ItemType Directory "$run/events" | Out-Null
$commands=[Collections.Generic.List[object]]::new()
foreach ($case in $cases.GetEnumerator()) {
  $info=[Diagnostics.ProcessStartInfo]::new($java)
  $info.UseShellExecute=$false; $info.CreateNoWindow=$true
  $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
  foreach ($arg in (@('-Xmx256m','-Dfile.encoding=UTF-8','-Duser.language=en','-Duser.country=US','-Duser.timezone=UTC') + $case.Value + @('-cp',$meta.classpath,'com.dxfeed.event.market.EventMappingOracle',"$run/events/$($case.Key)",$case.Key,"$repo/tests/fixtures/events/mapping/inputs"))) { $info.ArgumentList.Add($arg) }
  $start=[DateTime]::UtcNow.ToString('o')
  $p=[Diagnostics.Process]::Start($info)
  $stdout=$p.StandardOutput.ReadToEndAsync(); $stderr=$p.StandardError.ReadToEndAsync()
  if (!$p.WaitForExit(30000)) { $p.Kill($true); throw "Timeout: $($case.Key)" }
  [IO.File]::WriteAllText("$run/$($case.Key).stdout",$stdout.GetAwaiter().GetResult())
  [IO.File]::WriteAllText("$run/$($case.Key).stderr",$stderr.GetAwaiter().GetResult())
  $commands.Add(@{scenario=$case.Key;arguments=@($info.ArgumentList);startedUtc=$start;finishedUtc=[DateTime]::UtcNow.ToString('o');exit=$p.ExitCode;timeoutSeconds=30;heapMiB=256})
  if ($p.ExitCode) { throw "Scenario failed: $($case.Key)" }
}
function HashTree([string]$directory) {
  @(Get-ChildItem $directory -Recurse -File | Sort-Object FullName | ForEach-Object { @{path=[IO.Path]::GetRelativePath($directory,$_.FullName).Replace('\','/');sha256=(Get-FileHash $_.FullName).Hash.ToLowerInvariant()} })
}
@{format='qdcxx-event-mapping-oracle-v1';mode=$Mode;baselineMetadata='metadata.json';commands=@($commands);tools=(HashTree $PSScriptRoot);inputs=(HashTree "$repo/tests/fixtures/events/mapping/inputs");artifacts=(HashTree "$run/events")} | ConvertTo-Json -Depth 10 | Set-Content -Encoding utf8 "$run/event-metadata.json"
if (!$ExpectedDirectory) { $ExpectedDirectory="$repo/tests/fixtures/events/mapping/expected" }
$against=if ($Mode -eq 'Verify') { $ExpectedDirectory } else { "$run/events" }
try {
  & "$PSScriptRoot/Compare-Events.ps1" -Actual "$run/events" -Expected $against 1> "$run/event-verify.stdout" 2> "$run/event-verify.stderr"
} catch {
  [IO.File]::WriteAllText("$run/event-verify.stderr", $_.ToString()+"`n")
  [IO.File]::WriteAllText("$run/event-result.txt", "$Mode FAIL`n")
  throw
}
[IO.File]::WriteAllText("$run/event-result.txt", "$Mode PASS`n")
Get-Content "$run/event-result.txt"



