#Requires -Version 7.4
<#
.SYNOPSIS
Audit the explicit change inventory, preservation, authored whitespace and raw Git filters without staging.
.DESCRIPTION
Git objects used for clean/smudge verification are written only to a fresh ignored object directory.
The owner's index and object database are never written. Historical evidence is read, never normalized.
TraceOnly checks the complete retained matrix trace contract without writing reports or a manifest.
Full audit output defaults to the review-fix directory; original implementation reports remain evidence.
#>
param(
    [string]$EvidenceRoot = (Join-Path $PSScriptRoot 'evidence'),
    [string]$OutputRoot = (Join-Path $PSScriptRoot 'evidence/review-fixes/current'),
    [switch]$TraceOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Set-Location -LiteralPath $repo
$evidence = [IO.Path]::GetFullPath($EvidenceRoot)

function Get-MatrixCases([string]$Platform) {
    $profiles = if ($Platform.StartsWith('windows')) {
        @('msvc-Debug','msvc-Release','clang-Debug','clang-Release')
    } else { @('gcc-Debug','gcc-Release','gcc-Asan') }
    foreach ($profile in $profiles) { @{name=$profile;testing=$true;qtp=$true;tape=$true} }
    foreach ($testing in 0,1) {
        foreach ($qtp in 0,1) {
            foreach ($tape in 0,1) { @{name="matrix-$testing-$qtp-$tape";testing=$testing;qtp=$qtp;tape=$tape} }
        }
    }
}

function Get-NormalImage([string]$Image, [bool]$Windows) {
    if ($Windows) {
        if ($Image.StartsWith('\\?\')) { $Image = $Image.Substring(4) }
        return $Image.Replace('/','\').ToLowerInvariant()
    }
    return $Image
}

function Test-TraceInventory([string]$Platform, [switch]$NamesOnly) {
    $root = Join-Path $evidence $Platform
    $windows = $Platform.StartsWith('windows')
    $environment = Get-Content -LiteralPath (Join-Path $root 'environment.json') -Raw | ConvertFrom-Json
    if ($environment.platform -cne $(if ($windows) { 'windows' } else { 'linux' })) {
        throw "Trace environment platform mismatch: $Platform"
    }
    # This audit checks the retained final-b matrix. Linux /proc records resolved images,
    # whereas its environment records the /usr/local/bin and python3 symlink commands.
    $cmake = if ($windows) { $environment.cmake } else { '/usr/bin/cmake' }
    $ctest = if ($windows) { $environment.ctest } else { '/usr/bin/ctest' }
    $python = if ($windows) { $environment.pythonControl } else { '/usr/bin/python3.12' }
    $expected = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    foreach ($case in Get-MatrixCases $Platform) {
        foreach ($phase in @('configure','build')) {
            $expected.Add("$($case.name)-$phase.exec.tsv",@{root=$cmake;denied=$null})
        }
        if ($case.testing) {
            $expected.Add("$($case.name).inventory.exec.tsv",@{root=$ctest;denied=$null})
            $expected.Add("$($case.name)-test.exec.tsv",@{root=$ctest;denied=$null})
        }
    }
    foreach ($name in @('python','python3','py','absolute','descendant')) {
        $denied = if ($name -in @('absolute','descendant')) { $python } elseif ($windows) {
            "$($environment.buildRoot)\deny-aliases\$name.exe"
        } else { "$($environment.buildRoot)/deny-aliases/$name" }
        $expected.Add("control-$name.exec.tsv",@{
            root=$(if ($name -eq 'descendant') { $environment.pwsh } else { $null });denied=$denied
        })
    }
    $extras = if ($windows) { @('logger','setup','format-check','guard-orphan') } else { @('logger') }
    foreach ($extra in $extras) { $expected.Add("$extra.exec.tsv",@{root=$environment.pwsh;denied=$null}) }
    $actual = @(Get-ChildItem -LiteralPath $root -Filter '*.exec.tsv' -File)
    foreach ($name in $expected.Keys) {
        if ($name -cnotin $actual.Name) { throw "Missing required trace: $Platform/$name" }
    }
    foreach ($trace in $actual) {
        if (!$expected.ContainsKey($trace.Name)) { throw "Unexpected trace: $Platform/$($trace.Name)" }
    }
    if ($NamesOnly) { return }
    foreach ($trace in $actual) {
        $contract = $expected[$trace.Name]
        try { $text = [Text.UTF8Encoding]::new($false,$true).GetString([IO.File]::ReadAllBytes($trace.FullName)) }
        catch [Text.DecoderFallbackException] { throw "Invalid UTF-8 trace: $Platform/$($trace.Name)" }
        if (!$text.Length) { throw "Empty trace: $Platform/$($trace.Name)" }
        if (!$text.EndsWith("`n")) { throw "Unterminated trace: $Platform/$($trace.Name)" }
        $lines = $text.Substring(0,$text.Length - 1).Split("`n")
        $denials = 0
        $executions = 0
        $rootImages = 0
        foreach ($line in $lines) {
            if ($line -cnotmatch '^(EXEC|DENIED)\t([^\x00-\x1f\x7f]+)\r?$') {
                throw "Malformed trace row: $Platform/$($trace.Name)"
            }
            $kind = $Matches[1]
            $image = Get-NormalImage $Matches[2] $windows
            $absolute = if ($windows) { $image -match '^(?:[a-z]:\\|\\\\[^\\]+\\[^\\]+\\)' } else { $image.StartsWith('/') }
            if (!$absolute) { throw "Relative trace image: $Platform/$($trace.Name)" }
            $basename = ($image -split '[/\\]')[-1]
            if (!$basename -or ($image -split '[/\\]') -contains '.' -or
                ($image -split '[/\\]') -contains '..') {
                throw "Unresolved trace image: $Platform/$($trace.Name)"
            }
            $forbidden = $basename -match '^(python([0-9].*)?|pythonw([0-9].*)?|py)(\.exe)?$'
            if ($kind -eq 'DENIED') {
                ++$denials
                if (!$contract.denied -or !$forbidden -or
                    $image -cne (Get-NormalImage $contract.denied $windows)) {
                    throw "Unexpected denial image: $Platform/$($trace.Name)"
                }
            } else {
                ++$executions
                if ($forbidden) { throw "Executed Python image: $Platform/$($trace.Name)" }
                if ($contract.root -and $image -ceq (Get-NormalImage $contract.root $windows)) { ++$rootImages }
            }
        }
        $requiredDenials = if ($contract.denied) { 1 } else { 0 }
        if ($denials -ne $requiredDenials) { throw "Wrong denial count: $Platform/$($trace.Name)" }
        if ($contract.root) {
            $first = $lines[0].TrimEnd("`r").Split("`t")
            if (!$executions -or $first[0] -cne 'EXEC' -or
                (Get-NormalImage $first[1] $windows) -cne (Get-NormalImage $contract.root $windows)) {
                throw "Wrong trace root: $Platform/$($trace.Name)"
            }
            if ($trace.Name -eq 'guard-orphan.exec.tsv' -and $rootImages -lt 2) {
                throw "Missing orphan descendant: $Platform/$($trace.Name)"
            }
        } elseif ($executions -or $lines.Count -ne 1) {
            throw "Direct denial control executed an image: $Platform/$($trace.Name)"
        }
    }
    @{platform=$Platform;traces=$expected.Count;status='passed'}
}

# Reject incomplete inventories before parsing any platform's potentially large process traces.
foreach ($platform in @('windows-final-b','linux-final-b')) { Test-TraceInventory $platform -NamesOnly }
$traceSummary = @('windows-final-b','linux-final-b' | ForEach-Object { Test-TraceInventory $_ })
if ($TraceOnly) {
    $traceSummary | ForEach-Object { Write-Output "PASS trace inventory: $($_.platform), $($_.traces) required traces" }
    return
}
[IO.Directory]::CreateDirectory($OutputRoot) | Out-Null
$baseline = Get-Content (Join-Path $evidence 'baseline-hashes.json') -Raw | ConvertFrom-Json
$metadata = Get-Content (Join-Path $evidence 'baseline.json') -Raw | ConvertFrom-Json
$priorEvidence = @(Get-Content -LiteralPath (Join-Path $evidence 'review-fixes/prior-evidence-hashes.json') -Raw |
    ConvertFrom-Json)
foreach ($entry in $priorEvidence) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo $entry.path)).Hash -ine $entry.sha256) {
        throw "Prior implementation evidence changed: $($entry.path)"
    }
}
$allowed = @('.gitattributes','AGENTS.md','CMakeLists.txt','README.md','docs/CPP-BUILD.md','docs/CPP-BUILD.ru.md',
    'tools/format/CPP.md','tools/format/CPP.ru.md','docs/PYTHON-MIGRATION.md','docs/PYTHON-MIGRATION.ru.md',
    'docs/CURRENT-STATE.md','docs/CURRENT-STATE.ru.md','tasks/README.md','tasks/README.ru.md')
$indexHash = (Get-FileHash -LiteralPath (Join-Path $repo '.git/index')).Hash
if ($indexHash -ine $metadata.indexSha256) { throw 'Owner index bytes changed' }
if ((git rev-parse HEAD) -ne $metadata.head) { throw 'HEAD changed' }
$preserved = 0
foreach ($entry in $baseline) {
    if ($entry.path -in $allowed) { continue }
    if ((Get-FileHash -LiteralPath (Join-Path $repo $entry.path)).Hash -ine $entry.sha256) {
        throw "Protected file changed: $($entry.path)"
    }
    ++$preserved
}
$scenarioMap = Import-Csv -LiteralPath (Join-Path $evidence 'scenario-map.tsv') -Delimiter "`t"
$linuxGuard = Get-Content -LiteralPath (Join-Path $evidence 'linux-guard-rebuild.json') -Raw | ConvertFrom-Json
if ($linuxGuard.executedGuardSha256 -ne $linuxGuard.rebuiltGuardSha256 -or
    $linuxGuard.sourceSha256 -ne (Get-FileHash -LiteralPath 'tests/conformance/no_python_guard.cpp').Hash) {
    throw 'Final Linux guard source/binary equivalence evidence is stale'
}
$matrix = [Collections.Generic.List[object]]::new()
foreach ($platform in @('windows-final-b','linux-final-b')) {
    $root = Join-Path $evidence $platform
    if (!(Test-Path -LiteralPath (Join-Path $root 'result.txt'))) { throw "Incomplete matrix: $platform" }
    $cases = @(Get-MatrixCases $platform)
    foreach ($case in $cases) {
        foreach ($phase in @('configure','build')) {
            $log = Get-Content -LiteralPath (Join-Path $root "$($case.name)-$phase.log") -Raw
            if ($log -notmatch 'EXIT 0\s*$') { throw "Failed $platform/$($case.name)/$phase" }
        }
        $count = 0
        if ($case.testing) {
            $count = 17 + 8 * [int]$case.qtp + 11 * [int]$case.tape
            $tests = (Get-Content -LiteralPath (Join-Path $root "$($case.name).tests.json") -Raw | ConvertFrom-Json).tests
            $log = Get-Content -LiteralPath (Join-Path $root "$($case.name)-test.log") -Raw
            if ($tests.Count -ne $count -or $log -notmatch "100% tests passed out of $count" -or
                $log -notmatch 'EXIT 0\s*$') { throw "Incomplete tests: $platform/$($case.name)" }
            if ($log -match 'runtime error:|ERROR: AddressSanitizer|SUMMARY:.*Sanitizer') {
                throw "Sanitizer diagnostic in $platform/$($case.name)"
            }
            if ($case.qtp) {
                $observed = @{}
                foreach ($match in [regex]::Matches($log,'-- case (tsv|human)/([^:]+): chunks=([0-9;]+); exit=([0-9]+)')) {
                    # Mutation seeds run at 4096 only and can interleave with complete suites.
                    # Every frozen full schedule has multiple sizes; a seed cannot satisfy it.
                    if ($match.Groups[3].Value -eq '4096') { continue }
                    $suite = if ($match.Groups[1].Value -eq 'tsv') { 'offline' } else { 'human' }
                    $observed["$suite/$($match.Groups[2].Value)"] = @($match.Groups[3].Value.Replace(';',','),$match.Groups[4].Value)
                }
                if ($observed.Count -ne $scenarioMap.Count) { throw 'Named scenario count differs from frozen map' }
                foreach ($scenario in $scenarioMap) {
                    $actual = $observed["$($scenario.suite)/$($scenario.case)"]
                    if (!$actual -or $actual[0] -ne $scenario.chunks -or $actual[1] -ne $scenario.exit) {
                        throw "Named schedule/exit changed: $platform/$($case.name)/$($scenario.case)"
                    }
                }
                $legacy = [regex]::Matches($log,'-- legacy TSV/([^:]+): chunks=1,7,4096; default\+explicit; exit=([0-9]+)')
                if ($legacy.Count -ne 72) { throw 'Legacy default/explicit TSV coverage changed' }
                $legacyExits = @{}
                foreach ($match in $legacy) {
                    if ($legacyExits.ContainsKey($match.Groups[1].Value)) { throw 'Duplicate legacy TSV case' }
                    $legacyExits[$match.Groups[1].Value] = $match.Groups[2].Value
                }
                foreach ($scenario in $scenarioMap | Where-Object suite -eq 'offline') {
                    if ($legacyExits[$scenario.case] -ne $scenario.exit) { throw "Legacy TSV case changed: $($scenario.case)" }
                }
            }
        }
        $matrix.Add(@{platform=$platform;profile=$case.name;tests=$count;status='passed';namedMapVerified=[bool]($case.testing -and $case.qtp)})
    }
}
$matrix | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8NoBOM (Join-Path $OutputRoot 'matrix-summary.json')
$sourcePaths = @(@(git diff --name-only) + @(git ls-files --others --exclude-standard)) |
    Where-Object { $_ -eq 'CMakeLists.txt' -or $_ -match '\.(cpp|cmake|ps1)$' } | Sort-Object -Unique
@($sourcePaths | ForEach-Object { @{path=$_;sha256=(Get-FileHash -LiteralPath $_).Hash} }) |
    ConvertTo-Json -Depth 3 | Set-Content -Encoding utf8NoBOM (Join-Path $OutputRoot 'final-source-hashes.json')
$changed = @(@(git -c core.quotepath=false diff --name-only) + @(git -c core.quotepath=false ls-files --others --exclude-standard)) |
    Sort-Object -Unique
if ($LASTEXITCODE -ne 0) { throw 'Git inventory failed' }
$reportPath = [IO.Path]::GetRelativePath($repo,(Join-Path $OutputRoot 'handoff-audit.json')).Replace('\','/')
$manifestPath = 'tests/conformance/python-migration/changed-files.txt'
$changed = @($changed + $reportPath + $manifestPath) | Sort-Object -Unique
[IO.File]::WriteAllLines((Join-Path $repo $manifestPath),$changed,[Text.UTF8Encoding]::new($false))
foreach ($path in $changed) {
    if ($path.StartsWith('tests/conformance/python-migration/evidence/')) { continue }
    $full = Join-Path $repo $path
    if ($path -match '\.(md|cpp|hpp|ps1|cmake|txt)$' -or $path -eq '.gitattributes') {
        $content = [IO.File]::ReadAllText($full)
        if ($content -match '(?m)[ \t]+\r?$') { throw "Authored trailing whitespace: $path" }
    }
    if ($path.EndsWith('.md')) {
        foreach ($match in [regex]::Matches($content,'\]\(([^)]+)\)')) {
            $target = $match.Groups[1].Value.Trim('<','>')
            if ($target -match '^(https?://|mailto:|#)') { continue }
            $target = [Uri]::UnescapeDataString(($target -split '#',2)[0])
            if (!$target) { continue }
            $resolved = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetDirectoryName($full)) $target))
            if (!(Test-Path -LiteralPath $resolved)) { throw "Broken link in ${path}: $target" }
        }
    }
}
git diff --check
if ($LASTEXITCODE -ne 0) { throw 'Changed tracked whitespace failed' }
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw 'Staged whitespace failed' }
if (@(git diff --cached --name-only).Count) { throw 'Unexpected staged files' }

$work = Join-Path $repo ('.work/python-migration-handoff-' + [Guid]::NewGuid().ToString('N'))
$objects = Join-Path $work 'objects'
[IO.Directory]::CreateDirectory($objects) | Out-Null
$git = (Get-Command git -CommandType Application | Select-Object -First 1).Source

function Invoke-GitFilter([string[]]$Arguments, [string]$OutputPath) {
    $start = [Diagnostics.ProcessStartInfo]::new($git)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.WorkingDirectory = $repo
    $start.Environment['GIT_OBJECT_DIRECTORY'] = $objects
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $error = $process.StandardError.ReadToEndAsync()
        $file = [IO.File]::Create($OutputPath)
        try { $copy = $process.StandardOutput.BaseStream.CopyToAsync($file); [void]$copy.GetAwaiter().GetResult() }
        finally { $file.Dispose() }
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw $error.GetAwaiter().GetResult() }
    } finally { $process.Dispose() }
}

$rawCount = 0
$empty = Join-Path $work 'empty.md'
[IO.File]::WriteAllText($empty,'')
foreach ($path in $changed | Where-Object { $_.StartsWith('tests/conformance/python-migration/evidence/') }) {
    # The first audit creates its own report only after checking the other evidence.
    if ($path -eq $reportPath -and !(Test-Path -LiteralPath $path)) { continue }
    $hashFile = Join-Path $work 'object-id.txt'
    Invoke-GitFilter @('-c','core.autocrlf=true','hash-object','-w',"--path=$path",$path) $hashFile
    $objectId = [IO.File]::ReadAllText($hashFile).Trim()
    $roundtrip = Join-Path $work 'checkout.bin'
    Invoke-GitFilter @('-c','core.autocrlf=true','cat-file','--filters',"--path=$path",$objectId) $roundtrip
    if ((Get-FileHash -LiteralPath $path).Hash -ne (Get-FileHash -LiteralPath $roundtrip).Hash) {
        throw "Raw Git filter roundtrip changed bytes: $path"
    }
    $diagnostic = (& $git diff --no-index --check -- $empty $path 2>&1 | Out-String)
    # --no-index returns 1 for a clean differing file, 3 for differences plus whitespace errors.
    if ($LASTEXITCODE -notin @(0,1)) { throw "Raw Git whitespace attributes failed: $path`n$diagnostic" }
    ++$rawCount
}
$bad = Join-Path $work 'authored-whitespace.md'
[IO.File]::WriteAllText($bad,"negative control  `n")
$diagnostic = (& $git -c core.whitespace=blank-at-eol diff --no-index --check -- $empty $bad 2>&1 | Out-String)
if ($LASTEXITCODE -eq 0 -or $diagnostic -notmatch 'trailing whitespace') { throw 'Authored whitespace negative accepted' }
if ((Get-FileHash -LiteralPath (Join-Path $repo '.git/index')).Hash -ne $indexHash) { throw 'Audit modified owner index' }

@{utc=[DateTime]::UtcNow.ToString('o');head=$metadata.head;indexSha256=$indexHash;
    protectedFilesUnchanged=$preserved;rawFilterRoundtrips=$rawCount;authoredWhitespaceNegative='rejected';
    relativeLinks='passed';staging='not performed; isolated object clean/smudge filters exercised instead';
    priorEvidenceFilesUnchanged=$priorEvidence.Count;traceInventories=$traceSummary;files=$changed.Count} |
    ConvertTo-Json -Depth 4 |
    Set-Content -Encoding utf8NoBOM (Join-Path $repo $reportPath)
Write-Output "PASS preservation: $preserved protected files, $rawCount raw Git filter roundtrips; index unchanged; $($changed.Count) intended files"
