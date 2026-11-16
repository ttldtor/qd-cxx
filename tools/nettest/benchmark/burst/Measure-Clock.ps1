# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

[CmdletBinding()]
param(
    [string]$Directory,
    [string]$Output,
    [switch]$Help,
    [switch]$HelpRu
)

$ErrorActionPreference = 'Stop'

if ($HelpRu) {
    Write-Output 'Измеряет отклонение заданного графика выпуска по монотонным часам без сетевой передачи.'
    Write-Output 'Использование: Measure-Clock.ps1 -Directory КОРПУС -Output НОВЫЙ_JSON'
    Write-Output 'Сначала проверяет все байты корпуса; загружает расписание до начала измерения. Ожидание активно использует CPU.'
    Write-Output 'JSON содержит выпуск в окне и всего, p99/максимум опоздания и частоту часов. Результат не измеряет сокеты, QTP или сеть.'
    Write-Output 'Код 0: успех или справка; ненулевой код: ошибка ввода, проверки или записи.'
    exit 0
}
if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Measure a preloaded offered schedule using a monotonic clock, without network I/O.'
    Write-Output 'Usage: Measure-Clock.ps1 -Directory CORPUS -Output NEW_JSON'
    Write-Output 'Validates the complete corpus first and loads its schedule before timing. Active waiting uses CPU.'
    Write-Output 'JSON reports in-window and total releases, p99/maximum lateness and clock frequency. It does not measure sockets, QTP or the network.'
    Write-Output 'Exit 0 on success or help; invalid input, verification and I/O errors return nonzero.'
    exit 0
}
if (!$Directory -or !$Output) { throw 'Supply Directory and Output. Use -Help for details.' }
$corpusDirectory = (Resolve-Path -LiteralPath $Directory).Path
if (!(Test-Path -LiteralPath $corpusDirectory -PathType Container)) { throw 'Directory must contain a corpus.' }
if (Test-Path -LiteralPath $Output) { throw 'Output must be a new file.' }
$outputPath = [IO.Path]::GetFullPath($Output, (Get-Location).Path)
if (!(Test-Path -LiteralPath ([IO.Path]::GetDirectoryName($outputPath)) -PathType Container)) {
    throw 'Output parent directory must exist.'
}

& (Join-Path $PSScriptRoot 'Corpus.ps1') -Mode Validate -Directory $corpusDirectory | Out-Null
$manifest = Get-Content -LiteralPath (Join-Path $corpusDirectory 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.format -cne 'qdcxx-synthetic-burst-v1') { throw 'Unsupported corpus format.' }
if (-not ('BurstClockV1' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'Clock.cs') }
$result = [BurstClockV1]::Run((Join-Path $corpusDirectory 'corpus.bin'),
    [int]$manifest.datagrams, [int]$manifest.messages, [int]$manifest.windowMs)
$receipt = [ordered]@{
    format = 'qdcxx-burst-clock-v1'
    corpusSha256 = $manifest.sha256
    measurement = $result
}
$json = $receipt | ConvertTo-Json -Depth 4
$bytes = [Text.UTF8Encoding]::new($false).GetBytes($json + [Environment]::NewLine)
$stream = [IO.File]::Open($outputPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
Write-Output "Measured $($result.ReleasedDatagramsTotal) scheduled datagrams: $outputPath"
