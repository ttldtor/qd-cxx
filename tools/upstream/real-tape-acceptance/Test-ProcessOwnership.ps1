#requires -Version 7.4
<#
.SYNOPSIS
Regresses inherited streams, root deadlines and final absence of owned processes.
.DESCRIPTION
Windows-qualified test of the explicit native runner job plus raw Java worker. Creates
only a new repository .work directory and short-lived owned Java probes. Never kills a
process found by name or an unrelated PID. Full diagnostics and own PID records stay local.
.PARAMETER Runner
Explicit prebuilt qdcxx-run-utc executable.
.PARAMETER JavaHome
JDK 8 directory for the raw worker and process probes.
.PARAMETER RunDirectory
New result directory below repository .work.
.PARAMETER Help
Help without any dependency lookup or file work; no arguments is identical.
.NOTES
Exit 0: probes passed. Exception/nonzero: failure. A one-second root deadline permits at
most one further second for confirmed native-owner termination and stream cleanup.
#>
[CmdletBinding()]
param([string]$Runner, [string]$JavaHome, [string]$RunDirectory, [switch]$Help)
if ($Help -or $PSBoundParameters.Count -eq 0) {
    'Test-ProcessOwnership.ps1 -Runner EXE -JavaHome JDK8 -RunDirectory NEW_REPOSITORY_WORK_PATH'
    'Windows only: exiting parent with inherited streams, one-second root deadline, output cap, final own PID absence.'
    'Exit 0: checks passed; exception/nonzero: failure. No arguments / -Help starts no file work.'
    return
}
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$Runner -or !$JavaHome -or !$RunDirectory) { throw 'Runner, JavaHome and RunDirectory are required. Use -Help.' }
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
. (Join-Path $PSScriptRoot 'Acceptance.Common.ps1')
$run = Get-AcceptanceWorkPath $repo $RunDirectory -RequireNew
New-Item -ItemType Directory -Path $run | Out-Null
$java = Get-AcceptanceJava $JavaHome 'java'
$javac = Get-AcceptanceJava $JavaHome 'javac'
$runnerPath = (Resolve-Path -LiteralPath $Runner).Path
$results = [Collections.Generic.List[object]]::new()
$status = 'INCOMPLETE'
$started = [DateTime]::UtcNow.ToString('o')
try {
    $bootstrap = Initialize-AcceptanceWorker $runnerPath $java $javac (Join-Path $PSScriptRoot 'RawChild.java') $run 30 $repo
    $source = @'
import java.lang.management.ManagementFactory;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;

public final class OwnershipProbe {
  public static void main(String[] args) throws Exception {
    Path pids = Paths.get(args[2]);
    String pid = ManagementFactory.getRuntimeMXBean().getName().split("@", 2)[0];
    Files.write(pids, ((args[0].equals("sleep") ? "child" : "parent") + "\t" + pid + "\n").getBytes(StandardCharsets.US_ASCII),
        StandardOpenOption.CREATE, StandardOpenOption.APPEND);
    if (args[0].equals("sleep")) { Thread.sleep(30000); return; }
    if (args[0].equals("output")) {
      for (;;) { System.out.print(new String(new char[4096]).replace('\0', 'x')); System.out.flush(); Thread.sleep(2); }
    }
    new ProcessBuilder(args[1], "-Xmx32m", "-cp", System.getProperty("java.class.path"), "OwnershipProbe", "sleep", args[1], args[2])
        .inheritIO().start();
    long until = System.nanoTime() + 1000000000L;
    while (!new String(Files.readAllBytes(pids), StandardCharsets.US_ASCII).contains("child\t")) {
      if (System.nanoTime() >= until) { throw new IllegalStateException("child-not-ready"); }
      Thread.sleep(10);
    }
    System.out.println("owned-parent-exiting-with-inherited-child-streams");
    if (args[0].equals("wait")) { Thread.sleep(30000); }
  }
}
'@
    $sourcePath = Join-Path $run 'OwnershipProbe.java'
    [IO.File]::WriteAllText($sourcePath, $source, [Text.UTF8Encoding]::new($false))
    $compile = Invoke-AcceptanceProcess $javac @('-encoding', 'UTF-8', '-d', $run, $sourcePath) $run 'probe-javac' 30 8388608 $repo
    Assert-AcceptanceProcess $compile
    foreach ($case in @(@('exit', 1, 8388608), @('wait', 1, 8388608), @('output', 3, 1024))) {
        $pidPath = Join-Path $run "$($case[0]).pids"
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $result = Invoke-AcceptanceProcess $java @('-Xmx32m', '-cp', $run, 'OwnershipProbe', $case[0], $java, $pidPath) `
            $run "probe-$($case[0])" ([int]$case[1]) ([long]$case[2]) $repo
        $elapsed = $timer.Elapsed.TotalSeconds
        if ($elapsed -gt ([int]$case[1] + 1.25)) { throw 'Root deadline plus bounded cleanup was exceeded.' }
        if ($case[0] -eq 'exit' -and ($result.exitCode -ne 0 -or $result.timedOut -or $result.outputExceeded)) { throw 'Exiting inherited-stream parent did not complete normally.' }
        if ($case[0] -eq 'wait' -and !$result.timedOut) { throw 'Running root did not reach its deadline.' }
        if ($case[0] -eq 'output' -and !$result.outputExceeded) { throw 'Raw output cap was not enforced.' }
        $ownPids = @(Get-Content -LiteralPath $pidPath | ForEach-Object { [int]($_.Split([char]9)[1]) })
        if ($case[0] -in @('exit', 'wait') -and $ownPids.Count -ne 2) { throw 'Both owned probe identities were not recorded.' }
        $identities = @($ownPids) + @([int]$result.runnerPid)
        if ($result.Contains('workerPid')) { $identities += [int]$result.workerPid }
        $alive = @($identities | Where-Object { $null -ne (Get-Process -Id $_ -ErrorAction SilentlyContinue) })
        if ($alive.Count -ne 0) { throw 'An owned probe/worker/runner process survived return.' }
        $results.Add([ordered]@{ name = $case[0]; elapsedSeconds = $elapsed; deadlineSeconds = $case[1]
            allOwnedPidsAbsent = $true; ownedPids = $identities; result = $result })
    }
    $status = 'PASS'
} finally {
    [ordered]@{ status = $status; startedUtc = $started; finishedUtc = [DateTime]::UtcNow.ToString('o')
        workerSourceSha256 = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'RawChild.java')).Hash.ToLowerInvariant()
        helperSha256 = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'Acceptance.Common.ps1')).Hash.ToLowerInvariant()
        runnerSha256 = (Get-FileHash -LiteralPath $runnerPath).Hash.ToLowerInvariant(); results = $results
    } | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $run 'process-tests.json') -Encoding utf8NoBOM
}
'PASS: inherited streams, root deadline, output cap and final absence of all owned PIDs.'
