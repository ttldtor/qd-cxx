# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

[CmdletBinding()]
param(
    [string]$Program,
    [string]$Bundle,
    [string]$Java,
    [string]$Classpath,
    [string]$OutputDirectory,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
if ($Help -or !$PSBoundParameters.Count) {
    Write-Output 'Usage: Test-Loopback.ps1 -Program FILE -Bundle DIRECTORY -OutputDirectory NEW_DIRECTORY [-Java FILE -Classpath CLASSPATH]'
    Write-Output 'Run isolated physical-record NetTest loopback cases. Optional paired Java and Classpath arguments enable QD 3.355 STREAM interoperation.'
    Write-Output 'Creates the requested new directory with stdout/stderr logs. Terminates only its own child processes.'
    exit 0
}

if (!$Program -or !$Bundle -or !$OutputDirectory -or [bool]$Java -ne [bool]$Classpath) {
    throw 'Required parameters are Program, Bundle, OutputDirectory, and either both or neither of Java and Classpath.'
}
$Program = (Resolve-Path -LiteralPath $Program).Path
$Bundle = (Resolve-Path -LiteralPath $Bundle).Path
if ($Java) { $Java = (Resolve-Path -LiteralPath $Java).Path }
if (Test-Path -LiteralPath $OutputDirectory) { throw "OutputDirectory must be new: $OutputDirectory" }
$OutputDirectory = (New-Item -Path $OutputDirectory -ItemType Directory).FullName

function Start-CaseProcess([string]$File, [string[]]$Arguments, [string]$Name) {
    $start = [Diagnostics.ProcessStartInfo]::new($File)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [Text.Encoding]::UTF8
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $ready = $null
    $output = if ($File -eq $Program -and $Arguments[0] -eq 'p') {
        $ready = $process.StandardOutput.ReadLineAsync()
        $null
    } else {
        $process.StandardOutput.ReadToEndAsync()
    }
    return @{
        Process = $process
        Ready = $ready
        Output = $output
        ErrorText = $process.StandardError.ReadToEndAsync()
        Name = $Name
    }
}

function Save-CaseProcess($Child) {
    if ($null -eq $Child) { return }
    try {
        if (!$Child.Process.HasExited) { $Child.Process.Kill($true) }
        if (!$Child.Process.WaitForExit(5000)) { throw "Process did not terminate: $($Child.Name)" }
        if ($Child.Ready) {
            if (!$Child.Ready.Wait(2000)) { throw "Readiness capture timed out: $($Child.Name)" }
            $pending = $Child.Process.StandardOutput.ReadToEndAsync()
            if (!$pending.Wait(2000)) { throw "Output capture timed out: $($Child.Name)" }
            @($Child.Ready.Result, $pending.Result) | Set-Content -LiteralPath "$OutputDirectory/$($Child.Name).stdout"
        } else {
            if (!$Child.Output.Wait(2000)) { throw "Output capture timed out: $($Child.Name)" }
            $Child.Output.Result | Set-Content -LiteralPath "$OutputDirectory/$($Child.Name).stdout"
        }
        if (!$Child.ErrorText.Wait(2000)) { throw "Error capture timed out: $($Child.Name)" }
        $Child.ErrorText.Result | Set-Content -LiteralPath "$OutputDirectory/$($Child.Name).stderr"
    } finally {
        $Child.Process.Dispose()
    }
}

function Wait-CaseSuccess($Child) {
    if (!$Child.Process.WaitForExit(20000)) { throw "Process timed out: $($Child.Name)" }
    if ($Child.Process.ExitCode -ne 0) { throw "$($Child.Name) exit $($Child.Process.ExitCode)" }
}

function New-Port {
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    $listener.Start()
    try { return $listener.LocalEndpoint.Port } finally { $listener.Stop() }
}

function Wait-Listener($Child, [int]$Port, [switch]$JavaPeer) {
    $deadline = [DateTime]::UtcNow.AddSeconds(9)
    do {
        if ($Child.Process.HasExited) { throw "$($Child.Name) exited before listening" }
        if ($JavaPeer) {
            $probe = [Net.Sockets.TcpClient]::new()
            try {
                $probe.Connect([Net.IPAddress]::Loopback, $Port)
                return
            } catch [Net.Sockets.SocketException] {
                # Probe only the external reference peer. Native acceptance counts are exact.
            } finally {
                $probe.Dispose()
            }
        } elseif ($Child.Ready.IsCompleted -and $Child.Ready.Result -eq "PORT $Port") {
            return
        }
        Start-Sleep -Milliseconds 40
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "$($Child.Name) did not listen on expected port"
}

function Assert-Result([string]$Name, [string]$Key, [int]$Minimum, [int]$Connections) {
    $result = Get-Content -LiteralPath "$OutputDirectory/$Name.stdout" -Raw
    if ($result -notmatch "(?m)^RESULT .*\bconnections=$Connections\b.*\b$Key=(?<value>[0-9]+)\b" -or
        [int]$Matches.value -lt $Minimum) {
        throw "Missing $Key >= $Minimum on $Connections connections: $result"
    }
    Write-Output "$Name ${Key}=$($Matches.value)"
}

foreach ($contract in 'stream', 'ticker', 'history') {
    $port = New-Port
    $instances = if ($contract -eq 'stream') { '2' } else { '1' }
    $common = @('--bundle', $Bundle, '--port', $port, '--contract', $contract, '--instances', $instances,
                '--symbol', 'IBM', '--symbol', 'MSFT')
    $p = Start-CaseProcess $Program (@('p', '--duration', '3', '--rate', '1000', '--batch', '10') + $common) "$contract-p"
    $c = $null
    try {
        Wait-Listener $p $port
        $history = if ($contract -eq 'history') { @('--history-bound', '-1') } else { @() }
        $c = Start-CaseProcess $Program (@('c', '--duration', '2', '--qd-text') + $common + $history) "$contract-c"
        Wait-CaseSuccess $c
        Wait-CaseSuccess $p
    } finally {
        try { Save-CaseProcess $c } finally { Save-CaseProcess $p }
    }
    Assert-Result "$contract-p" written 100 ([int]$instances)
    Assert-Result "$contract-c" delivered 100 ([int]$instances)
}

if ($Java) {
    $ipf = Join-Path $OutputDirectory 'symbols.ipf'
    @('#STOCK::=TYPE,SYMBOL', 'STOCK,IBM', '##COMPLETE') | Set-Content -Encoding ascii -LiteralPath $ipf
    $javaOptions = @('-Dcom.devexperts.qd.tools.NetTest.record=TimeAndSale',
        '-Dcom.devexperts.qd.tools.NetTest.batchSize=10', '-cp', $Classpath, 'com.devexperts.qd.tools.NetTest')
    $javaStream = @('-c', 'stream', '-S', "1@$ipf", '-s', '1s')

    $port = New-Port
    $p = Start-CaseProcess $Java ($javaOptions + @('p', ":${port}", '-r', '*/1000') + $javaStream) 'java-p'
    $c = $null
    try {
        Wait-Listener $p $port -JavaPeer
        $c = Start-CaseProcess $Program @('c', '--bundle', $Bundle, '--port', $port, '--duration', '3') 'java-native-c'
        Wait-CaseSuccess $c
    } finally {
        try { Save-CaseProcess $c } finally { Save-CaseProcess $p }
    }
    Assert-Result 'java-native-c' delivered 100 1

    $port = New-Port
    $p = Start-CaseProcess $Program @('p', '--bundle', $Bundle, '--port', $port, '--duration', '5', '--batch', '10') 'native-java-p'
    $c = $null
    try {
        Wait-Listener $p $port
        $c = Start-CaseProcess $Java ($javaOptions + @('c', "127.0.0.1:${port}") + $javaStream) 'java-c'
        Wait-CaseSuccess $p
    } finally {
        try { Save-CaseProcess $c } finally { Save-CaseProcess $p }
    }
    Assert-Result 'native-java-p' written 100 1
    $observed = Get-Content -LiteralPath "$OutputDirectory/java-c.stdout" -Raw
    if ($observed -notmatch '(?m)NetTestConsumerSide[^\r\n]+?data [1-9][,0-9]* rps[^\r\n]+?connections 1') {
        throw 'Java QD did not report a positive consumer rate'
    }
    Write-Output 'Java QD consumer reported a positive record rate'
}
