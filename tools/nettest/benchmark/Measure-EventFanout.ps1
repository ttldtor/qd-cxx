# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

[CmdletBinding()]
param(
    [string]$Program,
    [string]$Java,
    [string]$Classpath,
    [string]$OutputDirectory,
    [int[]]$Clients = @(1, 2, 4, 8),
    [int[]]$Batches = @(1, 8),
    [ValidateRange(1, 1000000)][int]$Rate = 1000,
    [ValidateRange(1, 300)][int]$Duration = 10,
    [ValidateRange(1, 10)][int]$Repeats = 1,
    [string[]]$Pacing = @('sleep'),
    [string]$ListenAddress = '127.0.0.1',
    [switch]$Help,
    [switch]$HelpRu,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$')][string]$ProducerNamespace,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$')][string]$ReceiverNamespace,
    [switch]$TcpNoDelay
)

$ErrorActionPreference = 'Stop'
if ($HelpRu) {
    Write-Output 'Назначение: последовательное сравнение Event TimeAndSale/STREAM через Java QD 3.357.'
    Write-Output 'Использование: Measure-EventFanout.ps1 -Program ФАЙЛ -Java ФАЙЛ -Classpath ПУТИ -OutputDirectory НОВЫЙ_КАТАЛОГ'
    Write-Output 'Параметры: -Clients 1,2,4,8 (каждое 1..8), -Batches 1,8, -Rate 1000 (1..1000000 записей/с),'
    Write-Output ' -Duration 10 (1..300 с), -Repeats 1 (1..10), -Pacing sleep или yield (по умолчанию sleep),'
    Write-Output ' -ListenAddress 127.0.0.1 (конкретный числовой IPv4). Несколько режимов Pacing передаются массивом.'
    Write-Output ' На Linux можно указать -ProducerNamespace ИМЯ -ReceiverNamespace ИМЯ и IPv4 интерфейса издателя.'
    Write-Output ' -TcpNoDelay включает TCP_NODELAY у издателя.'
    Write-Output 'Rate * Duration и пакет должны помещаться в 22-битную последовательность. Нужны JDK с javac и QD 3.357.'
    Write-Output 'Создаёт каталог с summary.csv, JSON, процессными замерами и журналами каждого прогона. Создаёт процессы и TCP-сокеты.'
    Write-Output 'Код 0 — все доставки сверены, 1 — ошибка параметров, запуска, проверки или записи.'
    exit 0
}
if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Run serial Event TimeAndSale/STREAM fan-out comparisons with Java QD 3.357.'
    Write-Output 'Usage: Measure-EventFanout.ps1 -Program FILE -Java FILE -Classpath PATHS -OutputDirectory NEW_DIRECTORY'
    Write-Output 'Options: -Clients 1,2,4,8 (each 1..8), -Batches 1,8, -Rate 1000 (1..1000000 records/s),'
    Write-Output ' -Duration 10 (1..300 s), -Repeats 1 (1..10), -Pacing sleep or yield (default sleep),'
    Write-Output ' -ListenAddress 127.0.0.1 (concrete numeric IPv4). Multiple Pacing values may be passed as an array.'
    Write-Output ' On Linux, supply -ProducerNamespace NAME -ReceiverNamespace NAME and the publisher interface IPv4 address.'
    Write-Output ' -TcpNoDelay enables TCP_NODELAY on the publisher.'
    Write-Output 'Rate * Duration and batch must fit the 22-bit sequence range. A JDK with javac and QD 3.357 are required.'
    Write-Output 'Creates a directory containing summary.csv and each run''s JSON, process samples and logs. Starts processes and TCP sockets.'
    Write-Output 'Exit 0 when every delivery is verified, 1 for argument, launch, validation or output failure.'
    exit 0
}

if (!$Program -or !$Java -or !$Classpath -or !$OutputDirectory) {
    throw 'Supply Program, Java, Classpath and OutputDirectory. Use -Help for usage.'
}
if ($Clients.Count -eq 0 -or $Clients.Count -gt 8 -or
    @($Clients | Select-Object -Unique).Count -ne $Clients.Count -or
    @($Clients | Where-Object { $_ -lt 1 -or $_ -gt 8 }).Count) {
    throw 'Clients must contain distinct values from 1 to 8.'
}
if ($Batches.Count -eq 0 -or $Batches.Count -gt 2 -or
    @($Batches | Select-Object -Unique).Count -ne $Batches.Count -or
    @($Batches | Where-Object { $_ -notin @(1, 8) }).Count) {
    throw 'Batches must contain distinct values selected from 1 and 8.'
}
$Pacing = @($Pacing | ForEach-Object { $_.ToLowerInvariant() })
if ($Pacing.Count -eq 0 -or $Pacing.Count -gt 2 -or
    @($Pacing | Select-Object -Unique).Count -ne $Pacing.Count -or
    @($Pacing | Where-Object { $_ -notin @('sleep', 'yield') }).Count) {
    throw 'Pacing must contain distinct values selected from sleep and yield.'
}
$nominal = [long]$Duration * $Rate
if ($nominal -lt 2 -or $nominal + ($Batches | Measure-Object -Maximum).Maximum -ge (1 -shl 22)) {
    throw 'The offered record count is outside the unique sequence range.'
}
$address = $null
if (![Net.IPAddress]::TryParse($ListenAddress, [ref]$address) -or
    $address.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork -or
    $address.ToString() -cne $ListenAddress -or $address.Equals([Net.IPAddress]::Any)) {
    throw 'ListenAddress must be a concrete numeric IPv4 address.'
}
if (($ProducerNamespace -or $ReceiverNamespace) -and
    (!$IsLinux -or !$ProducerNamespace -or !$ReceiverNamespace -or
        $ProducerNamespace -ceq $ReceiverNamespace -or [Net.IPAddress]::IsLoopback($address))) {
    throw 'Network namespaces require Linux, two distinct names and a non-loopback IPv4 listen address.'
}
$Program = (Resolve-Path -LiteralPath $Program).Path
$Java = (Resolve-Path -LiteralPath $Java).Path
if (!(Test-Path -LiteralPath $Program -PathType Leaf) -or !(Test-Path -LiteralPath $Java -PathType Leaf)) {
    throw 'Program and Java must be executable files.'
}
$javacName = if ($IsWindows) { 'javac.exe' } else { 'javac' }
if (!(Test-Path -LiteralPath (Join-Path (Split-Path $Java) $javacName) -PathType Leaf)) {
    throw 'The Java directory must contain javac.'
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be new.' }
$runner = Join-Path $PSScriptRoot 'Measure-Stream.ps1'
if (!(Test-Path -LiteralPath $runner -PathType Leaf)) { throw 'Measure-Stream.ps1 is missing.' }

$output = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
$rows = [Collections.Generic.List[object]]::new()
$run = 0
foreach ($repeat in 1..$Repeats) {
    $orderedBatches = @($Batches)
    if ($repeat % 2 -eq 0) { [array]::Reverse($orderedBatches) }
    foreach ($mode in $Pacing) {
        foreach ($batch in $orderedBatches) {
            foreach ($clientCount in $Clients) {
                ++$run
                $name = '{0:D3}-{1}-{2}-{3}-{4}' -f $run, $repeat, $mode, $batch, $clientCount
                $dir = Join-Path $output $name
                $arguments = @{
                    Program = $Program
                    ProducerKind = 'Event'
                    Java = $Java
                    Classpath = $Classpath
                    OutputDirectory = $dir
                    Duration = $Duration
                    Rate = $Rate
                    Batch = $batch
                    Clients = $clientCount
                    Contract = 'stream'
                    ListenAddress = $ListenAddress
                    Precision = 'ns'
                    Pacing = $mode
                    TcpNoDelay = [bool]$TcpNoDelay
                }
                if ($ProducerNamespace) {
                    $arguments.ProducerNamespace = $ProducerNamespace
                    $arguments.ReceiverNamespace = $ReceiverNamespace
                }
                & $runner @arguments
                $result = Get-Content -LiteralPath (Join-Path $dir 'result.json') -Raw | ConvertFrom-Json
                $receivers = @($result.receivers)
                $processes = @($result.processes.receivers)
                $admitted = [long]$result.sender.admitted
                $delivered = [long]$result.totalDelivered
                if ($result.producerKind -cne 'Event' -or $result.record -cne 'TimeAndSale' -or
                    $result.contract -cne 'STREAM' -or $result.qdVersion -cne '3.357' -or
                    $result.clients -ne $clientCount -or $result.configuredBatch -ne $batch -or
                    $result.offeredRps -ne $Rate -or $result.pacing -cne $mode -or
                    $result.tcpNoDelay -ne [bool]$TcpNoDelay -or
                    $result.precision -cne 'ns' -or $admitted -le 0 -or
                    $receivers.Count -ne $clientCount -or $processes.Count -ne $clientCount -or
                    $delivered -ne $admitted * $clientCount) {
                    throw "Run $name returned inconsistent counts or configuration."
                }
                foreach ($receiver in $receivers) {
                    if ($receiver.delivered -ne $admitted -or $receiver.expected -ne $admitted -or
                        $receiver.sequenceErrors -ne 0 -or $receiver.fieldErrors -ne 0 -or
                        $receiver.latencySamples -ne $admitted -or $null -eq $receiver.latencyP95Ns) {
                        throw "Run $name has an inconsistent receiver result."
                    }
                }
                $producer = $result.processes.producer
                $receiverCpuSeconds = 0.0
                $receiverPeakWorkingSetBytes = 0L
                $receiverPeakPrivateBytes = 0L
                foreach ($process in $processes) {
                    $receiverCpuSeconds += $process.userCpuSeconds + $process.systemCpuSeconds
                    $receiverPeakWorkingSetBytes += [long]$process.sampledPeakWorkingSetBytes
                    $receiverPeakPrivateBytes += [long]$process.sampledPeakPrivateBytes
                }
                $rows.Add([pscustomobject]@{
                    run = $name
                    repeat = $repeat
                    clients = $clientCount
                    batch = $batch
                    pacing = $mode
                    durationSeconds = $Duration
                    offeredRecordsPerSecond = $Rate
                    admittedRecords = $admitted
                    nominalShortfall = $result.nominalShortfall
                    admissionRetries = $result.sender.admissionRetries
                    sourceRecordsPerSecond = $result.sourceRecordsPerSecond
                    deliveredRecords = $delivered
                    deliveredRecordsPerSecond = $result.deliveredIncludingDrainPerProducerSecond
                    sentDataFrames = $result.sender.sentFrames
                    dataFramesPerDeliveredRecord = $result.sender.sentFrames / $delivered
                    worstClientP95Ns = ($receivers | Measure-Object -Property latencyP95Ns -Maximum).Maximum
                    producerCpuSeconds = $producer.userCpuSeconds + $producer.systemCpuSeconds
                    receiverCpuSecondsSum = $receiverCpuSeconds
                    producerPeakWorkingSetBytes = $producer.sampledPeakWorkingSetBytes
                    receiverPeakWorkingSetBytesSum = $receiverPeakWorkingSetBytes
                    producerPeakPrivateBytes = $producer.sampledPeakPrivateBytes
                    receiverPeakPrivateBytesSum = $receiverPeakPrivateBytes
                })
                $rows | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $output 'summary.csv')
            }
        }
    }
}
Write-Output "Completed $run verified runs. Summary: $(Join-Path $output 'summary.csv')"
