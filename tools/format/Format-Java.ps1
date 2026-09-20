#requires -Version 7.4
[CmdletBinding()]
param(
    [ValidateSet('Check', 'Format')]
    [string]$Mode = 'Check',
    [string]$JavaHome = $env:JAVA_HOME
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$lock = Get-Content "$PSScriptRoot/google-java-format.lock.json" -Raw | ConvertFrom-Json

if ($JavaHome) {
    $java = Join-Path $JavaHome 'bin/java.exe'
    if (!(Test-Path -LiteralPath $java -PathType Leaf)) {
        $java = Join-Path $JavaHome 'bin/java'
    }
    $java = (Resolve-Path -LiteralPath $java).Path
} else {
    $java = (Get-Command java -CommandType Application).Source
}

$versionOutput = (& $java -version 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $versionOutput -notmatch 'version "(?<major>\d+)[\.\-"]') {
    throw "Cannot determine formatter Java version: $versionOutput"
}
if ([int]$Matches.major -lt $lock.minimumJavaVersion) {
    throw "google-java-format needs JDK $($lock.minimumJavaVersion)+. Pass -JavaHome separately from the oracle JDK 8."
}

# Git discovery includes new source files, but excludes ignored work/cache/capture copies.
Push-Location $repo
try {
    $files = @(& git ls-files --cached --others --exclude-standard -- '*.java')
    if ($LASTEXITCODE -ne 0) {
        throw 'Cannot enumerate repository Java sources.'
    }
    $files = @($files | Sort-Object -Unique | Where-Object {
        $_ -notmatch '(^|/)(captures|expected|target|\.work)/' -and
        (Test-Path -LiteralPath $_ -PathType Leaf)
    })
    if ($files.Count -eq 0) {
        throw 'No repository-owned Java sources found.'
    }

    $cache = Join-Path $PSScriptRoot '.work'
    $jar = Join-Path $cache "google-java-format-$($lock.version)-all-deps.jar"
    if (!(Test-Path -LiteralPath $jar)) {
        New-Item -ItemType Directory -Force -Path $cache | Out-Null
        $download = "$jar.download"
        Invoke-WebRequest -Uri $lock.url -OutFile $download
        if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant() -cne $lock.sha256) {
            throw 'Downloaded formatter SHA-256 differs from the lock; it will not be executed.'
        }
        Move-Item -LiteralPath $download -Destination $jar
    }
    if ((Get-FileHash -LiteralPath $jar -Algorithm SHA256).Hash.ToLowerInvariant() -cne $lock.sha256) {
        throw 'Cached formatter SHA-256 differs from the lock; it will not be executed.'
    }

    $arguments = @('-jar', $jar)
    if ($Mode -eq 'Check') {
        $arguments += @('--dry-run', '--set-exit-if-changed')
    } else {
        $arguments += '--replace'
    }
    & $java @arguments @files
    if ($LASTEXITCODE -ne 0) {
        throw "Java formatting $Mode failed (exit $LASTEXITCODE). Run -Mode Format for formatting differences."
    }
    "Java formatting ${Mode}: PASS ($($files.Count) files; google-java-format $($lock.version))."
} finally {
    Pop-Location
}
