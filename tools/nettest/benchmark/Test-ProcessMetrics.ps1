# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2
[CmdletBinding()]
param([string]$OutputDirectory, [switch]$Help, [switch]$HelpRu)
$ErrorActionPreference = 'Stop'
if ($HelpRu) {
    Write-Output 'Test-ProcessMetrics.ps1 -OutputDirectory НОВЫЙ_КАТАЛОГ'
    Write-Output 'Проверяет CPU процессов/потоков и память на собственном конечном дочернем pwsh в Windows/Linux.'
    Write-Output 'Сохраняет CSV, JSON, Prometheus и журналы; проверяет пересчёт из CSV, недоступные значения и очистку при ошибке.'
    Write-Output 'Требуется PowerShell 7.2+. Системные настройки не меняет; завершает только созданные процессы.'
    Write-Output 'Без параметров, -Help и -HelpRu выводят справку без запуска нагрузки. Код 0: успех, 1: ошибка.'
    exit 0
}
if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Test-ProcessMetrics.ps1 -OutputDirectory NEW_DIRECTORY'
    Write-Output 'Verify process/thread CPU and memory with an owned finite child pwsh on Windows/Linux.'
    Write-Output 'Retains CSV, JSON, Prometheus and logs; checks CSV reconciliation, unavailable observations and failure cleanup.'
    Write-Output 'Requires PowerShell 7.2+. Changes no system settings and stops only processes it creates.'
    Write-Output 'No arguments, -Help and -HelpRu print help without launching work. Exit 0: success; 1: failure.'
    exit 0
}
if (!$OutputDirectory -or (Test-Path -LiteralPath $OutputDirectory)) { throw 'Supply a new -OutputDirectory.' }
$output = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
. (Join-Path $PSScriptRoot 'ProcessMetrics.ps1')
. (Join-Path $PSScriptRoot 'MetricsExport.ps1')
function Assert-Resource([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
}
function Start-ResourceWorkload {
    $pwsh = Join-Path $PSHOME $(if ($IsWindows) { 'pwsh.exe' } else { 'pwsh' })
    $info = [Diagnostics.ProcessStartInfo]::new($pwsh)
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in @('-NoLogo','-NoProfile','-File',(Join-Path $PSScriptRoot 'ResourceMetricsChild.ps1'))) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($info)
    return @{ Process = $process; Name = 'producer'; ErrorTask = $process.StandardError.ReadToEndAsync() }
}
function Stop-ResourceWorkload($Child, [string]$LogName) {
    try {
        if (!$Child.Process.HasExited) { $Child.Process.Kill($true) }
        Assert-Resource ($Child.Process.WaitForExit(5000)) 'Owned child failed to exit.'
        $Child.CleanupExited = $true
        Assert-Resource ($Child.ErrorTask.Wait(3000)) 'Owned child stderr failed to close.'
        $Child.ErrorTask.GetAwaiter().GetResult() | Set-Content (Join-Path $output "$LogName.stderr")
    } finally { $Child.Process.Dispose() }
}
# Finalization must stop the child even if CSV export or cleanup-log writing fails.
# Preserve the export exception as the primary error and attach secondary diagnostics.
function Complete-ResourceWorkload($Session, $Child, [string]$Directory, [string]$LogName) {
    $exportError = $null
    try {
        Export-ResourceSamples $Session $Directory
    } catch {
        $exportError = $_
        throw
    } finally {
        try { Stop-ResourceWorkload $Child $LogName } catch {
            if ($null -eq $exportError) { throw }
            $exportError.Exception.Data['cleanupError'] = $_.Exception.Message
        }
    }
}
$session = New-ResourceSession -SampleIntervalMs 50
$child = Start-ResourceWorkload
try {
    Register-ResourceChild $session $child
    $ready = $child.Process.StandardOutput.ReadLineAsync()
    Assert-Resource ($ready.Wait(15000) -and $ready.Result -ceq 'READY') 'Workload did not become ready.'
    Add-ResourceSample $session $child startup
    $done = $child.Process.StandardOutput.ReadLineAsync()
    $child.Process.StandardInput.WriteLine('GO')
    $deadline = $session.Clock.ElapsedMilliseconds + 10000
    do {
        Add-ResourceSample $session $child active
        $null = $done.Wait($session.SampleIntervalMs)
    } while (!$done.IsCompleted -and $session.Clock.ElapsedMilliseconds -lt $deadline)
    Assert-Resource ($done.IsCompleted -and $done.Result -ceq 'DONE') 'Workload failed to complete.'
    Add-ResourceSample $session $child drained
    Start-Sleep -Milliseconds 60
    Add-ResourceSample $session $child drained
    $summary = Get-ResourceSummary $session $child
    $drained = Get-ResourceSummary $session $child drained
    Assert-Resource ($summary.status -eq 'ok' -and $summary.averageCpuCores -gt 0) 'Missing measured process CPU.'
    Assert-Resource ($summary.finalWorkingSetBytes -gt 0 -and $summary.osPeakWorkingSetBytes -ge $summary.finalWorkingSetBytes) 'Missing resident memory/OS lifetime peak.'
    Assert-Resource ($drained.phase -eq 'drained' -and $drained.finalWorkingSetBytes -gt 0) 'Missing final live drained sample.'
    Export-ResourceSamples $session $output
    $busy = @($summary.threads | Where-Object { $_.threadName -eq 'metrics-busy' -and $_.status -eq 'ok' })
    $idle = @($summary.threads | Where-Object { $_.threadName -eq 'metrics-idle' -and $_.status -eq 'ok' })
    Assert-Resource ($busy.Count -eq 1 -and $busy[0].averageCpuCores -gt 0.05) 'Busy thread CPU attribution failed.'
    Assert-Resource ($idle.Count -eq 1 -and $idle[0].averageCpuCores -lt $busy[0].averageCpuCores) 'Idle thread CPU attribution failed.'
    # Two different creation identities with the same TID must never share a CPU delta.
    $reuse = New-ResourceSession
    foreach ($sample in $session.Samples) { $reuse.Samples.Add($sample) }
    foreach ($identity in @('first-lifetime','second-lifetime')) {
        foreach ($tick in @(0,100)) {
            $reuse.Threads.Add([pscustomobject]@{ process='producer'; phase='active'; pid=$child.Process.Id
                processIdentity=$child.ResourceIdentity; tid=42; threadIdentity=$identity; threadName='reused'
                elapsedMs=$tick; userCpuMs=($tick / 2); systemCpuMs=0; status='ok' })
        }
    }
    $reuseSummary = Get-ResourceSummary $reuse $child
    Assert-Resource ($reuseSummary.threads.Count -eq 2 -and @($reuseSummary.threads | Where-Object samples -eq 2).Count -eq 2) 'Reused thread identities were combined.'
    if ($IsLinux) { Assert-Resource ($null -eq $summary.sampledPeakPrivateBytes -and $summary.privateBytesStatus.StartsWith('unavailable:')) 'Linux private bytes must remain unavailable.' }
    $result = [pscustomobject]@{
        producerKind = 'Event'; contract = 'STREAM'; clients = 1; offeredRps = 10; nominalRecords = 10
        sender = @{ admitted = 10 }; totalDelivered = 10; deliveredIncludingDrainPerProducerSecond = 10
        precision = 'ns'; pacing = 'sleep'; receivers = @(@{ latencyP95Ns = 1000000 })
        processes = @{ producer = $summary; receivers = @($summary) }
    }
    $row = New-NettestSummaryRow $result 'finite-child'
    Export-InvariantMetricCsv @($row) (Join-Path $output 'summary.csv')
    $result | ConvertTo-Json -Depth 9 | Set-Content (Join-Path $output 'result.json')
    $drained | ConvertTo-Json -Depth 9 | Set-Content (Join-Path $output 'drained.json')
    $metrics = ConvertTo-NettestPrometheus $result $row
    $metrics | Set-Content (Join-Path $output 'metrics.prom') -NoNewline
    Export-ResourceSamples $session $output
    $raw = @(Import-Csv (Join-Path $output 'process-samples.csv') | Where-Object { $_.phase -eq 'active' -and $_.status -eq 'ok' })
    $culture = [Globalization.CultureInfo]::InvariantCulture
    $cpu = (([double]::Parse($raw[-1].userCpuMs, $culture) - [double]::Parse($raw[0].userCpuMs, $culture)) +
        ([double]::Parse($raw[-1].systemCpuMs, $culture) - [double]::Parse($raw[0].systemCpuMs, $culture))) / 1000
    $csv = Import-Csv (Join-Path $output 'summary.csv')
    Assert-Resource ([Math]::Abs([double]::Parse($csv.producerCpuSeconds, $culture) - $cpu) -lt 1e-9) 'CSV CPU differs from raw deltas.'
    $line = @($metrics.Split("`n") | Where-Object { $_.StartsWith('qdcxx_nettest_producer_cpu_seconds{') })[0]
    $metricCpu = [double]::Parse($line.Substring($line.LastIndexOf(' ') + 1), $culture)
    Assert-Resource ([Math]::Abs($metricCpu - $cpu) -lt 1e-9) 'Dashboard CPU differs from raw CSV.'
    $threadRaw = @(Import-Csv (Join-Path $output 'thread-samples.csv') | Where-Object { $_.phase -eq 'active' -and $_.threadName -eq 'metrics-busy' -and $_.status -eq 'ok' })
    $threadCpu = (([double]::Parse($threadRaw[-1].userCpuMs, $culture) - [double]::Parse($threadRaw[0].userCpuMs, $culture)) +
        ([double]::Parse($threadRaw[-1].systemCpuMs, $culture) - [double]::Parse($threadRaw[0].systemCpuMs, $culture))) / 1000
    $threadLine = @($metrics.Split("`n") | Where-Object { $_.StartsWith('qdcxx_nettest_thread_cpu_seconds{') -and $_.Contains('role="producer"') -and $_.Contains('thread_name="metrics-busy"') })[0]
    Assert-Resource ([Math]::Abs([double]::Parse($threadLine.Substring($threadLine.LastIndexOf(' ') + 1), $culture) - $threadCpu) -lt 1e-9) 'Dashboard thread CPU differs from raw CSV.'
    $peak = ($raw | ForEach-Object { [long]::Parse($_.workingSetBytes, $culture) } | Measure-Object -Maximum).Maximum
    $peakLine = @($metrics.Split("`n") | Where-Object { $_.StartsWith('qdcxx_nettest_producer_peak_working_set_bytes{') })[0]
    Assert-Resource ([double]::Parse($peakLine.Substring($peakLine.LastIndexOf(' ') + 1), $culture) -eq $peak) 'Dashboard peak differs from raw CSV.'
    $cpuSeconds = ([double]::Parse($raw[-1].elapsedMs, $culture) - [double]::Parse($raw[0].elapsedMs, $culture)) / 1000
    $threadSeconds = ([double]::Parse($threadRaw[-1].elapsedMs, $culture) - [double]::Parse($threadRaw[0].elapsedMs, $culture)) / 1000
    $expectedMetrics = @{
        qdcxx_nettest_process_average_cpu_cores = $cpu / $cpuSeconds
        qdcxx_nettest_process_final_working_set_bytes = [double]::Parse($raw[-1].workingSetBytes, $culture)
        qdcxx_nettest_thread_average_cpu_cores = $threadCpu / $threadSeconds
    }
    foreach ($entry in $expectedMetrics.GetEnumerator()) {
        $matching = @($metrics.Split("`n") | Where-Object {
            $_.StartsWith($entry.Key + '{') -and $_.Contains('role="producer"') -and
            ($entry.Key -notlike '*thread*' -or $_.Contains('thread_name="metrics-busy"'))
        })
        Assert-Resource ($matching.Count -eq 1) "Missing dashboard metric $($entry.Key)."
        $value = [double]::Parse($matching[0].Substring($matching[0].LastIndexOf(' ') + 1), $culture)
        Assert-Resource ([Math]::Abs($value - $entry.Value) -lt 1e-9) "Dashboard $($entry.Key) differs from raw CSV."
    }
    $dashboard = Get-Content (Join-Path $PSScriptRoot 'windows/dashboard/grafana/dashboard.json') -Raw | ConvertFrom-Json
    foreach ($panel in $dashboard.panels) {
        foreach ($target in $panel.targets) {
            Assert-Resource (@($metrics.Split("`n") | Where-Object { $_.StartsWith($target.expr + '{') }).Count -gt 0) "Dashboard expression has no exported metric: $($target.expr)"
        }
    }
    $child.Process.StandardInput.WriteLine('STOP')
    Assert-Resource ($child.Process.WaitForExit(5000) -and $child.Process.ExitCode -eq 0) 'Workload did not exit successfully.'
    $before = $session.Samples.Count
    Add-ResourceSample $session $child afterExit
    Assert-Resource ($session.Samples.Count -eq $before) 'Exited process produced a fabricated sample.'
} finally {
    Complete-ResourceWorkload $session $child $output 'finite-child'
}

# Exercise exceptional caller cleanup against another owned child, without PID-based kills.
$failed = Start-ResourceWorkload
$failureProcessId = $failed.Process.Id
$injected = $false
try {
    try {
        $ready = $failed.Process.StandardOutput.ReadLineAsync()
        Assert-Resource ($ready.Wait(15000) -and $ready.Result -ceq 'READY') 'Failure-path workload did not become ready.'
        throw 'Injected measurement failure'
    } finally { Stop-ResourceWorkload $failed 'failure-child' }
} catch {
    if ($_.Exception.Message -ne 'Injected measurement failure') { throw }
    $injected = $true
}
Assert-Resource $injected 'Injected failure was not observed after cleanup.'
Assert-Resource ($null -eq (Get-Process -Id $failureProcessId -ErrorAction SilentlyContinue)) 'Owned child survived failure cleanup.'
# A real CSV I/O error must still terminate the owned child. Also fail the stderr
# write to ensure a secondary cleanup error cannot replace the original CSV error.
$exportDirectory = (New-Item -ItemType Directory -Path (Join-Path $output 'export-failure')).FullName
$null = New-Item -ItemType Directory -Path (Join-Path $exportDirectory 'process-samples.csv')
$null = New-Item -ItemType Directory -Path (Join-Path $output 'export-failure.stderr')
$exportSession = New-ResourceSession
$exportChild = Start-ResourceWorkload
$exportProcessId = $exportChild.Process.Id
$exportErrorObserved = $false
try {
    $ready = $exportChild.Process.StandardOutput.ReadLineAsync()
    Assert-Resource ($ready.Wait(15000) -and $ready.Result -ceq 'READY') 'CSV-failure workload did not become ready.'
    Register-ResourceChild $exportSession $exportChild
    Add-ResourceSample $exportSession $exportChild active
    try {
        Complete-ResourceWorkload $exportSession $exportChild $exportDirectory 'export-failure'
        throw 'Expected CSV export failure did not occur.'
    } catch {
        Assert-Resource ($_.Exception.Message.Contains('process-samples.csv')) 'Cleanup replaced the original CSV export error.'
        Assert-Resource ($_.Exception.Data.Contains('cleanupError')) 'Secondary cleanup-log error was not retained.'
        Assert-Resource ([bool]$exportChild.CleanupExited) 'CSV export failure skipped owned-child termination.'
        $exportErrorObserved = $true
    }
} finally {
    # Keep the regression itself safe when the behavior under test is broken.
    if (!$exportChild.CleanupExited) { Stop-ResourceWorkload $exportChild 'export-failure-fallback' }
}
Assert-Resource $exportErrorObserved 'CSV export failure was not observed.'
Assert-Resource ($null -eq (Get-Process -Id $exportProcessId -ErrorAction SilentlyContinue)) 'Owned child survived CSV export failure.'
# Numeric CSV must reconcile with JSON/Prometheus even with a comma-decimal locale.
$originalCulture = [Globalization.CultureInfo]::CurrentCulture
try {
    [Globalization.CultureInfo]::CurrentCulture = [Globalization.CultureInfo]::GetCultureInfo('fr-FR')
    Export-InvariantMetricCsv @([pscustomobject]@{ userCpuMs = 1250.5; privateBytes = $null }) (Join-Path $output 'locale.csv')
} finally { [Globalization.CultureInfo]::CurrentCulture = $originalCulture }
$locale = Import-Csv (Join-Path $output 'locale.csv')
Assert-Resource ($locale.userCpuMs -ceq '1250.5' -and $locale.privateBytes -ceq '') 'Locale changed CSV decimal units or unavailable memory.'
$unavailable = Get-ResourceCpuSummary @()
Assert-Resource ($null -eq (Get-NettestCpuSeconds $unavailable)) 'Unavailable CPU was converted to zero.'
Assert-Resource ((ConvertTo-NettestMetricNumber $null) -ceq 'NaN') 'Unavailable metric was converted to zero.'
$regressed = Get-ResourceCpuSummary @(@{ elapsedMs = 0; userCpuMs = 2; systemCpuMs = 0 }, @{ elapsedMs = 100; userCpuMs = 1; systemCpuMs = 0 })
Assert-Resource ($regressed.userCpuSeconds -lt 0 -and $regressed.status.StartsWith('counter-regressed:')) 'Signed counter regression was hidden.'
Write-Output "Passed process/thread CPU, memory, raw CSV/dashboard reconciliation and owned-child cleanup: $output"
