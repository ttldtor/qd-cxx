<#
.SYNOPSIS
Exercise strict subscription-oracle validation and immutable baseline comparison.
.DESCRIPTION
Runs the comparator against real capture artifacts, then mutates only fresh copies in RunDirectory.
Checks help, argument errors, valid/equal inputs, typed cells, TSV structure, case coverage, byte
changes, file sets, reparse points and finite budgets. Preserves all copies, logs and results.json.
The supplied source artifacts and the test's expected copy must retain their original hashes.
.PARAMETER Artifacts
Existing complete artifact tree, typically Capture.cmake's RUN/artifacts. No default.
.PARAMETER RunDirectory
Nonexistent test directory with an existing parent, outside Artifacts. No default.
.PARAMETER Help
Print usage without reading inputs or creating test files. No arguments do the same.
.EXAMPLE
pwsh -NoProfile -File tools/upstream/subscription-oracle/tests/Test-Comparison.ps1 -Artifacts run/artifacts -RunDirectory run/comparison-tests
.OUTPUTS
Short status on stdout; detailed private logs and results.json under RunDirectory.
.NOTES
Requires PowerShell 7.4+. Each comparator process has a 30-second timeout. No network or new
dependencies. Creates private copies, changes bytes and removes one copied file for negative
tests; never changes Artifacts. Exit 0 help/success, 2 test/I/O failure, 3 invalid combinations;
PowerShell binding errors can exit 1. Preserved test copies may include a reparse point.
#>
[CmdletBinding()]
param(
    [string]$Artifacts,
    [string]$RunDirectory,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$usage = @(
    'Usage: Test-Comparison.ps1 [-Help | -Artifacts DIRECTORY -RunDirectory FRESH_DIRECTORY]'
    'Test read-only artifact validation and exact comparison using private mutated copies.'
    'No arguments and -Help print this help without I/O.'
    'Requires existing capture artifacts and a nonexistent test directory outside them.'
    'Creates copies, stdout/stderr logs and results.json; preserves all evidence.'
    'Each child process has a 30-second timeout. No network or new dependencies.'
    'Exit: 0 help/success; 2 test/I/O failure; 3 argument combinations; binding errors 1.'
) -join "`n"

function Invoke-Comparator([string]$Name, [string[]]$Arguments, [int]$Exit) {
    $actualIndex = [Array]::IndexOf($Arguments, '-Actual')
    if ($actualIndex -ge 0 -and $actualIndex + 1 -lt $Arguments.Length) {
        $candidate = $Arguments[$actualIndex + 1]
        if ($candidate.StartsWith($script:run + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase) -and [IO.Directory]::Exists($candidate) -and
            $Name -notin @('stale-manifest', 'missing-manifest', 'bad-manifest', 'reparse-point')) {
            Update-Manifest $candidate
        }
    }
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = (Get-Process -Id $PID).Path
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile', '-File', $script:comparator) + $Arguments) {
        $info.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(30000)) {
            $process.Kill()
            throw "Own comparator process timed out: $Name"
        }
        $out = $stdout.GetAwaiter().GetResult()
        $err = $stderr.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $script:run "$Name.stdout"), $out, $script:utf8)
        [IO.File]::WriteAllText((Join-Path $script:run "$Name.stderr"), $err, $script:utf8)
        $script:results.Add([pscustomobject]@{
            name = $Name; exit = $process.ExitCode; expected_exit = $Exit
        })
        if ($process.ExitCode -ne $Exit -or
            ($Exit -eq 0 -and $err.Length -ne 0) -or
            ($Exit -ne 0 -and ($err.Length -eq 0 -or $out.Length -ne 0))) {
            throw "Unexpected exit/output for ${Name}; see preserved logs."
        }
        return [pscustomobject]@{ Stdout = $out; Stderr = $err }
    } finally {
        $process.Dispose()
    }
}

function Update-Manifest([string]$Path) {
    $root = (Get-Item -LiteralPath $Path).FullName.TrimEnd('\', '/')
    $files = [Collections.Generic.SortedDictionary[string, IO.FileInfo]]::new([StringComparer]::Ordinal)
    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File) {
        $relative = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
        if ($relative -cne 'manifest.tsv') { $files.Add($relative, $file) }
    }
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("path`tbytes`tsha256")
    foreach ($entry in $files.GetEnumerator()) {
        $hash = (Get-FileHash -LiteralPath $entry.Value.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $lines.Add("$($entry.Key)`t$($entry.Value.Length)`t$hash")
    }
    Set-Table (Join-Path $root 'manifest.tsv') $lines.ToArray()
}

function Get-TreeHashes([string]$Path) {
    $root = (Get-Item -LiteralPath $Path).FullName.TrimEnd('\', '/')
    return @(Get-ChildItem -LiteralPath $root -Recurse -File | Sort-Object FullName |
        ForEach-Object {
            $_.FullName.Substring($root.Length + 1) + ':' +
                (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        })
}

function New-Variant([string]$Name) {
    $path = Join-Path $script:run $Name
    Copy-Item -LiteralPath $script:baseline -Destination $path -Recurse
    return $path
}

function Set-Table([string]$Path, [string[]]$Lines) {
    [IO.File]::WriteAllText($Path, (($Lines -join "`n") + "`n"), $script:utf8)
}

function Get-Table([string]$Variant, [string]$Relative) {
    $path = Join-Path $Variant $Relative
    return [pscustomobject]@{ Path = $path; Lines = [IO.File]::ReadAllLines($path, $script:utf8) }
}

try {
    if ($PSBoundParameters.Count -eq 0 -or ($Help -and $PSBoundParameters.Count -eq 1)) {
        [Console]::Out.Write($usage + "`n")
        exit 0
    }
    if ($Help -or [string]::IsNullOrWhiteSpace($Artifacts) -or
        [string]::IsNullOrWhiteSpace($RunDirectory)) {
        [Console]::Error.Write("Test-Comparison: invalid arguments; use -Help.`n")
        exit 3
    }
    $source = (Get-Item -LiteralPath $Artifacts).FullName.TrimEnd('\', '/')
    $script:run = [IO.Path]::GetFullPath($RunDirectory)
    if ([IO.Directory]::Exists($script:run) -or [IO.File]::Exists($script:run) -or
        $script:run.Equals($source, [StringComparison]::OrdinalIgnoreCase) -or
        $script:run.StartsWith($source + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($script:run))) {
        throw 'RunDirectory must be fresh, outside Artifacts and have an existing parent.'
    }
    [void][IO.Directory]::CreateDirectory($script:run)
    $script:utf8 = [Text.UTF8Encoding]::new($false, $true)
    $script:comparator = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Compare-Oracle.ps1'))
    $script:results = [Collections.Generic.List[object]]::new()
    $start = [DateTime]::UtcNow
    [void](Invoke-Comparator 'source-validation' @('-Actual', $source) 0)
    $sourceBefore = Get-TreeHashes $source
    $script:baseline = Join-Path $script:run 'expected'
    Copy-Item -LiteralPath $source -Destination $script:baseline -Recurse
    $expectedBefore = Get-TreeHashes $script:baseline

    $noargs = Invoke-Comparator 'noargs' @() 0
    $helpResult = Invoke-Comparator 'help' @('-Help') 0
    if ($noargs.Stdout.Length -eq 0 -or $noargs.Stdout -cne $helpResult.Stdout) {
        throw 'No-argument/help equivalence failed.'
    }
    [void](Invoke-Comparator 'invalid-missing-actual' @('-Expected', 'not-read') 3)
    [void](Invoke-Comparator 'invalid-help-combination' @('-Help', '-Actual', 'not-read') 3)
    [void](Invoke-Comparator 'invalid-empty-actual' @('-Actual', '') 3)
    [void](Invoke-Comparator 'missing-directory' @('-Actual', (Join-Path $script:run 'missing')) 2)
    [void](Invoke-Comparator 'equal-baseline' @('-Actual', $source, '-Expected', $script:baseline) 0)

    $variant = New-Variant 'wrong-scalar'
    $table = Get-Table $variant 'collectors/observations.tsv'
    for ($i = 1; $i -lt $table.Lines.Length; $i++) {
        if ($table.Lines[$i] -match "`ti:") {
            $cells = $table.Lines[$i].Split([char]"`t")
            $cells[4] = if ($cells[4] -ceq 'i:0') { 'i:1' } else { 'i:0' }
            $table.Lines[$i] = $cells -join "`t"
            break
        }
    }
    if ($i -eq $table.Lines.Length) { throw 'Real collector capture has no scalar to mutate.' }
    Set-Table $table.Path $table.Lines
    [void](Invoke-Comparator 'wrong-scalar' @('-Actual', $variant, '-Expected', $script:baseline) 2)

    $variant = New-Variant 'duplicate-key'
    $table = Get-Table $variant 'collectors/observations.tsv'
    Set-Table $table.Path ($table.Lines + @($table.Lines[1]))
    [void](Invoke-Comparator 'duplicate-key' @('-Actual', $variant) 2)

    foreach ($mutation in @('bad-hex', 'bad-scalar-range', 'bad-arity', 'minus-one-index')) {
        $variant = New-Variant $mutation
        $table = Get-Table $variant 'collectors/observations.tsv'
        $cells = $table.Lines[1].Split([char]"`t")
        switch ($mutation) {
            'bad-hex' { $cells[4] = 'u:xyz' }
            'bad-scalar-range' { $cells[4] = 'i:9223372036854775808' }
            'bad-arity' { $cells += 'extra' }
            'minus-one-index' { $cells[2] = '-1' }
        }
        $table.Lines[1] = $cells -join "`t"
        Set-Table $table.Path $table.Lines
        $exit = if ($mutation -ceq 'minus-one-index') { 0 } else { 2 }
        [void](Invoke-Comparator $mutation @('-Actual', $variant) $exit)
    }

    $variant = New-Variant 'missing-case'
    $table = Get-Table $variant 'collectors/cases.tsv'
    Set-Table $table.Path (@($table.Lines[0]) + @($table.Lines | Select-Object -Skip 2))
    [void](Invoke-Comparator 'missing-case' @('-Actual', $variant) 2)

    $variant = New-Variant 'uncovered-case'
    $table = Get-Table $variant 'collectors/cases.tsv'
    Set-Table $table.Path ($table.Lines + @("test-uncovered`tproduction`tProduction.method`tobserved"))
    [void](Invoke-Comparator 'uncovered-case' @('-Actual', $variant) 2)

    $variant = New-Variant 'bad-header'
    $table = Get-Table $variant 'collectors/cases.tsv'
    $table.Lines[0] = $table.Lines[0].Replace('case', 'CASE')
    Set-Table $table.Path $table.Lines
    [void](Invoke-Comparator 'bad-header' @('-Actual', $variant) 2)

    foreach ($mutation in @('bom', 'crlf', 'invalid-utf8')) {
        $variant = New-Variant $mutation
        $path = Join-Path $variant 'collectors/cases.tsv'
        $bytes = [IO.File]::ReadAllBytes($path)
        switch ($mutation) {
            'bom' { [IO.File]::WriteAllBytes($path, ([byte[]]@(239, 187, 191) + $bytes)) }
            'crlf' { [IO.File]::WriteAllText($path, $script:utf8.GetString($bytes).Replace("`n", "`r`n"), $script:utf8) }
            'invalid-utf8' { $bytes[0] = 255; [IO.File]::WriteAllBytes($path, $bytes) }
        }
        [void](Invoke-Comparator $mutation @('-Actual', $variant) 2)
    }

    $variant = New-Variant 'missing-file'
    $target = Get-ChildItem -LiteralPath (Join-Path $variant 'wire') -Filter '*.qtp' -File | Select-Object -First 1
    if (-not $target.FullName.StartsWith($variant + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw 'Copied deletion target escaped its variant.' }
    Remove-Item -LiteralPath $target.FullName
    [void](Invoke-Comparator 'missing-file' @('-Actual', $variant, '-Expected', $script:baseline) 2)

    $variant = New-Variant 'extra-file'
    [IO.File]::WriteAllBytes((Join-Path $variant 'wire/test-extra.qtp'), [byte[]]@(0))
    [void](Invoke-Comparator 'extra-file' @('-Actual', $variant, '-Expected', $script:baseline) 2)

    foreach ($mutation in @('binary-bitflip', 'empty-binary')) {
        $variant = New-Variant $mutation
        $target = Get-ChildItem -LiteralPath (Join-Path $variant 'wire') -Filter '*.qtp' -File |
            Where-Object Length -gt 0 | Select-Object -First 1
        $bytes = [IO.File]::ReadAllBytes($target.FullName)
        if ($mutation -ceq 'binary-bitflip') {
            $bytes[0] = $bytes[0] -bxor 1
            [IO.File]::WriteAllBytes($target.FullName, $bytes)
            [void](Invoke-Comparator $mutation @('-Actual', $variant, '-Expected', $script:baseline) 2)
        } else {
            [IO.File]::WriteAllBytes($target.FullName, [byte[]]::new(0))
            [void](Invoke-Comparator $mutation @('-Actual', $variant) 0)
        }
    }

    $variant = New-Variant 'stale-manifest'
    $target = Get-ChildItem -LiteralPath (Join-Path $variant 'wire') -Filter '*.qtp' -File |
        Where-Object Length -gt 0 | Select-Object -First 1
    $bytes = [IO.File]::ReadAllBytes($target.FullName)
    $bytes[0] = $bytes[0] -bxor 1
    [IO.File]::WriteAllBytes($target.FullName, $bytes)
    $result = Invoke-Comparator 'stale-manifest' @('-Actual', $variant) 2
    if ($result.Stderr -notmatch 'Manifest disagrees') { throw 'Stale manifest was not diagnosed.' }

    $variant = New-Variant 'missing-manifest'
    Remove-Item -LiteralPath (Join-Path $variant 'manifest.tsv')
    [void](Invoke-Comparator 'missing-manifest' @('-Actual', $variant) 2)

    $variant = New-Variant 'bad-manifest'
    $table = Get-Table $variant 'manifest.tsv'
    $firstEntry = $table.Lines[1]
    $table.Lines[1] = $table.Lines[2]
    $table.Lines[2] = $firstEntry
    Set-Table $table.Path $table.Lines
    [void](Invoke-Comparator 'bad-manifest' @('-Actual', $variant) 2)

    $variant = New-Variant 'bad-provenance'
    $table = Get-Table $variant 'provenance.tsv'
    $table.Lines[1] = "pin`tupstream.lock`t" + ('0' * 39)
    Set-Table $table.Path $table.Lines
    [void](Invoke-Comparator 'bad-provenance' @('-Actual', $variant) 2)

    $variant = New-Variant 'file-budget'
    for ($i = 0; $i -lt 129; $i++) {
        [IO.File]::WriteAllBytes((Join-Path $variant "wire/budget-$i.qtp"), [byte[]]::new(0))
    }
    [void](Invoke-Comparator 'file-budget' @('-Actual', $variant) 2)

    $variant = New-Variant 'byte-budget'
    $target = Get-ChildItem -LiteralPath (Join-Path $variant 'wire') -Filter '*.qtp' -File | Select-Object -First 1
    [IO.File]::WriteAllBytes($target.FullName, [byte[]]::new(4MB + 1))
    [void](Invoke-Comparator 'byte-budget' @('-Actual', $variant) 2)

    $variant = New-Variant 'reparse-point'
    $link = Join-Path $variant 'wire/link'
    $type = if ($IsWindows) { 'Junction' } else { 'SymbolicLink' }
    $reparseCreated = $false
    try {
        [void](New-Item -ItemType $type -Path $link -Target (Join-Path $script:baseline 'wire'))
        $reparseCreated = $true
    } catch [ComponentModel.Win32Exception] {
        if ($_.Exception.NativeErrorCode -ne 5) { throw }
        $script:results.Add([pscustomobject]@{
            name = 'reparse-point'; exit = $null; expected_exit = 2
            outcome = 'not-executed'; reason = 'Native access denied creating test reparse point.'
        })
    } catch [UnauthorizedAccessException] {
        $script:results.Add([pscustomobject]@{
            name = 'reparse-point'; exit = $null; expected_exit = 2
            outcome = 'not-executed'; reason = 'Access denied creating test reparse point.'
        })
    }
    if ($reparseCreated) {
        $result = Invoke-Comparator 'reparse-point' @('-Actual', $variant) 2
        if ($result.Stderr -notmatch 'Reparse point') { throw 'Reparse rejection was not observed.' }
    }

    if (@(Compare-Object $sourceBefore (Get-TreeHashes $source)).Count -ne 0 -or
        @(Compare-Object $expectedBefore (Get-TreeHashes $script:baseline)).Count -ne 0) {
        throw 'Source or expected artifact bytes changed during testing.'
    }
    $skipped = @($script:results | Where-Object { $null -eq $_.exit }).Count
    $report = [ordered]@{
        result = if ($skipped -eq 0) { 'passed' } else { 'passed-with-skip' }
        start_utc = $start.ToString('o')
        end_utc = [DateTime]::UtcNow.ToString('o')
        source_and_expected_immutable = $true
        checks = $script:results.ToArray()
    }
    [IO.File]::WriteAllText((Join-Path $script:run 'results.json'),
        ($report | ConvertTo-Json -Depth 5), $script:utf8)
    [Console]::Out.Write("Comparison tests passed: $($script:results.Count - $skipped) executed, $skipped skipped.`n")
    exit 0
} catch {
    [Console]::Error.Write("Test-Comparison: $($_.Exception.Message)`n")
    exit 2
}
