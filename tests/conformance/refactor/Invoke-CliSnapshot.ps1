param(
    [Parameter(Mandatory)][string]$Bin,
    [Parameter(Mandatory)][string]$Output,
    [string]$Compare,
    [ValidateSet('All', 'Historical', 'Malformed')][string]$Suite = 'All'
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$cases = [Collections.Generic.List[object]]::new()
function Add-Case([string]$Tool, [string[]]$Arguments, [int]$ExpectedExit = -1) {
    $cases.Add(@{ tool = $Tool; arguments = $Arguments; expectedExit = $ExpectedExit })
}
if ($Suite -ne 'Malformed') {
    foreach ($tool in @('qdcxx-qtp-inspect', 'qdcxx-tape-inspect')) {
        Add-Case $tool @()
        Add-Case $tool @('--help')
        Add-Case $tool @('--input')
        Add-Case $tool @('--unknown', '1')
        Add-Case $tool @('--input', 'missing', '--profile', 'oracle-sre-v1', '--profile', 'oracle-sre-v1')
        foreach ($operand in @('', '-1', '+1', ' 1', '1 ', '1x', '18446744073709551616', '0', '0001', '18446744073709551615')) {
            Add-Case $tool @('--input', 'missing', '--profile', 'oracle-sre-v1', '--max-input', $operand)
        }
    }
    $fixtures = @(
        @('qdcxx-qtp-inspect', 'oracle-sre-v1', 'legacy', 'tests/fixtures/protocol/offline-inspection/expected/composed.qtp'),
        @('qdcxx-qtp-inspect', 'oracle-sre-v1', 'legacy', 'tests/fixtures/protocol/tape/expected/constrained.qtp'),
        @('qdcxx-tape-inspect', 'oracle-sre-v1', 'legacy', 'tests/fixtures/protocol/tape/expected/constrained.qtp'),
        @('qdcxx-tape-inspect', 'oracle-sre-v1', 'legacy', 'tests/fixtures/protocol/tape/expected/default.qtp'),
        @('qdcxx-tape-inspect', 'default-quote-v1', 'legacy', 'tests/fixtures/protocol/quote/expected/default-quote.qtp'),
        @('qdcxx-tape-inspect', 'default-quote-v1', 'raw-message-v1', 'tests/fixtures/protocol/raw-tape/expected/timing-sequence.qtp'),
        @('qdcxx-tape-inspect', 'oracle-field-types-v1', 'legacy', 'tests/fixtures/encoding/field-types/expected/mixed-stream.qtp'),
        @('qdcxx-tape-inspect', 'oracle-field-types-v1', 'raw-message-v1', 'tests/fixtures/encoding/field-types/expected/mixed-raw.qtp'),
        @('qdcxx-tape-inspect', 'default-profile-v1', 'legacy', 'tests/fixtures/events/mapping/expected/default/Profile-stream.qtp'),
        @('qdcxx-tape-inspect', 'default-profile-v1', 'raw-message-v1', 'tests/fixtures/events/mapping/expected/default/Profile-raw.qtp'),
        @('qdcxx-tape-inspect', 'default-profile-v1', 'legacy', 'tests/fixtures/events/mapping/expected/default/Profile-wire-omitted.qtp')
    )
    foreach ($fixture in $fixtures) {
        if (!(Test-Path -LiteralPath (Join-Path $root $fixture[3]))) { throw "Missing fixture $($fixture[3])" }
        $arguments = @('--profile', $fixture[1], '--input', $fixture[3])
        if ($fixture[0] -eq 'qdcxx-tape-inspect') { $arguments += @('--tape-mode', $fixture[2]) }
        foreach ($format in @('tsv', 'human')) {
            $formatted = $arguments + @('--output-format', $format)
            Add-Case $fixture[0] $formatted
            foreach ($chunk in @('1', '7', '137')) { Add-Case $fixture[0] ($formatted + @('--chunk-size', $chunk)) }
            foreach ($budget in @('--max-input', '--max-output', '--max-frame', '--max-records', '--max-fields')) {
                Add-Case $fixture[0] ($formatted + @($budget, '0'))
            }
            if ($fixture[0] -eq 'qdcxx-tape-inspect') {
                foreach ($version in @('1', '2')) { Add-Case $fixture[0] ($formatted + @('--diagnostics-version', $version)) }
            }
        }
    }
}
if ($Suite -ne 'Historical') {
    # Existing malformed fixtures only; zero budgets/unsupported inputs do not stand in for exit 5.
    $malformedFixtures = @(
        @('qdcxx-qtp-inspect', 'legacy', 'tests/fixtures/protocol/offline-inspection/designed/bad-description.qtp'),
        @('qdcxx-qtp-inspect', 'legacy', 'tests/fixtures/protocol/offline-inspection/designed/payload-truncated.qtp'),
        @('qdcxx-qtp-inspect', 'legacy', 'tests/fixtures/protocol/offline-inspection/designed/truncated-frame.qtp'),
        @('qdcxx-qtp-inspect', 'legacy', 'tests/fixtures/protocol/offline-inspection/designed/truncated-prefix.qtp'),
        @('qdcxx-qtp-inspect', 'legacy', 'tests/fixtures/protocol/offline-inspection/designed/wide-payload-truncated.qtp'),
        @('qdcxx-tape-inspect', 'legacy', 'tests/fixtures/protocol/tape-reader/expected/malformed-header-49.qtp'),
        @('qdcxx-tape-inspect', 'legacy', 'tests/fixtures/protocol/tape-reader/expected/malformed-data.qtp'),
        @('qdcxx-tape-inspect', 'legacy', 'tests/fixtures/protocol/tape-reader/expected/truncated-135.qtp'),
        @('qdcxx-tape-inspect', 'raw-message-v1', 'tests/fixtures/protocol/raw-tape/expected/timestamp-truncated-1.qtp'),
        @('qdcxx-tape-inspect', 'raw-message-v1', 'tests/fixtures/protocol/raw-tape/expected/timestamp-truncated-3.qtp')
    )
    foreach ($fixture in $malformedFixtures) {
        if (!(Test-Path -LiteralPath (Join-Path $root $fixture[2]))) { throw "Missing fixture $($fixture[2])" }
        $arguments = @('--profile', 'oracle-sre-v1', '--input', $fixture[2])
        $versions = @('default')
        if ($fixture[0] -eq 'qdcxx-tape-inspect') {
            $arguments += @('--tape-mode', $fixture[1])
            $versions += @('1', '2')
        }
        foreach ($format in @('tsv', 'human')) {
            foreach ($version in $versions) {
                foreach ($chunk in @('default', '1', '7', '137')) {
                    $options = $arguments + @('--output-format', $format)
                    if ($version -ne 'default') { $options += @('--diagnostics-version', $version) }
                    if ($chunk -ne 'default') { $options += @('--chunk-size', $chunk) }
                    Add-Case $fixture[0] $options 5
                }
            }
        }
    }
    $malformedCount = @($cases | Where-Object expectedExit -EQ 5).Count
    if ($malformedCount -ne 160) { throw "Malformed coverage changed: expected 160 cases, got $malformedCount" }
}
$rows = foreach ($case in $cases) {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path ([IO.Path]::GetFullPath($Bin)) ($case.tool + '.exe')
    $start.WorkingDirectory = $root
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $case.arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $stdout = [IO.MemoryStream]::new()
    $stderr = [IO.MemoryStream]::new()
    $outTask = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
    $errTask = $process.StandardError.BaseStream.CopyToAsync($stderr)
    $process.WaitForExit()
    [void]$outTask.GetAwaiter().GetResult()
    [void]$errTask.GetAwaiter().GetResult()
    if ($case.expectedExit -ge 0 -and $process.ExitCode -ne $case.expectedExit) {
        throw "Expected malformed exit $($case.expectedExit), got $($process.ExitCode): $($case.tool) $($case.arguments -join ' ')"
    }
    [ordered]@{ tool = $case.tool; arguments = $case.arguments; exit = $process.ExitCode
        stdout = [Convert]::ToBase64String($stdout.ToArray()); stderr = [Convert]::ToBase64String($stderr.ToArray()) }
    $process.Dispose()
    $stdout.Dispose()
    $stderr.Dispose()
}
$json = ConvertTo-Json -InputObject @($rows) -Depth 5
[IO.File]::WriteAllText([IO.Path]::GetFullPath($Output), $json + "`n")
if ($Compare -and [IO.File]::ReadAllText([IO.Path]::GetFullPath($Compare)) -cne ($json + "`n")) {
    throw 'CLI stdout/stderr/exit snapshot differs'
}
Write-Output "Captured $($rows.Count) exact CLI results; suite=$Suite; exit5=$(@($rows | Where-Object exit -EQ 5).Count); comparison=$([bool]$Compare)"
