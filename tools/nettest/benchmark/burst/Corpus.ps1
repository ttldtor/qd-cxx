# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2
[CmdletBinding()]
param(
    [ValidateSet('Generate','Validate')][string]$Mode,
    [string]$Directory,
    [ValidateRange(1,2000000)][int]$Messages = 625000,
    [ValidateRange(1,600000)][int]$WindowMs = 100,
    [ValidateRange(128,1472)][int]$PayloadLimit = 1472,
    [uint32]$Seed = 1,
    [switch]$Help,
    [switch]$HelpRu
)

$ErrorActionPreference = 'Stop'
if ($HelpRu) {
    Write-Output 'Создаёт или проверяет синтетический корпус датаграмм и график выпуска.'
    Write-Output 'Использование: Corpus.ps1 -Mode Generate|Validate -Directory КАТАЛОГ'
    Write-Output 'Generate: -Messages 625000 (1..2000000), -WindowMs 100 (1..600000), -PayloadLimit 1472 (128..1472), -Seed 1.'
    Write-Output 'Generate создаёт новый каталог, corpus.bin и manifest.json. Validate проверяет все байты, размеры, расписание и SHA-256.'
    Write-Output 'Расписание задаёт предлагаемую нагрузку. Сетевой передачи команда не выполняет.'
    Write-Output 'Код 0: успех или справка. Ошибка ввода, записи или проверки завершается ненулевым кодом.'
    exit 0
}
if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Generate or validate a synthetic datagram corpus and its offered schedule.'
    Write-Output 'Usage: Corpus.ps1 -Mode Generate|Validate -Directory DIRECTORY'
    Write-Output 'Generate: -Messages 625000 (1..2000000), -WindowMs 100 (1..600000), -PayloadLimit 1472 (128..1472), -Seed 1.'
    Write-Output 'Generate creates a new directory containing corpus.bin and manifest.json. Validate checks all bytes, sizes, schedule and SHA-256.'
    Write-Output 'The schedule specifies an offered workload. No network transmission is performed.'
    Write-Output 'Exit 0 on success or help; input, I/O and validation failures return nonzero.'
    exit 0
}
if (!$Mode -or !$Directory) { throw 'Supply -Mode and -Directory. Use -Help for details.' }
if ($Mode -eq 'Validate' -and ($PSBoundParameters.ContainsKey('Messages') -or $PSBoundParameters.ContainsKey('WindowMs') -or
    $PSBoundParameters.ContainsKey('PayloadLimit') -or $PSBoundParameters.ContainsKey('Seed'))) {
    throw 'Validate reads the generation parameters from manifest.json.'
}

if (-not ('BurstCorpusV1' -as [type])) {
    Add-Type -Path (Join-Path $PSScriptRoot 'Corpus.cs')
}

if ($Mode -eq 'Generate') {
    if (Test-Path -LiteralPath $Directory) { throw 'Directory must be new.' }
    $output = (New-Item -ItemType Directory -Path $Directory).FullName
    $corpus = Join-Path $output 'corpus.bin'
    $summary = [BurstCorpusV1]::Generate($corpus, $Messages, $WindowMs, $PayloadLimit, $Seed)
    $manifest = [ordered]@{
        format = 'qdcxx-synthetic-burst-v1'
        messages = $Messages
        windowMs = $WindowMs
        udpPayloadLimit = $PayloadLimit
        seed = $Seed
        messageSizes = @(24,40,64,80)
        schedule = 'floor(firstMessageOrdinal * windowNs / messages)'
        messageOrdinals = 'zero-based'
        datagrams = $summary.Datagrams
        payloadBytes = $summary.PayloadBytes
        messageBytes = $summary.MessageBytes
        minPayloadBytes = $summary.SmallestPayloadBytes
        maxPayloadBytes = $summary.LargestPayloadBytes
        corpusBytes = $summary.FileBytes
        sha256 = (Get-FileHash -LiteralPath $corpus -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding utf8
    Write-Output "Generated $($manifest.messages) messages in $($manifest.datagrams) datagrams: $output"
    return
}

$output = (Resolve-Path -LiteralPath $Directory).Path
$manifestFile = Get-Item -LiteralPath (Join-Path $output 'manifest.json')
if ($manifestFile -isnot [IO.FileInfo] -or $manifestFile.Length -gt 16384) { throw 'Invalid manifest file size.' }
$manifest = Get-Content -LiteralPath $manifestFile.FullName -Raw | ConvertFrom-Json
$numericFields = @('messages', 'windowMs', 'udpPayloadLimit', 'seed', 'datagrams', 'payloadBytes', 'messageBytes',
    'minPayloadBytes', 'maxPayloadBytes', 'corpusBytes')
foreach ($field in $numericFields) {
    if ($manifest.$field -isnot [long]) { throw "Manifest field $field must be an integer JSON number." }
}
if ($manifest.messageSizes -isnot [array] -or $manifest.messageSizes.Count -ne 4) {
    throw 'Manifest messageSizes must be an array of four integer JSON numbers.'
}
foreach ($size in $manifest.messageSizes) {
    if ($size -isnot [long]) { throw 'Manifest messageSizes must contain integer JSON numbers.' }
}
if ($manifest.format -cne 'qdcxx-synthetic-burst-v1' -or $manifest.messages -lt 1 -or $manifest.messages -gt 2000000 -or
    $manifest.windowMs -lt 1 -or $manifest.windowMs -gt 600000 -or $manifest.udpPayloadLimit -lt 128 -or
    $manifest.udpPayloadLimit -gt 1472 -or $manifest.seed -lt 0 -or $manifest.seed -gt [uint32]::MaxValue -or
    $manifest.datagrams -lt 1 -or $manifest.datagrams -gt $manifest.messages -or
    $manifest.payloadBytes -lt 1 -or $manifest.payloadBytes -gt $manifest.datagrams * 1472 -or
    $manifest.messageBytes -lt 1 -or $manifest.messageBytes -gt $manifest.messages * 80 -or
    $manifest.minPayloadBytes -lt 1 -or $manifest.minPayloadBytes -gt $manifest.udpPayloadLimit -or
    $manifest.maxPayloadBytes -lt $manifest.minPayloadBytes -or $manifest.maxPayloadBytes -gt $manifest.udpPayloadLimit -or
    $manifest.corpusBytes -lt 1 -or $manifest.corpusBytes -gt 16 + $manifest.datagrams * (16 + 1472) -or
    ($manifest.messageSizes -join ',') -cne '24,40,64,80' -or
    $manifest.schedule -cne 'floor(firstMessageOrdinal * windowNs / messages)' -or
    $manifest.messageOrdinals -cne 'zero-based' -or $manifest.sha256 -cnotmatch '^[0-9a-f]{64}$') {
    throw 'Invalid or unsupported manifest.'
}
$corpusFile = Get-Item -LiteralPath (Join-Path $output 'corpus.bin')
if ($corpusFile -isnot [IO.FileInfo] -or $corpusFile.Length -ne $manifest.corpusBytes -or
    $corpusFile.Length -gt 16 + 112 * $manifest.messages) { throw 'Invalid corpus file size.' }
$corpus = $corpusFile.FullName
if ((Get-FileHash -LiteralPath $corpus -Algorithm SHA256).Hash.ToLowerInvariant() -cne $manifest.sha256) {
    throw 'Corpus SHA-256 does not match the manifest.'
}
$summary = [BurstCorpusV1]::Validate($corpus, [int]$manifest.messages, [int]$manifest.windowMs,
    [int]$manifest.udpPayloadLimit, [uint32]$manifest.seed)
if ($summary.Datagrams -ne $manifest.datagrams -or $summary.PayloadBytes -ne $manifest.payloadBytes -or
    $summary.MessageBytes -ne $manifest.messageBytes -or $summary.SmallestPayloadBytes -ne $manifest.minPayloadBytes -or
    $summary.LargestPayloadBytes -ne $manifest.maxPayloadBytes -or $summary.FileBytes -ne $manifest.corpusBytes) {
    throw 'Corpus counts or sizes do not match the manifest.'
}
Write-Output "Verified $($manifest.messages) messages in $($summary.Datagrams) datagrams: $output"
