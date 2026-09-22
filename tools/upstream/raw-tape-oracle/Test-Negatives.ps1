#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunDirectory,[string]$ExpectedDirectory="$PSScriptRoot/../../../tests/fixtures/protocol/raw-tape/expected")
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if (Test-Path -LiteralPath $RunDirectory) { throw 'New negative run directory required' }
New-Item -ItemType Directory -Path $RunDirectory | Out-Null
$run=(Resolve-Path -LiteralPath $RunDirectory).Path
$expected=(Resolve-Path -LiteralPath $ExpectedDirectory).Path
$log=[Collections.Generic.List[string]]::new()
$mutations=@('byte','value','time','order','arity','missing','extra','completion','noncanonical','full-type')
foreach ($mutation in $mutations) {
    $actual=Join-Path $run $mutation
    Copy-Item -LiteralPath $expected -Destination $actual -Recurse
    $file=Join-Path $actual 'raw-quote.tsv'
    $text=[IO.File]::ReadAllText($file)
    switch ($mutation) {
        'byte' { $path=Join-Path $actual 'raw-quote.qtp'; $b=[IO.File]::ReadAllBytes($path); $b[$b.Length-2]=$b[$b.Length-2] -bxor 1; [IO.File]::WriteAllBytes($path,$b) }
        'value' { [IO.File]::WriteAllText($file,$text.Replace("field`t0`t0`t56`t1700000000", "field`t0`t0`t56`t1700000001")) }
        'time' { [IO.File]::WriteAllText($file,$text.Replace('6871947674115899392','6871947674115899393')) }
        'order' { [IO.File]::WriteAllText($file,$text.Replace("record`t0`t", "record`t1`t")) }
        'arity' { [IO.File]::WriteAllText($file,$text.Replace("field`t0`t0`t56`t1700000000", "field`t0`t0`t56")) }
        'missing' { Remove-Item -LiteralPath (Join-Path $actual 'raw-quote-frames.tsv') }
        'extra' { [IO.File]::WriteAllText((Join-Path $actual 'extra.tsv'),'extra') }
        'completion' { [IO.File]::WriteAllText($file,$text.Replace("complete`traw-quote", "complete`tsomething-else")) }
        'noncanonical' { [IO.File]::WriteAllText($file,$text.Replace("field`t0`t0`t56`t1700000000", "field`t0`t0`t56`t01700000000")) }
        'full-type' { [IO.File]::WriteAllText($file,$text.Replace("field`t0`t0`t56`t1700000000", "field`t0`t0`t8`t1700000000")) }
    }
    $rejected=$false
    try { & "$PSScriptRoot/Compare-RawTape.ps1" -Actual $actual -Expected $expected | Out-Null }
    catch { $rejected=$true; $log.Add("$mutation`tREJECTED`t$($_.Exception.Message)") }
    if (!$rejected) { throw "Mutation accepted: $mutation" }
}
& "$PSScriptRoot/Compare-RawTape.ps1" -Actual $expected -Expected $expected | ForEach-Object { $log.Add($_) }
$log.Add('BASELINE PASS; expectations unchanged')
$log | Set-Content -Encoding utf8 (Join-Path $run 'negative-results.tsv')
$log
