#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Actual, [Parameter(Mandatory)][string]$Expected)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Validate([string]$Root) {
    $files=@(Get-ChildItem -LiteralPath $Root -File | Sort-Object Name)
    if ($files.Count -ne 102) { throw 'Expected exactly 51 binary/observation pairs' }
    foreach ($file in $files | Where-Object Extension -eq '.tsv') {
        $name=$file.BaseName
        $binary=Join-Path $Root "$name.qtp"
        if (!(Test-Path -LiteralPath $binary) -or $file.Length -eq 0 -or $file.Length -gt 65536) { throw "Invalid pair $name" }
        $rows=@([IO.File]::ReadAllLines($file.FullName))
        if ($rows.Count -lt 3 -or $rows[0] -cne "case`t$name`t$((Get-Item $binary).Length)" -or $rows[-1] -cne "complete`t$name") { throw "Invalid completion $name" }
        $records=0; $errors=0; $end=$false
        for ($i=1;$i -lt $rows.Count-1;$i++) {
            $c=$rows[$i].Split("`t")
            if ($end -or $c -contains '') { throw "Rows after end or empty cell $name" }
            switch -CaseSensitive ($c[0]) {
                'descriptor' { if ($c.Count -ne 3 -or $c[1] -cnotmatch '^(?:[0-9a-f]{4})+$' -or $c[2] -cnotmatch '^(?:[0-9a-f]{4})+$') { throw 'descriptor' } }
                'property' { if ($c.Count -ne 3 -or $c[1] -cnotmatch '^(null|-|(?:[0-9a-f]{4})+)$' -or $c[2] -cnotmatch '^(null|-|(?:[0-9a-f]{4})+)$') { throw 'property' } }
                'record' {
                    if ($c.Count -ne 8 -or $c[1] -cne "$records" -or $c[2] -cne '15' -or $c[3] -cnotmatch '^[0-2]$' -or $c[4] -cnotmatch '^(-|(?:[0-9a-f]{4})+)$' -or $c[6] -cne '0' -or $c[7] -cne '0') { throw 'record identity' }
                    $values=@(if ($c[5] -ne '-') { $c[5].Split(',') })
                    if ($values.Count -ne @(1,3,0)[[int]$c[3]]) { throw 'record shape' }
                    foreach ($v in $values) { if ($v -cnotmatch '^(0|-?[1-9][0-9]*)$') { throw 'integer' }; $null=[int]::Parse($v,[Globalization.CultureInfo]::InvariantCulture) }
                    $records++
                }
                'error' { if ($c.Count -ne 3 -or $c[1] -cnotmatch '^(stream|message|unknown)$' -or $c[2] -cnotmatch '^(-|0|[1-9][0-9]*)$') { throw 'error' }; $errors++ }
                'heartbeat' { if ($c.Count -ne 2 -or $c[1] -cnotmatch '^(true|false)$') { throw 'heartbeat' } }
                'exception' { if ($c.Count -ne 2 -or $c[1] -cnotmatch '^([a-zA-Z0-9_$]+\.)+[a-zA-Z0-9_$]+$') { throw 'exception' } }
                'end' { if ($c.Count -ne 4 -or $c[1] -cne "$records" -or $c[2] -cne "$errors" -or $i -ne $rows.Count-2) { throw 'end counts' }; $end=$true }
                default { throw "Unknown row: $($c[0])" }
            }
        }
        if (!$end) { throw 'Missing end' }
    }
    return $files
}
$a=@(Validate $Actual); $e=@(Validate $Expected)
if (($a.Name -join "`n") -cne ($e.Name -join "`n")) { throw 'Different artifact set' }
for ($i=0;$i -lt $a.Count;$i++) {
    if ((Get-FileHash $a[$i].FullName).Hash -cne (Get-FileHash $e[$i].FullName).Hash) { throw "Artifact mismatch: $($a[$i].Name)" }
}
'PASS: 51 strictly validated cases, 102 exact artifacts'
