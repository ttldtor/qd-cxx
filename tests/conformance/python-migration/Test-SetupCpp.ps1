#Requires -Version 7.4
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$IsWindows) { throw 'Formatter acquisition checks require Windows.' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
. (Join-Path $repo 'tools/format/Setup-Cpp.Private.ps1')
$work = Join-Path $repo ('.work/python-migration-u3/' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$script:checks = 0

function Assert-SetupCheck([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
}

function New-TestArchive([string]$Path, [object[]]$Entries) {
    $zip = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($item in $Entries) {
            $entry = $zip.CreateEntry($item.Name)
            if ($item.ContainsKey('Attributes')) { $entry.ExternalAttributes = $item.Attributes }
            $stream = $entry.Open()
            try {
                $bytes = [Text.Encoding]::UTF8.GetBytes('synthetic executable data')
                $stream.Write($bytes)
            } finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
}

function Test-Rejection {
    param([string]$Name, [object[]]$Entries = @(@{Name = 'bin/tool.exe'}),
        [string]$Kind = 'archive', [string]$Expected, [switch]$Cold)
    $case = Join-Path $work $Name
    $cache = Join-Path $case 'cache'
    [IO.Directory]::CreateDirectory((Join-Path $cache 'test-1/bin')) | Out-Null
    $installed = Join-Path $cache 'test-1/bin/tool.exe'
    [IO.File]::WriteAllText($installed, 'previous valid installation')
    $before = (Get-FileHash -LiteralPath $installed).Hash
    if ($Cold) {
        # This is the exact newly created synthetic file; keep its parent directories for the case.
        Remove-Item -LiteralPath $installed
    }
    $source = Join-Path $case 'source.zip'
    New-TestArchive $source $Entries
    $tool = [pscustomobject]@{
        name = 'test'; version = '1'; url = 'https://invalid.example/test.zip'
        sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        executable = 'test-1/bin/tool.exe'
    }
    $downloader = { param($Url, $Output); Copy-Item -LiteralPath $source -Destination $Output }.GetNewClosure()
    switch ($Kind) {
        'wrong-hash' { $tool.sha256 = '0' * 64 }
        'tampered-cache' { [IO.File]::WriteAllText((Join-Path $cache 'test.zip'), 'tampered') }
        'interrupted' {
            $downloader = { param($Url, $Output); [IO.File]::WriteAllText($Output, 'partial'); throw 'Interrupted download' }
        }
        'truncated-zip' {
            [IO.File]::WriteAllBytes($source, [byte[]]@(0x50, 0x4b, 0x03, 0x04))
            $tool.sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        }
    }
    $failure = $null
    try { Install-LockedCppTool -Tool $tool -Cache $cache -Download $downloader | Out-Null } catch { $failure = $_ }
    Assert-SetupCheck ($null -ne $failure) "$Name unexpectedly accepted"
    Assert-SetupCheck ($failure.ToString() -match $Expected) "$Name rejected at wrong stage: $failure"
    if ($Cold) {
        Assert-SetupCheck (!(Test-Path -LiteralPath $installed)) "$Name published an invalid installation"
    } else {
        Assert-SetupCheck ((Get-FileHash -LiteralPath $installed).Hash -eq $before) "$Name changed previous installation"
    }
    $expectedFiles = if ($Cold) { 0 } else { 1 }
    Assert-SetupCheck (@(Get-ChildItem -LiteralPath (Join-Path $cache 'test-1') -File -Recurse).Count -eq $expectedFiles) "$Name published partial files"
    Assert-SetupCheck (@(Get-ChildItem -LiteralPath $cache -Force -Filter '.setup-*').Count -eq 0) "$Name left partial staging"
    if ($Kind -in @('wrong-hash', 'interrupted')) {
        Assert-SetupCheck (!(Test-Path -LiteralPath (Join-Path $cache 'test.zip'))) "$Name published partial download"
    }
    Assert-SetupCheck (!(Test-Path -LiteralPath (Join-Path $case 'escaped.txt'))) "$Name wrote outside staging"
    $script:checks++
    Write-Output "PASS $Name (no partial publication; cold=$Cold)"
}

Test-Rejection 'wrong-sha256' -Kind wrong-hash -Expected 'Integrity mismatch'
Test-Rejection 'tampered-cache' -Kind tampered-cache -Expected 'Integrity mismatch'
Test-Rejection 'interrupted-download' -Kind interrupted -Expected 'Interrupted download'
Test-Rejection 'truncated-zip' -Kind truncated-zip -Expected 'Central Directory|archive|ZIP'
Test-Rejection 'missing-executable' -Entries @(@{Name = 'other.txt'}) -Expected 'Missing executable'
Test-Rejection 'cold-missing-executable' -Entries @(@{Name = 'other.txt'}) -Expected 'Missing executable' -Cold
foreach ($entry in @(
    @{ Label = 'parent-slash'; Name = '../../escaped.txt' },
    @{ Label = 'parent-backslash'; Name = '..\..\escaped.txt' },
    @{ Label = 'parent-mixed'; Name = '../..\escaped.txt' },
    @{ Label = 'absolute-slash'; Name = '/escaped.txt' },
    @{ Label = 'absolute-backslash'; Name = '\escaped.txt' },
    @{ Label = 'absolute-drive'; Name = 'C:\escaped.txt' },
    @{ Label = 'absolute-unc'; Name = '\\localhost\share\escaped.txt' },
    @{ Label = 'alternate-stream'; Name = 'bin/tool.exe:escaped.txt' },
    @{ Label = 'device-name'; Name = 'bin/CON.txt' },
    @{ Label = 'trailing-dot'; Name = 'bin/.. /escaped.txt' }
)) {
    Test-Rejection $entry.Label -Entries @(@{Name = 'bin/tool.exe'}, @{Name = $entry.Name}) -Expected 'Unsafe archive entry'
}
Test-Rejection 'unix-symlink' -Entries @(@{Name = 'bin/link'; Attributes = -1577123840}) -Expected 'link or special'
Test-Rejection 'windows-reparse' -Entries @(@{Name = 'bin/link'; Attributes = 0x400}) -Expected 'link or special'
Test-Rejection 'case-duplicate' -Entries @(@{Name = 'bin/tool.exe'}, @{Name = 'BIN/TOOL.EXE'}) -Expected 'Duplicate archive'
Test-Rejection 'extraction-conflict' -Entries @(@{Name = 'bin'}, @{Name = 'bin/tool.exe'}) -Expected 'already exists|file exists'

# A junction must be refused before a download or write can reach its target.
$outside = Join-Path $work 'junction-target'
[IO.Directory]::CreateDirectory($outside) | Out-Null
$junction = Join-Path $work 'junction-cache'
New-Item -ItemType Junction -Path $junction -Target $outside | Out-Null
try {
    $junctionTool = [pscustomobject]@{
        name = 'test'; version = '1'; url = 'https://invalid.example/test.zip'
        sha256 = '0' * 64; executable = 'test-1/bin/tool.exe'
    }
    $failed = $false
    try {
        Install-LockedCppTool -Tool $junctionTool -Cache (Join-Path $junction 'nested') -Download {
            throw 'Junction check attempted a download'
        } | Out-Null
    } catch {
        $failed = $_.ToString() -match 'Reparse point'
    }
    Assert-SetupCheck $failed 'Filesystem junction accepted'
    Assert-SetupCheck (@(Get-ChildItem -LiteralPath $outside -Force).Count -eq 0) 'Junction target modified'
    $script:checks++
    Write-Output 'PASS filesystem-junction'
} finally {
    # Delete only this junction, never recurse into its target.
    [IO.Directory]::Delete($junction)
}

$linkedCache = Join-Path $work 'linked-install-cache'
[IO.Directory]::CreateDirectory((Join-Path $linkedCache 'test-1/bin')) | Out-Null
$linkedExe = Join-Path $linkedCache 'test-1/bin/tool.exe'
[IO.File]::WriteAllText($linkedExe, 'previous valid installation')
$installedJunction = Join-Path $linkedCache 'test-1/extra'
New-Item -ItemType Junction -Path $installedJunction -Target $outside | Out-Null
try {
    $failed = $false
    try {
        Install-LockedCppTool -Tool $junctionTool -Cache $linkedCache -Download {
            throw 'Existing installation link check attempted a download'
        } | Out-Null
    } catch { $failed = $_.ToString() -match 'Reparse point in existing installation' }
    Assert-SetupCheck $failed 'Existing installation link accepted'
    Assert-SetupCheck ([IO.File]::ReadAllText($linkedExe) -eq 'previous valid installation') 'Linked installation changed'
    Assert-SetupCheck (@(Get-ChildItem -LiteralPath $outside -Force).Count -eq 0) 'Linked installation target modified'
    $script:checks++
    Write-Output 'PASS existing-installation-junction'
} finally { [IO.Directory]::Delete($installedJunction) }

# Real cold/warm runs use the unchanged repository lock and normal network downloader.
$locked = Get-Content -LiteralPath (Join-Path $repo 'tools/format/cpp-tools.lock.json') -Raw | ConvertFrom-Json
$realCache = Join-Path $work 'locked-cache'
$executables = @()
foreach ($tool in $locked) { $executables += Install-LockedCppTool -Tool $tool -Cache $realCache }
$hashes = @($executables | ForEach-Object { (Get-FileHash -LiteralPath $_).Hash })
Write-Output 'PASS locked-cold-acquisition'
$script:checks++
$noDownload = { param($Url, $Output); throw 'Warm cache attempted network access' }
# A warm run must restore executable contents from the verified archive, as the old setup did.
foreach ($exe in $executables) { [IO.File]::WriteAllText($exe, 'damaged installation') }
for ($index = 0; $index -lt $locked.Count; $index++) {
    $exe = Install-LockedCppTool -Tool $locked[$index] -Cache $realCache -Download $noDownload
    Assert-SetupCheck ((Get-FileHash -LiteralPath $exe).Hash -eq $hashes[$index]) 'Warm-cache executable differs'
}
Write-Output 'PASS locked-warm-acquisition (download disabled)'
$script:checks++

$wrapper = Join-Path $repo 'tools/format/Format-Cpp.ps1'
$probe = Join-Path $work 'format-probe.cpp'
[IO.File]::WriteAllText($probe, "int  main(){return 0;}`n")
$pwsh = (Get-Process -Id $PID).Path
foreach ($wrong in @('uncrustify', 'clang-format')) {
    $uncrustify = if ($wrong -eq 'uncrustify') { $pwsh } else { $executables[0] }
    $clangFormat = if ($wrong -eq 'clang-format') { $pwsh } else { $executables[1] }
    $output = & $pwsh -NoProfile -File $wrapper -Mode Check -Files $probe -Uncrustify $uncrustify -ClangFormat $clangFormat 2>&1 | Out-String
    Assert-SetupCheck ($LASTEXITCODE -ne 0 -and $output -match 'Wrong formatter version') "Wrong $wrong version accepted"
    Write-Output "PASS wrong-version-$wrong"
    $script:checks++
}
$before = (Get-FileHash -LiteralPath $probe).Hash
$output = & $pwsh -NoProfile -File $wrapper -Mode Check -Files $probe -Uncrustify $executables[0] -ClangFormat $executables[1] 2>&1 | Out-String
Assert-SetupCheck ($LASTEXITCODE -ne 0 -and $output -match 'Formatting mismatch') 'Formatting defect accepted'
Assert-SetupCheck ((Get-FileHash -LiteralPath $probe).Hash -eq $before) 'Check modified its input'
Write-Output 'PASS formatting-defect-read-only'
$script:checks++
$formattedHash = $null
foreach ($mode in @('Format', 'Check', 'Format', 'Check')) {
    & $pwsh -NoProfile -File $wrapper -Mode $mode -Files $probe -Uncrustify $executables[0] -ClangFormat $executables[1]
    Assert-SetupCheck ($LASTEXITCODE -eq 0) "$mode failed"
    $hash = (Get-FileHash -LiteralPath $probe).Hash
    if ($null -eq $formattedHash) { $formattedHash = $hash }
    Assert-SetupCheck ($hash -eq $formattedHash) 'Pipeline is not idempotent'
}
Assert-SetupCheck ($formattedHash -ne $before) 'Format did not repair the defect'
Write-Output 'PASS format-check-idempotence'
$script:checks++
Write-Output "Setup-Cpp checks passed: $script:checks; retained work: $work"
