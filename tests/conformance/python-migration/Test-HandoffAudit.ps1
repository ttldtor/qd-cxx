#Requires -Version 7.4
<#
.SYNOPSIS
Reject missing and corrupt matrix traces in an isolated evidence copy.
.DESCRIPTION
The audit's trace-only mode runs the same trace checks as its full mode, without writing reports.
Canonical evidence is hashed before and after; mutations affect only a fresh ignored work directory.
#>
param(
    [string]$EvidenceRoot = (Join-Path $PSScriptRoot 'evidence'),
    [string]$Report = (Join-Path $PSScriptRoot 'evidence/review-fixes/trace-regressions.json')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$audit = Join-Path $PSScriptRoot 'Audit-Handoff.ps1'
$work = Join-Path $repo ('.work/python-migration-trace-audit-' + [Guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $Report) { throw 'Regression report must be fresh' }
$originals = [Collections.Generic.List[object]]::new()
$results = [Collections.Generic.List[object]]::new()
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($platform in @('windows-final-b','linux-final-b')) {
    $source = Join-Path $EvidenceRoot $platform
    $target = Join-Path $work $platform
    [IO.Directory]::CreateDirectory($target) | Out-Null
    foreach ($file in @((Get-Item -LiteralPath (Join-Path $source 'environment.json'))) +
            @(Get-ChildItem -LiteralPath $source -Filter '*.exec.tsv' -File)) {
        $originals.Add(@{path=$file.FullName;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash})
        [IO.File]::Copy($file.FullName,(Join-Path $target $file.Name))
    }
}

function Invoke-ExpectedRejection([string]$Name, [string]$Pattern) {
    $failure = $null
    try { & $audit -EvidenceRoot $work -TraceOnly | Out-Null }
    catch { $failure = $_.Exception.Message }
    if (!$failure -or $failure -notmatch $Pattern) { throw "Wrong rejection for ${Name}: $failure" }
    $results.Add(@{name=$Name;status='rejected';diagnostic=$failure})
    Write-Output "PASS rejection: $Name"
}

& $audit -EvidenceRoot $work -TraceOnly
$results.Add(@{name='intact-copy';status='passed'})
foreach ($platform in @('windows-final-b','linux-final-b')) {
    $target = Join-Path $work $platform
    $traces = @(Get-ChildItem -LiteralPath $target -Filter '*.exec.tsv' -File)
    $expectedCount = if ($platform.StartsWith('windows')) { 49 } else { 42 }
    if ($traces.Count -ne $expectedCount) { throw "Frozen trace count changed: $platform" }
    # Exercise every required phase/control, not just one representative file per category.
    foreach ($trace in $traces) {
        $bytes = [IO.File]::ReadAllBytes($trace.FullName)
        try {
            Remove-Item -LiteralPath $trace.FullName
            Invoke-ExpectedRejection "$platform/missing/$($trace.Name)" '^Missing required trace:'
        } finally { [IO.File]::WriteAllBytes($trace.FullName,$bytes) }
    }
    $positiveName = if ($platform.StartsWith('windows')) { 'msvc-Debug-build.exec.tsv' } else { 'gcc-Debug-build.exec.tsv' }
    foreach ($name in @($positiveName,'control-absolute.exec.tsv')) {
        $path = Join-Path $target $name
        $bytes = [IO.File]::ReadAllBytes($path)
        $text = $utf8.GetString($bytes)
        $rows = @($text.TrimEnd("`r","`n").Split("`n"))
        $first = $rows[0].TrimEnd("`r").Split("`t")
        $mutations = @(
            @{name='empty';text='';pattern='^Empty trace:'},
            @{name='unknown-record';text="UNKNOWN`t$($first[1])`n";pattern='^Malformed trace row:'},
            @{name='relative-image';text="$($first[0])`trelative.exe`n";pattern='^Relative trace image:'},
            @{name='extra-column';text="$($first[0])`t$($first[1])`textra`n";pattern='^Malformed trace row:'},
            @{name='blank-row';text=$text + "`n";pattern='^Malformed trace row:'},
            @{name='unterminated';text=$text.TrimEnd("`r","`n");pattern='^Unterminated trace:'}
        )
        if ($name.StartsWith('control-')) {
            $mutations += @(
                @{name='executed-python';text=$text.Replace('DENIED','EXEC');pattern='^Executed Python image:'},
                @{name='duplicate-denial';text=$text + $text;pattern='^Wrong denial count:'},
                @{name='wrong-interpreter';text=$text.Replace('python','python999');pattern='^Unexpected denial image:'}
            )
        } else {
            $wrongRoot = if ($platform.StartsWith('windows')) { 'C:\wrong\cmake.exe' } else { '/wrong/cmake' }
            $mutations += @(
                @{name='wrong-root';text="EXEC`t$wrongRoot`n";pattern='^Wrong trace root:'},
                @{name='unexpected-denial';text=$text.Replace('EXEC','DENIED');pattern='^Unexpected denial image:'}
            )
        }
        foreach ($mutation in $mutations) {
            try {
                [IO.File]::WriteAllText($path,$mutation.text,$utf8)
                Invoke-ExpectedRejection "$platform/$name/$($mutation.name)" $mutation.pattern
            } finally { [IO.File]::WriteAllBytes($path,$bytes) }
        }
        try {
            [IO.File]::WriteAllBytes($path,[byte[]]@(0xff,0x0a))
            Invoke-ExpectedRejection "$platform/$name/invalid-utf8" '^Invalid UTF-8 trace:'
        } finally { [IO.File]::WriteAllBytes($path,$bytes) }
        try {
            [IO.File]::WriteAllBytes($path,[byte[]](@(0xff,0xfe) + [Text.Encoding]::Unicode.GetBytes($text)))
            Invoke-ExpectedRejection "$platform/$name/utf16-bom" '^Invalid UTF-8 trace:'
        } finally { [IO.File]::WriteAllBytes($path,$bytes) }
    }
    $extra = Join-Path $target 'unexpected.exec.tsv'
    try {
        [IO.File]::WriteAllText($extra,"EXEC`t/unexpected`n",$utf8)
        Invoke-ExpectedRejection "$platform/extra-trace" '^Unexpected trace:'
    } finally { Remove-Item -LiteralPath $extra }
    $descendant = Join-Path $target 'control-descendant.exec.tsv'
    $bytes = [IO.File]::ReadAllBytes($descendant)
    try {
        $denied = @($utf8.GetString($bytes).Split("`n") | Where-Object { $_.StartsWith("DENIED`t") })[0]
        [IO.File]::WriteAllText($descendant,$denied + "`n",$utf8)
        Invoke-ExpectedRejection "$platform/descendant-without-exec" '^Wrong trace root:'
    } finally { [IO.File]::WriteAllBytes($descendant,$bytes) }
}
& $audit -EvidenceRoot $work -TraceOnly
$results.Add(@{name='restored-copy';status='passed'})
foreach ($original in $originals) {
    if ((Get-FileHash -LiteralPath $original.path).Hash -cne $original.sha256) {
        throw "Canonical evidence changed: $($original.path)"
    }
}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Report))) | Out-Null
@{utc=[DateTime]::UtcNow.ToString('o');auditSha256=(Get-FileHash -LiteralPath $audit).Hash;
    testSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;isolatedCopy=$work;
    canonicalFilesUnchanged=$originals.Count;negativeCount=@($results | Where-Object status -eq 'rejected').Count;
    results=$results} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Report -Encoding utf8NoBOM
Write-Output "PASS trace audit regressions: $($results.Count - 2) rejected mutations; intact/restored copies passed; $($originals.Count) canonical files unchanged"
