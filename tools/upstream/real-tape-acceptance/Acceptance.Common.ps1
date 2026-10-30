# Internal launcher helpers, dot-sourced by Compare-RealTapes.ps1 and Test-Acceptance.ps1.
# Inputs are explicit paths/argument arrays/budgets. The explicit native runner owns the
# Windows job before child execution. A Java worker redirects target streams to raw files;
# only bounded runner diagnostics pass through PowerShell pipes. Bootstrap compiler output
# is explicitly UTC diagnostic text. Setup/resource failures never authorize unrelated kills.
function Get-AcceptanceWorkPath([string]$Repo, [string]$Path, [switch]$RequireNew) {
    $resolved = [IO.Path]::GetFullPath($Path, $Repo)
    $prefix = [IO.Path]::GetFullPath((Join-Path $Repo '.work')) + [IO.Path]::DirectorySeparatorChar
    if (!$resolved.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        ($RequireNew -and (Test-Path -LiteralPath $resolved))) {
        throw 'Use a new result directory below repository .work.'
    }
    $ancestor = $resolved
    while ($ancestor) {
        if ((Test-Path -LiteralPath $ancestor) -and
            ((Get-Item -LiteralPath $ancestor).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Reparse points are not allowed for acceptance result paths.'
        }
        $parent = Split-Path $ancestor -Parent
        if (!$parent -or $parent -eq $ancestor) { break }
        $ancestor = $parent
    }
    return $resolved
}

function Invoke-AcceptanceOwnedRunner {
    param([string]$RunnerPath, [string]$Executable, [string[]]$Arguments, [string]$Directory, [string]$Name,
        [int]$TimeoutSeconds, [long]$OutputBytes, [string]$WorkingDirectory)
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $RunnerPath
    $info.WorkingDirectory = $WorkingDirectory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $utcLog = Join-Path $Directory "$Name.utc.log"
    foreach ($argument in @('--log', $utcLog, '--', $Executable) + $Arguments) { $info.ArgumentList.Add($argument) }
    $started = [DateTime]::UtcNow
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    $stdoutPath = Join-Path $Directory "$Name.runner.stdout"
    $stderrPath = Join-Path $Directory "$Name.runner.stderr"
    $stdout = $null
    $stderr = $null
    $processStarted = $false
    $timedOut = $false
    $outputExceeded = $false
    $cleanupDeadline = $null
    try {
        $stdout = [IO.File]::Create($stdoutPath)
        $stderr = [IO.File]::Create($stderrPath)
        if (!$process.Start()) { throw 'Could not start owned acceptance process.' }
        $processStarted = $true
        $outTask = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $errTask = $process.StandardError.BaseStream.CopyToAsync($stderr)
        # Root exit never disables the deadline: inherited pipes may still be open.
        while (!($process.WaitForExit(0) -and $outTask.IsCompleted -and $errTask.IsCompleted)) {
            $timedOut = ([DateTime]::UtcNow - $started).TotalSeconds -ge $TimeoutSeconds
            $outputExceeded = $stdout.Length -gt 8MB -or $stderr.Length -gt 8MB
            if ($timedOut -or $outputExceeded) {
                $cleanupDeadline = [DateTime]::UtcNow.AddSeconds(1)
                if (!$process.HasExited) { $process.Kill($true) }
                if (!$process.WaitForExit(1000)) { throw 'Owned native runner termination was not confirmed.' }
                break
            }
            Start-Sleep -Milliseconds 20
        }
        if (!$outTask.IsCompleted -or !$errTask.IsCompleted) {
            $process.StandardOutput.BaseStream.Dispose()
            $process.StandardError.BaseStream.Dispose()
            throw 'Owned native runner stream drain exceeded the root deadline.'
        }
        $null = $outTask.GetAwaiter().GetResult()
        $null = $errTask.GetAwaiter().GetResult()
        $outputExceeded = $outputExceeded -or $stdout.Length -gt 8MB -or $stderr.Length -gt 8MB
        return [ordered]@{
            name = $Name; executable = $Executable; arguments = $Arguments
            startedUtc = $started.ToString('o'); finishedUtc = [DateTime]::UtcNow.ToString('o')
            timeoutSeconds = $TimeoutSeconds; maxOutputBytes = $OutputBytes
            timedOut = $timedOut; outputExceeded = $outputExceeded; exitCode = $process.ExitCode
            stdout = $stdoutPath; stderr = $stderrPath; utcLog = $utcLog; runnerPid = $process.Id
            runner = $RunnerPath; streamKind = 'native runner UTC diagnostic text'
        }
    } finally {
        if ($processStarted -and !$process.HasExited) {
            if ($null -eq $cleanupDeadline) { $cleanupDeadline = [DateTime]::UtcNow.AddSeconds(1) }
            $process.Kill($true)
            $cleanupMillis = [Math]::Max(0, [int]($cleanupDeadline - [DateTime]::UtcNow).TotalMilliseconds)
            if (!$process.WaitForExit($cleanupMillis)) { throw 'Owned native runner cleanup was not confirmed.' }
        }
        if ($null -ne $stdout) { $stdout.Dispose() }
        if ($null -ne $stderr) { $stderr.Dispose() }
        $process.Dispose()
    }
}

function Initialize-AcceptanceWorker([string]$RunnerPath, [string]$JavaPath, [string]$JavacPath,
        [string]$SourcePath, [string]$Directory, [int]$TimeoutSeconds, [string]$WorkingDirectory) {
    if (!$IsWindows) { throw 'This PowerShell native-runner workflow is currently qualified on Windows only.' }
    $script:AcceptanceRunner = (Resolve-Path -LiteralPath $RunnerPath).Path
    $script:AcceptanceWorkerJava = $JavaPath
    $script:AcceptanceWorkerClasses = Join-Path $Directory 'worker'
    New-Item -ItemType Directory -Path $script:AcceptanceWorkerClasses | Out-Null
    $bootstrap = Invoke-AcceptanceOwnedRunner $script:AcceptanceRunner $JavacPath @('-J-Xmx128m',
        '-encoding', 'UTF-8', '-d', $script:AcceptanceWorkerClasses, $SourcePath) $Directory 'worker-bootstrap' `
        $TimeoutSeconds 8388608 $WorkingDirectory
    Assert-AcceptanceProcess $bootstrap
    return $bootstrap
}

function Invoke-AcceptanceProcess {
    param([string]$Executable, [string[]]$Arguments, [string]$Directory, [string]$Name,
        [int]$TimeoutSeconds, [long]$OutputBytes, [string]$WorkingDirectory)
    if (!$script:AcceptanceRunner -or !$script:AcceptanceWorkerClasses -or !$script:AcceptanceWorkerJava) {
        throw 'Initialize the explicitly owned raw worker before running acceptance children.'
    }
    $stdout = Join-Path $Directory "$Name.stdout"
    $stderr = Join-Path $Directory "$Name.stderr"
    $status = Join-Path $Directory "$Name.status.tsv"
    $workerMillis = [Math]::Max(1, $TimeoutSeconds * 1000 - 200)
    $workerArguments = @('-Xmx32m', '-Dfile.encoding=UTF-8', '-cp', $script:AcceptanceWorkerClasses,
        'qdcxx.oracle.RawChild', "$workerMillis", "$OutputBytes", $stdout, $stderr, $status,
        $WorkingDirectory, $Executable) + $Arguments
    $result = Invoke-AcceptanceOwnedRunner $script:AcceptanceRunner $script:AcceptanceWorkerJava `
        $workerArguments $Directory $Name $TimeoutSeconds $OutputBytes $WorkingDirectory
    $result.targetExecutable = $Executable
    $result.targetArguments = $Arguments
    $result.runnerStdout = $result.stdout
    $result.runnerStderr = $result.stderr
    $result.stdout = $stdout
    $result.stderr = $stderr
    $result.streamKind = 'exact raw target streams; separate native runner UTC diagnostics'
    $result.statusFile = $status
    if (Test-Path -LiteralPath $status -PathType Leaf) {
        if ((Get-Item -LiteralPath $status).Length -gt 1024) { throw 'Raw worker status exceeded its fixed budget.' }
        $text = Get-Content -LiteralPath $status -Raw
        if ($text -notmatch '(?m)^exit-code\t(-?\d+)$') { throw 'Raw worker status lacks an exit code.' }
        $result.targetExitCode = [int]$Matches[1]
        if ($text -notmatch '(?m)^timed-out\t(true|false)\toutput-exceeded\t(true|false)$') { throw 'Raw worker status lacks resource results.' }
        $result.timedOut = $result.timedOut -or $Matches[1] -eq 'true'
        $result.outputExceeded = $result.outputExceeded -or $Matches[2] -eq 'true'
        if ($text -notmatch '(?m)^worker-pid\t(\d+)$') { throw 'Raw worker status lacks its process identity.' }
        $result.workerPid = [int]$Matches[1]
        if (!$result.timedOut -and !$result.outputExceeded -and $result.exitCode -ne $result.targetExitCode) {
            throw 'Native runner exit differs from the raw worker result.'
        }
    } elseif (!$result.timedOut -and !$result.outputExceeded) { throw 'Raw worker did not publish terminal status.' }
    foreach ($path in @($stdout, $stderr)) {
        if ((Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).Length -gt $OutputBytes) { $result.outputExceeded = $true }
    }
    return $result
}

function Assert-AcceptanceProcess($Result) {
    if ($Result.timedOut -or $Result.outputExceeded -or $Result.exitCode -ne 0) {
        throw "Acceptance phase $($Result.name) failed; inspect private raw logs."
    }
}

function Get-AcceptanceJava([string]$JavaDirectory, [string]$Name) {
    $path = Join-Path $JavaDirectory "bin/$Name.exe"
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { $path = Join-Path $JavaDirectory "bin/$Name" }
    return (Resolve-Path -LiteralPath $path).Path
}
