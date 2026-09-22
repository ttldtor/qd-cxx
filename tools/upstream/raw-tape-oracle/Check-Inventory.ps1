#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$UpstreamRoot,
    [Parameter(Mandatory)][string]$JavaHome,
    [string]$WorkDir = '.work/raw-inventory',
    [string]$PrivateFile
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
$upstream = (Resolve-Path -LiteralPath $UpstreamRoot).Path
$output = [IO.Path]::GetFullPath($WorkDir, $repo)
$ignoredRoot = [IO.Path]::GetFullPath((Join-Path $repo '.work')) + [IO.Path]::DirectorySeparatorChar
if (!$output.StartsWith($ignoredRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Inventory output must stay under the repository .work directory; private output must not be published.'
}
$lock = Get-Content -LiteralPath (Join-Path $repo 'upstream.lock') -Raw
if ($lock -notmatch '(?m)^commit = "([0-9a-f]{40})"') {
    throw 'Cannot read locked upstream commit.'
}
$expectedCommit = $Matches[1]
$actualCommit = (& git -C $upstream rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -cne $expectedCommit) {
    throw 'Upstream HEAD differs from upstream.lock.'
}
$dirty = @(& git -C $upstream status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -ne 0) {
    throw 'Upstream tracked sources must be clean.'
}
$java = Join-Path $JavaHome 'bin/java.exe'
$javac = Join-Path $JavaHome 'bin/javac.exe'
$version = (& $java -version 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $version -notmatch 'version "1\.8\.') {
    throw 'Locked oracle requires JDK 8.'
}
$modules = @('dxlib', 'mars', 'dxfeed-promise', 'dxfeed-api', 'dxlib-qd-sandbox', 'auth', 'proto',
    'qd-core', 'proto-ssl', 'qd-rmi', 'dxfeed-impl', 'dxfeed-scheme', 'dxfeed-ipf-filter', 'qds-file')
$classPaths = @($modules | ForEach-Object {
    $classes = Join-Path $upstream "$_/target/classes"
    if (!(Test-Path -LiteralPath $classes -PathType Container)) {
        throw "Build locked upstream qds-file first: missing $classes"
    }
    $classes
})
New-Item -ItemType Directory -Force -Path $output, (Join-Path $output 'classes') | Out-Null
$classpath = $classPaths -join [IO.Path]::PathSeparator
$commands = [Collections.Generic.List[object]]::new()
function Invoke-Bounded([string]$Executable, [string[]]$Arguments, [string]$Name, [bool]$AllowFailure = $false) {
    $start = [DateTime]::UtcNow.ToString('o')
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $repo
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in $Arguments) {
        $info.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    if (!$process.Start()) { throw "Cannot start $Name" }
    $stdout = [IO.File]::Create((Join-Path $output "$Name.stdout"))
    $stderr = [IO.File]::Create((Join-Path $output "$Name.stderr"))
    try {
        $outTask = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $errTask = $process.StandardError.BaseStream.CopyToAsync($stderr)
        $timedOut = !$process.WaitForExit(30000)
        if ($timedOut) {
            $process.Kill($true)
            $process.WaitForExit()
        }
        $null = $outTask.GetAwaiter().GetResult()
        $null = $errTask.GetAwaiter().GetResult()
        $exitCode = $process.ExitCode
    } finally {
        $stdout.Dispose()
        $stderr.Dispose()
        $process.Dispose()
    }
    $commands.Add([ordered]@{ name = $Name; executable = $Executable; arguments = $Arguments;
        startedUtc = $start; finishedUtc = [DateTime]::UtcNow.ToString('o'); timeoutSeconds = 30;
        timedOut = $timedOut; exitCode = $exitCode })
    if (!$AllowFailure -and ($timedOut -or $exitCode -ne 0)) {
        throw "$Name failed; inspect ignored local logs."
    }
}

$metadata = [ordered]@{ startedUtc = [DateTime]::UtcNow.ToString('o'); upstreamCommit = $actualCommit;
    javaVersion = $version; sourceSha256 = (Get-FileHash "$PSScriptRoot/RawTapeInventory.java").Hash.ToLowerInvariant();
    launcherSha256 = (Get-FileHash $PSCommandPath).Hash.ToLowerInvariant(); commands = $commands }
try {
    Invoke-Bounded $javac @('-encoding', 'UTF-8', '-cp', $classpath, '-d', (Join-Path $output 'classes'),
        "$PSScriptRoot/RawTapeInventory.java") 'javac'
    $runtime = @('-Xmx128m', '-Duser.timezone=UTC', '-Dfile.encoding=UTF-8', '-cp',
        ((Join-Path $output 'classes') + [IO.Path]::PathSeparator + $classpath), 'qdcxx.oracle.RawTapeInventory')
    Invoke-Bounded $java ($runtime + 'selftest') 'selftest'
    if ($PrivateFile) {
        $sample = (Resolve-Path -LiteralPath $PrivateFile).Path
        if ((Get-Item -LiteralPath $sample).Length -gt 8MB) {
            throw 'Private input exceeds the finite 8 MiB inventory/replay file budget.'
        }
        $before = Get-FileHash -LiteralPath $sample -Algorithm SHA256
        $metadata.privateInput = [ordered]@{ path = $sample; bytes = (Get-Item -LiteralPath $sample).Length;
            sha256Before = $before.Hash.ToLowerInvariant() }
        Invoke-Bounded $java ($runtime + @('inventory', $sample)) 'private-inventory'
        Invoke-Bounded $java ($runtime + @('replay', $sample)) 'private-replay' $true
        $after = (Get-FileHash -LiteralPath $sample -Algorithm SHA256).Hash.ToLowerInvariant()
        $metadata.privateInput.sha256After = $after
        if ($after -cne $metadata.privateInput.sha256Before) { throw 'Private input changed during inspection.' }
    }
} finally {
    $metadata.finishedUtc = [DateTime]::UtcNow.ToString('o')
    $metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'metadata.json') -Encoding utf8
}
'Inventory checks complete; results and optional private replay diagnostics stay in ignored local output.'
