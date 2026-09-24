#Requires -Version 7.4
<#
.SYNOPSIS
Parity-only replay of unchanged historical Python checking against replacement mutations.
.DESCRIPTION
This is not part of the migrated development path or CTest. It intentionally requires a
historical Python interpreter; all Python source files remain unchanged and bytecode is disabled.
#>
param(
    [Parameter(Mandatory)][string]$Python,
    [Parameter(Mandatory)][string]$Inspector,
    [Parameter(Mandatory)][string]$Mutations,
    [Parameter(Mandatory)][string]$RunDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$root = Join-Path $repo 'tests/fixtures/protocol/offline-inspection'
$Inspector = (Resolve-Path -LiteralPath $Inspector).Path
$Mutations = (Resolve-Path -LiteralPath $Mutations).Path
$RunDirectory = [IO.Path]::GetFullPath($RunDirectory)
[IO.Directory]::CreateDirectory($RunDirectory) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$original = [IO.File]::ReadAllText((Join-Path $root 'designed.json'))
$exactLabels = @('value','symbol','local-id','name','complete-count')
$proof = [Collections.Generic.List[string]]::new()
$proof.Add("suite`tlabel`toldStage`tnewStage")

function Invoke-Legacy([string]$Label, [string]$FixtureRoot, [string]$Manifest, [string]$Stage) {
    $start = [Diagnostics.ProcessStartInfo]::new($Python)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.Environment['PYTHONDONTWRITEBYTECODE'] = '1'
    foreach ($argument in @('-B',(Join-Path $repo 'tests/conformance/check_offline_inspection.py'),
            $Inspector,$FixtureRoot,'--manifest',$Manifest,'--quick')) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $out = $process.StandardOutput.ReadToEndAsync()
        $err = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(30000)) { $process.Kill($true); throw "Parity timeout: $Label" }
        $stdout = $out.GetAwaiter().GetResult()
        $stderr = $err.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $RunDirectory "$Label.stdout"),$stdout,$utf8)
        [IO.File]::WriteAllText((Join-Path $RunDirectory "$Label.stderr"),$stderr,$utf8)
        if ($process.ExitCode -ne 1 -or $stderr -notmatch 'AssertionError') { throw "Wrong legacy failure: $Label" }
        $actualStage = if ($stderr.Contains('assert (p.returncode,p.stdout,p.stderr)==')) { 'exact-comparison' }
            elseif ($stderr.Contains('assert validate(p.stdout,True)==records')) { 'java-record-comparison' }
            elseif ($stderr.Contains("document['format']=='qdcxx-offline-designed-v1' and document['cases']")) { 'manifest-selection' }
            elseif ($stderr -match 'in (validate|unsigned|units|values|java_records)') { 'strict-validation' }
            else { throw "Unidentified legacy rejection: $Label`n$stderr" }
        if ($actualStage -ne $Stage) { throw "Stage mismatch: $Label / $actualStage / $Stage" }
        $proof.Add("offline`t$Label`t$actualStage`t$Stage")
        Write-Output "PASS old/new $Label at $Stage"
    } finally { $process.Dispose() }
}

foreach ($file in Get-ChildItem -LiteralPath $Mutations -Filter 'offline-*.txt') {
    $label = $file.BaseName.Substring('offline-'.Length)
    if ($label.StartsWith('java-')) { continue }
    $document = $original | ConvertFrom-Json
    $case = $document.cases | Where-Object name -eq 'two-data'
    $case.stdout = [IO.File]::ReadAllText($file.FullName)
    $document.cases = @($case)
    $manifest = Join-Path $RunDirectory "$label.json"
    [IO.File]::WriteAllText($manifest,($document | ConvertTo-Json -Depth 20),$utf8)
    $hash = (Get-FileHash -LiteralPath $manifest).Hash
    Invoke-Legacy $label $root $manifest $(if ($label -in $exactLabels) { 'exact-comparison' } else { 'strict-validation' })
    if ((Get-FileHash -LiteralPath $manifest).Hash -ne $hash) { throw 'Legacy rewrote temporary manifest' }
}
$empty = Join-Path $RunDirectory 'empty-selection.json'
[IO.File]::WriteAllText($empty,'{"format":"qdcxx-offline-designed-v1","cases":[]}',$utf8)
Invoke-Legacy 'empty-selection' $root $empty 'manifest-selection'
$clone = Join-Path $RunDirectory 'fixtures'
Copy-Item -LiteralPath $root -Destination $clone -Recurse
foreach ($label in @('java-value','java-unknown','java-completion')) {
    $source = Join-Path $Mutations "offline-$label.txt"
    $destination = Join-Path $clone 'expected/offline-inspection.tsv'
    [IO.File]::Copy($source,$destination,$true)
    $hash = (Get-FileHash -LiteralPath $destination).Hash
    Invoke-Legacy $label $clone (Join-Path $clone 'designed.json') $(if ($label -eq 'java-value') { 'java-record-comparison' } else { 'strict-validation' })
    if ((Get-FileHash -LiteralPath $destination).Hash -ne $hash) { throw 'Legacy rewrote temporary observation' }
}
# The old human script embeds its nine mutations rather than accepting a manifest override.
# Check that the replacement's disposable bytes are precisely those original expressions.
$human = Get-Content (Join-Path $repo 'tests/fixtures/protocol/human-inspection/designed.json') -Raw | ConvertFrom-Json
$text = ($human.cases | Where-Object name -eq 'lone-high').stdout
$last = $text.Substring(0,$text.Length-1).Split("`n")[-1]
$expected = @{
    'label'=$text.Replace('S.v=','v='); 'value'=$text.Replace('S.v=1','S.v=2');
    'escape'=$text.Replace('\uD800','\uD801'); 'escape-lookalike'=$text.Replace('\uD800','\\uD800');
    'completion-count'=$text.Replace('records=1','records=2');
    'missing-completion'=$text.Substring(0,$text.LastIndexOf('Complete:'));
    'double-completion'=$text+$last+"`n"; 'trailing-output'=$text+"Record 999: injected`n";
    'frame-offset'=$text.Replace('bytes=[0,9)','bytes=[1,9)')
}
foreach ($label in $expected.Keys | Sort-Object) {
    $actual = [IO.File]::ReadAllBytes((Join-Path $Mutations "human-$label.txt"))
    if ([Convert]::ToHexString($actual) -cne [Convert]::ToHexString($utf8.GetBytes($expected[$label]))) {
        throw "Different human mutation bytes: $label"
    }
    $proof.Add("human`t$label`texact-comparison (embedded legacy baseline)`texact-comparison")
    Write-Output "PASS identical human mutation bytes: $label"
}
if ($proof.Count -ne 27) { throw 'Missing named mutation in parity map' }
[IO.File]::WriteAllLines((Join-Path $RunDirectory 'parity.tsv'),$proof,$utf8)
Write-Output 'PASS 17 offline stages and 9 byte-identical human mutations; historical human baseline is separate evidence'
