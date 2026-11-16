# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2
param([Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be new.' }
$out = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
$corpus = Join-Path $out 'input'
$generator = Join-Path $PSScriptRoot '../burst/Corpus.ps1'
$multicast = Join-Path $PSScriptRoot 'Multicast.ps1'
$shell = (Get-Process -Id $PID).Path
& $shell -NoProfile -File $generator -Mode Generate -Directory $corpus -Messages 137 -WindowMs 500 -PayloadLimit 128 -Seed 13
if ($LASTEXITCODE -ne 0) { throw 'Corpus generation failed.' }

$lease = [Net.Sockets.UdpClient]::new(0)
$port = ([Net.IPEndPoint]$lease.Client.LocalEndPoint).Port
$lease.Dispose()
$group = '239.255.88.7'
$hostAddress = '127.0.0.1'

function Start-Receiver([string]$Name) {
    $info = [Diagnostics.ProcessStartInfo]::new($shell)
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($arg in @('-NoProfile','-File',$multicast,'-Mode','Receive','-Directory',$corpus,'-Group',$group,
        '-Port',"$port",'-Interface',$hostAddress,'-Output',"$out/$Name.json",'-TimeoutMs','5000')) {
        $info.ArgumentList.Add($arg)
    }
    $process = [Diagnostics.Process]::Start($info)
    $ready = $process.StandardOutput.ReadLineAsync()
    if (!$ready.Wait(10000) -or $ready.Result -ne 'READY') {
        if (!$process.HasExited) { $process.Kill($true) }
        throw "Receiver $Name did not become ready."
    }
    return $process
}

function Finish-Receiver($Process, [string]$Name) {
    try {
        if (!$Process.WaitForExit(8000)) { throw "Receiver $Name timed out." }
        if ($Process.ExitCode -ne 0) { throw "Receiver $Name failed: $($Process.StandardError.ReadToEnd())" }
        return (Get-Content "$out/$Name.json" -Raw | ConvertFrom-Json).measurement
    } finally {
        if (!$Process.HasExited) { $Process.Kill($true); $Process.WaitForExit() }
        $Process.Dispose()
    }
}

$p = Start-Receiver 'complete'
try {
    & $shell -NoProfile -File $multicast -Mode Send -Directory $corpus -Group $group -Port $port -Interface $hostAddress -Output "$out/send.json"
    if ($LASTEXITCODE -ne 0) { throw 'Sender failed.' }
    $receiverProcess = $p
    $p = $null
    $receiver = Finish-Receiver $receiverProcess 'complete'
} finally { if ($p) { if (!$p.HasExited) { $p.Kill($true); $p.WaitForExit() }; $p.Dispose() } }
$sender = (Get-Content "$out/send.json" -Raw | ConvertFrom-Json).measurement
if ($sender.SentDatagrams -ne 69 -or $sender.SendErrors -ne 0 -or $receiver.UniqueDatagrams -ne 69 -or
    $receiver.UniqueMessages -ne 137 -or !$receiver.Completed) { throw 'Loopback delivery mismatch.' }

$p = Start-Receiver 'altered'
try {
    $file = [IO.File]::OpenRead((Join-Path $corpus 'corpus.bin'))
    $reader = [IO.BinaryReader]::new($file)
    $packets = @()
    try {
        $file.Position = 16
        foreach ($i in 0..2) {
            $null = $reader.ReadUInt64()
            $bytes = $reader.ReadUInt32()
            $null = $reader.ReadUInt32()
            $packets += ,$reader.ReadBytes([int]$bytes)
        }
    } finally { $reader.Dispose() }
    $socket = [Net.Sockets.Socket]::new([Net.Sockets.AddressFamily]::InterNetwork,
        [Net.Sockets.SocketType]::Dgram, [Net.Sockets.ProtocolType]::Udp)
    try {
        $socket.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP, [Net.Sockets.SocketOptionName]::MulticastInterface,
            [Net.IPAddress]::Parse($hostAddress).GetAddressBytes())
        $socket.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP, [Net.Sockets.SocketOptionName]::MulticastTimeToLive, 1)
        $socket.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP, [Net.Sockets.SocketOptionName]::MulticastLoopback, $true)
        $socket.Connect([Net.IPEndPoint]::new([Net.IPAddress]::Parse($group), $port))
        foreach ($packet in @($packets[1], $packets[0], $packets[0])) { $null = $socket.Send([byte[]]$packet) }
        $corrupt = [byte[]]$packets[2].Clone()
        $corrupt[$corrupt.Length - 1] = $corrupt[$corrupt.Length - 1] -bxor 1
        $null = $socket.Send($corrupt)
        $oversized = [byte[]]::new(2000)
        [Array]::Copy($packets[2], $oversized, $packets[2].Length)
        $null = $socket.Send($oversized)
        $unknown = [byte[]]::new(16)
        $null = $socket.Send($unknown)
    } finally { $socket.Dispose() }
    $receiverProcess = $p
    $p = $null
    $receiver = Finish-Receiver $receiverProcess 'altered'
} finally { if ($p) { if (!$p.HasExited) { $p.Kill($true); $p.WaitForExit() }; $p.Dispose() } }
if ($receiver.UniqueDatagrams -ne 2 -or $receiver.MissingDatagrams -ne 67 -or
    $receiver.DuplicateDatagrams -ne 1 -or $receiver.OutOfOrderDatagrams -ne 1 -or
    $receiver.CorruptDatagrams -ne 2 -or $receiver.UnknownDatagrams -ne 1 -or $receiver.Completed) {
    throw 'Altered datagram counters differ from the injected sequence.'
}
Write-Output 'Multicast loopback and altered-datagram checks passed.'
