#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Actual,[Parameter(Mandatory)][string]$Expected)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$types=@(56,2,120,120,56,2,120,120)
function Assert([bool]$Value,[string]$Reason) { if (!$Value) { throw $Reason } }
function Integer([string]$Value,[bool]$Wide=$false) {
    Assert ($Value -cmatch '^(0|-?[1-9][0-9]*)$') 'noncanonical integer'
    if ($Wide) { $null=[long]::Parse($Value,[Globalization.CultureInfo]::InvariantCulture) }
    else { $null=[int]::Parse($Value,[Globalization.CultureInfo]::InvariantCulture) }
}
function Units([string]$Value) { Assert ($Value -cmatch '^(-|(?:[0-9a-f]{4})+)$') 'UTF-16 units' }
function Bits([string]$Value) { Assert ($Value -cmatch '^[0-9a-f]{16}$') 'double bits' }
function Decimal([string]$Value) { Assert ($Value -cmatch '^(NaN|-?Infinity|-?(0|[1-9][0-9]*)(\.[0-9]+)?(E-?[0-9]+)?)$') 'decimal spelling' }
function HexBytes([string]$Value) { Assert ($Value -cmatch '^(-|(?:[0-9a-f]{2})+)$') 'bytes' }
function Validate([string]$Root) {
    $files=@(Get-ChildItem -LiteralPath $Root -File | Sort-Object Name)
    $names=@('default-quote','missing','reordered','duplicate','unknown-fields','unknown-record') + @(1..11 | ForEach-Object { "char-$_" }) + @(-2147483648,-1,0,1,2147483647 | ForEach-Object { "time-$_" }) + @(0..18 | ForEach-Object { "decimal-$_" })
    $expectedFiles=@($names | ForEach-Object { "$_.qtp"; "$_.tsv" }) + @('schema.tsv','inputs.tsv','put.tsv','primitives.tsv')
    Assert (($files.Name -join '|') -ceq (($expectedFiles | Sort-Object) -join '|')) 'exact artifact set'
    foreach ($file in $files) {
        Assert ($file.Length -gt 0 -and $file.Length -le 2097152) 'finite artifact size'
        if ($file.Extension -eq '.qtp') { continue }
        $rows=[IO.File]::ReadAllLines($file.FullName)
        $name=$file.BaseName
        Assert ($rows[-1] -ceq "complete`t$name") "completion $name"
        $record=0; $field=0; $pending=$false; $ended=$false; $inputCount=0; $quoteFields=0
        for ($n=0;$n -lt $rows.Count-1;$n++) {
            $c=$rows[$n].Split("`t")
            Assert (!$ended -and $c -notcontains '') "empty/trailing rows $name"
            switch -CaseSensitive ($c[0]) {
                'case' { Assert ($n -eq 0 -and $c.Count -eq 2 -and $c[1] -ceq $name) 'case header' }
                'record' {
                    Assert (!$pending -and $c.Count -eq 6 -and $c[1] -ceq "$record" -and $c[2] -ceq '0' -and $c[4] -ceq '0' -and $c[5] -ceq '0') 'record identity/order'
                    Units $c[3]; $symbol=$c[3]; $field=0; $pending=$true
                }
                'field' {
                    Assert ($pending -and $field -lt 8 -and $c.Count -eq 7 -and $c[1] -ceq "$record" -and $c[2] -ceq "$field" -and $c[3] -ceq "$($types[$field])") 'field order/full type'
                    Integer $c[4] ($types[$field] -eq 120)
                    if ($types[$field] -eq 120) { Decimal $c[5]; Bits $c[6] }
                    else { Assert ($c[5] -ceq '-' -and $c[6] -ceq '-') 'primitive semantic columns'; if ($types[$field] -eq 2) { Assert ([int]$c[4] -ge 0 -and [int]$c[4] -le 1114111) 'code point' } }
                    $field++
                }
                { $_ -cin @('mapped','input') } {
                    Assert ($c.Count -eq 15) 'event arity'
                    if ($c[0] -ceq 'mapped') { Assert ($pending -and $field -eq 8 -and $c[1] -ceq "$record" -and $c[2] -ceq $symbol) 'mapped identity'; $pending=$false; $record++ }
                    else { Assert ($name -ceq 'inputs' -and $c[1] -ceq "$inputCount") 'input order'; $inputCount++ }
                    Units $c[2]
                    foreach ($i in @(3,7,11,14)) { Integer $c[$i] $true }
                    foreach ($i in @(4,8,12,13)) { Integer $c[$i] }
                    foreach ($i in @(5,6,9,10)) { Bits $c[$i] }
                    foreach ($i in @(4,8)) { Assert ([int]$c[$i] -ge 0 -and [int]$c[$i] -le 65535) 'mapped char' }
                    if ($c[0] -ceq 'mapped') { Assert ($c[12] -ceq '0' -and $c[13] -ceq '0' -and $c[14] -ceq '0') 'default metadata loss' }
                }
                'end' { Assert (!$pending -and $c.Count -eq 3 -and $c[1] -ceq "$record" -and $c[2] -ceq '0' -and $n -eq $rows.Count-2) 'end counts'; $ended=$true }
                'schema' { Assert ($name -ceq 'schema' -and $n -eq 0 -and $c.Count -eq 5 -and $c[1] -ceq 'com.dxfeed.api.impl.DXFeedScheme' -and $c[2] -ceq '331' -and $c[3] -ceq '0' -and $c[4] -ceq 'com.devexperts.qd.kit.PentaCodec') 'scheme identity' }
                'settings' { Assert ($n -eq 1 -and $rows[$n] -ceq "settings`toverrides=none`twide=true`tregional=unchanged`tsequence=disabled`tnano=disabled") 'settings' }
                'scheme-record' { Assert ($c.Count -eq 6) 'record layout'; foreach ($i in @(1,3,4)) { Integer $c[$i] }; Units $c[2]; Assert ($c[5] -cmatch '^(true|false)$') 'hasTime' }
                { $_ -cin @('scheme-int','scheme-obj','quote-field') } {
                    Assert ($c.Count -eq 6) 'field layout'
                    Integer $c[1]; Integer $c[2]; Units $c[3]; Units $c[4]; Integer $c[5]
                    if ($c[0] -ceq 'quote-field') { Assert ($c[1] -ceq "$quoteFields" -and $c[5] -ceq "$($types[$quoteFields])") 'quote identities'; $quoteFields++ }
                }
                'char-write' { Assert ($name -ceq 'primitives' -and $c.Count -eq 5) 'char-write'; Integer $c[1]; HexBytes $c[2]; Assert ($c[3] -cmatch '^(ok|UTFDataFormatException)$') 'write status'; Integer $c[4] }
                'char-read' { Assert ($name -ceq 'primitives' -and $c.Count -eq 6) 'char-read'; Integer $c[1]; HexBytes $c[2]; if ($c[3] -cne '-') { Integer $c[3] }; Assert ($c[4] -cmatch '^(ok|UTFDataFormatException|EOFException)$') 'read status'; Integer $c[5] }
                'time' { Assert ($name -ceq 'primitives' -and $c.Count -eq 4) 'time'; Integer $c[1]; HexBytes $c[2]; Integer $c[3] $true; Assert ([long]$c[3] -eq [long]$c[1]*1000) 'time units' }
                'decimal-raw' { Assert ($name -ceq 'primitives' -and $c.Count -eq 6) 'decimal-raw'; Integer $c[1]; Integer $c[2] $true; HexBytes $c[3]; Decimal $c[4]; Bits $c[5] }
                'decimal-compose' { Assert ($name -ceq 'primitives' -and $c.Count -eq 6) 'decimal-compose'; Integer $c[1]; Bits $c[2]; Integer $c[3] $true; Decimal $c[4]; Bits $c[5] }
                default { throw "Unknown typed row $($c[0])" }
            }
        }
        if ($name -ceq 'schema') { Assert ($quoteFields -eq 8) 'quote shape' }
        elseif ($name -ceq 'inputs') { Assert ($inputCount -eq 5) 'input count' }
        elseif ($name -cne 'primitives') {
            $expectedCount=if ($name -cin @('default-quote','put')) { 5 } elseif ($name -ceq 'unknown-record') { 0 } else { 1 }
            Assert ($ended -and $record -eq $expectedCount) 'case count'
        }
    }
    $put=[IO.File]::ReadAllLines((Join-Path $Root 'put.tsv'))
    $replay=[IO.File]::ReadAllLines((Join-Path $Root 'default-quote.tsv'))
    Assert ($put.Count -eq $replay.Count) 'production put/replay row count'
    for ($i=1;$i -lt $put.Count-1;$i++) {
        Assert ($put[$i] -ceq $replay[$i]) 'production put/replay raw or mapped mismatch'
    }
    return $files
}
$a=@(Validate $Actual)
$e=@(Validate $Expected)
for ($i=0;$i -lt $a.Count;$i++) {
    Assert ((Get-FileHash $a[$i].FullName).Hash -ceq (Get-FileHash $e[$i].FullName).Hash) "artifact mismatch: $($a[$i].Name)"
}
"PASS: strictly validated $($a.Count) exact Quote artifacts"
