#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Program,
    [Parameter(Mandatory)][string]$Inspector,
    [Parameter(Mandatory)][string]$RunDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path "$PSScriptRoot/../../../..").Path
$fixtures = Join-Path $repo 'tests/fixtures/protocol/raw-tape/expected'
$run = [IO.Path]::GetFullPath($RunDirectory)
if (Test-Path -LiteralPath $run) { throw 'RunDirectory must be new; retained evidence is never overwritten.' }
New-Item -ItemType Directory -Path $run | Out-Null
$env:TZ = 'UTC'
$baselinePath = Join-Path $run 'baseline.tsv'
& $Inspector --input "$fixtures/raw-quote.qtp" --profile default-quote-v1 --tape-mode raw-message-v1 > $baselinePath 2> "$run/inspector.stderr"
if ($LASTEXITCODE) { throw 'Baseline inspector failed.' }
$baseline = [IO.File]::ReadAllText($baselinePath)
$cases = @(
    @('raw-timestamp', '1600000000123', '1600000000124'),
    @('packed-sequence', '6871947674115899392', '6871947674115899393'),
    @('raw-value', '120:2566530', '120:2566531'),
    @('full-type', '120:2566530', '8:2566530'),
    @('field-order', '56:1700000000,2:78', '2:78,56:1700000000'),
    @('mapped-bits', '4059100000000000', '4059100000000001'),
    @('row-arity', "descriptor-v3`t", "descriptor-v3`textra`t"),
    @('unknown-row', "format`t", "unknown`t"),
    @('noncanonical-number', "timing`t3`t", "timing`t03`t")
)
$results = [Collections.Generic.List[object]]::new()
foreach ($case in $cases) {
    if (!$baseline.Contains($case[1])) { throw "Mutation target absent: $($case[0])" }
    $file = Join-Path $run "$($case[0]).tsv"
    [IO.File]::WriteAllText($file, $baseline.Replace($case[1], $case[2]), [Text.UTF8Encoding]::new($false))
    $started = [DateTime]::UtcNow.ToString('o')
    & $Program $fixtures raw-quote $file > "$run/$($case[0]).log" 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 0) { throw "Mutation accepted: $($case[0])" }
    $results.Add([ordered]@{name=$case[0]; startedUtc=$started; finishedUtc=[DateTime]::UtcNow.ToString('o'); exit=$exitCode})
}
foreach ($case in @(
    @('missing-completion', [regex]::Replace($baseline, 'complete\t[^\n]*\n', '')),
    @('duplicate-completion', ($baseline + [regex]::Match($baseline, 'complete\t[^\n]*\n').Value)),
    @('partial-row', $baseline.TrimEnd("`n")),
    @('crlf', $baseline.Replace("`n", "`r`n"))
)) {
    $file = Join-Path $run "$($case[0]).tsv"
    [IO.File]::WriteAllText($file, $case[1], [Text.UTF8Encoding]::new($false))
    $started = [DateTime]::UtcNow.ToString('o')
    & $Program $fixtures raw-quote $file > "$run/$($case[0]).log" 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 0) { throw "Mutation accepted: $($case[0])" }
    $results.Add([ordered]@{name=$case[0]; startedUtc=$started; finishedUtc=[DateTime]::UtcNow.ToString('o'); exit=$exitCode})
}
$timingPath = Join-Path $run 'timing-baseline.tsv'
& $Inspector --input "$fixtures/timing-sequence.qtp" --profile oracle-sre-v1 --tape-mode raw-message-v1 > $timingPath 2> "$run/timing-inspector.stderr"
if ($LASTEXITCODE) { throw 'Timing inspector baseline failed.' }
$timing = [IO.File]::ReadAllText($timingPath)
$record = [regex]::Match($timing, "record`t15`t6`t[^`n]*`n").Value
if (!$record) { throw 'Missing migration target.' }
$changed = $timing.Replace($record, '').Replace("frame`t15`t", $record.Replace("record`t15`t", "record`t14`t") + "frame`t15`t")
$migration = Join-Path $run 'frame-migration.tsv'
[IO.File]::WriteAllText($migration, $changed, [Text.UTF8Encoding]::new($false))
$started = [DateTime]::UtcNow.ToString('o')
& $Program $fixtures timing-sequence $migration > "$run/frame-migration.log" 2>&1
$exitCode = $LASTEXITCODE
if ($exitCode -eq 0) { throw 'Equal-timestamp frame migration accepted.' }
$results.Add([ordered]@{name='frame-migration'; startedUtc=$started; finishedUtc=[DateTime]::UtcNow.ToString('o'); exit=$exitCode})
& $Program $fixtures timing-sequence $timingPath > "$run/timing-baseline.log" 2>&1
if ($LASTEXITCODE) { throw 'Unchanged timing baseline failed.' }
& $Program $fixtures raw-quote $baselinePath > "$run/baseline.log" 2>&1
if ($LASTEXITCODE) { throw 'Unchanged post-mutation baseline failed.' }
$results | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 "$run/results.json"
Write-Output "PASS: $($results.Count) timed CLI mutations rejected; unchanged Java baseline passed."
