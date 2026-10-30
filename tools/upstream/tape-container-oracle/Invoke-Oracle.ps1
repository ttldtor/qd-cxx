<#
.SYNOPSIS
Captures or verifies closed GZIP/ZIP observations using an existing pinned Java production classpath.
.DESCRIPTION
Compiles only ContainerOracle.java into the output directory. Never invokes Maven or modifies
production roots. Captures UTC/raw streams, SHA256 inventories and deterministic outcome rows.
Requires PowerShell7.4, a JDK8, a JSON array of exactly14 production class directories, an explicit
revision, and the repository native qdcxx-run-utc logger. Output directory must not already exist.
Verify compares outcome rows with ExpectedDirectory/results.tsv; archive timestamps may differ.
Logs/metadata include local paths and belong in a private evidence directory, not public fixtures.
.PARAMETER Help
Prints this help without required work inputs or capture side effects.
.PARAMETER Mode
Capture (default) creates fresh evidence. Verify additionally compares canonical outcomes.
.PARAMETER JavaHome
Explicit JDK8 root. No global Java settings or dependency acquisition occurs.
.PARAMETER ClasspathJson
UTF8 JSON array of exactly14 existing production class directories, in runtime order.
.PARAMETER Revision
Exact accepted baseline/current commit. Does not change upstream.lock.
.PARAMETER OutputDirectory
Fresh task-owned directory for classes, files, streams and metadata; never cleaned automatically.
.PARAMETER ExpectedDirectory
Existing evidence directory required only by Verify.
.PARAMETER Logger
Existing native qdcxx-run-utc executable; argv is passed without shell interpolation.
.OUTPUTS
PASS on success. Exit0 for success/help; exit1 for invalid arguments, timeout, child failure,
changed class inventory or outcome mismatch. Failed evidence is preserved.
.EXAMPLE
./Invoke-Oracle.ps1 -Help
#>
[CmdletBinding()]
param(
    [Alias('h')][switch]$Help,
    [string]$Mode = 'Capture',
    [string]$JavaHome,
    [string]$ClasspathJson,
    [string]$Revision,
    [string]$OutputDirectory,
    [string]$ExpectedDirectory,
    [string]$Logger
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Help -or $PSBoundParameters.Count -eq 0) {
    Get-Help -Name $PSCommandPath -Full | Out-String -Width 110 | Write-Output
    exit 0
}
try {
    if ($Mode -notin @('Capture','Verify') -or !$JavaHome -or !$ClasspathJson -or !$Revision -or !$OutputDirectory -or !$Logger) { throw 'Missing/invalid arguments; use -Help' }
    if ($Revision -notin @('0ddc0a09779fb61d74cc0b37c6449ff47dcb715f','f71e4cb390d1586f2d4e3a83d9b9067e3326ef19')) { throw 'Unsupported production pin' }
    if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be fresh' }
    $classpath = @(Get-Content -LiteralPath $ClasspathJson -Raw | ConvertFrom-Json)
    if ($classpath.Count -ne 14) { throw 'Expected exactly fourteen production class roots' }
    $classpath = @($classpath | ForEach-Object { $path = (Resolve-Path -LiteralPath $_).Path; if (!(Test-Path -LiteralPath $path -PathType Container)) { throw 'Missing production class root' }; $path })
    $java = (Resolve-Path -LiteralPath (Join-Path $JavaHome 'bin/java.exe')).Path
    $javac = (Resolve-Path -LiteralPath (Join-Path $JavaHome 'bin/javac.exe')).Path
    $Logger = (Resolve-Path -LiteralPath $Logger).Path
    if ($Mode -eq 'Verify' -and (!$ExpectedDirectory -or !(Test-Path -LiteralPath (Join-Path $ExpectedDirectory 'results.tsv') -PathType Leaf))) { throw 'Verify requires ExpectedDirectory/results.tsv' }
    $output = [IO.Path]::GetFullPath($OutputDirectory)
    $started = [DateTime]::UtcNow.ToString('o')
    New-Item -ItemType Directory -Path $output | Out-Null
    $classes = Join-Path $output 'classes'
    New-Item -ItemType Directory -Path $classes | Out-Null
    $commands = [Collections.Generic.List[object]]::new()
    function Invoke-Owned([string]$name, [string]$executable, [string[]]$arguments) {
        $psi = [Diagnostics.ProcessStartInfo]::new()
        $psi.FileName = $Logger
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        foreach ($argument in @('--log',(Join-Path $output "$name.utc.log"),'--',$executable)+$arguments) { $psi.ArgumentList.Add($argument) }
        $phase = [DateTime]::UtcNow.ToString('o')
        $process = [Diagnostics.Process]::Start($psi)
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $timedOut = !$process.WaitForExit(30000)
        if ($timedOut) { $process.Kill($true); $process.WaitForExit() }
        [IO.File]::WriteAllText((Join-Path $output "$name.stdout"),$stdout.GetAwaiter().GetResult())
        [IO.File]::WriteAllText((Join-Path $output "$name.stderr"),$stderr.GetAwaiter().GetResult())
        $exitCode = $process.ExitCode
        $process.Dispose()
        $commands.Add([ordered]@{name=$name; executable=$executable; arguments=$arguments; startUtc=$phase; finishUtc=[DateTime]::UtcNow.ToString('o'); timeoutSeconds=30; timedOut=$timedOut; exitCode=$exitCode})
        if ($timedOut -or $exitCode -ne 0) { throw "$name failed; streams retained" }
    }
    function Inventory {
        for ($index=0; $index -lt $classpath.Count; ++$index) {
            Get-ChildItem -LiteralPath $classpath[$index] -File -Recurse | Sort-Object FullName | ForEach-Object { [ordered]@{root=$index; path=[IO.Path]::GetRelativePath($classpath[$index],$_.FullName).Replace('\','/'); sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} }
        }
    }
    $before = @(Inventory)
    $before | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $output 'classpath-before.json')
    Invoke-Owned 'java-version' $java @('-version')
    if ((Get-Content (Join-Path $output 'java-version.stdout') -Raw) -notmatch 'version "1\.8\.' -and (Get-Content (Join-Path $output 'java-version.stderr') -Raw) -notmatch 'version "1\.8\.') { throw 'Oracle requires JDK8' }
    Invoke-Owned 'javac' $javac @('-J-Xmx128m','-encoding','UTF-8','-cp',($classpath -join ';'),'-d',$classes,(Join-Path $PSScriptRoot 'ContainerOracle.java'))
    $files = Join-Path $output 'files'
    Invoke-Owned 'capture' $java @('-Xmx128m','-Dfile.encoding=UTF-8','-Duser.timezone=UTC','-cp',((@($classes)+$classpath)-join ';'),'qdcxx.oracle.ContainerOracle',$files)
    Copy-Item -LiteralPath (Join-Path $files 'results.tsv') -Destination (Join-Path $output 'results.tsv')
    if ($Mode -eq 'Verify' -and (Get-FileHash (Join-Path $output 'results.tsv')).Hash -cne (Get-FileHash (Join-Path $ExpectedDirectory 'results.tsv')).Hash) { throw 'Canonical production observations differ' }
    $after = @(Inventory)
    $after | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $output 'classpath-after.json')
    if (($before | ConvertTo-Json -Depth 4 -Compress) -cne ($after | ConvertTo-Json -Depth 4 -Compress)) { throw 'Production classes changed' }
    @(Get-ChildItem -LiteralPath $files -File | Sort-Object Name | ForEach-Object {[ordered]@{path=$_.Name; bytes=$_.Length; sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}}) | ConvertTo-Json | Set-Content (Join-Path $output 'fixtures.json')
    [ordered]@{status='PASS'; mode=$Mode; revision=$Revision; startUtc=$started; finishUtc=[DateTime]::UtcNow.ToString('o'); javaSha256=(Get-FileHash $java).Hash; javacSha256=(Get-FileHash $javac).Hash; loggerSha256=(Get-FileHash $Logger).Hash; sourceSha256=(Get-FileHash (Join-Path $PSScriptRoot 'ContainerOracle.java')).Hash; launcherSha256=(Get-FileHash $PSCommandPath).Hash; classpath=$classpath; commands=$commands} | ConvertTo-Json -Depth 7 | Set-Content (Join-Path $output 'metadata.json')
    Write-Output 'PASS: production container observations; no C++ acceptance claim'
} catch {
    Write-Error $_
    exit 1
}
