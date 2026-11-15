# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

param([Parameter(Mandatory)][string]$OutputDirectory)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$shell = (Get-Process -Id $PID).Path
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be new.' }
$output = (New-Item -Path $OutputDirectory -ItemType Directory).FullName
$corpus = Join-Path $PSScriptRoot 'Corpus.ps1'
$clock = Join-Path $PSScriptRoot 'Measure-Clock.ps1'

function Invoke-Script([string]$Script, [string[]]$Arguments, [int]$ExpectedExit = 0) {
    & $shell -NoProfile -File $Script @Arguments
    if (($ExpectedExit -eq 0 -and $LASTEXITCODE -ne 0) -or ($ExpectedExit -ne 0 -and $LASTEXITCODE -eq 0)) {
        throw "Unexpected exit $LASTEXITCODE from $Script."
    }
}

foreach ($script in $corpus, $clock) {
    Invoke-Script $script @('-Help')
    Invoke-Script $script @('-HelpRu')
}
$first = Join-Path $output 'small-a'
$second = Join-Path $output 'small-b'
foreach ($path in $first, $second) {
    Invoke-Script $corpus @('-Mode', 'Generate', '-Directory', $path, '-Messages', '137', '-WindowMs', '100',
        '-PayloadLimit', '128', '-Seed', '13')
    Invoke-Script $corpus @('-Mode', 'Validate', '-Directory', $path)
}
if ((Get-FileHash (Join-Path $first 'corpus.bin')).Hash -ne (Get-FileHash (Join-Path $second 'corpus.bin')).Hash) {
    throw 'Equal corpus inputs produced different bytes.'
}
$receipt = Join-Path $output 'clock.json'
Invoke-Script $clock @('-Directory', $first, '-Output', $receipt)
$result = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -ErrorAction Stop
if ($result.format -cne 'qdcxx-burst-clock-v1' -or $result.measurement.Scope -cne 'clock-only' -or
    $result.measurement.ScheduledMessages -ne 137 -or $result.measurement.ReleasedMessagesTotal -ne 137 -or
    $result.measurement.ReleasedMessagesInWindow -gt 137 -or
    $result.measurement.LatenessP99Nanoseconds -gt $result.measurement.LatenessMaxNanoseconds) {
    throw 'Unexpected clock measurement receipt.'
}

$binary = Join-Path $second 'corpus.bin'
$manifestPath = Join-Path $second 'manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$manifest.windowMs = 100.1
$manifest | ConvertTo-Json | Set-Content -LiteralPath $manifestPath
Invoke-Script $corpus @('-Mode', 'Validate', '-Directory', $second) 1

$manifest.windowMs = 100
$stream = [IO.File]::Open($binary, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite)
try {
    $stream.Position = $stream.Length - 1
    $tail = $stream.ReadByte()
    $stream.Position--
    $stream.WriteByte([byte]($tail -bxor 1))
} finally { $stream.Dispose() }
$manifest.sha256 = (Get-FileHash -LiteralPath $binary).Hash.ToLowerInvariant()
$manifest | ConvertTo-Json | Set-Content -LiteralPath $manifestPath
Invoke-Script $corpus @('-Mode', 'Validate', '-Directory', $second) 1

$full = Join-Path $output 'default'
Invoke-Script $corpus @('-Mode', 'Generate', '-Directory', $full)
Invoke-Script $corpus @('-Mode', 'Validate', '-Directory', $full)
$manifest = Get-Content -LiteralPath (Join-Path $full 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.messages -ne 625000 -or $manifest.windowMs -ne 100 -or $manifest.maxPayloadBytes -gt 1472 -or
    $manifest.sha256 -cne 'f2cd55c4592391ca586dba6e91f9ec555374397003d05f24324987eee921f7c7') {
    throw 'Default corpus does not have the expected offered profile.'
}
