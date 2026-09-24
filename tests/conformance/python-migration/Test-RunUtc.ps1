#Requires -Version 7.4
<#
.SYNOPSIS
Exercise the UTC text logger using real child processes and raw stream bytes.
.DESCRIPTION
LegacyPython is used only for unchanged historical-launcher characterization. The default
suite requires PowerShell alone. WorkRoot is disposable test storage; no fixtures are edited.
#>
[CmdletBinding()]
param(
    [string]$WorkRoot = (Join-Path ([IO.Path]::GetTempPath()) "qdcxx-utc-$([Guid]::NewGuid().ToString('N'))"),
    [string]$LegacyPython
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$WorkRoot = [IO.Path]::GetFullPath($WorkRoot)
[IO.Directory]::CreateDirectory($WorkRoot) | Out-Null
$pwsh = (Get-Process -Id $PID).Path
$utf8 = [Text.UTF8Encoding]::new($false, $true)
$probe = Join-Path $WorkRoot 'probe space 雪.ps1'
$invoker = Join-Path $WorkRoot 'invoke.ps1'
[IO.File]::WriteAllText($probe, @'
param([string]$Mode)
$ErrorActionPreference = 'Stop'
switch ($Mode) {
    'args' {
        [Console]::WriteLine((ConvertTo-Json -InputObject @($args) -Compress))
        [Console]::WriteLine('TZ=' + $env:TZ)
    }
    'text' {
        $bytes = [byte[]](239,187,191,65,13,10,66,13,67,10,10,68,13,13,10,69,255,192,175,10,70)
        $output = [Console]::OpenStandardOutput()
        $output.Write($bytes)
        $output.Flush()
        $errorOutput = [Console]::OpenStandardError()
        $errorOutput.Write([byte[]](255,254,90,10))
        $errorOutput.Flush()
    }
    'exit' { exit 37 }
    'large' {
        $fill = 'x' * 256
        for ($index = 0; $index -lt 5000; ++$index) {
            [Console]::Out.WriteLine("O${index}:$fill")
            [Console]::Error.WriteLine("E${index}:$fill")
        }
        [Console]::Out.WriteLine('O-final')
        [Console]::Error.Write('E-final')
    }
    'running' {
        [IO.File]::WriteAllText($args[0], [string]$PID)
        while ($true) { [Console]::Out.WriteLine('running ' + ('x' * 4096)) }
    }
    'orphan' {
        [IO.File]::WriteAllText($args[1], [string]$PID)
        $descendant = [Diagnostics.Process]::new()
        $descendant.StartInfo.FileName = (Get-Process -Id $PID).Path
        $descendant.StartInfo.UseShellExecute = $false
        foreach ($value in @('-NoProfile', '-File', $PSCommandPath, 'running', $args[0])) {
            $descendant.StartInfo.ArgumentList.Add($value)
        }
        if (-not $descendant.Start()) { throw 'Cannot launch descendant.' }
        while (-not (Test-Path -LiteralPath $args[0])) { Start-Sleep -Milliseconds 20 }
        $descendant.Dispose()
    }
    default { throw "Unknown probe mode: $Mode" }
}
'@, $utf8)
[IO.File]::WriteAllText($invoker, @'
param([string]$Config)
$ErrorActionPreference = 'Stop'
$settings = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
$beforeTz = $env:TZ
$beforeZone = [TimeZoneInfo]::Local.Id
if ($settings.legacy) {
    $env:PYTHONDONTWRITEBYTECODE = '1'
    & $settings.legacy -B $settings.launcher --log $settings.log -- $settings.executable @($settings.arguments)
} else {
    & $settings.launcher -Log $settings.log -Executable $settings.executable -ArgumentList @($settings.arguments)
}
$childExit = $LASTEXITCODE
@{ beforeTz = $beforeTz; afterTz = $env:TZ; beforeZone = $beforeZone; afterZone = [TimeZoneInfo]::Local.Id } |
    ConvertTo-Json | Set-Content -LiteralPath $settings.parentState -Encoding utf8
exit $childExit
'@, $utf8)

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Start-Logger([string]$Name, [string[]]$ChildArguments, [string]$Command = $pwsh, [string]$LogPath = '') {
    if (-not $LogPath) { $LogPath = Join-Path $WorkRoot "$Name.log" }
    $launcher = if ($LegacyPython) { Join-Path $repo 'tools/run_utc.py' } else { Join-Path $repo 'tools/Run-Utc.ps1' }
    $config = Join-Path $WorkRoot "$Name.json"
    $settings = @{
        legacy = $LegacyPython; launcher = $launcher; log = $LogPath; executable = $Command
        arguments = $ChildArguments; parentState = (Join-Path $WorkRoot "$Name.env.json")
    }
    [IO.File]::WriteAllText($config, (ConvertTo-Json $settings -Depth 5), $utf8)
    $process = [Diagnostics.Process]::new()
    $process.StartInfo.FileName = $pwsh
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = $utf8
    $process.StartInfo.StandardErrorEncoding = $utf8
    foreach ($value in @('-NoProfile', '-File', $invoker, $config)) { $process.StartInfo.ArgumentList.Add($value) }
    $process.StartInfo.Environment['TZ'] = 'Pacific/Honolulu'
    if (-not $process.Start()) { throw 'Unable to start test launcher.' }
    return $process
}

function Invoke-Logger([string]$Name, [string[]]$ChildArguments, [string]$Command = $pwsh, [string]$LogPath = '') {
    $process = Start-Logger $Name $ChildArguments $Command $LogPath
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(30000)) { throw "$Name exceeded the 30-second test deadline." }
        if (-not [Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout, $stderr), 5000)) {
            throw "$Name output drain exceeded 5 seconds."
        }
        return @{ Exit = $process.ExitCode; Out = $stdout.Result; Err = $stderr.Result }
    } finally {
        if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit(5000) | Out-Null }
        $process.Dispose()
    }
}

function Read-Messages([string]$Name, $Result) {
    $bytes = [IO.File]::ReadAllBytes((Join-Path $WorkRoot "$Name.log"))
    Assert-True (-not ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191)) "$Name has a BOM."
    Assert-True (-not ($bytes -contains 13)) "$Name has CR bytes."
    $text = $utf8.GetString($bytes)
    Assert-True ($text.EndsWith("`n")) "$Name has no terminal LF."
    # Historical Python print uses the host's console newline, while both log formats use LF.
    Assert-True ($Result.Out.Replace("`r`n", "`n") -ceq $text) "$Name console/log contents differ."
    $messages = foreach ($line in $text.TrimEnd("`n").Split("`n")) {
        Assert-True ($line -cmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z ') "$Name has a non-UTC timestamp."
        $line.Substring(25)
    }
    return ,@($messages)
}

$parentTz = $env:TZ
$hostZone = [TimeZoneInfo]::Local.Id
$arguments = @('', 'space value', '雪', 'a;b', '"', 'C:\end\', 'a"b\', '-option')
$result = Invoke-Logger 'arguments' (@('-NoProfile', '-File', $probe, 'args') + $arguments)
Assert-True ($result.Exit -eq 0 -and $result.Err -eq '') "arguments failed: $($result.Err)"
$messages = Read-Messages 'arguments' $result
$observed = @($messages[1] | ConvertFrom-Json)
Assert-True ($observed.Count -eq $arguments.Count) 'Argument count changed.'
for ($index = 0; $index -lt $arguments.Count; ++$index) {
    Assert-True ($observed[$index] -ceq $arguments[$index]) "Argument $index changed."
}
Assert-True ($messages[2] -ceq 'TZ=UTC') 'Child TZ was not UTC.'
$parentState = Get-Content -LiteralPath (Join-Path $WorkRoot 'arguments.env.json') -Raw | ConvertFrom-Json
Assert-True ($parentState.beforeTz -ceq 'Pacific/Honolulu' -and $parentState.afterTz -ceq $parentState.beforeTz) 'Launcher changed its parent TZ.'
Assert-True ($parentState.afterZone -ceq $parentState.beforeZone) 'Launcher changed the host timezone.'
Assert-True ($env:TZ -ceq $parentTz -and [TimeZoneInfo]::Local.Id -ceq $hostZone) 'Parent TZ/host timezone changed.'
Write-Output 'PASS arguments; child-only TZ; parent/host unchanged'

$result = Invoke-Logger 'text' @('-NoProfile', '-File', $probe, 'text')
Assert-True ($result.Exit -eq 0 -and $result.Err -eq '') 'Text child failed.'
$messages = Read-Messages 'text' $result
$expected = @("$([char]0xFEFF)A", 'B', 'C', '', 'D', '', "E$([char]0xFFFD)$([char]0xFFFD)$([char]0xFFFD)", 'F')
$stderrLine = "$([char]0xFFFD)$([char]0xFFFD)Z"
# The historical launcher merged bytes before decoding; its final stdout fragment joins
# stderr's following row. The replacement retains each stream's final fragment separately.
if ($LegacyPython) {
    $expected[-1] += $stderrLine
} else {
    Assert-True (@($messages | Where-Object { $_ -ceq $stderrLine }).Count -eq 1) 'UTF-16 BOM changed stderr decoding.'
    $messages = @($messages | Where-Object { $_ -cne $stderrLine })
}
Assert-True ($messages.Count -eq $expected.Count + 2) 'Text line count changed.'
for ($index = 0; $index -lt $expected.Count; ++$index) {
    Assert-True ($messages[$index + 1] -ceq $expected[$index]) "Text line $index changed."
}
Assert-True ($messages[-1] -ceq 'EXIT 0') 'Success exit record missing.'
Write-Output 'PASS CRLF/bare-CR/LF; invalid UTF-8 replacement; BOM decoding; empty/final unterminated lines; LF/no log BOM'

$result = Invoke-Logger 'nonzero' @('-NoProfile', '-File', $probe, 'exit')
Assert-True ($result.Exit -eq 37 -and $result.Err -eq '') 'Nonzero child exit changed.'
$messages = Read-Messages 'nonzero' $result
Assert-True ($messages[-1] -ceq 'EXIT 37') 'Nonzero exit record missing.'
$result = Invoke-Logger 'missing' @() (Join-Path $WorkRoot 'missing-executable')
Assert-True ($result.Exit -eq 1) 'Missing executable did not fail.'
Write-Output 'PASS child exit 37; missing executable'

$result = Invoke-Logger 'large' @('-NoProfile', '-File', $probe, 'large')
Assert-True ($result.Exit -eq 0 -and $result.Err -eq '') "Large streams failed: $($result.Err)"
$messages = Read-Messages 'large' $result
foreach ($prefix in @('O', 'E')) {
    $rows = @($messages | Where-Object { $_.StartsWith($prefix) -and $_ -ne 'EXIT 0' })
    Assert-True ($rows.Count -eq 5001) "$prefix stream lost lines."
    for ($index = 0; $index -lt 5000; ++$index) {
        Assert-True ($rows[$index] -ceq "$prefix${index}:$('x' * 256)") "$prefix stream order/content changed at $index."
    }
    Assert-True ($rows[-1] -ceq "$prefix-final") "$prefix final line missing."
}
Write-Output 'PASS concurrent stdout/stderr above pipe capacity: 5000 lines + final line each'

$result = Invoke-Logger 'unwritable' @('-NoProfile', '-File', $probe, 'exit') $pwsh $WorkRoot
Assert-True ($result.Exit -ne 0) 'Directory log target did not fail.'
Write-Output 'PASS unwritable log'

if (-not $LegacyPython) {
    foreach ($mode in @('running', 'orphan')) {
        $pidFile = Join-Path $WorkRoot "$mode.pid"
        $rootPidFile = Join-Path $WorkRoot "$mode-root.pid"
        if (Test-Path -LiteralPath $pidFile) { Remove-Item -LiteralPath $pidFile }
        if (Test-Path -LiteralPath $rootPidFile) { Remove-Item -LiteralPath $rootPidFile }
        $process = Start-Logger "closed-console-$mode" @('-NoProfile', '-File', $probe, $mode, $pidFile, $rootPidFile)
        $childPid = $null
        try {
            $stderr = $process.StandardError.ReadToEndAsync()
            $header = $process.StandardOutput.ReadLineAsync()
            Assert-True ($header.Wait(15000)) 'Logger metadata was not written.'
            $deadline = [DateTime]::UtcNow.AddSeconds(15)
            while (-not (Test-Path -LiteralPath $pidFile) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 20 }
            Assert-True (Test-Path -LiteralPath $pidFile) 'Long-running child did not start.'
            $childPid = [int][IO.File]::ReadAllText($pidFile)
            if ($mode -eq 'orphan') {
                $rootPid = [int][IO.File]::ReadAllText($rootPidFile)
                while ((Get-Process -Id $rootPid -ErrorAction SilentlyContinue) -and [DateTime]::UtcNow -lt $deadline) {
                    Start-Sleep -Milliseconds 20
                }
                Assert-True (-not (Get-Process -Id $rootPid -ErrorAction SilentlyContinue)) 'Orphan root did not exit before console failure.'
                Assert-True ([bool](Get-Process -Id $childPid -ErrorAction SilentlyContinue)) 'Descendant was not alive after root exit.'
            }
            $process.StandardOutput.Dispose()
            Assert-True ($process.WaitForExit(10000)) 'Closed-console cleanup exceeded 10 seconds.'
            Assert-True ($process.ExitCode -eq 1) 'Console failure did not return launcher exit 1.'
            Assert-True ($stderr.Wait(5000) -and $stderr.Result.Contains('UTC LAUNCHER FAILED:')) 'Console failure diagnostic missing.'
            Assert-True (-not (Get-Process -Id $childPid -ErrorAction SilentlyContinue)) 'Child survived console failure.'
        } finally {
            if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit(5000) | Out-Null }
            if ($null -ne $childPid) { Stop-Process -Id $childPid -Force -ErrorAction SilentlyContinue }
            $process.Dispose()
        }
        Write-Output "PASS real closed-console failure; bounded process cleanup; root=$mode"
    }
    if (-not $IsWindows -and (Test-Path -LiteralPath '/dev/full')) {
        $result = Invoke-Logger 'full-device' @('-NoProfile', '-File', $probe, 'exit') $pwsh '/dev/full'
        Assert-True ($result.Exit -eq 1 -and $result.Err.Contains('UTC LAUNCHER FAILED:')) '/dev/full write failure not reported.'
        Write-Output 'PASS real log write failure (/dev/full)'
    }
}
Write-Output "PASS UTC logger suite; legacy=$([bool]$LegacyPython); PowerShell=$($PSVersionTable.PSVersion); OS=$([Environment]::OSVersion)"
