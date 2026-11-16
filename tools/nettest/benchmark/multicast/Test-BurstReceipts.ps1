# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

[CmdletBinding()]
param(
    [string]$Directory,
    [string]$SendReceipt,
    [string]$ReceiveReceipt,
    [string]$Output,
    [ValidateRange(0,9223372036854775807)][long]$MaxSendP99Ns,
    [switch]$Help,
    [switch]$HelpRu
)

$ErrorActionPreference = 'Stop'
if ($HelpRu) {
    Write-Output 'Сверяет корпус и отчёты отправителя и получателя синтетического UDP-всплеска.'
    Write-Output 'Использование: Test-BurstReceipts.ps1 -Directory КОРПУС -SendReceipt JSON -ReceiveReceipt JSON -Output НОВЫЙ_JSON [-MaxSendP99Ns N]'
    Write-Output 'Проверяет совместимость, доставку, потери, порядок, байты, оба локальных окна и необязательный предел p99.'
    Write-Output 'Выход 0 означает прохождение всех проверок, 1 — их невыполнение или ошибку входных данных. При корректных отчётах результат записывается и при отрицательном исходе.'
    Write-Output 'Результат не подтверждает физический канал или межмашинную одностороннюю задержку.'
    exit 0
}
if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Reconcile a synthetic UDP burst corpus and sender/receiver receipts.'
    Write-Output 'Usage: Test-BurstReceipts.ps1 -Directory CORPUS -SendReceipt JSON -ReceiveReceipt JSON -Output NEW_JSON [-MaxSendP99Ns N]'
    Write-Output 'Checks compatibility, delivery, loss, ordering, bytes, both local windows and an optional p99 bound.'
    Write-Output 'Exit 0 when every check passes, 1 for failed checks or invalid input. Valid rejecting comparisons also write evidence.'
    Write-Output 'The result does not certify a physical network or cross-host one-way latency.'
    exit 0
}
if (!$Directory -or !$SendReceipt -or !$ReceiveReceipt -or !$Output) { throw 'Supply all inputs and a new output file.' }
$directoryPath = (Resolve-Path -LiteralPath $Directory).Path
$sendPath = (Resolve-Path -LiteralPath $SendReceipt).Path
$receivePath = (Resolve-Path -LiteralPath $ReceiveReceipt).Path
if (Test-Path -LiteralPath $Output) { throw 'Output must be new.' }
$outputPath = [IO.Path]::GetFullPath($Output, (Get-Location).Path)
if (!(Test-Path -LiteralPath ([IO.Path]::GetDirectoryName($outputPath)) -PathType Container)) {
    throw 'Output parent must exist.'
}
foreach ($path in $sendPath, $receivePath) {
    $file = Get-Item -LiteralPath $path
    if ($file -isnot [IO.FileInfo] -or $file.Length -gt 16384) { throw 'Invalid receipt size.' }
}
& (Join-Path $PSScriptRoot '../burst/Corpus.ps1') -Mode Validate -Directory $directoryPath | Out-Null
$manifest = Get-Content -LiteralPath (Join-Path $directoryPath 'manifest.json') -Raw | ConvertFrom-Json
$send = Get-Content -LiteralPath $sendPath -Raw | ConvertFrom-Json
$receive = Get-Content -LiteralPath $receivePath -Raw | ConvertFrom-Json
foreach ($receipt in $send, $receive) {
    foreach ($name in 'format', 'mode', 'corpusSha256', 'group', 'interface') {
        if ($receipt.$name -isnot [string]) { throw "Receipt field $name must be a string." }
    }
}
if ($send.format -cne 'qdcxx-burst-multicast-v1' -or $receive.format -cne 'qdcxx-burst-multicast-v1' -or
    $send.mode -cne 'send' -or $receive.mode -cne 'receive' -or
    $send.measurement -isnot [pscustomobject] -or $receive.measurement -isnot [pscustomobject]) {
    throw 'Invalid receipt formats or roles.'
}
foreach ($receipt in $send, $receive) {
    $address = $null
    $interfaceAddress = $null
    if ($receipt.corpusSha256 -cnotmatch '^[0-9a-f]{64}$' -or
        $receipt.port -isnot [long] -or $receipt.port -lt 1 -or $receipt.port -gt 65535 -or
        ![Net.IPAddress]::TryParse($receipt.group, [ref]$address) -or
        $address.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork -or $address.GetAddressBytes()[0] -ne 239 -or
        ![Net.IPAddress]::TryParse($receipt.interface, [ref]$interfaceAddress) -or
        $interfaceAddress.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork -or
        $interfaceAddress.Equals([Net.IPAddress]::Any) -or $interfaceAddress.GetAddressBytes()[0] -ge 224) {
        throw 'Invalid receipt identity.'
    }
}
$senderFields = @('ClockFrequencyHz','WindowNanoseconds','ScheduledDatagrams','ScheduledMessages','AttemptedDatagrams',
    'SentDatagrams','SentMessages','SentDatagramsInWindow','SentMessagesInWindow','PayloadBytesSent','SendErrors',
    'LatenessP99Nanoseconds','LatenessMaxNanoseconds','ElapsedNanoseconds')
$receiverFields = @('ExpectedDatagrams','ExpectedMessages','UniqueDatagrams','UniqueMessages','UniqueDatagramsInWindow',
    'UniqueMessagesInWindow','ReceiveSpanNanoseconds','MissingDatagrams','MissingMessages','DuplicateDatagrams',
    'OutOfOrderDatagrams','CorruptDatagrams','UnknownDatagrams','PayloadBytesReceived','SocketReceiveBufferBytes')
foreach ($name in $senderFields) {
    $value = $send.measurement.$name
    if ($value -isnot [long] -or $value -lt 0) { throw "Invalid sender field $name." }
}
foreach ($name in $receiverFields) {
    $value = $receive.measurement.$name
    if ($value -isnot [long] -or $value -lt 0) { throw "Invalid receiver field $name." }
}
if ($receive.measurement.Completed -isnot [bool]) { throw 'Invalid receiver Completed field.' }
$s = $send.measurement
$r = $receive.measurement
if ($s.ClockFrequencyHz -eq 0 -or $s.LatenessP99Nanoseconds -gt $s.LatenessMaxNanoseconds -or
    $s.SentDatagrams -gt $s.AttemptedDatagrams -or $s.AttemptedDatagrams -gt $s.ScheduledDatagrams -or
    $s.SentMessages -gt $s.ScheduledMessages -or $s.SentDatagramsInWindow -gt $s.SentDatagrams -or
    $s.SentMessagesInWindow -gt $s.SentMessages -or $r.UniqueDatagrams -gt $r.ExpectedDatagrams -or
    $r.UniqueMessages -gt $r.ExpectedMessages -or $r.UniqueDatagramsInWindow -gt $r.UniqueDatagrams -or
    $r.UniqueMessagesInWindow -gt $r.UniqueMessages -or $r.MissingDatagrams -ne $r.ExpectedDatagrams - $r.UniqueDatagrams -or
    $r.MissingMessages -ne $r.ExpectedMessages - $r.UniqueMessages) {
    throw 'Inconsistent receipt counters.'
}
$checks = [ordered]@{
    corpusIdentity = $send.corpusSha256 -ceq $manifest.sha256 -and $receive.corpusSha256 -ceq $manifest.sha256
    multicastEndpoint = $send.group -ceq $receive.group -and $send.port -eq $receive.port
    offeredSchedule = $s.WindowNanoseconds -eq $manifest.windowMs * 1000000 -and
        $s.ScheduledDatagrams -eq $manifest.datagrams -and $s.ScheduledMessages -eq $manifest.messages -and
        $r.ExpectedDatagrams -eq $manifest.datagrams -and $r.ExpectedMessages -eq $manifest.messages
    senderIntegrity = $s.AttemptedDatagrams -eq $manifest.datagrams -and $s.SentDatagrams -eq $manifest.datagrams -and
        $s.SentMessages -eq $manifest.messages -and $s.PayloadBytesSent -eq $manifest.payloadBytes -and $s.SendErrors -eq 0
    receiverIntegrity = $r.UniqueDatagrams -eq $manifest.datagrams -and $r.UniqueMessages -eq $manifest.messages -and
        $r.MissingDatagrams -eq 0 -and $r.MissingMessages -eq 0 -and $r.DuplicateDatagrams -eq 0 -and
        $r.OutOfOrderDatagrams -eq 0 -and $r.CorruptDatagrams -eq 0 -and $r.UnknownDatagrams -eq 0 -and
        $r.PayloadBytesReceived -eq $manifest.payloadBytes -and $r.Completed
    senderWindow = $s.SentDatagramsInWindow -eq $manifest.datagrams -and $s.SentMessagesInWindow -eq $manifest.messages
    receiverWindow = $r.UniqueDatagramsInWindow -eq $manifest.datagrams -and $r.UniqueMessagesInWindow -eq $manifest.messages -and
        $r.ReceiveSpanNanoseconds -lt $manifest.windowMs * 1000000
    senderP99 = if ($PSBoundParameters.ContainsKey('MaxSendP99Ns')) { $s.LatenessP99Nanoseconds -le $MaxSendP99Ns } else { $null }
}
$passed = @($checks.Values | Where-Object { $_ -ceq $false }).Count -eq 0
$result = [ordered]@{
    format = 'qdcxx-burst-receipt-check-v1'
    corpusSha256 = $manifest.sha256
    sendReceiptSha256 = (Get-FileHash -LiteralPath $sendPath).Hash.ToLowerInvariant()
    receiveReceiptSha256 = (Get-FileHash -LiteralPath $receivePath).Hash.ToLowerInvariant()
    messages = $manifest.messages
    datagrams = $manifest.datagrams
    windowNanoseconds = $manifest.windowMs * 1000000
    maxSendP99Nanoseconds = if ($PSBoundParameters.ContainsKey('MaxSendP99Ns')) { $MaxSendP99Ns } else { $null }
    observed = [ordered]@{
        sender = [ordered]@{
            totalDatagrams = $s.SentDatagrams
            totalMessages = $s.SentMessages
            datagramsInWindow = $s.SentDatagramsInWindow
            messagesInWindow = $s.SentMessagesInWindow
            payloadBytes = $s.PayloadBytesSent
            errors = $s.SendErrors
            p99Nanoseconds = $s.LatenessP99Nanoseconds
        }
        receiver = [ordered]@{
            uniqueDatagrams = $r.UniqueDatagrams
            uniqueMessages = $r.UniqueMessages
            datagramsInWindow = $r.UniqueDatagramsInWindow
            messagesInWindow = $r.UniqueMessagesInWindow
            payloadBytes = $r.PayloadBytesReceived
            missingDatagrams = $r.MissingDatagrams
            missingMessages = $r.MissingMessages
            duplicates = $r.DuplicateDatagrams
            outOfOrder = $r.OutOfOrderDatagrams
            corrupt = $r.CorruptDatagrams
            unknown = $r.UnknownDatagrams
        }
    }
    receiverWindowAnchor = 'first-valid-unique-datagram'
    checks = $checks
    passed = $passed
    physicalNetworkCertified = $false
    oneWayLatencyCertified = $false
}
$bytes = [Text.UTF8Encoding]::new($false).GetBytes(($result | ConvertTo-Json -Depth 4) + [Environment]::NewLine)
$stream = [IO.File]::Open($outputPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
Write-Output "Burst receipt check passed=$passed : $outputPath"
if (!$passed) { exit 1 }
