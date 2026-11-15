# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

[CmdletBinding()]
param(
    [string]$Program,
    [string]$Bundle,
    [ValidateSet('Record', 'Event')][string]$ProducerKind = 'Record',
    [string]$Java,
    [string]$Classpath,
    [string]$OutputDirectory,
    [ValidateRange(1, 300)][int]$Duration = 10,
    [ValidateRange(1, 1000000)][int]$Rate = 1000,
    [ValidateRange(1, 32)][int]$Batch = 1,
    [ValidateRange(1, 8)][int]$Clients = 1,
    [ValidateSet('stream', 'ticker')][string]$Contract = 'stream',
    [string]$ListenAddress = '127.0.0.1',
    [ValidateSet('Auto', 'ms', 'ns')][string]$Precision = 'Auto',
    [ValidateSet('sleep', 'yield')][string]$Pacing = 'sleep',
    [switch]$Help,
    [switch]$HelpRu
)

$ErrorActionPreference = 'Stop'
if ($HelpRu) {
    Write-Output 'Использование: Measure-Stream.ps1 -Program ФАЙЛ -Bundle КАТАЛОГ -Java ФАЙЛ -Classpath QD357 -OutputDirectory НОВЫЙ_КАТАЛОГ'
    Write-Output 'Сверяет IBM TimeAndSale/STREAM с QD 3.357 на IPv4 loopback. Нужен JDK с javac.'
    Write-Output 'Для qdcxx-event-nettest укажите -ProducerKind Event, без -Bundle и с -Batch 1.'
    Write-Output 'Event: -Clients 1..8, -Contract stream|ticker, -ListenAddress IPv4, -Precision Auto|ms|ns, -Pacing sleep|yield (sleep по умолчанию).'
    Write-Output 'Параметры: -Duration 10 (1..300 с), -Rate 1000 (записей/с), -Batch 1 (1..32). Нагрузка должна укладываться в 22 бит.'
    Write-Output 'Сохраняет JSON, замеры процессов и журналы в новом каталоге. Завершает только свои дочерние процессы.'
    Write-Output 'Код 0 — доставка и сверка успешны, 1 — ошибка аргументов, сборки, замера или доставки.'
    exit 0
}
if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Usage: Measure-Stream.ps1 -Program FILE -Bundle DIR -Java FILE -Classpath QD357 -OutputDirectory NEW_DIR'
    Write-Output 'Verify IBM TimeAndSale/STREAM on IPv4 loopback with Java QD 3.357. Requires a JDK with javac.'
    Write-Output 'For qdcxx-event-nettest, pass -ProducerKind Event with -Batch 1 and omit -Bundle.'
    Write-Output 'Event: -Clients 1..8, -Contract stream|ticker, -ListenAddress IPv4, -Precision Auto|ms|ns, -Pacing sleep|yield (default sleep).'
    Write-Output 'Options: -Duration 10 (1..300 seconds), -Rate 1000 (records/sec), -Batch 1 (1..32).'
    Write-Output 'Offered records plus one batch must fit the 22-bit sequence range.'
    Write-Output 'Writes JSON, process samples and logs to a new directory. Stops its own child processes.'
    Write-Output 'Exit 0 when delivery and validation succeed, 1 on an argument, build, measurement or delivery failure.'
    exit 0
}

if (!$Program -or !$Java -or !$Classpath -or !$OutputDirectory) {
    throw 'Supply Program, Java, Classpath and OutputDirectory. Use -Help for details.'
}
if (($ProducerKind -eq 'Record' -and !$Bundle) -or ($ProducerKind -eq 'Event' -and ($Bundle -or $Batch -ne 1))) {
    throw 'Record mode requires a bundle. Event mode requires batch 1 and uses the built-in schema.'
}
$address = $null
if (![Net.IPAddress]::TryParse($ListenAddress, [ref]$address) -or $address.AddressFamily -ne 'InterNetwork' -or
    $address.ToString() -cne $ListenAddress -or $address.Equals([Net.IPAddress]::Any)) {
    throw 'ListenAddress must be a concrete numeric IPv4 address.'
}
if ($Precision -eq 'Auto') { $Precision = if ($ProducerKind -eq 'Event') { 'ns' } else { 'ms' } }
$Precision = $Precision.ToLowerInvariant()
$Contract = $Contract.ToLowerInvariant()
$Pacing = $Pacing.ToLowerInvariant()
if ($ProducerKind -eq 'Record' -and ($Clients -ne 1 -or $Contract -ne 'stream' -or $ListenAddress -ne '127.0.0.1' -or $Precision -ne 'ms' -or $Pacing -ne 'sleep')) {
    throw 'Record uses one loopback STREAM client, ms timestamps and default pacing.'
}
$sequenceLimit = 1 -shl 22
if ($ProducerKind -eq 'Event' -and [long]$Duration * $Rate -lt 2) {
    throw 'Event requires an offered window of at least two records.'
}
if ([long]$Duration * $Rate + $Batch -ge $sequenceLimit) {
    throw 'The offered workload exceeds the unique sequence range.'
}

$Program = (Resolve-Path -LiteralPath $Program).Path
$Java = (Resolve-Path -LiteralPath $Java).Path
if ($ProducerKind -eq 'Record') {
    $Bundle = (Resolve-Path -LiteralPath $Bundle).Path
}
$javacName = if ($IsWindows) { 'javac.exe' } else { 'javac' }
$javac = (Resolve-Path -LiteralPath (Join-Path (Split-Path $Java) $javacName)).Path
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be new.' }
$output = (New-Item -Path $OutputDirectory -ItemType Directory).FullName
$classes = (New-Item -Path (Join-Path $output 'classes') -ItemType Directory).FullName
$record = if ($Contract -eq 'ticker') { 'Quote' } else { 'TimeAndSale' }
$producerArgs = @('p', '--port', '0', '--duration', "$Duration", '--rate', "$Rate",
    '--batch', "$Batch", '--startup', '60', '--record', $record, '--symbol', 'IBM', '--contract', $Contract)
$schemaHash = $null
$provenanceHash = $null
if ($ProducerKind -eq 'Record') {
    $producerArgs += @('--bundle', $Bundle)
    $schemaHash = (Get-FileHash -LiteralPath (Join-Path $Bundle 'schema.json')).Hash
    $provenanceHash = (Get-FileHash -LiteralPath (Join-Path $Bundle 'provenance.json')).Hash
} else {
    $producerArgs += @('--clients', "$Clients", '--address', $ListenAddress, '--start-signal', 'stdin')
    if ($Pacing -eq 'yield') { $producerArgs += @('--pacing', $Pacing) }
}
$PSNativeCommandUseErrorActionPreference = $false
& $javac -cp $Classpath -d $classes (Join-Path $PSScriptRoot 'StreamReceiver.java') *> (Join-Path $output 'javac.log')
if ($LASTEXITCODE -ne 0) { throw "Receiver compilation failed. See $output/javac.log." }
$javaVersion = (& $Java -version 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot read Java version.' }

$clock = [Diagnostics.Stopwatch]::StartNew()
$samples = [Collections.Generic.List[object]]::new()
$jdkOptions = @('-Xms32m', '-Xmx256m', '-Djava.net.preferIPv4Stack=true', '-Dcom.devexperts.qd.monitoring.MonitoredQDEndpoint$BuilderImpl.disable=true')
if ($Precision -eq 'ns') { $jdkOptions += '-Ddxscheme.nanoTime=true' }

# Own redirected pipes and a bounded stdout prefix until the readiness marker.
function Start-Child([string]$File, [string[]]$Arguments, [string]$Name) {
    $info = [Diagnostics.ProcessStartInfo]::new($File)
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [Text.Encoding]::UTF8
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($info)
    @{
        Process = $process
        Name = $Name
        Prefix = [Collections.Generic.List[string]]::new()
        Pending = $process.StandardOutput.ReadLineAsync()
        Output = $null
        ErrorText = $process.StandardError.ReadToEndAsync()
    }
}

function Add-Sample($Child, [string]$Phase) {
    if ($null -eq $Child -or $Child.Process.HasExited) { return }
    $p = $Child.Process
    try {
        $p.Refresh()
        $samples.Add([pscustomobject]@{
            elapsedMs = $clock.Elapsed.TotalMilliseconds
            process = $Child.Name
            phase = $Phase
            userCpuMs = $p.UserProcessorTime.TotalMilliseconds
            systemCpuMs = $p.PrivilegedProcessorTime.TotalMilliseconds
            workingSetBytes = $p.WorkingSet64
            privateBytes = $p.PrivateMemorySize64
        })
    } catch {
        if (!$p.HasExited) { throw }
    }
}

function Wait-Marker($Child, [string]$Pattern, [int]$TimeoutMs, $Other = $null, [switch]$KeepReading) {
    $deadline = $clock.ElapsedMilliseconds + $TimeoutMs
    while ($clock.ElapsedMilliseconds -lt $deadline) {
        if ($Child.Pending.Wait(100)) {
            $line = $Child.Pending.GetAwaiter().GetResult()
            $Child.Pending = $null
            if ($null -eq $line) { throw "$($Child.Name) closed stdout before readiness." }
            $Child.Prefix.Add($line)
            if ($line -match $Pattern) {
                if ($KeepReading) { $Child.Pending = $Child.Process.StandardOutput.ReadLineAsync() }
                else { $Child.Output = $Child.Process.StandardOutput.ReadToEndAsync() }
                return $line
            }
            if ($Child.Prefix.Count -ge 256) { throw "$($Child.Name) emitted too many lines before readiness." }
            $Child.Pending = $Child.Process.StandardOutput.ReadLineAsync()
        }
        Add-Sample $Child startup
        Add-Sample $Other startup
    }
    throw "$($Child.Name) timed out before readiness."
}

function Wait-Child($Child, [int]$TimeoutMs, [string]$Phase, $Other = $null) {
    $deadline = $clock.ElapsedMilliseconds + $TimeoutMs
    do {
        Add-Sample $Child $Phase
        foreach ($peer in $Other) { Add-Sample $peer $Phase }
        if ($Child.Process.WaitForExit(100)) { return }
    } while ($clock.ElapsedMilliseconds -lt $deadline)
    throw "$($Child.Name) did not terminate before its timeout."
}

function Read-Text($Task) {
    if (!$Task.Wait(3000)) { throw 'Timed out reading child output.' }
    return $Task.GetAwaiter().GetResult()
}

function Get-ResultLine([string]$Text) {
    $lines = [regex]::Matches($Text, '(?m)^RESULT ([^\r\n]+)\r?$')
    if ($lines.Count -ne 1) { throw 'Expected one RESULT line.' }
    return $lines[0].Groups[1].Value
}

function Save-Child($Child) {
    if ($null -eq $Child) { return }
    try {
        if (!$Child.Process.HasExited) {
            try { $Child.Process.Kill($true) } catch {
                if (!$Child.Process.HasExited) { throw }
            }
        }
        if (!$Child.Process.WaitForExit(5000)) { throw "Could not stop $($Child.Name)." }
        if ($Child.Pending) {
            $line = Read-Text $Child.Pending
            if ($null -ne $line) { $Child.Prefix.Add($line) }
        }
        if (!$Child.Output) { $Child.Output = $Child.Process.StandardOutput.ReadToEndAsync() }
        @($Child.Prefix.ToArray(); (Read-Text $Child.Output)) |
            Set-Content -LiteralPath (Join-Path $output "$($Child.Name).stdout")
        Read-Text $Child.ErrorText | Set-Content -LiteralPath (Join-Path $output "$($Child.Name).stderr")
    } finally {
        $Child.Process.Dispose()
    }
}

function Get-ProcessSummary($Child) {
    $Name = $Child.Name
    $rows = @($samples | Where-Object { $_.process -eq $Name -and $_.phase -eq 'active' })
    if ($rows.Count -lt 2) { throw "Too few live active samples for $Name." }
    $first = $rows[0]
    $last = $rows[-1]
    $user = ($last.userCpuMs - $first.userCpuMs) / 1000
    $system = ($last.systemCpuMs - $first.systemCpuMs) / 1000
    $seconds = ($last.elapsedMs - $first.elapsedMs) / 1000
    $memory = @($rows.workingSetBytes | Sort-Object)
    $median = ($memory[[int][Math]::Floor(($memory.Count - 1) / 2)] +
        $memory[[int][Math]::Floor($memory.Count / 2)]) / 2
    return @{
        pid = $Child.Process.Id
        samples = $rows.Count
        firstSampleMs = $first.elapsedMs
        lastSampleMs = $last.elapsedMs
        userCpuSeconds = $user
        systemCpuSeconds = $system
        averageCpuCores = ($user + $system) / $seconds
        firstWorkingSetBytes = $first.workingSetBytes
        medianWorkingSetBytes = $median
        sampledPeakWorkingSetBytes = $memory[-1]
        sampledPeakPrivateBytes = ($rows | Measure-Object privateBytes -Maximum).Maximum
    }
}

function Confirm-Probe($Producer, $Receivers) {
    $deadline = $clock.ElapsedMilliseconds + 15000
    $nextProbe = 0
    $confirmed = [Collections.Generic.HashSet[string]]::new()
    while ($confirmed.Count -lt $Receivers.Count -and $clock.ElapsedMilliseconds -lt $deadline) {
        if ($clock.ElapsedMilliseconds -ge $nextProbe) {
            $Producer.Process.StandardInput.WriteLine('PROBE')
            $nextProbe = $clock.ElapsedMilliseconds + 500
        }
        foreach ($child in $Receivers) {
            if ($confirmed.Contains($child.Name)) { continue }
            while ($child.Pending.IsCompleted) {
                $line = $child.Pending.GetAwaiter().GetResult()
                $child.Pending = $null
                if ($null -eq $line -or $child.Prefix.Count -ge 256) { throw "$($child.Name) did not confirm its probe." }
                $child.Prefix.Add($line)
                $child.Pending = $child.Process.StandardOutput.ReadLineAsync()
                if ($line -ceq 'PROBE_RECEIVED') { $null = $confirmed.Add($child.Name); break }
            }
        }
        if ($confirmed.Count -lt $Receivers.Count) { Start-Sleep -Milliseconds 10 }
    }
    if ($confirmed.Count -ne $Receivers.Count) { throw 'Not all receivers confirmed their subscription probes.' }
}

$producer = $null
$receivers = [Collections.Generic.List[object]]::new()
$summary = $null
try {
    $producer = Start-Child $Program $producerArgs producer
    $isEvent = $ProducerKind -eq 'Event'
    $ready = Wait-Marker $producer '^PORT [1-9][0-9]*$' 10000 -KeepReading:$isEvent
    $port = $ready.Substring(5)
    $receiverClasspath = $classes + [IO.Path]::PathSeparator + $Classpath
    $receiverArgs = $jdkOptions + @('-cp', $receiverClasspath, 'StreamReceiver', "${ListenAddress}:$port", 'IBM', $Contract, $Precision)
    if ($isEvent) { $receiverArgs += 'probe' }
    foreach ($index in 1..$Clients) {
        $name = if ($index -eq 1) { 'receiver' } else { "receiver-$index" }
        $receivers.Add((Start-Child $Java $receiverArgs $name))
    }
    if ($isEvent) {
        $null = Wait-Marker $producer '^READY$' 50000
        Confirm-Probe $producer $receivers
        $producer.Process.StandardInput.WriteLine('GO')
        $producer.Process.StandardInput.Close()
    }
    foreach ($child in $receivers) { $null = Wait-Marker $child '^ACTIVE$' 20000 $producer }
    Wait-Child $producer (($Duration + 8) * 1000) active $receivers
    if ($producer.Process.ExitCode -ne 0) { throw "$ProducerKind NetTest exited $($producer.Process.ExitCode)." }

    $native = @{}
    foreach ($field in (Get-ResultLine (Read-Text $producer.Output)).Split(' ')) {
        $parts = $field.Split('=', 2)
        if ($parts.Count -ne 2) { throw 'Malformed native RESULT field.' }
        $native[$parts[0]] = [double]::Parse($parts[1], [Globalization.CultureInfo]::InvariantCulture)
    }
    $expected = if ($ProducerKind -eq 'Record') { $native.written } else { $native.admitted }
    $nominalRecords = [long]$Duration * $Rate
    if ($expected -le 0 -or $expected -gt $nominalRecords -or $expected -ge $sequenceLimit -or $native.connections -ne $Clients -or
        $native.generated -ne $expected -or $native.seconds -ne $Duration -or
        ($ProducerKind -eq 'Record' -and $native.composed -ne $expected) -or
        ($isEvent -and $native.ingressRecords -ne $expected) -or
        ($isEvent -and $Contract -eq 'stream' -and $native.sentFrames -ne $expected * $Clients)) {
        throw 'Producer count, connection or measurement-window check failed.'
    }

    foreach ($child in $receivers) {
        $child.Process.StandardInput.WriteLine([string][long]$expected)
        $child.Process.StandardInput.Close()
    }
    $observations = @(foreach ($child in $receivers) {
        Wait-Child $child 10000 drain
        if ($child.Process.ExitCode -ne 0) { throw "$($child.Name) exited $($child.Process.ExitCode)." }
        $observed = Get-ResultLine (Read-Text $child.Output) | ConvertFrom-Json
        if ($observed.qdVersion -cne 'QDS-3.357' -or $observed.expected -ne $expected -or
            $observed.precision -cne $Precision -or $observed.sequenceErrors -ne 0 -or $observed.fieldErrors -ne 0 -or
            $observed.latencySamples -ne $observed.delivered -or $observed.delivered -le 0 -or
            $null -eq $observed.latencyP95Ns -or $null -eq $observed.latencyP99Ns -or
            ($Precision -eq 'ns' -and $observed.nonzeroNanoParts -le 0) -or
            ($Contract -eq 'stream' -and $observed.delivered -ne $expected) -or
            ($Contract -eq 'ticker' -and ($observed.lastSequence -ne $expected -or $observed.delivered -gt $expected))) {
            throw "$($child.Name) delivery, sequence, fields or latency sample counts did not reconcile."
        }
        $observed
    })
    $processes = @(foreach ($child in $receivers) { Get-ProcessSummary $child })
    $totalDelivered = ($observations | Measure-Object -Property delivered -Sum).Sum
    $summary = [ordered]@{
        qdVersion = '3.357'
        javaVersion = $javaVersion
        jvmOptions = $jdkOptions
        os = [Runtime.InteropServices.RuntimeInformation]::OSDescription
        architecture = [string][Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture
        logicalProcessors = [Environment]::ProcessorCount
        producerSha256 = (Get-FileHash -LiteralPath $Program).Hash
        producerKind = $ProducerKind
        schemaSha256 = $schemaHash
        provenanceSha256 = $provenanceHash
        record = $record
        contract = $Contract.ToUpperInvariant()
        clients = $Clients
        precision = $Precision
        pacing = $Pacing
        listenAddress = $ListenAddress
        symbol = 'IBM'
        receiverRole = 'QD record verifier'
        offeredRps = $Rate
        nominalRecords = $nominalRecords
        nominalShortfall = $nominalRecords - $expected
        configuredBatch = $Batch
        sender = $native
        receiver = $observations[0]
        receivers = $observations
        totalDelivered = $totalDelivered
        sourceRecordsPerSecond = $expected / $native.seconds
        deliveredIncludingDrainPerProducerSecond = $totalDelivered / $native.seconds
        processes = @{
            producer = Get-ProcessSummary $producer
            receiver = $processes[0]
            receivers = $processes
        }
    }
} finally {
    $cleanupError = $null
    foreach ($child in @($receivers.ToArray(); $producer)) {
        try { Save-Child $child } catch { if (!$cleanupError) { $cleanupError = $_ } }
    }
    $samples | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $output 'process-samples.csv')
    if ($cleanupError) { throw $cleanupError }
}
$summary | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $output 'result.json')
Write-Output "Verified $totalDelivered delivered records across $Clients clients. Report: $output/result.json"
