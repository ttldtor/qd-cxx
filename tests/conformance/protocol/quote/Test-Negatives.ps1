#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Program,
    [Parameter(Mandatory)][string]$RunDirectory
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
$source = Join-Path $repo 'tests/fixtures/protocol/quote/expected'
$run = [IO.Path]::GetFullPath($RunDirectory)
if (Test-Path -LiteralPath $run) { throw 'RunDirectory must be new; evidence is never overwritten.' }
New-Item -ItemType Directory -Path $run | Out-Null
$env:TZ = 'UTC'
$cases = @(
    @('full-type', "field`t0`t2`t120`t2566530", "field`t0`t2`t8`t2566530"),
    @('raw-value', "field`t0`t2`t120`t2566530", "field`t0`t2`t120`t2566531"),
    @('record-id', "record`t0`t0`t00490042004d", "record`t0`t1`t00490042004d"),
    @('field-order', "field`t0`t0`t56`t1700000000`t-`t-`nfield`t0`t1`t2`t78`t-`t-", "field`t0`t1`t2`t78`t-`t-`nfield`t0`t0`t56`t1700000000`t-`t-"),
    @('row-arity', "field`t0`t0`t56`t1700000000`t-`t-", "field`t0`t0`t56`t1700000000`t-`t-`textra"),
    @('mapped-bits', "mapped`t0`t00490042004d`t1700000000000`t78`t4059100000000000", "mapped`t0`t00490042004d`t1700000000000`t78`t4059100000000001"),
    @('completion', "complete`tdefault-quote`n", ''),
    @('unknown-row', "case`tdefault-quote", "unknown`tdefault-quote")
)
$results = [Collections.Generic.List[object]]::new()
foreach ($case in $cases) {
    $path = Join-Path $run $case[0]
    New-Item -ItemType Directory -Path $path | Out-Null
    Get-ChildItem -LiteralPath $source -File | Copy-Item -Destination $path
    $file = Join-Path $path 'default-quote.tsv'
    $content = [IO.File]::ReadAllText($file)
    if (!$content.Contains($case[1])) { throw "Mutation target absent: $($case[0])" }
    [IO.File]::WriteAllText($file, $content.Replace($case[1],$case[2]), [Text.UTF8Encoding]::new($false))
    $started = [DateTime]::UtcNow.ToString('o')
    & $Program $path > "$run/$($case[0]).log" 2>&1
    $exit = $LASTEXITCODE
    if ($exit -eq 0) { throw "Mutation accepted: $($case[0])" }
    $results.Add([ordered]@{name=$case[0];startedUtc=$started;finishedUtc=[DateTime]::UtcNow.ToString('o');exit=$exit})
}
$path = Join-Path $run 'binary-byte'
New-Item -ItemType Directory -Path $path | Out-Null
Get-ChildItem -LiteralPath $source -File | Copy-Item -Destination $path
$file = Join-Path $path 'default-quote.qtp'
$bytes = [IO.File]::ReadAllBytes($file)
$bytes[0] = 0
[IO.File]::WriteAllBytes($file,$bytes)
& $Program $path > "$run/binary-byte.log" 2>&1
$exit = $LASTEXITCODE
if ($exit -eq 0) { throw 'Binary mutation accepted' }
$results.Add([ordered]@{name='binary-byte';exit=$exit;finishedUtc=[DateTime]::UtcNow.ToString('o')})
& $Program $source > "$run/baseline.log" 2>&1
if ($LASTEXITCODE) { throw 'Unchanged baseline failed' }
$results | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 "$run/results.json"
Write-Output "PASS: $($results.Count) C++ mutations rejected; unchanged baseline passed"
