# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

# Internal Run-Sweep.ps1 helper. Inputs are a validated Measure-Stream result and its
# run name; outputs are a flat CSV row and Prometheus snapshot text. Missing resource
# observations remain null in CSV and NaN in Prometheus. No I/O or network side effects.
function Get-NettestCpuSeconds($Process) {
    if ($null -eq $Process.userCpuSeconds -or $null -eq $Process.systemCpuSeconds) { return $null }
    return $Process.userCpuSeconds + $Process.systemCpuSeconds
}

function New-NettestSummaryRow($Result, [string]$RunName) {
    [pscustomobject][ordered]@{
        run = $RunName
        producer = $Result.producerKind
        contract = $Result.contract
        clients = $Result.clients
        targetRecordsPerSecond = $Result.offeredRps
        nominalRecords = $Result.nominalRecords
        admitted = if ($Result.producerKind -eq 'Event') { $Result.sender.admitted } else { $Result.sender.written }
        deliveredTotal = $Result.totalDelivered
        deliveredPerSecondTotal = $Result.deliveredIncludingDrainPerProducerSecond
        producerCpuSeconds = Get-NettestCpuSeconds $Result.processes.producer
        producerPeakWorkingSetBytes = $Result.processes.producer.sampledPeakWorkingSetBytes
        producerPeakPrivateBytes = $Result.processes.producer.sampledPeakPrivateBytes
        precision = $Result.precision
        pacing = $Result.pacing
        producerAverageCpuCores = $Result.processes.producer.averageCpuCores
        producerCpuIntervalSeconds = $Result.processes.producer.elapsedSeconds
        producerFinalWorkingSetBytes = $Result.processes.producer.finalWorkingSetBytes
        producerFinalPrivateBytes = $Result.processes.producer.finalPrivateBytes
        producerOsPeakWorkingSetBytes = $Result.processes.producer.osPeakWorkingSetBytes
    }
}

function ConvertTo-NettestMetricNumber($Value) {
    if ($null -eq $Value) { return 'NaN' }
    return ([double]$Value).ToString('R', [Globalization.CultureInfo]::InvariantCulture)
}

function ConvertTo-NettestMetricLabel([string]$Value) {
    return $Value.Replace('\', '\\').Replace('"', '\"').Replace("`n", '\n')
}

function ConvertTo-NettestPrometheus($Result, $Row) {
    $labels = 'producer="' + (ConvertTo-NettestMetricLabel $Row.producer) + '",contract="' + $Row.contract.ToLowerInvariant() +
        '",clients="' + $Row.clients + '",pacing="' + $Row.pacing + '"'
    $metrics = [Collections.Generic.List[string]]::new()
    $gauges = [ordered]@{
        offered_records_per_second = $Row.targetRecordsPerSecond
        delivered_records_per_second = $Row.deliveredPerSecondTotal
        producer_cpu_seconds = $Row.producerCpuSeconds
        producer_peak_working_set_bytes = $Row.producerPeakWorkingSetBytes
    }
    foreach ($entry in $gauges.GetEnumerator()) {
        $name = 'qdcxx_nettest_' + $entry.Key
        $metrics.Add("# TYPE $name gauge")
        $metrics.Add($name + '{' + $labels + '} ' + (ConvertTo-NettestMetricNumber $entry.Value))
    }
    $metrics.Add('# TYPE qdcxx_nettest_receiver_latency_p95_seconds gauge')
    $metrics.Add('# TYPE qdcxx_nettest_receiver_cpu_seconds gauge')
    $metrics.Add('# TYPE qdcxx_nettest_receiver_peak_working_set_bytes gauge')
    for ($index = 0; $index -lt $Result.receivers.Count; ++$index) {
        $receiver = $Result.receivers[$index]
        $process = $Result.processes.receivers[$index]
        $clientLabels = $labels + ',client="' + ($index + 1) + '"'
        $p95 = if ($Result.precision -eq 'ns') {
            if ($null -ne $receiver.latencyP95Ns) { [double]$receiver.latencyP95Ns / 1e9 }
        } else {
            if ($null -ne $receiver.latencyP95Ms) { [double]$receiver.latencyP95Ms / 1e3 }
        }
        $metrics.Add('qdcxx_nettest_receiver_latency_p95_seconds{' + $clientLabels + '} ' + (ConvertTo-NettestMetricNumber $p95))
        $metrics.Add('qdcxx_nettest_receiver_cpu_seconds{' + $clientLabels + '} ' + (ConvertTo-NettestMetricNumber (Get-NettestCpuSeconds $process)))
        $metrics.Add('qdcxx_nettest_receiver_peak_working_set_bytes{' + $clientLabels + '} ' + (ConvertTo-NettestMetricNumber $process.sampledPeakWorkingSetBytes))
    }
    $resourceFields = [ordered]@{
        cpu_seconds = 'cpuSeconds'
        average_cpu_cores = 'averageCpuCores'
        cpu_interval_seconds = 'elapsedSeconds'
        final_working_set_bytes = 'finalWorkingSetBytes'
        sampled_peak_working_set_bytes = 'sampledPeakWorkingSetBytes'
        final_private_bytes = 'finalPrivateBytes'
        sampled_peak_private_bytes = 'sampledPeakPrivateBytes'
        os_peak_working_set_bytes = 'osPeakWorkingSetBytes'
    }
    foreach ($field in $resourceFields.Keys) { $metrics.Add("# TYPE qdcxx_nettest_process_$field gauge") }
    $metrics.Add('# TYPE qdcxx_nettest_thread_cpu_seconds gauge')
    $metrics.Add('# TYPE qdcxx_nettest_thread_average_cpu_cores gauge')
    $processes = @($Result.processes.producer) + @($Result.processes.receivers)
    for ($index = 0; $index -lt $processes.Count; ++$index) {
        $process = $processes[$index]
        $role = if ($index -eq 0) { 'producer' } else { 'receiver' }
        $processLabels = $labels + ',role="' + $role + '",client="' + $index + '"'
        foreach ($entry in $resourceFields.GetEnumerator()) {
            $value = if ($entry.Value -eq 'cpuSeconds') { Get-NettestCpuSeconds $process } else { $process.($entry.Value) }
            $metrics.Add('qdcxx_nettest_process_' + $entry.Key + '{' + $processLabels + '} ' + (ConvertTo-NettestMetricNumber $value))
        }
        foreach ($thread in $process.threads) {
            $threadLabels = $processLabels + ',tid="' + $thread.tid + '",thread_identity="' + $thread.threadIdentity +
                '",thread_name="' + (ConvertTo-NettestMetricLabel $thread.threadName) + '"'
            $metrics.Add('qdcxx_nettest_thread_cpu_seconds{' + $threadLabels + '} ' + (ConvertTo-NettestMetricNumber (Get-NettestCpuSeconds $thread)))
            $metrics.Add('qdcxx_nettest_thread_average_cpu_cores{' + $threadLabels + '} ' + (ConvertTo-NettestMetricNumber $thread.averageCpuCores))
        }
    }
    return ($metrics -join [char]10) + [char]10
}

# Shared by the samplers and sweeps. Export numeric cells with explicit invariant
# formatting so CSV, JSON and Prometheus describe the same values on every locale.
# Inputs are flat rows; output is one CSV file. I/O errors propagate to the caller.
function Export-InvariantMetricCsv($Rows, [string]$LiteralPath) {
    $Rows | ForEach-Object {
        $values = [ordered]@{}
        foreach ($property in $_.PSObject.Properties) {
            $value = $property.Value
            if ($value -is [IFormattable]) {
                $format = if ($value -is [double] -or $value -is [single]) { 'R' } else { $null }
                $value = $value.ToString($format, [Globalization.CultureInfo]::InvariantCulture)
            }
            $values[$property.Name] = $value
        }
        [pscustomobject]$values
    } | Export-Csv -NoTypeInformation -LiteralPath $LiteralPath
}