# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

[CmdletBinding()]
param(
    [ValidateSet('Event','Record')][string]$ProducerKind = 'Event',
    [ValidateSet('stream','ticker')][string]$Contract = 'stream',
    [ValidateSet('sleep','yield')][string]$Pacing = 'sleep',
    [ValidateRange(1,8)][int]$Clients = 1,
    [ValidateRange(1,300)][int]$Duration = 10,
    [ValidateRange(1,10)][int]$Repeats = 1,
    [string]$Rates = '1000,10000,100000',
    [string]$ListenAddress = '127.0.0.1',
    [string]$OutputDirectory,
    [string]$PushgatewayUrl,
    [ValidateRange(10,10000)][int]$SampleIntervalMs = 100,
    [switch]$Help
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Run serial synthetic QTP throughput and delivery measurements on Windows.'
    Write-Output 'Usage: run.cmd -OutputDirectory NEW_DIR [-ProducerKind Event|Record] [-Contract stream|ticker]'
    Write-Output '  [-Clients 1..8] [-Rates 1000,10000,100000] [-Duration 1..300] [-Repeats 1..10]'
    Write-Output '  [-ListenAddress NUMERIC_IP] [-Pacing sleep|yield] [-PushgatewayUrl URL]'
    Write-Output 'Default: Event, STREAM, one client, 10 s per run, one repeat, 127.0.0.1.'
    Write-Output 'Writes system baseline, per-run JSON and process CSV, and summary.csv to a new directory.'
    Write-Output '-SampleIntervalMs 100 (10..10000) controls process/thread sampling; every run saves metrics.prom.'
    Write-Output 'The optional Pushgateway receives one snapshot after each completed run.'
    Write-Output 'Sequence, client, source and schema limits apply. See README.md for scope and measurements.'
    Write-Output 'Exit 0 when all runs pass, nonzero on configuration, start, validation or measurement failure.'
    exit 0
}
if (!$OutputDirectory) { throw 'Supply -OutputDirectory. Use -Help for usage.' }
$Contract = $Contract.ToLowerInvariant()
$Pacing = $Pacing.ToLowerInvariant()
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be new.' }
if ($ProducerKind -eq 'Record' -and ($Clients -ne 1 -or $Contract -ne 'stream' -or $Pacing -ne 'sleep')) {
    throw 'Record mode supports one STREAM receiver and default pacing.'
}
$rateValues = @($Rates.Split(',') | ForEach-Object {
    $value = 0
    if (![int]::TryParse($_.Trim(), [ref]$value)) { throw "Invalid rate: $_" }
    $value
})
if ($rateValues.Count -eq 0 -or $rateValues.Count -gt 20) { throw 'Supply 1..20 rates.' }
foreach ($rate in $rateValues) {
    if ($rate -lt 1 -or $rate -gt 1000000 -or ([long]$Duration * $rate + 1) -ge (1 -shl 22)) {
        throw "Rate $rate is outside the runner limits for this duration."
    }
}
if ($PushgatewayUrl) {
    $url = [uri]$PushgatewayUrl
    if (!$url.IsAbsoluteUri -or $url.Scheme -notin @('http','https')) {
        throw 'PushgatewayUrl must be an absolute http or https address.'
    }
}

$runner = Join-Path $PSScriptRoot 'tools/Measure-Stream.ps1'
$java = Join-Path $PSScriptRoot 'java/bin/java.exe'
$programName = if ($ProducerKind -eq 'Event') { 'bin/qdcxx-event-nettest.exe' } else { 'bin/qdcxx-nettest.exe' }
$program = Join-Path $PSScriptRoot $programName
$jars = @('qds.jar','dxlib.jar','dxfeed-api.jar' | ForEach-Object { Join-Path $PSScriptRoot "lib/$_" })
foreach ($path in @($runner,$java,$program) + $jars) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Bundle file missing: $path" }
}
. (Join-Path $PSScriptRoot 'tools/MetricsExport.ps1')
$output = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
$baseline = [ordered]@{
    capturedUtc = [DateTime]::UtcNow.ToString('o')
    operatingSystem = [Runtime.InteropServices.RuntimeInformation]::OSDescription
    architecture = [string][Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture
    logicalProcessors = [Environment]::ProcessorCount
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name,LoadPercentage,NumberOfCores,NumberOfLogicalProcessors)
    memory = @(Get-CimInstance Win32_OperatingSystem | Select-Object TotalVisibleMemorySize,FreePhysicalMemory)
    adapters = @(Get-NetAdapter -ErrorAction SilentlyContinue | Select-Object Name,InterfaceDescription,Status,LinkSpeed)
}
$baseline | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $output 'baseline.json')
$rows = [Collections.Generic.List[object]]::new()
$classpath = $jars -join [IO.Path]::PathSeparator
$run = 0
foreach ($rate in $rateValues) {
    foreach ($repeat in 1..$Repeats) {
        ++$run
        $runName = '{0:D3}-{1}-{2}' -f $run,$rate,$repeat
        $resultDirectory = Join-Path $output $runName
        $parameters = @{
            Program = $program
            ProducerKind = $ProducerKind
            Java = $java
            Classpath = $classpath
            OutputDirectory = $resultDirectory
            Duration = $Duration
            Rate = $rate
            Batch = 1
            Clients = $Clients
            Contract = $Contract
            ListenAddress = $ListenAddress
            Pacing = $Pacing
            SampleIntervalMs = $SampleIntervalMs
        }
        if ($ProducerKind -eq 'Record') { $parameters.Bundle = Join-Path $PSScriptRoot 'schema' }
        & $runner @parameters
        $j = Get-Content -LiteralPath (Join-Path $resultDirectory 'result.json') -Raw | ConvertFrom-Json
        $row = New-NettestSummaryRow $j $runName
        $rows.Add($row)
        Export-InvariantMetricCsv $rows (Join-Path $output 'summary.csv')

        $metrics = ConvertTo-NettestPrometheus $j $row
        $metrics | Set-Content -LiteralPath (Join-Path $resultDirectory 'metrics.prom') -NoNewline
        if ($PushgatewayUrl) {
            $target = $PushgatewayUrl.TrimEnd('/') + '/metrics/job/qd_cxx_nettest'
            Invoke-RestMethod -Method Put -Uri $target -ContentType 'text/plain; version=0.0.4; charset=utf-8' -Body $metrics | Out-Null
        }
    }
}
Write-Output "Completed $run runs. Summary: $(Join-Path $output 'summary.csv')"
