# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2
[CmdletBinding()]
param(
    [ValidateSet('Send','Receive')][string]$Mode,
    [string]$Directory,
    [string]$Group,
    [ValidateRange(1,65535)][int]$Port,
    [string]$Interface,
    [string]$Output,
    [ValidateRange(100,600000)][int]$TimeoutMs = 30000,
    [switch]$Help,
    [switch]$HelpRu
)
$ErrorActionPreference = 'Stop'
if ($HelpRu) {
    Write-Output 'Передаёт или принимает один синтетический всплеск по IPv4 multicast.'
    Write-Output 'Использование: Multicast.ps1 -Mode Send|Receive -Directory КОРПУС -Group 239.X.X.X -Port N -Interface IPV4 -Output НОВЫЙ_JSON [-TimeoutMs 30000]'
    Write-Output 'Сначала проверяет корпус и загружает его в память. TTL=1. Получатель печатает READY после вступления в группу.'
    Write-Output 'Отчёт содержит фактические отправки в окне, опоздание, доставку, пропуски, дубли, перестановки и ошибки данных.'
    Write-Output 'Для измерения физической сети необходимы отдельные машины. Код 0 означает выполнение, ненулевой код означает ошибку.'
    exit 0
}
if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Send or receive one synthetic burst using IPv4 multicast.'
    Write-Output 'Usage: Multicast.ps1 -Mode Send|Receive -Directory CORPUS -Group 239.X.X.X -Port N -Interface IPV4 -Output NEW_JSON [-TimeoutMs 30000]'
    Write-Output 'Validates and preloads the corpus. TTL=1. The receiver prints READY after joining the group.'
    Write-Output 'The receipt includes actual in-window sends, lateness, delivery, gaps, duplicates, reordering and content errors.'
    Write-Output 'Separate hosts are required to measure a physical network. Exit 0 means the operation ran; nonzero means an error.'
    exit 0
}
if (!$Mode -or !$Directory -or !$Group -or !$Port -or !$Interface -or !$Output) {
    throw 'Supply all required parameters. Use -Help for usage.'
}
$groupIp = $null
$interfaceIp = $null
if (![Net.IPAddress]::TryParse($Group, [ref]$groupIp) -or
    $groupIp.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork -or $groupIp.GetAddressBytes()[0] -ne 239) {
    throw 'Group must be an IPv4 address in 239.0.0.0/8.'
}
if (![Net.IPAddress]::TryParse($Interface, [ref]$interfaceIp) -or
    $interfaceIp.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork -or
    $interfaceIp.Equals([Net.IPAddress]::Any) -or $interfaceIp.GetAddressBytes()[0] -ge 224) {
    throw 'Interface must be a concrete IPv4 address.'
}
$corpusDirectory = (Resolve-Path -LiteralPath $Directory).Path
if (Test-Path -LiteralPath $Output) { throw 'Output must be a new file.' }
$outputPath = [IO.Path]::GetFullPath($Output, (Get-Location).Path)
if (!(Test-Path -LiteralPath ([IO.Path]::GetDirectoryName($outputPath)) -PathType Container)) {
    throw 'Output parent directory must exist.'
}
& (Join-Path $PSScriptRoot '../burst/Corpus.ps1') -Mode Validate -Directory $corpusDirectory | Out-Null
$manifest = Get-Content -LiteralPath (Join-Path $corpusDirectory 'manifest.json') -Raw | ConvertFrom-Json
if (-not ('BurstMulticastV1' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'Multicast.cs') }
$inputPath = Join-Path $corpusDirectory 'corpus.bin'
$loaded = [BurstMulticastV1]::Load($inputPath, [int]$manifest.datagrams, [int]$manifest.messages, [int]$manifest.windowMs, [string]$manifest.sha256)
if ($Mode -eq 'Receive') {
    $result = [BurstMulticastV1]::Receive($loaded, $groupIp, $interfaceIp, $Port, $TimeoutMs)
} else {
    $result = [BurstMulticastV1]::Send($loaded, $groupIp, $interfaceIp, $Port)
}
$receipt = [ordered]@{
    format = 'qdcxx-burst-multicast-v1'
    mode = $Mode.ToLowerInvariant()
    corpusSha256 = $manifest.sha256
    group = $groupIp.ToString()
    port = $Port
    interface = $interfaceIp.ToString()
    measurement = $result
}
$bytes = [Text.UTF8Encoding]::new($false).GetBytes(($receipt | ConvertTo-Json -Depth 5) + [Environment]::NewLine)
$stream = [IO.File]::Open($outputPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
Write-Output "Receipt: $outputPath"
