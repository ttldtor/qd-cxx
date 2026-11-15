# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2

[CmdletBinding()]
param(
    [string]$Program,
    [string]$Bundle,
    [string]$Java,
    [string]$Classpath,
    [string]$OutputDirectory,
    [ValidateRange(1, 300)][int]$Duration = 10,
    [ValidateRange(1, 1000000)][int]$Rate = 1000,
    [ValidateRange(1, 32)][int]$Batch = 1,
    [switch]$Help,
    [switch]$HelpRu
)

$ErrorActionPreference = 'Stop'
if ($HelpRu) {
    Write-Output 'Использование: Measure-Stream.ps1 -Program ФАЙЛ -Bundle КАТАЛОГ -Java ФАЙЛ -Classpath QD357 -OutputDirectory НОВЫЙ_КАТАЛОГ'
    Write-Output 'Сверяет IBM TimeAndSale/STREAM с QD 3.357 на IPv4 loopback. Нужен JDK с javac.'
    Write-Output 'Параметры: -Duration 10 (1..300 с), -Rate 1000 (записей/с), -Batch 1 (1..32). Нагрузка должна укладываться в 22 бит.'
    Write-Output 'Сохраняет JSON, замеры процессов и журналы в новом каталоге. Завершает только свои дочерние процессы.'
    Write-Output 'Код 0 — доставка и сверка успешны, 1 — ошибка аргументов, сборки, замера или доставки.'
    exit 0
}
if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Usage: Measure-Stream.ps1 -Program FILE -Bundle DIR -Java FILE -Classpath QD357 -OutputDirectory NEW_DIR'
    Write-Output 'Verify IBM TimeAndSale/STREAM on IPv4 loopback with Java QD 3.357. Requires a JDK with javac.'
    Write-Output 'Options: -Duration 10 (1..300 seconds), -Rate 1000 (records/sec), -Batch 1 (1..32).'
    Write-Output 'Offered records plus one batch must fit the 22-bit sequence range.'
    Write-Output 'Writes JSON, process samples and logs to a new directory. Stops its own child processes.'
    Write-Output 'Exit 0 when delivery and validation succeed, 1 on an argument, build, measurement or delivery failure.'
    exit 0
}

if (!$Program -or !$Bundle -or !$Java -or !$Classpath -or !$OutputDirectory) {
    throw 'Supply Program, Bundle, Java, Classpath and OutputDirectory. Use -Help for details.'
}
$sequenceLimit = 1 -shl 22
if ([long]$Duration * $Rate + $Batch -ge $sequenceLimit) {
    throw 'The offered workload exceeds the unique sequence range.'
}

$Program = (Resolve-Path -LiteralPath $Program).Path
$Bundle = (Resolve-Path -LiteralPath $Bundle).Path
$Java = (Resolve-Path -LiteralPath $Java).Path
$javacName = if ($IsWindows) { 'javac.exe' } else { 'javac' }
$javac = (Resolve-Path -LiteralPath (Join-Path (Split-Path $Java) $javacName)).Path
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be new.' }
$output = (New-Item -Path $OutputDirectory -ItemType Directory).FullName
$classes = (New-Item -Path (Join-Path $output 'classes') -ItemType Directory).FullName
$schema = Join-Path $Bundle 'schema.json'
$provenance = Join-Path $Bundle 'provenance.json'
$schemaHash = (Get-FileHash -LiteralPath $schema).Hash
$provenanceHash = (Get-FileHash -LiteralPath $provenance).Hash
$PSNativeCommandUseErrorActionPreference = $false
& $javac -cp $Classpath -d $classes (Join-Path $PSScriptRoot 'StreamReceiver.java') *> (Join-Path $output 'javac.log')
if ($LASTEXITCODE -ne 0) { throw "Receiver compilation failed. See $output/javac.log." }
$javaVersion = (& $Java -version 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot read Java version.' }

$clock = [Diagnostics.Stopwatch]::StartNew()
$samples = [Collections.Generic.List[object]]::new()
$jdkOptions = @('-Djava.net.preferIPv4Stack=true', '-Dcom.devexperts.qd.monitoring.MonitoredQDEndpoint$BuilderImpl.disable=true')

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

function Wait-Marker($Child, [string]$Pattern, [int]$TimeoutMs, $Other = $null) {
    $deadline = $clock.ElapsedMilliseconds + $TimeoutMs
    while ($clock.ElapsedMilliseconds -lt $deadline) {
        if ($Child.Pending.Wait(100)) {
            $line = $Child.Pending.GetAwaiter().GetResult()
            $Child.Pending = $null
            if ($null -eq $line) { throw "$($Child.Name) closed stdout before readiness." }
            $Child.Prefix.Add($line)
            if ($line -match $Pattern) {
                $Child.Output = $Child.Process.StandardOutput.ReadToEndAsync()
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
        Add-Sample $Other $Phase
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

function Get-ProcessSummary([string]$Name) {
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
        pid = if ($Name -eq 'producer') { $producer.Process.Id } else { $receiver.Process.Id }
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

$producer = $null
$receiver = $null
$summary = $null
try {
    $producer = Start-Child $Program @('p', '--bundle', $Bundle, '--port', '0',
        '--duration', "$Duration", '--rate', "$Rate", '--batch', "$Batch", '--startup', '25',
        '--record', 'TimeAndSale', '--symbol', 'IBM', '--contract', 'stream') producer
    $ready = Wait-Marker $producer '^PORT [1-9][0-9]*$' 10000
    $port = $ready.Substring(5)
    $receiverClasspath = $classes + [IO.Path]::PathSeparator + $Classpath
    $receiver = Start-Child $Java ($jdkOptions + @('-cp', $receiverClasspath,
        'StreamReceiver', "127.0.0.1:$port", 'IBM')) receiver
    $null = Wait-Marker $receiver '^ACTIVE$' 20000 $producer
    Wait-Child $producer (($Duration + 8) * 1000) active $receiver
    if ($producer.Process.ExitCode -ne 0) { throw "Record NetTest exited $($producer.Process.ExitCode)." }

    $native = @{}
    foreach ($field in (Get-ResultLine (Read-Text $producer.Output)).Split(' ')) {
        $parts = $field.Split('=', 2)
        if ($parts.Count -ne 2) { throw 'Malformed native RESULT field.' }
        $native[$parts[0]] = [double]::Parse($parts[1], [Globalization.CultureInfo]::InvariantCulture)
    }
    $written = [long]$native.written
    if ($written -le 0 -or $written -ge $sequenceLimit -or $native.connections -ne 1 -or $native.generated -ne $written -or
        $native.composed -ne $written -or $native.seconds -le 0) {
        throw 'Producer count, connection or measurement-window check failed.'
    }

    $receiver.Process.StandardInput.WriteLine([string]$written)
    $receiver.Process.StandardInput.Close()
    Wait-Child $receiver 10000 drain
    if ($receiver.Process.ExitCode -ne 0) { throw "Java receiver exited $($receiver.Process.ExitCode)." }
    $observed = Get-ResultLine (Read-Text $receiver.Output) | ConvertFrom-Json
    if ($observed.qdVersion -cne 'QDS-3.357' -or $observed.expected -ne $written -or $observed.delivered -ne $written -or
        $observed.sequenceErrors -ne 0 -or $observed.fieldErrors -ne 0 -or
        $observed.latencySamples -ne $written) {
        throw 'Java delivery, sequence, fields or latency sample counts did not reconcile.'
    }
    $summary = [ordered]@{
        qdVersion = '3.357'
        javaVersion = $javaVersion
        jvmOptions = $jdkOptions
        os = [Runtime.InteropServices.RuntimeInformation]::OSDescription
        architecture = [string][Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture
        logicalProcessors = [Environment]::ProcessorCount
        producerSha256 = (Get-FileHash -LiteralPath $Program).Hash
        schemaSha256 = $schemaHash
        provenanceSha256 = $provenanceHash
        record = 'TimeAndSale'
        contract = 'STREAM'
        symbol = 'IBM'
        receiverRole = 'QD record verifier'
        offeredRps = $Rate
        configuredBatch = $Batch
        sender = $native
        receiver = $observed
        deliveredIncludingDrainPerProducerSecond = $written / $native.seconds
        processes = @{
            producer = Get-ProcessSummary producer
            receiver = Get-ProcessSummary receiver
        }
    }
} finally {
    try {
        Save-Child $receiver
    } finally {
        try { Save-Child $producer } finally {
            $samples | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $output 'process-samples.csv')
        }
    }
}
$summary | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $output 'result.json')
Write-Output "Verified $written records. Report: $output/result.json"
