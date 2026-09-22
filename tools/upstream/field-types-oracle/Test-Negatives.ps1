#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunDirectory,[string]$ExpectedDirectory="$PSScriptRoot/../../../tests/fixtures/encoding/field-types/expected")
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=[IO.Path]::GetFullPath($RunDirectory)
if(Test-Path -LiteralPath $root){throw 'RunDirectory must be new'}
New-Item -ItemType Directory -Path $root | Out-Null
$expected=(Resolve-Path -LiteralPath $ExpectedDirectory).Path
$mutations=@('type','value','unit','null-empty','order','record','arity','missing-field','missing-record','unknown-row','duplicate-row','missing-completion','duplicate-completion','misplaced-completion','extra-file','binary','primitive-consumption')
$results=[Collections.Generic.List[object]]::new()
foreach($mutation in $mutations){
    $dir=Join-Path $root $mutation
    New-Item -ItemType Directory -Path $dir | Out-Null
    Get-ChildItem -LiteralPath $expected -File | Copy-Item -Destination $dir
    $path=Join-Path $dir 'mixed-stream.tsv';$rows=[IO.File]::ReadAllLines($path)
    switch($mutation){
        'type' {$rows[3]=$rows[3].Replace("`t40`t","`t8`t")}
        'value' {$c=$rows[3].Split("`t");$c[5]='2';$rows[3]=$c -join "`t"}
        'unit' {$c=$rows[10].Split("`t");$c[6]='007500610069006c';$rows[10]=$c -join "`t"}
        'null-empty' {$rows[2]=$rows[2].Replace("`tnull","`t-")}
        'order' {$swap=$rows[2];$rows[2]=$rows[3];$rows[3]=$swap}
        'record' {$c=$rows[1].Split("`t");$c[3]='5';$rows[1]=$c -join "`t"}
        'arity' {$rows[2]+="`textra"}
        'missing-field' {$rows=@($rows[0..1]) + @($rows[3..($rows.Count-1)])}
        'missing-record' {$rows=@($rows[0])+@($rows[11..($rows.Count-1)])}
        'unknown-row' {$rows[2]="unknown`t0"}
        'duplicate-row' {$rows=@($rows[0..2])+@($rows[2..($rows.Count-1)])}
        'missing-completion' {$rows=$rows[0..($rows.Count-2)]}
        'duplicate-completion' {$rows+=@($rows[-1])}
        'misplaced-completion' {$rows=@($rows[0],$rows[-1])+@($rows[1..($rows.Count-2)])}
        'extra-file' {[IO.File]::WriteAllText((Join-Path $dir 'unexpected.tsv'),"unexpected`n")}
        'binary' {$p=Join-Path $dir 'mixed-stream.qtp';$b=[IO.File]::ReadAllBytes($p);$b[$b.Length-2]=$b[$b.Length-2] -bxor 1;[IO.File]::WriteAllBytes($p,$b)}
        'primitive-consumption' {$p=Join-Path $dir 'primitives.tsv';$v=[IO.File]::ReadAllText($p);$v=$v.Replace("string-read`tvalue-0-prefix-1`tbuffered`t7f`tnull`tok`t1","string-read`tvalue-0-prefix-1`tbuffered`t7f`tnull`tok`t0");[IO.File]::WriteAllText($p,$v)}
    }
    [IO.File]::WriteAllText($path,($rows -join "`n")+"`n")
    $failed=$false
    try {& "$PSScriptRoot/Compare-FieldTypes.ps1" -Actual $dir -Expected $expected *> "$dir-check.log"}
    catch {$failed=$true;[IO.File]::AppendAllText("$dir-check.log","REJECTED: $_`n")}
    if(!$failed){throw "Mutation accepted: $mutation"}
    $results.Add(@{mutation=$mutation;rejected=$true;utc=[DateTime]::UtcNow.ToString('o')})
}
& "$PSScriptRoot/Compare-FieldTypes.ps1" -Actual $expected -Expected $expected *> "$root/baseline.log"
$results | ConvertTo-Json | Set-Content -Encoding utf8 "$root/results.json"
"PASS: $($mutations.Count) rejected mutations and unchanged typed baseline"
