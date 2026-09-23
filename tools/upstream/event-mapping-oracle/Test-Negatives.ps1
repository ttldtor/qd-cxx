#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunDirectory,[string]$ExpectedDirectory="$PSScriptRoot/../../../tests/fixtures/events/mapping/expected")
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=[IO.Path]::GetFullPath($RunDirectory)
if (Test-Path $root) { throw 'New RunDirectory required' }
New-Item -ItemType Directory $root | Out-Null
$expected=(Resolve-Path $ExpectedDirectory).Path
$before=@(Get-ChildItem $expected -Recurse -File | Sort-Object FullName | Get-FileHash | Select-Object -ExpandProperty Hash)
$mutations=@('quote-value','profile-flags','profile-string','sequence','schema-type','default','unknown-row','duplicate-event','arity','missing-complete','duplicate-complete','trailing-row','missing-file','binary')
$results=[Collections.Generic.List[object]]::new()
foreach ($mutation in $mutations) {
  $dir=Join-Path $root $mutation
  Copy-Item -LiteralPath $expected -Destination $dir -Recurse
  $case=if ($mutation -eq 'sequence') { 'both' } elseif ($mutation -eq 'default') { 'minimal' } else { 'default' }
  $path=Join-Path $dir "$case/observations.tsv"
  $rows=[Collections.Generic.List[string]]::new([string[]][IO.File]::ReadAllLines($path))
  $index=0
  $prefix=if ($mutation -in 'profile-flags','profile-string','default') { 'profile' } elseif ($mutation -eq 'schema-type') { 'schema' } else { 'quote' }
  for ($i=0;$i -lt $rows.Count;$i++) { if ($rows[$i].StartsWith("$prefix`t") -and ($prefix -eq 'schema' -or $rows[$i].Contains('-read'))) { $index=$i; break } }
  $cells=$rows[$index].Split("`t")
  switch ($mutation) {
    'quote-value' { $cells[6]='3ff0000000000000'; $rows[$index]=$cells -join "`t" }
    'profile-flags' { $cells[17]='0'; $rows[$index]=$cells -join "`t" }
    'profile-string' { $cells[18]='-'; $rows[$index]=$cells -join "`t" }
    'sequence' { $cells[13]='0'; $rows[$index]=$cells -join "`t" }
    'schema-type' { $cells[5]='8'; $rows[$index]=$cells -join "`t" }
    'default' { $cells[4]='0000000000000000'; $rows[$index]=$cells -join "`t" }
    'unknown-row' { $rows[$index]='unknown' }
    'duplicate-event' { $rows.Insert($index,$rows[$index]) }
    'arity' { $rows[$index]+="`textra" }
    'missing-complete' { $rows.RemoveAt($rows.Count-1) }
    'duplicate-complete' { $rows.Add($rows[$rows.Count-1]) }
    'trailing-row' { $rows.Add('unknown') }
    'missing-file' { Remove-Item -LiteralPath (Join-Path $dir 'default/Quote-stream.qtp') }
    'binary' { $p=Join-Path $dir 'default/Quote-stream.qtp'; $b=[IO.File]::ReadAllBytes($p);$b[$b.Length-2]=$b[$b.Length-2] -bxor 1;[IO.File]::WriteAllBytes($p,$b) }
  }
  [IO.File]::WriteAllText($path,($rows -join "`n")+"`n")
  $failed=$false
  try { & "$PSScriptRoot/Compare-Events.ps1" -Actual $dir -Expected $expected *> "$root/$mutation.log" }
  catch { $failed=$true; [IO.File]::AppendAllText("$root/$mutation.log","REJECTED: $_`n") }
  if (!$failed) { throw "Mutation accepted: $mutation" }
  $results.Add(@{mutation=$mutation;rejected=$true;utc=[DateTime]::UtcNow.ToString('o')})
}
$after=@(Get-ChildItem $expected -Recurse -File | Sort-Object FullName | Get-FileHash | Select-Object -ExpandProperty Hash)
if (($before -join ',') -cne ($after -join ',')) { throw 'Expectations changed' }
& "$PSScriptRoot/Compare-Events.ps1" -Actual $expected -Expected $expected *> "$root/baseline.log"
$results | ConvertTo-Json | Set-Content -Encoding utf8 "$root/results.json"
"PASS: $($mutations.Count) rejected mutations; expectations unchanged"
