# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

# Internal helper for Measure-Stream.ps1 and Test-ProcessMetrics.ps1. Call New-ResourceSession,
# Register-ResourceChild for each owned Process, Add-ResourceSample at named boundaries,
# Get-ResourceSummary, and Export-ResourceSamples in finally. Inputs are live owned children;
# outputs retain cumulative counters, monotonic observation times and unavailable reasons.
# Argument/I/O failures throw; OS observation races are retained as unavailable CSV rows.
. (Join-Path $PSScriptRoot 'MetricsExport.ps1')
if (!('Qdcxx.Benchmark.ResourceReader' -as [type])) {
    Add-Type -Path (Join-Path $PSScriptRoot 'ProcessMetrics.cs')
}

function New-ResourceSession {
    param([ValidateRange(10, 10000)][int]$SampleIntervalMs = 100)
    @{
        Clock = [Diagnostics.Stopwatch]::StartNew()
        OriginTicks = [Diagnostics.Stopwatch]::GetTimestamp()
        SampleIntervalMs = $SampleIntervalMs
        Samples = [Collections.Generic.List[object]]::new()
        Threads = [Collections.Generic.List[object]]::new()
        SamplerWallMs = 0.0
        SamplerCpuMs = 0.0
    }
}

function Register-ResourceChild($Session, $Child) {
    $Child.ResourceReader = [Qdcxx.Benchmark.ResourceReader]::new($Child.Process)
    $Child.ResourceIdentity = $Child.ResourceReader.Identity
}

function Add-ResourceSample($Session, $Child, [string]$Phase) {
    if ($null -eq $Child -or $Child.Process.HasExited) { return }
    $overhead = [Diagnostics.Stopwatch]::StartNew()
    $sampler = [Diagnostics.Process]::GetCurrentProcess()
    try {
        $cpuStart = $sampler.TotalProcessorTime.TotalMilliseconds
        $snapshot = $Child.ResourceReader.Read()
        $start = ($snapshot.StartTicks - $Session.OriginTicks) * 1000.0 / [Diagnostics.Stopwatch]::Frequency
        $finish = ($snapshot.EndTicks - $Session.OriginTicks) * 1000.0 / [Diagnostics.Stopwatch]::Frequency

        $row = [pscustomobject][ordered]@{
            elapsedMs = ($start + $finish) / 2
            process = $Child.Name
            phase = $Phase
            userCpuMs = $snapshot.UserCpuMs
            systemCpuMs = $snapshot.SystemCpuMs
            workingSetBytes = $snapshot.WorkingSetBytes
            privateBytes = $snapshot.PrivateBytes
            pid = $Child.Process.Id
            processIdentity = $snapshot.Identity
            sampleStartMs = $start
            sampleEndMs = $finish
            osPeakWorkingSetBytes = $snapshot.OsPeakWorkingSetBytes
            status = $snapshot.Status
            privateBytesStatus = $snapshot.PrivateBytesStatus
            affinity = $snapshot.Affinity
            affinityStatus = $snapshot.AffinityStatus
        }
        $Session.Samples.Add($row)
        foreach ($thread in $snapshot.Threads) {
            $threadStart = ($thread.StartTicks - $Session.OriginTicks) * 1000.0 / [Diagnostics.Stopwatch]::Frequency
            $threadEnd = ($thread.EndTicks - $Session.OriginTicks) * 1000.0 / [Diagnostics.Stopwatch]::Frequency
            $Session.Threads.Add([pscustomobject][ordered]@{
                elapsedMs = ($threadStart + $threadEnd) / 2
                process = $Child.Name
                phase = $Phase
                pid = $row.pid
                processIdentity = $row.processIdentity
                tid = $thread.Tid
                threadIdentity = $thread.Identity
                threadName = $thread.Name
                userCpuMs = $thread.UserCpuMs
                systemCpuMs = $thread.SystemCpuMs
                sampleStartMs = $threadStart
                sampleEndMs = $threadEnd
                status = if ($snapshot.Status -eq 'ok') { $thread.Status } else { 'unavailable: parent process snapshot failed' }
            })
        }
        $sampler.Refresh()
        $Session.SamplerCpuMs += $sampler.TotalProcessorTime.TotalMilliseconds - $cpuStart
    } finally {
        $Session.SamplerWallMs += $overhead.Elapsed.TotalMilliseconds
        $sampler.Dispose()
    }
}

function Get-ResourceCpuSummary($Rows) {
    if ($Rows.Count -lt 2) {
        return @{ samples = $Rows.Count; status = 'unavailable: fewer than two live samples'; userCpuSeconds = $null; systemCpuSeconds = $null; averageCpuCores = $null }
    }
    $first = $Rows[0]
    $last = $Rows[-1]
    $seconds = ($last.elapsedMs - $first.elapsedMs) / 1000
    $user = ($last.userCpuMs - $first.userCpuMs) / 1000
    $system = ($last.systemCpuMs - $first.systemCpuMs) / 1000
    @{
        samples = $Rows.Count
        firstSampleMs = $first.elapsedMs
        lastSampleMs = $last.elapsedMs
        elapsedSeconds = $seconds
        userCpuSeconds = $user
        systemCpuSeconds = $system
        averageCpuCores = if ($seconds -gt 0) { ($user + $system) / $seconds } else { $null }
        status = if ($user -lt 0 -or $system -lt 0) { 'counter-regressed: signed deltas retained' } elseif ($seconds -le 0) { 'unavailable: non-positive interval' } else { 'ok' }
    }
}

function Get-ResourceSummary($Session, $Child, [string]$Phase = 'active') {
    $rows = @($Session.Samples | Where-Object { $_.process -eq $Child.Name -and $_.pid -eq $Child.Process.Id -and $_.processIdentity -eq $Child.ResourceIdentity -and $_.phase -eq $Phase -and $_.status -eq 'ok' })
    $result = Get-ResourceCpuSummary $rows
    $result.pid = $Child.Process.Id
    $result.phase = $Phase
    $result.finalBoundary = "last observed live $Phase sample (before exit; no drain-completion claim)"
    $result.sampleIntervalMs = $Session.SampleIntervalMs
    $result.logicalProcessors = [Environment]::ProcessorCount
    $result.affinityStatus = 'unavailable: no successful sample; averageCpuCores has no host-normalized divisor'
    $result.quotaStatus = 'not recorded; no host-normalized percentage exported'
    $result.memoryApi = if ($IsWindows) { 'System.Diagnostics.Process (GetProcessMemoryInfo): working set, private commitment, lifetime peak working set' } else { '/proc/PID/status: VmRSS resident set and VmHWM lifetime resident high-water mark; private bytes unavailable' }
    if ($rows.Count) {
        $memory = @($rows.workingSetBytes | Where-Object { $null -ne $_ } | Sort-Object)
        $private = @($rows.privateBytes | Where-Object { $null -ne $_ })
        $result.processIdentity = $rows[0].processIdentity
        $result.firstAffinity = $rows[0].affinity
        $result.lastAffinity = $rows[-1].affinity
        $result.affinityStatus = $rows[-1].affinityStatus
        $result.firstWorkingSetBytes = $rows[0].workingSetBytes
        $result.finalWorkingSetBytes = $rows[-1].workingSetBytes
        $result.finalPrivateBytes = $rows[-1].privateBytes
        $result.finalMemorySampleMs = $rows[-1].elapsedMs
        $result.privateBytesStatus = $rows[-1].privateBytesStatus
        $result.osPeakWorkingSetBytes = $rows[-1].osPeakWorkingSetBytes
        $result.osPeakScope = 'process lifetime through last live sample'
        $result.sampledPeakWorkingSetBytes = if ($memory.Count) { $memory[-1] } else { $null }
        $result.medianWorkingSetBytes = if ($memory.Count) { ($memory[[int][Math]::Floor(($memory.Count - 1) / 2)] + $memory[[int][Math]::Floor($memory.Count / 2)]) / 2 } else { $null }
        $result.sampledPeakPrivateBytes = if ($private.Count) { ($private | Measure-Object -Maximum).Maximum } else { $null }
    }
    $threadRows = @($Session.Threads | Where-Object { $_.process -eq $Child.Name -and $_.pid -eq $Child.Process.Id -and $_.processIdentity -eq $Child.ResourceIdentity -and $_.phase -eq $Phase -and $_.status -eq 'ok' })
    $result.threads = @(foreach ($group in ($threadRows | Group-Object pid,processIdentity,tid,threadIdentity)) {
        $thread = Get-ResourceCpuSummary @($group.Group)
        $thread.tid = $group.Group[0].tid
        $thread.threadIdentity = $group.Group[0].threadIdentity
        $thread.threadName = $group.Group[-1].threadName
        $thread
    })
    $result.threadCoverage = 'Observed live thread identities only; threads born and exited between samples are absent. Each thread has its own first/last interval; do not sum averages or equate sampled deltas to whole-process CPU.'
    return $result
}

function Export-ResourceSamples($Session, [string]$OutputDirectory) {
    Export-InvariantMetricCsv $Session.Samples (Join-Path $OutputDirectory 'process-samples.csv')
    Export-InvariantMetricCsv $Session.Threads (Join-Path $OutputDirectory 'thread-samples.csv')
    [ordered]@{
        samplingIntervalMs = $Session.SampleIntervalMs
        clock = 'System.Diagnostics.Stopwatch monotonic; milliseconds from session OriginTicks; independent observation span for each process/thread'
        sampleTimestamp = 'midpoint of sampleStartMs and sampleEndMs; counters are not an atomic snapshot'
        samplerWallMs = $Session.SamplerWallMs
        samplerCpuMs = $Session.SamplerCpuMs
        overheadScope = 'complete Add-ResourceSample reader and row construction in harness process; excludes CSV serialization, initial Add-Type compilation and scheduler wait; CPU has OS counter resolution'
        samples = $Session.Samples.Count
        threadSamples = $Session.Threads.Count
        availableLogicalProcessors = [Environment]::ProcessorCount
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'sampling.json')
}
