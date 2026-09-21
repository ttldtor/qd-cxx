#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Capture','Verify')][string]$Mode,
    [Parameter(Mandatory)][string]$Upstream,
    [Parameter(Mandatory)][string]$JavaHome,
    [Parameter(Mandatory)][string]$Maven,
    [Parameter(Mandatory)][string]$RunDirectory,
    [string]$ExpectedDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
$fixtures = Join-Path $repo 'tests/fixtures/protocol/stream-data'
$Upstream = (Resolve-Path $Upstream).Path
$JavaHome = (Resolve-Path $JavaHome).Path
$Maven = (Resolve-Path $Maven).Path
$RunDirectory = [IO.Path]::GetFullPath($RunDirectory)
if (Test-Path $RunDirectory) { throw 'RunDirectory must be new; captures never overwrite a previous run.' }
if (!$ExpectedDirectory) { $ExpectedDirectory = Join-Path $fixtures 'expected' }
$ExpectedDirectory = [IO.Path]::GetFullPath($ExpectedDirectory)
if ($RunDirectory.StartsWith($ExpectedDirectory.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar,
    [StringComparison]::OrdinalIgnoreCase) -or $RunDirectory -eq $ExpectedDirectory) {
    throw 'RunDirectory must be separate from expectations.'
}
$lock = @{}
Get-Content "$repo/upstream.lock" | ForEach-Object {
    if ($_ -match '^(repository|version|commit) = "([^"]+)"$') { $lock[$matches[1]] = $matches[2] }
}
function GitValue([string[]]$Arguments) {
    $value = & git -C $Upstream @Arguments
    if ($LASTEXITCODE) { throw "git failed: $Arguments" }
    return ($value -join "`n")
}
function Assert-Upstream {
    if ((GitValue @('remote','get-url','origin')) -cne $lock.repository) { throw 'Upstream origin mismatch' }
    if ((GitValue @('rev-parse','HEAD')) -cne $lock.commit) { throw 'Upstream commit mismatch' }
    if ((GitValue @('rev-parse',"refs/tags/qd-qds-$($lock.version)^{commit}")) -cne $lock.commit) { throw 'Upstream tag mismatch' }
    if (GitValue @('status','--porcelain','--untracked-files=all')) { throw 'Upstream source is not clean' }
    [xml]$pom = Get-Content "$Upstream/pom.xml" -Raw
    if ($pom.project.version -cne $lock.version) { throw 'POM version mismatch' }
    if ((Get-Content "$Upstream/ReleaseNotes.txt" -TotalCount 1) -notmatch "^QDS $([regex]::Escape($lock.version))\b") {
        throw 'Release notes version mismatch'
    }
}
Assert-Upstream
# Reject ambient JVM injection; options for this profile are explicit below.
foreach ($key in @('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS')) {
    if ([Environment]::GetEnvironmentVariable($key)) { throw "Unset $key for an isolated oracle run" }
}
New-Item -ItemType Directory -Path $RunDirectory | Out-Null
New-Item -ItemType Directory -Path "$RunDirectory/raw","$RunDirectory/normalized","$RunDirectory/harness","$RunDirectory/build-work" | Out-Null
$commands = [Collections.Generic.List[object]]::new()
function Native([string]$Exe, [string[]]$Arguments, [string]$Stdout, [string]$Stderr) {
    $commands.Add([ordered]@{ executable=$Exe; arguments=$Arguments; cwd=(Get-Location).Path; stdout=$Stdout; stderr=$Stderr })
    & $Exe @Arguments 1> $Stdout 2> $Stderr
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $Exe; see $Stdout and $Stderr" }
}
function WriteUtf8([string]$Path, [string]$Value) {
    [IO.File]::WriteAllText($Path, $Value, [Text.UTF8Encoding]::new($false))
}
function HashFiles([string]$Root, [string]$Filter='*') {
    @(Get-ChildItem $Root -Recurse -File -Filter $Filter | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path=[IO.Path]::GetRelativePath($Root,$_.FullName).Replace('\','/'); sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
}
$oldJava = $env:JAVA_HOME
$oldMaven = $env:MAVEN_OPTS
$oldLocation = Get-Location
try {
    Set-Location $repo
    $env:JAVA_HOME = $JavaHome
    $env:MAVEN_OPTS = "-Duser.timezone=UTC -Duser.home=$($env:USERPROFILE.Replace('\','/'))"
    $java = Join-Path $JavaHome 'bin/java.exe'
    $javac = Join-Path $JavaHome 'bin/javac.exe'
    Native $java @('-version') "$RunDirectory/java-version.stdout" "$RunDirectory/java-version.stderr"
    if ((Get-Content "$RunDirectory/java-version.stderr" -Raw) -notmatch 'version "1\.8\.') {
        throw 'This oracle profile requires JDK 8, matching the upstream enforcer rule.'
    }
    Native $Maven @('-version') "$RunDirectory/maven-version.txt" "$RunDirectory/maven-version.stderr"
    $m2 = "$PSScriptRoot/../schema-oracle/.work/m2"
    # Native upstream reactor; tests and build-cache reuse are deliberately disabled.
    # JAXB schemagen briefly writes schema1.xsd in the process working directory.
    # Keep even that intermediate file inside the generated run directory.
    Push-Location "$RunDirectory/build-work"
    try {
        Native $Maven @('-B','-f',"$Upstream/pom.xml",'-pl','dxfeed-scheme','-am',
            "-Dmaven.repo.local=$m2",'-Dmaven.build.cache.enabled=false','-Dmaven.test.skip=true','clean','compile') `
            "$RunDirectory/build.stdout" "$RunDirectory/build.stderr"
    } finally { Pop-Location }
    Assert-Upstream
    $modules = @('dxlib','mars','dxfeed-promise','dxfeed-api','dxlib-qd-sandbox','auth','proto','qd-core','proto-ssl','qd-rmi','dxfeed-impl','dxfeed-scheme')
    $entries = @($modules | ForEach-Object { (Resolve-Path "$Upstream/$_/target/classes").Path })
    $cp = (@("$RunDirectory/harness") + $entries) -join ';'
    Native $javac @('-encoding','UTF-8','-cp',($entries -join ';'),'-d',"$RunDirectory/harness","$PSScriptRoot/StreamDataOracle.java","$PSScriptRoot/../protocol-oracle/ProtocolOracle.java","$PSScriptRoot/../protocol-oracle/FramingOracle.java","$PSScriptRoot/../schema-oracle/SchemaOracle.java") `
        "$RunDirectory/javac.stdout" "$RunDirectory/javac.stderr"
    $javaArgs = @('-Dfile.encoding=UTF-8','-Duser.language=en','-Duser.country=US','-Duser.timezone=UTC','-cp',$cp,'com.devexperts.qd.qtp.StreamDataOracle')
    Native $java (@('-Dfile.encoding=UTF-8','-Duser.language=en','-Duser.country=US','-Duser.timezone=UTC','-cp',$cp,'qdcxx.oracle.SchemaOracle','environment')) "$RunDirectory/environment.tsv" "$RunDirectory/environment.stderr"
    $cases = @('stream-data')
    Native $java ($javaArgs + @("$fixtures/inputs.tsv")) "$RunDirectory/raw/stream-data.tsv" "$RunDirectory/raw/stream-data.stderr"
    $raw = [IO.File]::ReadAllText("$RunDirectory/raw/stream-data.tsv")
    if (!$raw.Contains("complete`tstream-data")) { throw 'Incomplete observation' }
    WriteUtf8 "$RunDirectory/normalized/stream-data.tsv" ($raw.Replace("`r`n","`n"))
    $classpathHashes = foreach ($entry in $entries) {
        $files = HashFiles $entry
        $text = ($files | ForEach-Object { "$($_.path)`t$($_.sha256)" }) -join "`n"
        $digest = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($text))).ToLowerInvariant()
        [ordered]@{ directory=$entry; fileCount=$files.Count; sha256=$digest }
    }
    $toolHashes = @(Get-ChildItem $PSScriptRoot -File | Sort-Object Name | ForEach-Object {
        [ordered]@{path=$_.Name; sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
    $metadata = [ordered]@{
        format='qdcxx-stream-data-oracle-v1'; mode=$Mode; upstream=$lock; upstreamDirectory=$Upstream
        upstreamClean=$true; powershell=$PSVersionTable.PSVersion.ToString(); cwd=$repo
        javaHome=$JavaHome; maven=$Maven; mavenOpts=$env:MAVEN_OPTS; classpath=$cp
        toolHashes=$toolHashes; harnessClasses=(HashFiles "$RunDirectory/harness")
        protocolToolSha256=(Get-FileHash "$PSScriptRoot/../protocol-oracle/ProtocolOracle.java").Hash.ToLowerInvariant(); framingToolSha256=(Get-FileHash "$PSScriptRoot/../protocol-oracle/FramingOracle.java").Hash.ToLowerInvariant(); environmentToolSha256=(Get-FileHash "$PSScriptRoot/../schema-oracle/SchemaOracle.java").Hash.ToLowerInvariant(); inputsSha256=(Get-FileHash "$fixtures/inputs.tsv").Hash.ToLowerInvariant()
        classpathContent=$classpathHashes; buildDependencyJars=(HashFiles $m2 '*.jar')
        normalization=@('CRLF to LF'); commands=$commands
    }
    WriteUtf8 "$RunDirectory/metadata.json" (($metadata | ConvertTo-Json -Depth 12) + "`n")
    Assert-Upstream
    if ($Mode -eq 'Verify') {
        $actualNames = @(Get-ChildItem "$RunDirectory/normalized" -File | ForEach-Object Name | Sort-Object)
        $expectedNames = @(Get-ChildItem $ExpectedDirectory -File | ForEach-Object Name | Sort-Object)
        if (Compare-Object $actualNames $expectedNames) { throw 'VERIFY FAILED: expected file set differs' }
        $mismatches = @($actualNames | Where-Object {
            (Get-FileHash "$RunDirectory/normalized/$_").Hash -cne (Get-FileHash "$ExpectedDirectory/$_").Hash
        })
        if ($mismatches.Count) {
            WriteUtf8 "$RunDirectory/result.txt" "VERIFY FAILED: $($mismatches -join ', ')`n"
            throw "VERIFY FAILED: $($mismatches -join ', ')"
        }
        WriteUtf8 "$RunDirectory/result.txt" "VERIFY PASS: $($cases.Count) cases; expectations unchanged.`n"
    } else { WriteUtf8 "$RunDirectory/result.txt" "CAPTURE COMPLETE: $($cases.Count) cases; review normalized output before promotion.`n" }
    Get-Content "$RunDirectory/result.txt"
} finally {
    $env:JAVA_HOME=$oldJava
    $env:MAVEN_OPTS=$oldMaven
    Set-Location $oldLocation
}


