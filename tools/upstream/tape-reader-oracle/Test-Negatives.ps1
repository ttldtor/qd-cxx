#requires -Version 7.4
param([Parameter(Mandatory)][string]$Expected, [Parameter(Mandatory)][string]$RunDirectory)
$ErrorActionPreference='Stop'
if (Test-Path $RunDirectory) { throw 'Use a new negative directory' }
New-Item -ItemType Directory $RunDirectory | Out-Null
$mutations=@('value','type','id','position','unknown-row','missing-complete','duplicate-complete','empty','missing-file')
foreach ($mutation in $mutations) {
    $dest=Join-Path $RunDirectory $mutation
    Copy-Item $Expected $dest -Recurse
    $file=Join-Path $dest 'original.tsv'
    $s=[IO.File]::ReadAllText($file)
    switch ($mutation) {
        value { $s=$s.Replace("`t7`t0`t0","`t8`t0`t0") }
        type { $s=$s.Replace("`t15`t0`t","`t14`t0`t") }
        id { $s=$s.Replace("record`t0`t15`t0`t","record`t0`t15`t1`t") }
        position { $s=$s.Replace("case`toriginal`t137","case`toriginal`t138") }
        unknown-row { $s=$s.Replace("descriptor`t","unknown`t") }
        missing-complete { $s=$s.Replace("complete`toriginal`n",'') }
        duplicate-complete { $s+="complete`toriginal`n" }
        empty { $s='' }
        missing-file { Remove-Item -LiteralPath (Join-Path $dest 'empty.qtp') }
    }
    [IO.File]::WriteAllText($file,$s)
    $rejected=$false
    try { & "$PSScriptRoot/Compare-Probes.ps1" -Actual $dest -Expected $Expected } catch { $rejected=$true }
    if (!$rejected) { throw "Mutation accepted: $mutation" }
    "REJECTED: $mutation"
}
$empty=Join-Path $RunDirectory 'empty-coverage'
New-Item -ItemType Directory $empty | Out-Null
$rejected=$false
try { & "$PSScriptRoot/Compare-Probes.ps1" -Actual $empty -Expected $Expected } catch { $rejected=$true }
if (!$rejected) { throw 'Empty coverage accepted' }
'REJECTED: empty-coverage'
& "$PSScriptRoot/Compare-Probes.ps1" -Actual $Expected -Expected $Expected
