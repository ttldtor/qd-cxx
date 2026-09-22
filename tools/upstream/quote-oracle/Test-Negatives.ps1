#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunDirectory,
    [string]$ExpectedDirectory = "$PSScriptRoot/../../../tests/fixtures/protocol/quote/expected"
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=[IO.Path]::GetFullPath($RunDirectory)
if (Test-Path -LiteralPath $root) { throw 'RunDirectory must be new' }
New-Item -ItemType Directory -Path $root | Out-Null
$expected=(Resolve-Path -LiteralPath $ExpectedDirectory).Path
$mutations=@('type','value','order','record','arity','missing-completion','extra-file','decimal-bits','binary')
$results=[Collections.Generic.List[object]]::new()
foreach ($mutation in $mutations) {
    $dir=Join-Path $root $mutation
    New-Item -ItemType Directory -Path $dir | Out-Null
    Get-ChildItem -LiteralPath $expected -File | Copy-Item -Destination $dir
    $path=Join-Path $dir 'default-quote.tsv'
    $rows=[IO.File]::ReadAllLines($path)
    switch ($mutation) {
        'type' { $rows[2]=$rows[2].Replace("`t56`t","`t8`t") }
        'value' { $rows[4]=$rows[4].Replace("`t2566530`t","`t2566531`t"); if ($rows[4] -ceq [IO.File]::ReadAllLines($path)[4]) { $cells=$rows[4].Split("`t"); $cells[4]=([long]$cells[4]+1).ToString(); $rows[4]=$cells -join "`t" } }
        'order' { $swap=$rows[2]; $rows[2]=$rows[3]; $rows[3]=$swap }
        'record' { $rows[1]=$rows[1].Replace("record`t0`t0`t","record`t0`t1`t") }
        'arity' { $rows[2]+="`textra" }
        'missing-completion' { $rows=$rows[0..($rows.Count-2)] }
        'extra-file' { [IO.File]::WriteAllText((Join-Path $dir 'unexpected.tsv'),"unexpected`n") }
        'decimal-bits' { $cells=$rows[4].Split("`t"); $cells[6]='0000000000000000'; $rows[4]=$cells -join "`t" }
        'binary' { $b=[IO.File]::ReadAllBytes((Join-Path $dir 'default-quote.qtp')); $b[$b.Length-2]=$b[$b.Length-2] -bxor 1; [IO.File]::WriteAllBytes((Join-Path $dir 'default-quote.qtp'),$b) }
    }
    [IO.File]::WriteAllText($path,($rows -join "`n")+"`n")
    $failed=$false
    try { & "$PSScriptRoot/Compare-Quote.ps1" -Actual $dir -Expected $expected *> "$dir-check.log" }
    catch { $failed=$true; [IO.File]::AppendAllText("$dir-check.log","REJECTED: $_`n") }
    if (!$failed) { throw "Mutation accepted: $mutation" }
    $results.Add(@{mutation=$mutation;rejected=$true;utc=[DateTime]::UtcNow.ToString('o')})
}
& "$PSScriptRoot/Compare-Quote.ps1" -Actual $expected -Expected $expected *> "$root/baseline.log"
$results | ConvertTo-Json | Set-Content -Encoding utf8 "$root/results.json"
"PASS: $($mutations.Count) rejected mutations and unchanged typed baseline"
