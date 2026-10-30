#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunDirectory,[string]$ExpectedDirectory="$PSScriptRoot/../../../tests/fixtures/encoding/field-types/expected")
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=[IO.Path]::GetFullPath($RunDirectory)
if(Test-Path -LiteralPath $root){throw 'RunDirectory must be new'}
New-Item -ItemType Directory -Path $root | Out-Null
$expected=(Resolve-Path -LiteralPath $ExpectedDirectory).Path
$mutations=@('type','value','unit','null-empty','order','record','arity','missing-field','missing-record','unknown-row','duplicate-row','missing-completion','duplicate-completion','misplaced-completion','extra-file','binary','primitive-consumption','scalar-value','scalar-wire-type','scalar-consumed','scalar-byte','scalar-missing','scalar-fanout','object-null-empty','object-byte','object-consumed','object-missing','object-local-type')
$mutations+=@('indexed-time','indexed-slot','indexed-lookup','indexed-last-winner','indexed-missing','indexed-binary')
$results=[Collections.Generic.List[object]]::new()
foreach($mutation in $mutations){
    $dir=Join-Path $root $mutation
    New-Item -ItemType Directory -Path $dir | Out-Null
    Get-ChildItem -LiteralPath $expected -File | Copy-Item -Destination $dir
    Copy-Item -LiteralPath (Join-Path $expected 'scalars') -Destination $dir -Recurse
    Copy-Item -LiteralPath (Join-Path $expected 'objects') -Destination $dir -Recurse
    Copy-Item -LiteralPath (Join-Path $expected 'indexed') -Destination $dir -Recurse
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
    if ($mutation.StartsWith('scalar-')) {
        $scalarPath = Join-Path $dir 'scalars/samples.tsv'
        $original = [IO.File]::ReadAllText($scalarPath)
        $scalarRows = [IO.File]::ReadAllLines($scalarPath)
        $index = [Array]::FindIndex($scalarRows, [Predicate[string]] { param($row) $row.StartsWith("sample`tvalue-0`t0`t15`t") })
        if ($index -lt 0) { throw 'Scalar positive row absent' }
        $c = $scalarRows[$index].Split("`t")
        switch ($mutation) {
            'scalar-value' { $c[7] = '1' }
            'scalar-wire-type' { $c[2] = '8' }
            'scalar-consumed' { $c[9] = '1' }
            'scalar-byte' { $c[4] = $c[4].Replace('8391', '8392') }
            'scalar-fanout' { $c[5] = '3' }
            'scalar-missing' { $scalarRows = $scalarRows[0..($scalarRows.Count - 2)] }
        }
        if ($mutation -ne 'scalar-missing') { $scalarRows[$index] = $c -join "`t" }
        $changed = ($scalarRows -join "`n") + "`n"
        if ($original -ceq $changed) { throw "Scalar mutation changed nothing: $mutation" }
        [IO.File]::WriteAllText($scalarPath, $changed)
    }
    if ($mutation.StartsWith('object-')) {
        $objectPath=Join-Path $dir 'objects/samples.tsv'
        $original=[IO.File]::ReadAllText($objectPath)
        $objectRows=[IO.File]::ReadAllLines($objectPath)
        switch ($mutation) {
            'object-null-empty' { $objectRows[1]=$objectRows[1].Replace("`tnull`t77","`tbytes:empty`t77") }
            'object-byte' { $objectRows[1]=$objectRows[1].Replace('7f804d','00804d') }
            'object-consumed' {
                $index=[Array]::FindIndex($objectRows,[Predicate[string]]{param($row)$row.StartsWith("FieldReader-message-part`t137`tsplit`tknown`t")})
                if($index -lt 0){throw 'UTF split observation absent'}
                $c=$objectRows[$index].Split("`t");$c[8]='2';$objectRows[$index]=$c -join "`t"
            }
            'object-local-type' { $objectRows[2]=$objectRows[2].Replace("`tactual-local-type`t9`t","`tactual-local-type`t233`t") }
            'object-missing' { $objectRows=$objectRows[0..($objectRows.Count-2)] }
        }
        $changed=($objectRows -join "`n")+"`n"
        if($original -ceq $changed){throw "Object mutation changed nothing: $mutation"}
        [IO.File]::WriteAllText($objectPath,$changed)
    }
    [IO.File]::WriteAllText($path,($rows -join "`n")+"`n")
    if($mutation.StartsWith('indexed-')){
        $indexedPath=Join-Path $dir 'indexed/observations.tsv'
        $indexedRows=[IO.File]::ReadAllLines($indexedPath)
        $original=[IO.File]::ReadAllText($indexedPath)
        if($mutation -ceq 'indexed-binary'){
            $binaryPath=Join-Path $dir 'indexed/batch-subset-last.qtp'
            $binary=[IO.File]::ReadAllBytes($binaryPath)
            $binary[-1]=$binary[-1] -bxor 1
            [IO.File]::WriteAllBytes($binaryPath,$binary)
        } elseif($mutation -ceq 'indexed-missing'){
            $indexedRows=$indexedRows[0..($indexedRows.Count-2)]
        } else {
            $prefix=switch($mutation){
                'indexed-time' {'delivered'+[char]9+'file-Pair4'+[char]9}
                'indexed-slot' {'slot'+[char]9+'Pair4'+[char]9+'0'+[char]9}
                'indexed-lookup' {'source-lookup'+[char]9+'Pair4'+[char]9+'High'+[char]9}
                'indexed-last-winner' {'batch-complete'+[char]9+'batch-subset-last'+[char]9}
            }
            $index=[Array]::FindIndex($indexedRows,[Predicate[string]]{param($row)$row.StartsWith($prefix)})
            if($index -lt 0){throw 'Indexed mutation target absent'}
            $cells=$indexedRows[$index].Split([char]9)
            switch($mutation){
                'indexed-time' {$cells[4]='1'}
                'indexed-slot' {$cells[2]='1'}
                'indexed-lookup' {$cells[3]='field:1'}
                'indexed-last-winner' {$cells[3]='Pair8|HighAlias:8@0|Low:8@1|Sentinel:8@2'}
            }
            $indexedRows[$index]=$cells -join [char]9
        }
        if($mutation -cne 'indexed-binary'){
            $changed=($indexedRows -join [char]10)+[char]10
            if($original -ceq $changed){throw 'Indexed mutation changed nothing'}
            [IO.File]::WriteAllText($indexedPath,$changed)
        }
    }
    $failed=$false
    try {& "$PSScriptRoot/Compare-FieldTypes.ps1" -Actual $dir -Expected $expected *> "$dir-check.log"}
    catch {$failed=$true;[IO.File]::AppendAllText("$dir-check.log","REJECTED: $_`n")}
    if(!$failed){throw "Mutation accepted: $mutation"}
    $results.Add(@{mutation=$mutation;rejected=$true;utc=[DateTime]::UtcNow.ToString('o')})
}
& "$PSScriptRoot/Compare-FieldTypes.ps1" -Actual $expected -Expected $expected *> "$root/baseline.log"
$results | ConvertTo-Json | Set-Content -Encoding utf8 "$root/results.json"
"PASS: $($mutations.Count) rejected mutations and unchanged typed baseline"
