#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Actual,[Parameter(Mandatory)][string]$Expected)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$arity=@{case=2;startup=3;record=7;schema=8;lookup=5;quote=17;profile=23;cursor=6;value=7;replay=5;reject=4;unsupported=3;complete=2}
$cases=@('default','sequence','nano','both','explicit','endpoint-file','endpoint-api','mutation','set-before','xml','xml-system','xml-properties','xml-extension','xml-unsupported','reorder','minimal','rejected','numeric-int','integer-representation','tiny-unsupported','invalid-boolean')
function Inventory([string]$root) {
  if (((@(Get-ChildItem $root -Directory).Name | Sort-Object) -join ',') -cne (($cases | Sort-Object) -join ',')) { throw "Scenario inventory mismatch: $root" }
  foreach ($case in $cases) {
    $files=@('observations.tsv')
    if ($case -notin 'rejected','xml-unsupported') { $files+=@('Quote-stream.qtp','Quote-raw.qtp','Profile-stream.qtp','Profile-raw.qtp','Quote-wire-omitted.qtp','Profile-wire-omitted.qtp') }
    if (((@(Get-ChildItem "$root/$case" -File).Name | Sort-Object) -join ',') -cne (($files | Sort-Object) -join ',')) { throw "Artifact inventory mismatch: $case" }
    $lines=[IO.File]::ReadAllLines("$root/$case/observations.tsv")
    if ($lines.Length -lt 2 -or $lines[0] -cne "case`t$case" -or $lines[-1] -cne "complete`t$case") { throw "Incomplete: $case" }
    $complete=0
    foreach ($line in $lines) {
      $cells=$line.Split("`t")
      if (!$arity.ContainsKey($cells[0]) -or $cells.Length -ne $arity[$cells[0]]) { throw "Bad row: $case : $line" }
      if ($cells[0] -eq 'complete') { $complete++ }
    }
    if ($case -notin 'rejected','xml-unsupported') {
      foreach ($event in @('quote','profile')) {
        $eventName=if ($event -eq 'quote') { 'Quote' } else { 'Profile' }
        $timeColumn=if ($event -eq 'quote') { 15 } else { 22 }
        foreach ($mode in @('stream','raw')) {
          $put=@($lines | Where-Object { $_.StartsWith("$event`t$eventName-$mode-put`t") })
          $read=@($lines | Where-Object { $_.StartsWith("$event`t$eventName-$mode-read`t") })
          if ($put.Count -ne 3 -or $read.Count -ne 3) { throw "Event count: $case $event $mode" }
          for ($i=0;$i -lt 3;$i++) {
            $pc=$put[$i].Split("`t"); $rc=$read[$i].Split("`t")
            if ($rc[$timeColumn] -cne $(if ($mode -eq 'raw') { '1600000000123' } else { '0' })) { throw 'Event metadata mismatch' }
            $pc[1]=$rc[1]; $pc[$timeColumn]=$rc[$timeColumn]
            if (($pc -join "`t") -cne ($rc -join "`t")) { throw "Delegate/FileReader event mismatch: $case $event $mode $i" }
          }
        }
      }
    }
    if ($complete -ne 1) { throw "Duplicate completion: $case" }
  }
  @(Get-ChildItem $root -Recurse -File | ForEach-Object { [IO.Path]::GetRelativePath([IO.Path]::GetFullPath($root),$_.FullName).Replace('\','/') } | Sort-Object)
}
$a=@(Inventory $Actual); $e=@(Inventory $Expected)
if (($a -join ',') -cne ($e -join ',')) { throw 'Artifact set mismatch' }
foreach ($file in $a) {
  if ((Get-FileHash "$Actual/$file").Hash -cne (Get-FileHash "$Expected/$file").Hash) { throw "Byte mismatch: $file" }
}
Write-Output "PASS: $($cases.Count) scenarios, $($a.Count) artifacts, all bytes and row shapes"




