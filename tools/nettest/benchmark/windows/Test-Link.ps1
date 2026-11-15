# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

[CmdletBinding()]
param(
    [string]$Iperf,
    [string]$Server,
    [ValidateRange(1,65535)][int]$Port = 5201,
    [ValidateSet('tcp','udp')][string]$Protocol = 'tcp',
    [string]$TargetsMbps = '100,1000,0',
    [ValidateRange(1,3600)][int]$Duration = 30,
    [ValidateRange(1,16)][int]$Parallel = 1,
    [string]$Adapter,
    [string]$OutputDirectory,
    [switch]$Reverse,
    [switch]$Help
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Measure a physical link with an operator-supplied iperf3 and an already running remote iperf3 server.'
    Write-Output 'Usage: Test-Link.ps1 -Iperf FILE -Server ADDRESS -OutputDirectory NEW_DIR'
    Write-Output '  [-Port 5201] [-Protocol tcp|udp] [-TargetsMbps 100,1000,0] [-Duration 30] [-Parallel 1]'
    Write-Output '  [-Adapter NAME] [-Reverse]'
    Write-Output 'A target of 0 means an uncapped TCP run. UDP requires positive target rates.'
    Write-Output 'Each target applies per parallel stream. Adapter statistics observe an interface and do not bind traffic.'
    Write-Output 'Writes iperf3 JSON, stderr, a summary CSV and optional before/after interface counters to a new directory.'
    Write-Output 'Exit 0 for successful runs, nonzero for arguments, process failures or invalid results.'
    exit 0
}
foreach ($name in 'Iperf','Server','OutputDirectory') {
    if (!$PSBoundParameters[$name]) { throw "Missing -$name. Use -Help for usage." }
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be new.' }
$Iperf = (Resolve-Path -LiteralPath $Iperf).Path
$targets = @($TargetsMbps.Split(',') | ForEach-Object {
    $value = 0
    if (![int]::TryParse($_.Trim(), [ref]$value) -or $value -lt 0 -or $value -gt 1000000) {
        throw "Invalid Mbps target: $_"
    }
    if ($Protocol -eq 'udp' -and $value -eq 0) { throw 'UDP requires a positive target rate.' }
    $value
})
if ($targets.Count -eq 0 -or $targets.Count -gt 20) { throw 'Supply 1..20 targets.' }
if ($Adapter) {
    $selected = @(Get-NetAdapter -Name $Adapter -ErrorAction Stop)
    if ($selected.Count -ne 1) { throw 'Adapter must identify exactly one interface.' }
}
$version = & $Iperf --version
$output = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
$metadata = [ordered]@{
    capturedUtc = [DateTime]::UtcNow.ToString('o')
    server = $Server
    port = $Port
    protocol = $Protocol
    durationSeconds = $Duration
    parallelStreams = $Parallel
    reverse = [bool]$Reverse
    iperfSha256 = (Get-FileHash -LiteralPath $Iperf).Hash
    iperfVersion = $version -join [char]10
    adapter = if ($Adapter) { $selected | Select-Object Name,InterfaceDescription,Status,LinkSpeed } else { $null }
}
$metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'metadata.json')
$rows = [Collections.Generic.List[object]]::new()
$run = 0
foreach ($target in $targets) {
    ++$run
    $name = '{0:D3}-{1}Mbps' -f $run,$target
    if ($Adapter) {
        Get-NetAdapterStatistics -Name $Adapter | Select-Object Name,ReceivedBytes,SentBytes,ReceivedDiscardedPackets,OutboundDiscardedPackets |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output "$name-adapter-before.json")
    }
    $arguments = @('-c',$Server,'-p',"$Port",'-t',"$Duration",'-P',"$Parallel",'-J')
    if ($Protocol -eq 'udp') { $arguments += '-u' }
    if ($target -ne 0) { $arguments += @('-b',"${target}M") }
    if ($Reverse) { $arguments += '-R' }
    $jsonPath = Join-Path $output "$name.json"
    $errorPath = Join-Path $output "$name.stderr"
    & $Iperf @arguments 1> $jsonPath 2> $errorPath
    if ($Adapter) {
        Get-NetAdapterStatistics -Name $Adapter | Select-Object Name,ReceivedBytes,SentBytes,ReceivedDiscardedPackets,OutboundDiscardedPackets |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output "$name-adapter-after.json")
    }
    $result = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
    if ($result.error -or !$result.end) { throw "iperf3 did not produce a successful result: $jsonPath" }
    $sent = $result.end.sum_sent.bits_per_second
    $received = $result.end.sum_received.bits_per_second
    foreach ($value in @($sent,$received)) {
        if ($null -eq $value -or $value -isnot [ValueType] -or ![double]::IsFinite([double]$value) -or $value -lt 0) {
            throw "iperf3 must report numeric sender and receiver rates in end.sum_sent and end.sum_received: $jsonPath"
        }
    }
    $lostPercent = $null
    $jitterMilliseconds = $null
    if ($Protocol -eq 'udp') {
        $lostPercent = $result.end.sum_received.lost_percent
        $jitterMilliseconds = $result.end.sum_received.jitter_ms
        foreach ($value in @($lostPercent,$jitterMilliseconds)) {
            if ($null -eq $value -or $value -isnot [ValueType] -or ![double]::IsFinite([double]$value) -or $value -lt 0) {
                throw "iperf3 must report receiver loss and jitter in end.sum_received: $jsonPath"
            }
        }
        if ($lostPercent -gt 100) { throw "Invalid UDP loss percentage: $jsonPath" }
    }
    $rows.Add([pscustomobject]@{
        run = $name
        protocol = $Protocol
        targetMbpsPerStream = $target
        parallelStreams = $Parallel
        senderBitsPerSecond = $sent
        receiverBitsPerSecond = $received
        udpLostPercent = $lostPercent
        udpJitterMilliseconds = $jitterMilliseconds
    })
    $rows | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $output 'summary.csv')
}
Write-Output "Completed $run link tests. Summary: $(Join-Path $output 'summary.csv')"
