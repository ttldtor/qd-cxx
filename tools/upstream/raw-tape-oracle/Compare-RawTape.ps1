#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Actual,[Parameter(Mandatory)][string]$Expected)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Assert([bool]$Value,[string]$Reason) { if (!$Value) { throw $Reason } }
function Number([string]$Value,[bool]$Wide=$false) {
    Assert ($Value -cmatch '^(0|-?[1-9][0-9]*)$') 'canonical integer'
    if ($Wide) { $null=[long]::Parse($Value,[Globalization.CultureInfo]::InvariantCulture) }
    else { $null=[int]::Parse($Value,[Globalization.CultureInfo]::InvariantCulture) }
}
function Units([string]$Value) { Assert ($Value -cmatch '^(null|-|(?:[0-9a-f]{4})+)$') 'UTF16 units' }
$cases=@('raw-quote','raw-synthetic','raw-hs','stream-synthetic','connector-quote','connector-default',
    'opt-absent','opt-empty','opt-null','opt-hs','opt-unknown','opt-hs-unknown','timing-sequence',
    'empty-envelope','explicit-zero-empty','heartbeat-absent','heartbeat-unknown-bit','heartbeat-extension',
    'heartbeat-fields','heartbeat-marks-no-time','heartbeat-preserves-time','heartbeat-negative-bits',
    'mixed-raw-stream','unsupported-message','nonzero-flags','nonzero-flags-empty-opt')
foreach ($time in @('absent','NONE','MESSAGE','FIELD')) { foreach ($send in @(-1,5,15)) { foreach ($actualType in @(5,15)) { $cases += "matrix-$time-$send-$actualType" } } }
$cases+=@('-9223372036854775808','-2147483648001','-2147483648000','-1001','-1000','-999','-1','0','1','999','1000','2147483647999','2147483648000','9223372036854775807' | ForEach-Object { "boundary-$_" })
$cases+=@(0..20 | ForEach-Object { "heartbeat-truncated-$_" })
$cases+=@(1..9 | ForEach-Object { "timestamp-truncated-$_" })
$names=@($cases | ForEach-Object { "$_.qtp"; "$_.tsv"; "$_-frames.tsv" })+@('packing.tsv','settings.tsv')
function Validate([string]$Root) {
    $files=@(Get-ChildItem -LiteralPath $Root -File | Sort-Object Name)
    Assert (($files.Name -join '|') -ceq (($names | Sort-Object) -join '|')) 'exact artifact set'
    foreach ($file in $files) {
        Assert ($file.Length -gt 0 -and $file.Length -le 1048576) 'finite artifact'
        if ($file.Extension -eq '.qtp') { continue }
        $rows=[IO.File]::ReadAllLines($file.FullName)
        $base=$file.BaseName
        $isFrames=$base.EndsWith('-frames')
        $records=0; $calls=0; $errors=0; $fields=0; $frame=0; $endOffset=0; $previousType=-1; $pendingFields=-1; $needsMapping=$false
        if (!$isFrames) { Assert ($rows[-1] -ceq "complete`t$base") 'completion' }
        for ($n=0; $n -lt $rows.Count; $n++) {
            $c=$rows[$n].Split("`t")
            Assert ($c -notcontains '') 'empty cell'
            switch -CaseSensitive ($c[0]) {
                'case' { Assert ($n -eq 0 -and $c.Count -eq 3 -and $c[1] -ceq $base -and $c[2] -cin @('FileWriter','DXEndpoint-TapeConnector-FileWriterHandler','diagnostic-wire')) 'case' }
                'descriptor' { Assert ($c.Count -eq 6 -and $c[1] -ceq "$calls") 'descriptor'; $calls++; foreach ($i in @(2,3)) { Assert ($c[$i] -cmatch '^\[(RAW_DATA|STREAM_DATA)?\]$') 'send/receive' }; Units $c[4]; Units $c[5] }
                'property' { Assert ($c.Count -eq 3) 'property'; Units $c[1]; Units $c[2] }
                'heartbeat' { Assert ($c.Count -eq 7 -and $c[1] -ceq "$calls") 'heartbeat'; $calls++; foreach ($i in @(2,3,4,5)) { if ($c[$i] -cne '-') { Number $c[$i] ($i -eq 2) } }; Assert ($c[6] -cmatch '^(true|false)$') 'heartbeat boolean' }
                'callback' { Assert ($c.Count -eq 3 -and $c[1] -ceq "$calls" -and $c[2] -cin @('10','15','20')) 'callback'; $calls++; $previousType=[int]$c[2] }
                'record' {
                    Assert ($pendingFields -le 0 -and !$needsMapping -and $c.Count -eq 8 -and $c[1] -ceq "$records" -and $c[2] -ceq "$previousType") 'record order'
                    Number $c[3]; Units $c[4]; Units $c[5]; Number $c[6]; Number $c[7] $true
                    $pendingFields=switch ($c[4]) { '0053' {1}; '0052' {3}; '0045' {0}; '00510075006f00740065' {8}; default {throw 'record name'} }
                    $needsMapping=$pendingFields -eq 8; $fields=0; $records++
                }
                'field' {
                    Assert ($c.Count -eq 5 -and $pendingFields -gt 0 -and $c[1] -ceq "$($records-1)" -and $c[2] -ceq "$fields") 'field order'
                    $expectedType=if ($needsMapping) { @(56,2,120,120,56,2,120,120)[$fields] } else {8}
                    Assert ($c[3] -ceq "$expectedType") 'full type'; Number $c[4] ($expectedType -eq 120); $fields++; $pendingFields--
                }
                'mapped' {
                    Assert ($c.Count -eq 13 -and $needsMapping -and $pendingFields -eq 0 -and $c[1] -ceq "$($records-1)") 'mapped shape'; $needsMapping=$false
                    foreach ($i in @(2,3,10)) { Number $c[$i] $true }; foreach ($i in @(4,5,11,12)) { Number $c[$i] }
                    foreach ($i in @(6,7,8,9)) { Assert ($c[$i] -cmatch '^[0-9a-f]{16}$') 'mapped bits' }
                }
                'error' { Assert ($c.Count -eq 4 -and $c[1] -ceq "$calls" -and $c[2] -cin @('stream','message','unknown')) 'error'; Number $c[3]; $calls++; $errors++ }
                'end' { Assert ($pendingFields -le 0 -and !$needsMapping -and $n -eq $rows.Count-2 -and $c.Count -eq 4 -and $c[1] -ceq "$records" -and $c[2] -ceq "$errors" -and $c[3] -ceq "$calls") 'end counts' }
                'frame' { Assert ($isFrames -and $c.Count -eq 5 -and $c[1] -ceq "$frame" -and $c[2] -ceq "$endOffset") 'frame structure'; foreach ($i in @(1,2,3,4)) { Number $c[$i] }; Assert ([int]$c[3] -gt $endOffset) 'frame forward'; $frame++; $endOffset=[int]$c[3] }
                'packing' { Assert ($base -ceq 'packing' -and $c.Count -eq 3) 'packing'; Number $c[1] $true; Number $c[2] $true }
                'settings' { Assert ($base -ceq 'settings' -and $n -eq 0 -and $rows[$n] -ceq "settings`tBINARY`tNONE`tMESSAGE`tRAW_DATA`tMAX_SPEED`tcycle=false`toverrides=none") 'settings' }
                'schema' { Assert ($base -ceq 'settings' -and $rows[$n] -ceq "schema`tcom.dxfeed.api.impl.DXFeedScheme`t331`t12`t0") 'unchanged schema' }
                'schema-field' { Assert ($base -ceq 'settings' -and $c.Count -eq 5) 'schema field'; foreach ($i in @(1,2,3)) { Number $c[$i] }; Units $c[4] }
                'input' { Assert ($base -ceq 'settings' -and $c.Count -eq 10) 'input'; Number $c[1]; Units $c[2]; foreach ($i in @(3,4,5)) { Number $c[$i] $true }; foreach ($i in @(6,7,8,9)) { Assert ($c[$i] -cmatch '^[0-9a-f]{16}$') 'input bits' } }
                'complete' { Assert ($n -eq $rows.Count-1) 'final completion'; if ($isFrames) { Assert ($c.Count -eq 4 -and $c[1] -ceq 'frames' -and $c[2] -ceq "$frame" -and $c[3] -ceq "$endOffset") 'frame completion'; Assert ((Get-Item -LiteralPath (Join-Path $Root ($base.Substring(0,$base.Length-7)+'.qtp'))).Length -eq $endOffset) 'binary length' } }
                default { throw "Unknown row: $($c[0])" }
            }
        }
    }
}
Validate $Actual
Validate $Expected
foreach ($name in $names) {
    Assert ((Get-FileHash -LiteralPath (Join-Path $Actual $name)).Hash -ceq (Get-FileHash -LiteralPath (Join-Path $Expected $name)).Hash) "artifact mismatch: $name"
}
"PASS: $($cases.Count) cases; $($names.Count) exact artifacts, typed values/timing/structure and bytes"
