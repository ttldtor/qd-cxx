#Requires -Version 7.4
<#
.SYNOPSIS
Run one command with child-only TZ=UTC and timestamped UTF-8 text logging.
.DESCRIPTION
Pass arguments as an array; no shell command string is interpreted. Both redirected streams
are drained concurrently. Each stream retains its line order; their relative order is unspecified.
UTF-8 decoding replaces invalid sequences, CR/LF/CRLF terminate lines, and a final unterminated
line is retained. The log uses LF without a BOM. This is a text log, not a raw-byte capture.
Launcher, log and console failures return 1 and terminate a started child process tree.
An internal PowerShell worker establishes ownership before launching the command: a Windows
job or a Unix process group. Descendants which deliberately escape a Unix session are outside
this boundary. Remaining owned descendants are terminated when logging ends.
.EXAMPLE
& ./tools/Run-Utc.ps1 -Log build.log -Executable cmake -ArgumentList @('--build', '.work/build')
#>
[CmdletBinding(DefaultParameterSetName = 'Run')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Run')][string]$Log,
    [Parameter(Mandatory, ParameterSetName = 'Run')][string]$Executable,
    [Parameter(ParameterSetName = 'Run')][string[]]$ArgumentList = @(),
    [Parameter(Mandatory, ParameterSetName = 'Worker', DontShow)][string]$InternalWorkerConfig
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8 = [System.Text.UTF8Encoding]::new($false, $false)
$process = [System.Diagnostics.Process]::new()
$writer = $null
$console = $null
$started = $false
$result = 1
$job = [IntPtr]::Zero
$configPath = $null
$isWorker = $PSCmdlet.ParameterSetName -eq 'Worker'

function Write-UtcLine([string]$Message) {
    $stamp = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ss.fffZ', [Globalization.CultureInfo]::InvariantCulture)
    $line = "$stamp $Message`n"
    $bytes = $utf8.GetBytes($line)
    $console.Write($bytes, 0, $bytes.Length)
    $console.Flush()
    $writer.Write($line)
    $writer.Flush()
}

try {
    # Console.OpenStandardOutput intentionally hides broken-pipe errors on Windows.
    # A non-owning FileStream reports them so a failed log destination stops the child.
    if (-not ('QdcxxUtcOutput' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.ComponentModel;
using Microsoft.Win32.SafeHandles;
public static class QdcxxUtcOutput {
    [DllImport("kernel32.dll")]
    private static extern IntPtr GetStdHandle(int kind);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateJobObject(IntPtr attributes, string name);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr OpenJobObject(uint access, bool inherit, string name);
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool SetInformationJobObject(IntPtr job, int kind, ref Limits limits, uint size);
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool TerminateJobObject(IntPtr job, uint code);
    [DllImport("kernel32.dll")]
    private static extern bool CloseHandle(IntPtr handle);
    [DllImport("libc", SetLastError = true)]
    private static extern int setsid();
    [DllImport("libc", SetLastError = true)]
    private static extern int kill(int pid, int signal);
    [StructLayout(LayoutKind.Sequential)]
    private struct BasicLimits {
        public long ProcessTime, JobTime;
        public uint Flags;
        public UIntPtr MinimumWorkingSet, MaximumWorkingSet;
        public uint ActiveProcesses;
        public UIntPtr Affinity;
        public uint Priority, Scheduling;
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct IoCounters { public ulong ReadOps, WriteOps, OtherOps, ReadBytes, WriteBytes, OtherBytes; }
    [StructLayout(LayoutKind.Sequential)]
    private struct Limits {
        public BasicLimits Basic;
        public IoCounters Io;
        public UIntPtr ProcessMemory, JobMemory, PeakProcessMemory, PeakJobMemory;
    }
    public static IntPtr CreateOwnership(string name) {
        if (!OperatingSystem.IsWindows()) return IntPtr.Zero;
        IntPtr job = CreateJobObject(IntPtr.Zero, name);
        if (job == IntPtr.Zero) throw new Win32Exception();
        Limits limits = new Limits();
        limits.Basic.Flags = 0x2000; // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if (!SetInformationJobObject(job, 9, ref limits, (uint)Marshal.SizeOf<Limits>())) {
            int error = Marshal.GetLastPInvokeError();
            CloseHandle(job);
            throw new Win32Exception(error);
        }
        return job;
    }
    public static void EnterOwnership(string name) {
        if (OperatingSystem.IsWindows()) {
            IntPtr job = OpenJobObject(1, false, name); // JOB_OBJECT_ASSIGN_PROCESS
            if (job == IntPtr.Zero) throw new Win32Exception();
            try {
                if (!AssignProcessToJobObject(job, new IntPtr(-1))) throw new Win32Exception();
            } finally { CloseHandle(job); }
        } else if (setsid() < 0) { throw new Win32Exception(); }
    }
    public static void StopOwnership(IntPtr job, int leader) {
        if (OperatingSystem.IsWindows()) {
            if (job != IntPtr.Zero && !TerminateJobObject(job, 1)) throw new Win32Exception();
        } else if (kill(-leader, 9) < 0 && Marshal.GetLastPInvokeError() != 3) {
            throw new Win32Exception(); // ESRCH means the owned group has already ended.
        }
    }
    public static void CloseOwnership(IntPtr job) {
        if (job != IntPtr.Zero) CloseHandle(job);
    }
    public static Stream Open() {
        IntPtr handle = OperatingSystem.IsWindows() ? GetStdHandle(-11) : new IntPtr(1);
        return new FileStream(new SafeFileHandle(handle, false), FileAccess.Write, 4096, false);
    }

}
'@
    }
    if ($isWorker) {
        $configuration = Get-Content -LiteralPath $InternalWorkerConfig -Raw | ConvertFrom-Json
        [QdcxxUtcOutput]::EnterOwnership($configuration.jobName)
        $process.StartInfo.FileName = $configuration.executable
        $process.StartInfo.UseShellExecute = $false
        foreach ($argument in $configuration.arguments) { $process.StartInfo.ArgumentList.Add([string]$argument) }
        $started = $process.Start()
        if (-not $started) { throw 'The command process did not start.' }
        $process.WaitForExit()
        exit $process.ExitCode
    }
    $logPath = [IO.Path]::GetFullPath($Log)
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($logPath)) | Out-Null
    $writer = [IO.StreamWriter]::new($logPath, $false, $utf8)
    $console = [QdcxxUtcOutput]::Open()
    $command = ConvertTo-Json -InputObject (@($Executable) + $ArgumentList) -Compress
    Write-UtcLine "COMMAND $command; cwd=$((Get-Location).Path); TZ=UTC"

    $info = $process.StartInfo
    $jobName = "qdcxx-utc-$([Guid]::NewGuid().ToString('N'))"
    $job = [QdcxxUtcOutput]::CreateOwnership($jobName)
    $configPath = [IO.Path]::GetTempFileName()
    [IO.File]::WriteAllText($configPath, (ConvertTo-Json -InputObject @{
        executable = $Executable; arguments = $ArgumentList; jobName = $jobName
    } -Depth 4 -Compress), $utf8)
    $info.FileName = (Get-Process -Id $PID).Path
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = $utf8
    $info.StandardErrorEncoding = $utf8
    $info.WorkingDirectory = (Get-Location).ProviderPath
    $info.Environment['TZ'] = 'UTC'
    foreach ($argument in @('-NoProfile', '-File', $PSCommandPath, '-InternalWorkerConfig', $configPath)) {
        $info.ArgumentList.Add($argument)
    }
    $started = $process.Start()
    if (-not $started) { throw 'The child process did not start.' }

    # One outstanding read per stream bounds queued lines and prevents a full stderr pipe
    # from blocking stdout. ReadLineAsync implements the historical universal-newline policy.
    # Disable BOM detection: the contract is UTF-8 even when a child emits a UTF-16 BOM.
    # A UTF-8 BOM is retained as U+FEFF, as in the historical text decoder.
    $readers = @(
        [IO.StreamReader]::new($process.StandardOutput.BaseStream, $utf8, $false, 4096, $true),
        [IO.StreamReader]::new($process.StandardError.BaseStream, $utf8, $false, 4096, $true)
    )
    $pending = @($readers[0].ReadLineAsync(), $readers[1].ReadLineAsync())
    while ($null -ne $pending[0] -or $null -ne $pending[1]) {
        $ready = $false
        for ($index = 0; $index -lt 2; ++$index) {
            if ($null -eq $pending[$index] -or -not $pending[$index].IsCompleted) { continue }
            $ready = $true
            $line = $pending[$index].GetAwaiter().GetResult()
            if ($null -eq $line) {
                $pending[$index] = $null
            } else {
                Write-UtcLine $line
                $pending[$index] = $readers[$index].ReadLineAsync()
            }
        }
        if (-not $ready) {
            $active = [Threading.Tasks.Task[]]@($pending | Where-Object { $null -ne $_ })
            [Threading.Tasks.Task]::WaitAny($active, 100) | Out-Null
        }
    }
    $process.WaitForExit()
    $result = $process.ExitCode
    Write-UtcLine "EXIT $result"
} catch {
    $result = 1
    $failure = $_.Exception.Message
    if ($started) {
        try {
            if (-not $isWorker) { [QdcxxUtcOutput]::StopOwnership($job, $process.Id) }
            if (-not $process.HasExited) {
                $process.Kill($true)
                if (-not $process.WaitForExit(5000)) { $failure += '; child cleanup exceeded 5 seconds' }
            }
        } catch { $failure += "; child cleanup failed: $($_.Exception.Message)" }
    }
    # A failed destination is not retried indefinitely. Stderr is best-effort diagnostics;
    # the nonzero exit remains authoritative if every output destination is unavailable.
    try { [Console]::Error.WriteLine("UTC LAUNCHER FAILED: $failure") } catch { }
} finally {
    if (-not $isWorker -and $started) {
        try { [QdcxxUtcOutput]::StopOwnership($job, $process.Id) } catch { $result = 1 }
    }
    if ($job -ne [IntPtr]::Zero) { [QdcxxUtcOutput]::CloseOwnership($job) }
    if ($null -ne $configPath) { Remove-Item -LiteralPath $configPath -Force -ErrorAction SilentlyContinue }
    if ($null -ne $writer) {
        try { $writer.Dispose() } catch { $result = 1 }
    }
    if ($null -ne $console) {
        try { $console.Dispose() } catch { $result = 1 }
    }
    $process.Dispose()
}
exit $result
