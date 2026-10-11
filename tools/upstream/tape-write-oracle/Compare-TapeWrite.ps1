#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Actual, [Parameter(Mandatory)][string]$Expected)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$cases = @('extras-raw-message','extras-raw-none','extras-stream-none','extras-raw-singletons',
    'extras-heartbeat-regular','extras-raw-hs','extras-raw-default-options','extras-raw-field-time',
    'quote-raw','profile-raw','unsupported-full-identities')
$arity = @{case=2; settings=9; schema=6; slot=6; lookup=3; record=11; value=8;
    'writer-close'=2; descriptor=3; property=3; heartbeat=3; callback=2; error=2;
    end=4; complete=2; diagnostic=2; frame=6; 'wire-record'=4; 'wire-field'=5;
    failure=3; lock=2; tool=3; provider=3; input=3; 'text-input'=4; 'empty-writer'=2}
function Integer([string]$Value, [long]$Min=[long]::MinValue, [long]$Max=[long]::MaxValue) {
    [long]$number = 0
    if ($Value -cnotmatch '^(0|-[1-9][0-9]*|[1-9][0-9]*)$' -or ![long]::TryParse($Value,[ref]$number) -or $number -lt $Min -or $number -gt $Max) { throw "Canonical integer: $Value" }
}
function Units([string]$Value) {
    if ($Value -cnotmatch '^(null|-|(?:[0-9a-f]{4})+)$') { throw "UTF-16 units: $Value" }
}
function ValidateRow([string[]]$Cells, [bool]$Unsupported) {
    switch ($Cells[0]) {
        slot {
            Integer $Cells[2] 0 100; Integer $Cells[3] 0 255; Units $Cells[4]; Units $Cells[5]
            $types = @(0,2,8,10,40,56,72,88,120)
            if ($Unsupported) { $types = @(24,137) }
            if ([int]$Cells[3] -notin $types) { throw 'Slot full identity' }
        }
        lookup { Units $Cells[1]; Units $Cells[2] }
        'text-input' { Integer $Cells[1] 0 7; if ($Cells[2] -cnotin 'null','string','char-array') { throw 'Text input representation' }; Units $Cells[3] }
        record {
            if ($Cells[1] -cnotin 'input','read') { throw 'Record phase' }
            Integer $Cells[2] 0 7; Integer $Cells[3] 15 15; Integer $Cells[4] 0 100;
            Units $Cells[5]; Integer $Cells[6] ([int]::MinValue) ([int]::MaxValue);
            Units $Cells[7]; Units $Cells[8]; Integer $Cells[9] 0 0; Integer $Cells[10]
        }
        value {
            Integer $Cells[2] 0 7; Integer $Cells[4] 0 100; Units $Cells[7]
            $type = [int]$Cells[5]
            $supported = @(2,8,10,40,56,72,88,120)
            if ($Unsupported) { $supported = @(24,137) }
            if ($type -notin $supported -or "$type" -cne $Cells[5]) { throw 'Full type identity' }
            if ($Cells[3] -eq 'obj') {
                if ($type -notin 10,137) { throw 'Object storage type' }; Integer $Cells[6] 0 0
            } elseif ($Cells[3] -eq 'int') {
                if ($Cells[7] -cne 'null') { throw 'Scalar string payload' }
                if ($type -eq 120) { Integer $Cells[6] }
                elseif ($type -eq 2) { Integer $Cells[6] 0 1114111 }
                else { Integer $Cells[6] ([int]::MinValue) ([int]::MaxValue) }
            } else { throw 'Storage kind' }
        }
        'wire-field' {
            Integer $Cells[1] 0 100; Integer $Cells[2] 0 100; Units $Cells[3]; Integer $Cells[4] 0 255
            $types = @(2,8,10,40,56,72,88,120)
            if ($Unsupported) { $types = @(24,137) }
            if ([int]$Cells[4] -notin $types) { throw 'Wire full identity' }
        }
        'wire-record' { Integer $Cells[1] 0 100; Units $Cells[2]; Integer $Cells[3] 0 100 }
        frame { foreach ($index in 1..4) { Integer $Cells[$index] 0 2097152 }; Integer $Cells[5] -1 255 }
        descriptor { Units $Cells[1]; Units $Cells[2] }
        property { Units $Cells[1]; Units $Cells[2] }
        heartbeat { if ($Cells[1] -cne '-') { Integer $Cells[1] }; if ($Cells[2] -cnotin 'true','false') { throw 'Heartbeat empty flag' } }
        callback { if ($Cells[1] -cnotin '10','15','20') { throw 'Callback message' } }
        end { Integer $Cells[1] 8 8; Integer $Cells[2] 0 0; Integer $Cells[3] 8 24 }
    }
}
function Inventory([string]$Root) {
    $rootPath = [IO.Path]::GetFullPath($Root)
    $files = @('classpath-receipt.json','provenance.tsv','extras/failures.tsv',
        'events/observations.tsv','events/Quote-stream.qtp','events/Quote-raw.qtp',
        'events/Profile-stream.qtp','events/Profile-raw.qtp','events/Quote-wire-omitted.qtp','events/Profile-wire-omitted.qtp')
    foreach ($case in $cases) { $files += @("extras/$case.qtp","extras/$case.tsv","extras/$case-wire.tsv") }
    $actualFiles = @(Get-ChildItem -LiteralPath $rootPath -Recurse -Force -File | ForEach-Object {
        [IO.Path]::GetRelativePath($rootPath,$_.FullName).Replace('\','/') } | Sort-Object -CaseSensitive)
    if (($actualFiles -join "`n") -cne (($files | Sort-Object -CaseSensitive) -join "`n")) { throw "Artifact set mismatch: $Root" }
    $directories = @(Get-ChildItem -LiteralPath $rootPath -Recurse -Force -Directory | ForEach-Object { [IO.Path]::GetRelativePath($rootPath,$_.FullName).Replace('\','/') } | Sort-Object)
    if (($directories -join ',') -cne 'events,extras') { throw 'Directory set mismatch' }
    $items = @((Get-Item -LiteralPath $rootPath)) + @(Get-ChildItem -LiteralPath $rootPath -Recurse -Force)
    foreach ($item in $items) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Symbolic artifact path' }
        if (!$item.PSIsContainer -and $item.Length -gt 2097152) { throw 'Artifact byte budget' }
    }
    foreach ($file in $files | Where-Object { $_ -like '*.tsv' -and $_ -ne 'events/observations.tsv' }) {
        $lines = [IO.File]::ReadAllLines("$rootPath/$file")
        if ($lines.Length -lt 2 -or !$lines[0].StartsWith("case`t") -or !$lines[-1].StartsWith("complete`t")) { throw "Completion mismatch: $file" }
        $id = $lines[0].Split("`t")[1]
        if ($lines[-1] -cne "complete`t$id" -or @($lines | Where-Object { $_.StartsWith("complete`t") }).Count -ne 1) { throw "Completion count: $file" }
        foreach ($line in $lines) {
            $cells = $line.Split("`t")
            if (!$arity.ContainsKey($cells[0]) -or $cells.Length -ne $arity[$cells[0]]) { throw "Row arity: $file : $line" }
            ValidateRow $cells ($file -like '*unsupported-full-identities*')
        }
        if ($file -match 'extras/.+(?<!-wire)\.tsv$' -and $file -ne 'extras/failures.tsv') {
            $records = @($lines | Where-Object { $_.StartsWith("record`tread`t") })
            if ($records.Count -ne 8 -or @($lines | Where-Object { $_.StartsWith("error`t") }).Count -ne 0) { throw "Production acceptance: $file" }
            $ends = @($lines | Where-Object { $_.StartsWith("end`t") })
            if ($ends.Count -ne 1 -or $ends[0] -notmatch '^end\t8\t0\t(8|24)$') { throw "Production counts: $file" }
            for ($i=0; $i -lt 8; $i++) {
                $rc = $records[$i].Split("`t")
                if ($rc[2] -cne "$i" -or $rc[3] -cne '15' -or $rc[9] -cne '0') { throw "Record order/flags: $file" }
                $sourceRecord = @($lines | Where-Object { $_.StartsWith("record`tinput`t$i`t") })
                if ($sourceRecord.Count -ne 1 -or $sourceRecord[0].Split("`t")[8] -cne $rc[8]) { throw "Symbol semantics: $file/$i" }
                $input = @($lines | Where-Object { $_.StartsWith("value`tinput`t$i`t") })
                $read = @($lines | Where-Object { $_.StartsWith("value`tread`t$i`t") })
                if ($input.Count -eq 0 -or $input.Count -ne $read.Count) { throw "Field inventory: $file" }
                for ($j=0; $j -lt $input.Count; $j++) {
                    if ($input[$j].Replace("`tinput`t","`tread`t") -cne $read[$j]) { throw "Raw semantic mismatch: $file/$i/$j" }
                }
            }
        }
    }
    $eventLines = [IO.File]::ReadAllLines("$rootPath/events/observations.tsv")
    if ($eventLines[0] -cne "case`tdefault" -or $eventLines[-1] -cne "complete`tdefault") { throw 'Real event completion' }
    $historical = Join-Path $PSScriptRoot '../../../tests/fixtures/events/mapping/expected/default'
    foreach ($file in Get-ChildItem -LiteralPath $historical -File) {
        if ($file.Name -eq 'observations.tsv') {
            # The historical checkout uses automatic text conversion; compare complete lines only here.
            if (([IO.File]::ReadAllLines("$rootPath/events/$($file.Name)") -join "`n") -cne ([IO.File]::ReadAllLines($file.FullName) -join "`n")) { throw 'Unchanged real event observations' }
        } elseif ((Get-FileHash "$rootPath/events/$($file.Name)").Hash -cne (Get-FileHash $file.FullName).Hash) { throw "Unchanged real event regression: $($file.Name)" }
    }
    $receipt = Get-Content "$rootPath/classpath-receipt.json" -Raw | ConvertFrom-Json
    if ($receipt.version -ne 1 -or $receipt.revision -cne '0ddc0a09779fb61d74cc0b37c6449ff47dcb715f' -or $receipt.modules.Count -ne 14) { throw 'Provider receipt contract' }
    if (@($receipt.modules.name | Sort-Object -Unique).Count -ne 14) { throw 'Duplicate provider module' }
    foreach ($module in $receipt.modules) { Integer "$($module.count)" 1 50000; if ($module.sha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Provider digest grammar' } }
    $provenance = [IO.File]::ReadAllLines("$rootPath/provenance.tsv")
    if (@($provenance | Where-Object { $_.StartsWith("tool`t") }).Count -ne 5 -or @($provenance | Where-Object { $_.StartsWith("input`t") }).Count -ne 1 -or @($provenance | Where-Object { $_.StartsWith("provider`t") }).Count -ne 7 -or $provenance[1] -cne "lock`t0ddc0a09779fb61d74cc0b37c6449ff47dcb715f") { throw 'Provenance inventory/lock' }
    foreach ($line in $provenance) {
        $c = $line.Split("`t")
        if ($c[0] -eq 'tool' -and (Get-FileHash (Join-Path $PSScriptRoot $c[1])).Hash.ToLowerInvariant() -cne $c[2]) { throw "Current tool provenance mismatch: $($c[1])" }
        if ($c[0] -eq 'input' -and ($c[1] -cne 'inputs.tsv' -or (Get-FileHash "$PSScriptRoot/../../../tests/fixtures/protocol/tape-write/inputs.tsv").Hash.ToLowerInvariant() -cne $c[2])) { throw 'Current input provenance mismatch' }
        if ($c[0] -in 'tool','provider' -and $c[2] -cnotmatch '^[0-9a-f]{64}$') { throw 'Provenance digest grammar' }
    }
    return $actualFiles
}
$a = @(Inventory $Actual)
$e = @(Inventory $Expected)
foreach ($file in $a) {
    if ((Get-FileHash "$Actual/$file").Hash -cne (Get-FileHash "$Expected/$file").Hash) { throw "Byte mismatch: $file" }
}
"PASS: $($a.Count) exact artifacts; raw values, record order, completion, real events, provenance"
