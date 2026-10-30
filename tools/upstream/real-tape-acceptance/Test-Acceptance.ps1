#requires -Version 7.4
<#
.SYNOPSIS
Runs portable ordinary-tape parity, comparator mutations, EOF/error and CLI checks.
.DESCRIPTION
Uses existing public generated fixtures only. Creates a new ignored result directory,
compiles the locked oracle through Compare-RealTapes.ps1, and retains full raw evidence.
Does not build upstream or C++, modify fixtures, use private recordings or use the network.
.PARAMETER Inspector
Built version-7 ordinary-v1 C++ tape inspector.
.PARAMETER UpstreamRoot
Clean prebuilt checkout matching upstream.lock.
.PARAMETER JavaHome
Oracle JDK 8 home.
.PARAMETER Runner
Explicit native qdcxx-run-utc executable for pre-execution Windows job ownership.
.PARAMETER RunDirectory
New directory below repository .work for all results and derived negative files.
.PARAMETER DesignedV8
Additionally validates designed output8 rows derived from actual output7 captures.
This does not execute or qualify a production C++ output8 implementation.
.PARAMETER BinaryCatalog
Additionally requires actual C++ binary-catalog-v1/output8 success for both immutable
producer quote-message files. Existing four old-mode refusals remain mandatory.
.PARAMETER SourceProfile
Additionally requires actual output9 for both producer MESSAGE recordings, strict source/time
mutations, actual protocol-header checks and compatible repeated-header acceptance.
.PARAMETER Help
Shows usage without inputs, dependencies or file I/O. No arguments is identical.
.NOTES
Exit 0: all supported positive/negative checks passed; named STREAM/MESSAGE gates remain
explicitly pending. Nonzero/exception: setup or regression failure. Bounds: 128 MiB Java,
60 seconds per child, 8 MiB input, 256 MiB output; no unrelated process is stopped.
#>
[CmdletBinding()]
param([string]$Inspector, [string]$UpstreamRoot, [string]$JavaHome, [string]$Runner,
    [string]$RunDirectory, [switch]$DesignedV8, [switch]$BinaryCatalog, [switch]$SourceProfile, [switch]$Help)
if ($Help -or $PSBoundParameters.Count -eq 0) {
    @'
Test-Acceptance.ps1 -Inspector EXE -UpstreamRoot PATH -JavaHome JDK8 -Runner EXE
                   -RunDirectory NEW_REPOSITORY_WORK_PATH
                   [-DesignedV8] [-BinaryCatalog] [-SourceProfile]
Portable full comparisons, raw value/schema/source/time/order mutation controls,
true EOF/late read and close errors, truncation, closed sink and help/no-I/O checks.
Uses public fixtures only; no upstream/C++ builds or network. Full logs below .work.
Bounds per child: 60 seconds, Java 128 MiB, input 8 MiB, output 256 MiB.
Windows-qualified process-tree ownership. POSIX launcher is not qualified.
DesignedV8 tests labelled designed rows only; BinaryCatalog requires the actual C++8 artifact.
SourceProfile requires actual output9; one uncompressed MESSAGE ordinary source only.
Exit 0: assigned checks passed, including explicit pending STREAM/MESSAGE gates.
Exception/nonzero: setup/check failure. No arguments / -Help starts no file work.
'@
    return
}
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$Inspector -or !$UpstreamRoot -or !$JavaHome -or !$Runner -or !$RunDirectory) { throw 'All five working parameters are required. Use -Help.' }
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
. (Join-Path $PSScriptRoot 'Acceptance.Common.ps1')
$run = Get-AcceptanceWorkPath $repo $RunDirectory -RequireNew
New-Item -ItemType Directory -Path $run | Out-Null
$compare = Join-Path $repo 'tests/conformance/protocol/real-tape/Compare-RealTapes.ps1'
$inspectorPath = (Resolve-Path -LiteralPath $Inspector).Path
$runnerPath = (Resolve-Path -LiteralPath $Runner).Path
$results = [Collections.Generic.List[object]]::new()
$started = [DateTime]::UtcNow.ToString('o')
$status = 'INCOMPLETE'
$utf8 = [Text.UTF8Encoding]::new($false)
$sourceHash = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'RealTapeAcceptance.java')).Hash.ToLowerInvariant()
$workerHash = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'RawChild.java')).Hash.ToLowerInvariant()
$scriptHash = (Get-FileHash -LiteralPath $PSCommandPath).Hash.ToLowerInvariant()
function Write-TestProjection([string]$Path, [string[]]$Rows) {
    [IO.File]::WriteAllText($Path, ($Rows -join "`n") + "`n", $utf8)
}
function Convert-DesignedV8([string[]]$Rows) {
    $result = [Collections.Generic.List[string]]::new()
    $frame = -1
    foreach ($row in $Rows) {
        $cells = $row.Split([char]9)
        switch ($cells[0]) {
            'format' { $result.Add("format`tqdcxx-tape-inspect`t8`tbinary-catalog-v1`tbinary-catalog-v1") }
            'frame' { $frame = [long]$cells[1]; $result.Add($row) }
            { $_ -in @('descriptor', 'descriptor-v3') } {
                # This is intentionally a designed canonical descriptor, not observed C++8 metadata.
                $result.Add("descriptor-v8`t$frame`t0`t-`t1`t-`t0`t-`t0")
            }
            'wire-data' {
                $values = if ($cells[8]) { @($cells[8].Split(',') | ForEach-Object { $_ + ':-' }) -join ',' } else { '' }
                $timing = if ($cells.Count -eq 12) { $cells[9..11] } else { @('-', '0', '0') }
                $result.Add((@('wire-data-v8') + $cells[1..7] + @($values) + $timing + @('0', '-')) -join "`t")
            }
            default { $result.Add($row) }
        }
    }
    return $result.ToArray()
}
function Invoke-TestJava([string]$Name, [string]$InputPath, [string]$ProjectionPath,
        [string]$Mode, [string[]]$Extra = @(), [string]$ExpectedReason = '') {
    $args = @('--input', $InputPath, '--cpp-output', $ProjectionPath, '--tape-mode', $Mode) + $Extra
    $result = Invoke-AcceptanceProcess $java ($runtime + $args) $run $Name 60 268435456 $repo
    if ($result.timedOut -or $result.outputExceeded -or $result.exitCode -ne 1) { throw "Expected Java rejection: $Name" }
    $reason = @(Get-Content -LiteralPath $result.stderr | Where-Object { $_ -match '^reason\t' })
    if ($ExpectedReason -and ($reason -join '') -notmatch $ExpectedReason) { throw "Wrong rejection phase: $Name" }
    $results.Add([ordered]@{ name = $Name; expected = 'rejected'; result = $result; reason = $reason })
}
function Invoke-TestCppRejection([string]$Name, [string]$InputPath, [string]$Mode, [int]$ExitCode,
        [string[]]$Extra = @()) {
    $args = @('--input', $InputPath, '--wire-schema', 'ordinary-v1', '--output-format', 'tsv', '--tape-mode', $Mode) + $Extra
    $result = Invoke-AcceptanceProcess $inspectorPath $args $run $Name 60 268435456 $repo
    if ($result.timedOut -or $result.outputExceeded -or $result.exitCode -ne $ExitCode -or
        (Get-Content -LiteralPath $result.stdout -Raw) -match '(?m)^complete\t') { throw "Expected C++ rejection: $Name" }
    $results.Add([ordered]@{ name = $Name; expected = $ExitCode; result = $result })
}
function Test-SourceProjection($Case) {
    $sourceRows = [IO.File]::ReadAllLines((Join-Path $Case.directory 'cpp.stdout'))
    $first = [Array]::FindIndex($sourceRows, [Predicate[string]]{param($line) $line.StartsWith("wire-data-v9`t")})
    if ($first -lt 0) { throw 'Expected actual output9 data' }
    foreach ($column in @(1,3,4,11,12,13,16,17,22,23,24,25,26)) {
        $changed = [string[]]$sourceRows.Clone()
        $cells = $changed[$first].Split([char]9)
        $cells[$column] = switch ($column) {
            16 {'1'} 17 {'0'} 22 {'none'} 23 {'sidecar'} 24 {'zero'}
            default { ([long]$cells[$column] + 1).ToString() }
        }
        $changed[$first] = $cells -join "`t"
        $path = Join-Path $run "source-negative-cell-$column.tsv"
        Write-TestProjection $path $changed
        Invoke-TestJava "source-negative-cell-$column" $Case.input $path 'raw-message-v1' `
            @('--cpp-projection','self-contained-segments-v1') 'v9-|record-mismatch'
    }
    foreach ($kind in @('resource-missing','resource-duplicate','source-end-missing','complete-missing','old-data-row','trailing-row')) {
        $changed = [Collections.Generic.List[string]]::new()
        $changed.AddRange([string[]]$sourceRows)
        switch ($kind) {
            'resource-missing' { $i=$changed.FindIndex([Predicate[string]]{param($line) $line.StartsWith("resource`t")}); $changed.RemoveAt($i) }
            'resource-duplicate' { $i=$changed.FindIndex([Predicate[string]]{param($line) $line.StartsWith("resource`t")}); $changed.Insert($i,$changed[$i]) }
            'source-end-missing' { $i=$changed.FindIndex([Predicate[string]]{param($line) $line.StartsWith("source-end`t")}); $changed.RemoveAt($i) }
            'complete-missing' { $changed.RemoveAt($changed.Count-1) }
            'old-data-row' { $changed[$first]=$changed[$first].Replace('wire-data-v9','wire-data-v8') }
            'trailing-row' { $changed.Add("markers`t0") }
        }
        $path = Join-Path $run "source-negative-$kind.tsv"
        Write-TestProjection $path $changed.ToArray()
        Invoke-TestJava "source-negative-$kind" $Case.input $path 'raw-message-v1' `
            @('--cpp-projection','self-contained-segments-v1') 'v9-|cpp-'
    }
    Invoke-TestJava 'source-late-read-io' $Case.input (Join-Path $Case.directory 'cpp.stdout') 'raw-message-v1' `
        @('--cpp-projection','self-contained-segments-v1','--inject-late-io') 'source-incomplete'
    Invoke-TestJava 'source-close-io' $Case.input (Join-Path $Case.directory 'cpp.stdout') 'raw-message-v1' `
        @('--cpp-projection','self-contained-segments-v1','--inject-close-io') 'source-incomplete'

    $sourceBytes = [IO.File]::ReadAllBytes($Case.input)
    $firstFrame = @($sourceRows | Where-Object { $_.StartsWith("frame`t") })[0].Split([char]9)
    if ($firstFrame[4] -cne '1' -or [long]$firstFrame[2] -ne 0) { throw 'Expected initial protocol descriptor' }
    $headerEnd = [int]$firstFrame[3]
    $repeated = [Collections.Generic.List[byte]]::new()
    $repeated.AddRange([byte[]]$sourceBytes[0..($headerEnd-1)])
    $repeated.AddRange($sourceBytes)
    $repeatedInput = Join-Path $run 'source-repeated-matching.qtp'
    [IO.File]::WriteAllBytes($repeatedInput, $repeated.ToArray())
    $repeatDirectory = Join-Path $run 'source-repeated-matching'
    $null = & $compare -InputFile $repeatedInput -Inspector $inspectorPath -UpstreamRoot $UpstreamRoot `
        -JavaHome $JavaHome -Runner $runnerPath -RunDirectory $repeatDirectory -TapeMode raw-message-v1 `
        -SourceProfile self-contained-segments-v1
    $results.Add([ordered]@{name='source-repeated-matching'; expected='actual-output9-full-parity'; metadata=(Join-Path $repeatDirectory 'metadata.json')})

    foreach ($kind in @('missing','unknown','conflicting-update')) {
        $bytes = if ($kind -eq 'conflicting-update') { $repeated.ToArray() } else { [byte[]]$sourceBytes.Clone() }
        $projectionRows = if ($kind -eq 'conflicting-update') {
            [IO.File]::ReadAllLines((Join-Path $repeatDirectory 'cpp.stdout'))
        } else { [string[]]$sourceRows.Clone() }
        $needle = if ($kind -eq 'missing') {'time'} else {'message'}
        $headerText = [Text.Encoding]::ASCII.GetString($bytes)
        $index = $headerText.IndexOf($needle, [StringComparison]::Ordinal)
        if ($kind -eq 'conflicting-update') { $index=$headerText.IndexOf($needle,$index+$needle.Length,[StringComparison]::Ordinal) }
        if ($index -lt 0) { throw 'Expected production physical MESSAGE property' }
        for ($i=0; $i -lt $needle.Length; $i++) { $bytes[$index+$i]=[byte][char]'x' }
        $inputPath = Join-Path $run "source-physical-$kind.qtp"
        [IO.File]::WriteAllBytes($inputPath,$bytes)
        $identity = [Array]::FindIndex($projectionRows,[Predicate[string]]{param($line) $line.StartsWith("source`t")})
        $cells=$projectionRows[$identity].Split([char]9)
        if ($cells[2] -cne 'utf16le') { throw 'This launcher qualifies Windows native paths only' }
        $cells[3]=([Text.Encoding]::Unicode.GetBytes($inputPath) | ForEach-Object {$_.ToString('x2')}) -join ''
        $projectionRows[$identity]=$cells -join "`t"
        $path=Join-Path $run "source-physical-$kind.tsv"
        Write-TestProjection $path $projectionRows
        Invoke-TestJava "source-physical-$kind" $inputPath $path 'raw-message-v1' `
            @('--cpp-projection','self-contained-segments-v1') 'v9-java-physical-time-property'
    }
}
try {
    $cases = @(
        @('baseline-compact', 'tests/fixtures/protocol/real-tape/producer/baseline/expected/compact-none.qtp', 'legacy'),
        @('baseline-quote', 'tests/fixtures/protocol/real-tape/producer/baseline/expected/quote-none.qtp', 'legacy'),
        @('current-compact', 'tests/fixtures/protocol/real-tape/producer/current/expected/compact-none.qtp', 'legacy'),
        @('current-quote', 'tests/fixtures/protocol/real-tape/producer/current/expected/quote-none.qtp', 'legacy'),
        @('mixed-stream', 'tests/fixtures/encoding/field-types/expected/mixed-stream.qtp', 'legacy'),
        @('mixed-raw', 'tests/fixtures/encoding/field-types/expected/mixed-raw.qtp', 'raw-message-v1'),
        @('mixed-order', 'tests/fixtures/encoding/field-types/expected/mixed-order.qtp', 'legacy')
    )
    foreach ($case in $cases) {
        $caseDir = Join-Path $run $case[0]
        $null = & $compare -InputFile (Join-Path $repo $case[1]) -Inspector $inspectorPath -UpstreamRoot $UpstreamRoot `
            -JavaHome $JavaHome -Runner $runnerPath -RunDirectory $caseDir -TapeMode $case[2]
        $results.Add([ordered]@{ name = $case[0]; expected = 'full-parity'; metadata = Join-Path $caseDir 'metadata.json' })
    }
    $baseDir = Join-Path $run 'baseline-compact'
    $base = Get-Content -LiteralPath (Join-Path $baseDir 'metadata.json') -Raw | ConvertFrom-Json
    $java = Get-AcceptanceJava $JavaHome 'java'
    $testBootstrap = Initialize-AcceptanceWorker $runnerPath $java (Get-AcceptanceJava $JavaHome 'javac') `
        (Join-Path $PSScriptRoot 'RawChild.java') $run 60 $repo
    $results.Add([ordered]@{ name = 'process-worker-bootstrap'; expected = 'PASS'; result = $testBootstrap })
    $runtime = [string[]]$base.runtime
    $inputPath = $base.input.path
    $projection = Join-Path $baseDir 'cpp.stdout'
    $rows = [IO.File]::ReadAllLines($projection)
    $dataIndices = @(for ($i = 0; $i -lt $rows.Count; $i++) { if ($rows[$i].StartsWith("wire-data`t")) { $i } })
    $schemaIndex = @(for ($i = 0; $i -lt $rows.Count; $i++) { if ($rows[$i].StartsWith("schema-field`t")) { $i } })[0]
    $bindingIndex = @(for ($i = 0; $i -lt $rows.Count; $i++) { if ($rows[$i].StartsWith("wire-field`t")) { $i } })[0]
    foreach ($mutation in @('value', 'type', 'sender', 'local', 'generation', 'symbol', 'order', 'extra-record', 'schema-type', 'binding')) {
        $changed = [string[]]$rows.Clone()
        $index = $dataIndices[0]
        $cells = $changed[$index].Split([char]9)
        switch ($mutation) {
            'value' { $cells[8] = '8:-2147483647:-' }
            'type' { $cells[8] = '56:-2147483648:-' }
            'sender' { $cells[4] = '99' }
            'local' { $cells[5] = '99' }
            'generation' { $cells[3] = '99' }
            'symbol' { $cells[7] = '004100420043' }
            'order' { $changed[$index] = $rows[$dataIndices[1]]; $changed[$dataIndices[1]] = $rows[$index] }
            'extra-record' { $changed = @($changed[0..($changed.Count - 2)]) + @($rows[$index], $changed[-1]) }
            'schema-type' { $changed[$schemaIndex] = $changed[$schemaIndex] -replace "`t8$", "`t56" }
            'binding' { $changed[$bindingIndex] = $changed[$bindingIndex] -replace "`t0$", "`t1" }
        }
        if ($mutation -in @('value', 'type', 'sender', 'local', 'generation', 'symbol')) { $changed[$index] = $cells -join "`t" }
        $mutated = Join-Path $run "mutation-$mutation.tsv"
        Write-TestProjection $mutated $changed
        Invoke-TestJava "mutation-$mutation" $inputPath $mutated 'legacy' @() 'mismatch|extra-projection|cpp-data-context'
    }
    $wrongFrames = [string[]]$rows.Clone()
    $frameIndex = @(for ($i = 0; $i -lt $rows.Count; $i++) { if ($rows[$i].StartsWith("frame`t")) { $i } })[0]
    $wrongFrames[$frameIndex] = $wrongFrames[$frameIndex] -replace '^frame\t0\t', "frame`t99`t"
    $wrongFramePath = Join-Path $run 'mutation-frame-order.tsv'
    Write-TestProjection $wrongFramePath $wrongFrames
    Invoke-TestJava 'mutation-frame-order' $inputPath $wrongFramePath 'legacy' @() 'cpp-frame-order'
    $metadataRows = @($rows | Where-Object { $_ -match '^(schema|schema-record|schema-field|schema-end|wire-record|wire-field)\t' })
    $withoutMetadata = @($rows | Where-Object { $_ -notmatch '^(schema|schema-record|schema-field|schema-end|wire-record|wire-field)\t' })
    $firstData = @(for ($i = 0; $i -lt $withoutMetadata.Count; $i++) { if ($withoutMetadata[$i].StartsWith("wire-data`t")) { $i } })[0]
    $wrongSchemaOrder = @($withoutMetadata[0..$firstData]) + $metadataRows + @($withoutMetadata[($firstData + 1)..($withoutMetadata.Count - 1)])
    $wrongSchemaPath = Join-Path $run 'mutation-schema-publication.tsv'
    Write-TestProjection $wrongSchemaPath $wrongSchemaOrder
    Invoke-TestJava 'mutation-schema-publication' $inputPath $wrongSchemaPath 'legacy' @() 'cpp-data-context'
    $rawDir = Join-Path $run 'mixed-raw'
    $raw = Get-Content -LiteralPath (Join-Path $rawDir 'metadata.json') -Raw | ConvertFrom-Json
    $rawRows = [IO.File]::ReadAllLines((Join-Path $rawDir 'cpp.stdout'))
    $rawIndex = @(for ($i = 0; $i -lt $rawRows.Count; $i++) { if ($rawRows[$i].StartsWith("wire-data`t")) { $i } })[0]
    foreach ($column in @(9, 10, 11)) {
        $changed = [string[]]$rawRows.Clone()
        $cells = $changed[$rawIndex].Split([char]9)
        $cells[$column] = if ($cells[$column] -eq '-') { '0' } else { ([long]$cells[$column] + 1).ToString() }
        $changed[$rawIndex] = $cells -join "`t"
        $mutated = Join-Path $run "mutation-time-$column.tsv"
        Write-TestProjection $mutated $changed
        Invoke-TestJava "mutation-time-$column" $raw.input.path $mutated 'raw-message-v1' @() 'record-mismatch'
    }
    Invoke-TestJava 'late-read-io' $inputPath $projection 'legacy' @('--inject-late-io') 'source-incomplete'
    Invoke-TestJava 'close-io' $inputPath $projection 'legacy' @('--inject-close-io') 'source-incomplete'
    Invoke-TestJava 'record-budget' $inputPath $projection 'legacy' @('--max-records', '1') 'wire-record-budget'
    Invoke-TestJava 'input-budget' $inputPath $projection 'legacy' @('--max-input', '1') 'input-byte-budget'
    foreach ($producer in @('baseline', 'current')) {
        $message = Join-Path $repo "tests/fixtures/protocol/real-tape/producer/$producer/expected/quote-message.qtp"
        Invoke-TestCppRejection "$producer-stream-message-legacy-gate" $message 'legacy' 4
        Invoke-TestCppRejection "$producer-stream-message-raw-gate" $message 'raw-message-v1' 4
        if ($BinaryCatalog) {
            $caseDir = Join-Path $run "$producer-stream-message-binary"
            $null = & $compare -InputFile $message -Inspector $inspectorPath -UpstreamRoot $UpstreamRoot `
                -JavaHome $JavaHome -Runner $runnerPath -RunDirectory $caseDir -TapeMode raw-message-v1 -CppMode binary-catalog-v1
            $results.Add([ordered]@{ name = "$producer-stream-message-binary"; expected = 'actual-output8-full-parity'
                metadata = Join-Path $caseDir 'metadata.json' })
        }
        if ($SourceProfile) {
            $caseDir = Join-Path $run "$producer-stream-message-source"
            $null = & $compare -InputFile $message -Inspector $inspectorPath -UpstreamRoot $UpstreamRoot `
                -JavaHome $JavaHome -Runner $runnerPath -RunDirectory $caseDir -TapeMode raw-message-v1 `
                -SourceProfile self-contained-segments-v1
            $results.Add([ordered]@{name="$producer-stream-message-source";expected='actual-output9-full-parity';metadata=(Join-Path $caseDir 'metadata.json')})
            if ($producer -ceq 'baseline') { $sourceCase=@{input=$message;directory=$caseDir} }
        }
    }
    if ($SourceProfile) { Test-SourceProjection $sourceCase }
    if ($DesignedV8) {
        Invoke-TestJava 'designed-v8-invalid-projection-option' $inputPath $projection 'legacy' `
            @('--cpp-projection', 'invalid') 'cpp-projection'
        $invalidMode = $false
        try { $null = & $compare -CppMode 'invalid' } catch { $invalidMode = $true }
        if (!$invalidMode) { throw 'Invalid CppMode was accepted without working inputs.' }
        $results.Add([ordered]@{ name = 'designed-v8-invalid-cpp-mode-no-io'; expected = 'usage-rejected' })
        $designedCases = @(@('none', $inputPath, $rows, 'legacy'), @('message', $raw.input.path, $rawRows, 'raw-message-v1'))
        foreach ($designedCase in $designedCases) {
            $designed = [string[]](Convert-DesignedV8 $designedCase[2])
            $designedPath = Join-Path $run "designed-v8-$($designedCase[0]).tsv"
            Write-TestProjection $designedPath $designed
            $arguments = @('--input', $designedCase[1], '--cpp-output', $designedPath, '--tape-mode', $designedCase[3],
                '--cpp-projection', 'binary-catalog-v1')
            $result = Invoke-AcceptanceProcess $java ($runtime + $arguments) $run "designed-v8-$($designedCase[0])" 60 268435456 $repo
            Assert-AcceptanceProcess $result
            $results.Add([ordered]@{ name = "designed-v8-$($designedCase[0])"; expected = 'designed-projection-parity'
                actualCpp8Executed = $false; result = $result })
        }
        $designed = [string[]](Convert-DesignedV8 $rows)
        $first = @(for ($i = 0; $i -lt $designed.Count; $i++) { if ($designed[$i].StartsWith("wire-data-v8`t")) { $i } })[0]
        $descriptor = @(for ($i = 0; $i -lt $designed.Count; $i++) { if ($designed[$i].StartsWith("descriptor-v8`t")) { $i } })[0]
        foreach ($mutation in @('columns-short', 'columns-extra', 'bytes-null', 'bytes-empty', 'bytes-content',
                'new-type', 'scalar-units', 'int-range', 'char-range', 'raw-leading-zero', 'raw-negative-zero', 'raw-overflow',
                'utf-raw', 'utf-absent', 'utf-upper', 'utf-width', 'flags', 'flags-negative-zero', 'removal-zero',
                'none-timestamp-zero', 'none-sequence', 'none-event-time', 'sender', 'local', 'generation', 'frame', 'order',
                'symbol-upper', 'old-data-row', 'header', 'descriptor-columns', 'descriptor-presence', 'descriptor-absent-text',
                'descriptor-mask', 'descriptor-upper', 'descriptor-frame')) {
            $changed = [string[]]$designed.Clone()
            $cells = $changed[$first].Split([char]9)
            $desc = $changed[$descriptor].Split([char]9)
            $applyData = $true; $applyDescriptor = $false
            switch ($mutation) {
                'columns-short' { $cells = $cells[0..12] }
                'columns-extra' { $cells += '-' }
                'bytes-null' { $cells[8] = '8:-2147483648:-:null' }
                'bytes-empty' { $cells[8] = '8:-2147483648:-:empty' }
                'bytes-content' { $cells[8] = '8:-2147483648:-:00' }
                'new-type' { $cells[8] = '137:0:null:-' }
                'scalar-units' { $cells[8] = '8:0:empty:-' }
                'int-range' { $cells[8] = '8:2147483648:-:-' }
                'char-range' { $cells[8] = '2:1114112:-:-' }
                'raw-leading-zero' { $cells[8] = '8:00:-:-' }
                'raw-negative-zero' { $cells[8] = '8:-0:-:-' }
                'raw-overflow' { $cells[8] = '120:9223372036854775808:-:-' }
                'utf-raw' { $cells[8] = '10:1:null:-' }
                'utf-absent' { $cells[8] = '10:0:-:-' }
                'utf-upper' { $cells[8] = '10:0:00AF:-' }
                'utf-width' { $cells[8] = '10:0:000:-' }
                'flags' { $cells[12] = '1' }
                'flags-negative-zero' { $cells[12] = '-0' }
                'removal-zero' { $cells[13] = '0' }
                'none-timestamp-zero' { $cells[9] = '0' }
                'none-sequence' { $cells[10] = '1' }
                'none-event-time' { $cells[11] = '1' }
                'sender' { $cells[4] = '99' }
                'local' { $cells[5] = '99' }
                'generation' { $cells[3] = '99' }
                'frame' { $cells[1] = '99' }
                'order' { $cells[2] = '99' }
                'symbol-upper' { $cells[7] = '00AF' }
                'old-data-row' { $cells[0] = 'wire-data' }
                'header' { $changed[0] = "format`tqdcxx-tape-inspect`t7`tordinary-v1`tlegacy"; $applyData = $false }
                'descriptor-columns' { $desc = $desc[0..7]; $applyDescriptor = $true }
                'descriptor-presence' { $desc[2] = '2'; $applyDescriptor = $true }
                'descriptor-absent-text' { $desc[3] = '0061'; $applyDescriptor = $true }
                'descriptor-mask' { $desc[8] = '1'; $applyDescriptor = $true }
                'descriptor-upper' { $desc[2] = '1'; $desc[3] = '00AF'; $applyDescriptor = $true }
                'descriptor-frame' { $desc[1] = '99'; $applyDescriptor = $true }
            }
            if ($applyData) { $changed[$first] = $cells -join "`t" }
            if ($applyDescriptor) { $changed[$descriptor] = $desc -join "`t" }
            $path = Join-Path $run "designed-v8-negative-$mutation.tsv"
            Write-TestProjection $path $changed
            Invoke-TestJava "designed-v8-negative-$mutation" $inputPath $path 'legacy' @('--cpp-projection', 'binary-catalog-v1')
        }
        $designedRaw = [string[]](Convert-DesignedV8 $rawRows)
        $firstRaw = @(for ($i = 0; $i -lt $designedRaw.Count; $i++) { if ($designedRaw[$i].StartsWith("wire-data-v8`t")) { $i } })[0]
        foreach ($column in @(9, 10, 11)) {
            $changed = [string[]]$designedRaw.Clone()
            $cells = $changed[$firstRaw].Split([char]9)
            $cells[$column] = if ($cells[$column] -eq '-') { '0' } else { ([long]$cells[$column] + 1).ToString() }
            $changed[$firstRaw] = $cells -join "`t"
            $path = Join-Path $run "designed-v8-negative-message-$column.tsv"
            Write-TestProjection $path $changed
            Invoke-TestJava "designed-v8-negative-message-$column" $raw.input.path $path 'raw-message-v1' `
                @('--cpp-projection', 'binary-catalog-v1') 'record-mismatch'
        }
        $publicBytes = [IO.File]::ReadAllBytes($inputPath)
        $sourceFrames = @($rows | Where-Object { $_.StartsWith("frame`t") } | ForEach-Object { ,$_.Split([char]9) })
        $sourceMetadata = @($designed | Where-Object { $_ -match '^(schema|schema-record|schema-field|schema-end|wire-record|wire-field)\t' })
        foreach ($case in @(@('initial-empty', 0, $false), @('existing-empty', 2, $false),
                @('late-empty', 5, $false), @('equal-nonempty', 2, $true))) {
            $insertFrame = [int]$case[1]
            $insertOffset = [int]$sourceFrames[$insertFrame][2]
            $insertBytes = if ($case[2]) {
                $start = [int]$sourceFrames[1][2]; $end = [int]$sourceFrames[1][3]
                [byte[]]$publicBytes[$start..($end - 1)]
            } else { [byte[]]@(1, 2) }
            $source = [Collections.Generic.List[byte]]::new()
            if ($insertOffset -ne 0) { $source.AddRange([byte[]]$publicBytes[0..($insertOffset - 1)]) }
            $source.AddRange([byte[]]$insertBytes)
            $source.AddRange([byte[]]$publicBytes[$insertOffset..($publicBytes.Length - 1)])
            $derivedInput = Join-Path $run "designed-v8-$($case[0]).qtp"
            [IO.File]::WriteAllBytes($derivedInput, $source.ToArray())
            $derivedRows = [Collections.Generic.List[string]]::new()
            foreach ($line in $designed) {
                $cells = $line.Split([char]9)
                if ($cells[0] -eq 'frame' -and [int]$cells[1] -eq $insertFrame) {
                    $size = $insertBytes.Length
                    $payload = if ($case[2]) { $size - 2 } else { 0 }
                    $derivedRows.Add((@('frame', $insertFrame, $insertOffset, ($insertOffset + $size), 2,
                        ($insertOffset + 2), $payload)) -join "`t")
                    if ($case[2]) {
                        foreach ($metadataLine in $sourceMetadata) {
                            $metaCells = $metadataLine.Split([char]9)
                            if ($metaCells[0] -eq 'schema') { $metaCells[1] = "$insertFrame"; $metaCells[2] = '2' }
                            else { $metaCells[1] = '2' }
                            $derivedRows.Add($metaCells -join "`t")
                        }
                    }
                }
                if ($cells[0] -eq 'frame' -and [int]$cells[1] -ge $insertFrame) {
                    $cells[1] = ([int]$cells[1] + 1).ToString()
                    $cells[2] = ([int]$cells[2] + $insertBytes.Length).ToString()
                    $cells[3] = ([int]$cells[3] + $insertBytes.Length).ToString()
                    if ($cells[5] -ne '-') { $cells[5] = ([int]$cells[5] + $insertBytes.Length).ToString() }
                } elseif ($cells[0] -in @('schema', 'descriptor-v8', 'wire-data-v8') -and [int]$cells[1] -ge $insertFrame) {
                    $cells[1] = ([int]$cells[1] + 1).ToString()
                    if ($case[2] -and $cells[0] -eq 'wire-data-v8') { $cells[3] = '2' }
                } elseif ($cells[0] -eq 'complete') {
                    $cells[1] = ([int]$cells[1] + $insertBytes.Length).ToString()
                    $cells[2] = ([int]$cells[2] + 1).ToString()
                }
                $derivedRows.Add($cells -join "`t")
            }
            $derivedPath = Join-Path $run "designed-v8-$($case[0]).tsv"
            Write-TestProjection $derivedPath $derivedRows.ToArray()
            $arguments = @('--input', $derivedInput, '--cpp-output', $derivedPath, '--tape-mode', 'legacy',
                '--cpp-projection', 'binary-catalog-v1')
            $result = Invoke-AcceptanceProcess $java ($runtime + $arguments) $run "designed-v8-$($case[0])" 60 268435456 $repo
            Assert-AcceptanceProcess $result
            $results.Add([ordered]@{ name = "designed-v8-$($case[0])"; expected = 'designed-generation-parity'
                actualCpp8Executed = $false; result = $result })
        }
    }
    $quote = Join-Path $repo 'tests/fixtures/protocol/real-tape/producer/baseline/expected/quote-none.qtp'
    $quoteBytes = [IO.File]::ReadAllBytes($quote)
    $descriptionFrame = ($rows | Where-Object { $c = $_.Split([char]9); $c.Count -gt 4 -and $c[0] -eq 'frame' -and $c[4] -eq '2' } | Select-Object -First 1).Split([char]9)
    foreach ($cut in @(1, ([int]$descriptionFrame[3] - 1))) {
        $truncated = Join-Path $run "truncated-description-$cut.qtp"
        [IO.File]::WriteAllBytes($truncated, [IO.File]::ReadAllBytes($inputPath)[0..($cut - 1)])
        Invoke-TestCppRejection "truncated-description-$cut" $truncated 'legacy' 5
        Invoke-TestJava "java-truncated-description-$cut" $truncated $projection 'legacy' @() 'parser-incomplete|source-incomplete'
    }
    $truncatedTail = Join-Path $run 'truncated-data-tail.qtp'
    [IO.File]::WriteAllBytes($truncatedTail, $quoteBytes[0..($quoteBytes.Length - 3)])
    Invoke-TestCppRejection 'truncated-data-tail' $truncatedTail 'legacy' 5
    Invoke-TestJava 'java-truncated-data-tail' $truncatedTail (Join-Path $run 'baseline-quote/cpp.stdout') 'legacy' @() 'parser-incomplete'
    Invoke-TestCppRejection 'nonzero-flags' (Join-Path $repo 'tests/fixtures/protocol/raw-tape/expected/nonzero-flags-empty-opt.qtp') 'raw-message-v1' 4
    Invoke-TestCppRejection 'output-budget' $inputPath 'legacy' 6 @('--max-output', '1')

    # Repeating a public complete data frame creates a valid large source; source bytes and
    # expected values still originate exclusively from the portable production writer fixture.
    $frameCells = ($rows | Where-Object { $c = $_.Split([char]9); $c.Count -gt 4 -and $c[0] -eq 'frame' -and $c[4] -eq '15' } | Select-Object -First 1).Split([char]9)
    $sourceBytes = [IO.File]::ReadAllBytes($inputPath)
    $sinkInput = Join-Path $run 'closed-sink.qtp'
    $stream = [IO.File]::Create($sinkInput)
    try {
        $stream.Write($sourceBytes, 0, [int]$frameCells[2])
        for ($i = 0; $i -lt 10000; $i++) { $stream.Write($sourceBytes, [int]$frameCells[2], [int]$frameCells[3] - [int]$frameCells[2]) }
    } finally { $stream.Dispose() }
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $inspectorPath; $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    foreach ($arg in @('--input', $sinkInput, '--wire-schema', 'ordinary-v1', '--output-format', 'tsv', '--max-frames', '20000')) { $info.ArgumentList.Add($arg) }
    $process = [Diagnostics.Process]::Start($info)
    try {
        $errorTask = $process.StandardError.ReadToEndAsync()
        $firstTask = $process.StandardOutput.BaseStream.ReadAsync([byte[]]::new(1), 0, 1)
        if (!$firstTask.Wait(10000) -or $firstTask.Result -ne 1 -or $process.HasExited) { throw 'Closed-sink setup did not expose a live writer.' }
        $process.StandardOutput.Dispose()
        if (!$process.WaitForExit(60000)) { $process.Kill($true); $process.WaitForExit(); throw 'Closed-sink child timed out.' }
        $sinkError = $errorTask.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $run 'closed-sink.stderr'), $sinkError, $utf8)
        if ($process.ExitCode -ne 3) { throw 'Closed sink did not report native I/O failure.' }
        $results.Add([ordered]@{ name = 'closed-sink'; expected = 3; exitCode = $process.ExitCode })
    } finally {
        if (!$process.HasExited) { $process.Kill($true); $process.WaitForExit() }
        $process.Dispose()
    }

    $empty = New-Item -ItemType Directory -Path (Join-Path $run 'empty-cwd')
    $processTest = Join-Path $PSScriptRoot 'Test-ProcessOwnership.ps1'
    $null = & $processTest -Runner $runnerPath -JavaHome $JavaHome -RunDirectory (Join-Path $run 'process-ownership')
    $results.Add([ordered]@{ name = 'inherited-pipes-deadline-output-final-pid-absence'
        expected = 'PASS'; metadata = Join-Path $run 'process-ownership/process-tests.json' })
    foreach ($target in @($compare, $PSCommandPath, $processTest)) {
        Push-Location $empty.FullName
        try {
            $noargs = (& $target) -join "`n"
            $helpText = (& $target -Help) -join "`n"
            if ($noargs -cne $helpText -or @(Get-ChildItem -Force).Count -ne 0) { throw 'PowerShell help/no-I/O contract failed.' }
            $rejected = $false
            try { & $target -JavaHome 'not-a-working-input' } catch { $rejected = $true }
            if (!$rejected -or @(Get-ChildItem -Force).Count -ne 0) { throw 'Incomplete PowerShell arguments started work.' }
        } finally { Pop-Location }
    }
    $help0 = Invoke-AcceptanceProcess $java $runtime $run 'java-noargs' 60 268435456 $empty.FullName
    $help1 = Invoke-AcceptanceProcess $java ($runtime + '--help') $run 'java-help' 60 268435456 $empty.FullName
    Assert-AcceptanceProcess $help0; Assert-AcceptanceProcess $help1
    if ((Get-FileHash -LiteralPath $help0.stdout).Hash -cne (Get-FileHash -LiteralPath $help1.stdout).Hash -or
        (Get-Item -LiteralPath $help0.stderr).Length -ne 0 -or (Get-Item -LiteralPath $help1.stderr).Length -ne 0 -or
        @(Get-ChildItem -LiteralPath $empty.FullName -Force).Count -ne 0) { throw 'Java help/no-I/O contract failed.' }
    $results.Add([ordered]@{ name = 'java-and-powershell-help-no-io'; expected = 'PASS'; results = @($help0, $help1) })
    $invalidJava = Invoke-AcceptanceProcess $java ($runtime + @('--input', 'missing-required-options')) $run 'java-incomplete-args' 60 268435456 $empty.FullName
    if ($invalidJava.exitCode -ne 1 -or $invalidJava.timedOut -or $invalidJava.outputExceeded -or
        (Get-Content -LiteralPath $invalidJava.stderr -Raw) -notmatch 'reason\trequired-option' -or
        @(Get-ChildItem -LiteralPath $empty.FullName -Force).Count -ne 0) { throw 'Java incomplete arguments started work or were accepted.' }
    $results.Add([ordered]@{ name = 'java-incomplete-args-no-io'; expected = 1; result = $invalidJava })
    $rawRuntime = @('-Xmx32m', '-cp', $script:AcceptanceWorkerClasses, 'qdcxx.oracle.RawChild')
    $rawHelp0 = Invoke-AcceptanceProcess $java $rawRuntime $run 'raw-worker-noargs' 60 268435456 $empty.FullName
    $rawHelp1 = Invoke-AcceptanceProcess $java ($rawRuntime + '--help') $run 'raw-worker-help' 60 268435456 $empty.FullName
    Assert-AcceptanceProcess $rawHelp0; Assert-AcceptanceProcess $rawHelp1
    if ((Get-FileHash -LiteralPath $rawHelp0.stdout).Hash -cne (Get-FileHash -LiteralPath $rawHelp1.stdout).Hash -or
        (Get-Item -LiteralPath $rawHelp0.stderr).Length -ne 0 -or (Get-Item -LiteralPath $rawHelp1.stderr).Length -ne 0 -or
        @(Get-ChildItem -LiteralPath $empty.FullName -Force).Count -ne 0) { throw 'Raw worker help/no-I/O contract failed.' }
    $results.Add([ordered]@{ name = 'raw-worker-help-no-io'; expected = 'PASS'; results = @($rawHelp0, $rawHelp1) })
    $status = if ($BinaryCatalog) { 'PASS-WITH-BINARY-CATALOG-NAMED-CASES' } else { 'PASS-WITH-NAMED-GATES' }
} finally {
    if ((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'RealTapeAcceptance.java')).Hash.ToLowerInvariant() -cne $sourceHash -or
        (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'RawChild.java')).Hash.ToLowerInvariant() -cne $workerHash -or
        (Get-FileHash -LiteralPath $PSCommandPath).Hash.ToLowerInvariant() -cne $scriptHash) { $status = 'FAIL-SOURCE-CHANGED' }
    [ordered]@{ status = $status; startedUtc = $started; finishedUtc = [DateTime]::UtcNow.ToString('o')
        sourceSha256 = $sourceHash; workerSourceSha256 = $workerHash; scriptSha256 = $scriptHash
        designedV8Requested = [bool]$DesignedV8; actualBinaryCatalogRequested = [bool]$BinaryCatalog
        actualSourceProfileRequested = [bool]$SourceProfile
        results = $results } | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $run 'tests.json') -Encoding utf8NoBOM
}
if ($status -cnotin @('PASS-WITH-NAMED-GATES', 'PASS-WITH-BINARY-CATALOG-NAMED-CASES')) { throw 'Portable acceptance did not complete on unchanged sources.' }
$qualification = if ($BinaryCatalog) { 'actual binary-catalog named cases included' } else { 'STREAM/MESSAGE remains a named message-policy gate' }
"PASS: $($results.Count) portable checks; $qualification. Private evidence: $run"
