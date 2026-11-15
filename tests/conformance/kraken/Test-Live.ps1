# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
[CmdletBinding()]
param(
    [string]$Gateway,
    [string]$Client,
    [string]$TrustPem,
    [string]$Output,
    [switch]$Help
)
$ErrorActionPreference = 'Stop'

if ($Help) {
    Write-Output @"
Usage: Test-Live.ps1 -Gateway EXE -Client EXE -TrustPem FILE -Output NEW_DIRECTORY
Runs the public Kraken BTC/USD ticker through one QTP gateway into two receiver processes.
TrustPem selects explicit certificate authorities. The native client comes from the gateway conformance tests.
The gateway listens on IPv4 loopback. The script writes fresh stdout and stderr logs to Output.
Exit 0 means both receivers observed the same complete quote admitted by the gateway. Failure exits 1.
"@
    exit 0
}

if (!$Gateway -or !$Client -or !$TrustPem -or !$Output) {
    throw 'Gateway, Client, TrustPem and Output are required.'
}

$Gateway = (Resolve-Path -LiteralPath $Gateway).ProviderPath
$Client = (Resolve-Path -LiteralPath $Client).ProviderPath
$TrustPem = (Resolve-Path -LiteralPath $TrustPem).ProviderPath
if (Test-Path -LiteralPath $Output) { throw 'Use a new output directory.' }
$Output = (New-Item -ItemType Directory -Path $Output).FullName
$children = [System.Collections.Generic.List[object]]::new()

function Start-OwnedProcess([string]$Name, [string]$Path, [string[]]$Arguments) {
    $start = [System.Diagnostics.ProcessStartInfo]::new($Path)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($value in $Arguments) { $start.ArgumentList.Add($value) }
    $process = [System.Diagnostics.Process]::Start($start)
    $child = [pscustomobject]@{
        Name = $Name
        Process = $process
        Stderr = $process.StandardError.ReadToEndAsync()
        Startup = $null
        Stdout = $null
        FirstLine = ''
    }
    $children.Add($child)
    return $child
}

function Finish-OwnedProcess($Child) {
    if (!$Child.Process.WaitForExit(40000)) { throw "Process timed out: $($Child.Name)" }
    if ($Child.Process.ExitCode -ne 0) { throw "Process failed: $($Child.Name), exit $($Child.Process.ExitCode)" }
}

function Read-ClientQuotes([string]$Log) {
    $pattern = '^QUOTE BTC/USD (?<bid>-?\d+) (?<ask>-?\d+) (?<bidSize>-?\d+) (?<askSize>-?\d+) ' +
        '\S+ \S+ \S+ \S+ 0 0 0 0 0 0 0 0$'
    foreach ($line in $Log -split '\r?\n') {
        if ($line -notmatch '^QUOTE ') { continue }
        if ($line -notmatch $pattern) { throw "Unexpected client Quote: $line" }
        "$($Matches.bid) $($Matches.ask) $($Matches.bidSize) $($Matches.askSize)"
    }
}

try {
    $gatewayProcess = Start-OwnedProcess 'gateway' $Gateway @(
        '--trust-pem', $TrustPem, '--listen-port', '0', '--duration-ms', '25000', '--trace-quotes'
    )
    $gatewayProcess.Startup = $gatewayProcess.Process.StandardOutput.ReadLineAsync()
    $first = $gatewayProcess.Startup.WaitAsync([TimeSpan]::FromSeconds(8)).GetAwaiter().GetResult()
    $gatewayProcess.FirstLine = $first
    $gatewayProcess.Stdout = $gatewayProcess.Process.StandardOutput.ReadToEndAsync()
    if ($first -notmatch '^LISTEN 127\.0\.0\.1 ([1-9][0-9]{0,4})$' -or [int]$Matches[1] -gt 65535) {
        throw "Expected loopback listener, got: $first"
    }
    $address = "127.0.0.1:$($Matches[1])"
    $firstClient = Start-OwnedProcess 'client-a' $Client @($address, '12000')
    $firstClient.Stdout = $firstClient.Process.StandardOutput.ReadToEndAsync()
    $secondClient = Start-OwnedProcess 'client-b' $Client @($address, '12000')
    $secondClient.Stdout = $secondClient.Process.StandardOutput.ReadToEndAsync()
    if ($firstClient.Process.Id -eq $secondClient.Process.Id) { throw 'Expected independent clients' }

    Finish-OwnedProcess $firstClient
    Finish-OwnedProcess $secondClient
    Finish-OwnedProcess $gatewayProcess

    $gatewayRows = $gatewayProcess.Stdout.GetAwaiter().GetResult() -split '\r?\n'
    $admissions = @{}
    foreach ($line in $gatewayRows) {
        if ($line -match '^PUBLISH (?<generation>[1-9]\d*) (?<timestamp>\S+) (?<bid>-?\d+) (?<ask>-?\d+) (?<bidSize>-?\d+) (?<askSize>-?\d+)$') {
            $admissions["$($Matches.bid) $($Matches.ask) $($Matches.bidSize) $($Matches.askSize)"] = $line
        }
    }
    $left = @(Read-ClientQuotes ($firstClient.Stdout.GetAwaiter().GetResult()))
    $right = @(Read-ClientQuotes ($secondClient.Stdout.GetAwaiter().GetResult()))
    $shared = @($left | Where-Object {$_ -cin $right -and $admissions.ContainsKey($_)} | Select-Object -Unique)
    if ($shared.Count -eq 0) {
        throw "No common admitted quote: gateway=$($admissions.Count), client A=$($left.Count), client B=$($right.Count)"
    }
    if (!($gatewayRows -match '^STATS observed=\d+ published=[1-9]\d* sent_frames=[1-9]\d* retired_peers=\d+ pending_publications=0$')) {
        throw 'Expected successful output and a drained publication queue.'
    }
    $receipt = @(
        "UTC $([DateTime]::UtcNow.ToString('o'))"
        "PASS BTC/USD client-a=$($left.Count) client-b=$($right.Count) common=$($shared.Count)"
        $admissions[$shared[0]]
    )
} finally {
    $cleanupErrors = [System.Collections.Generic.List[string]]::new()
    foreach ($child in $children) {
        try {
            if (!$child.Process.HasExited) {
                $child.Process.Kill()
                if (!$child.Process.WaitForExit(5000)) { throw "Failed to stop $($child.Name)" }
            }
            if ($null -eq $child.Stdout) {
                if ($child.Startup) { $child.FirstLine = $child.Startup.GetAwaiter().GetResult() }
                $child.Stdout = $child.Process.StandardOutput.ReadToEndAsync()
            }
            $out = $child.Stdout.GetAwaiter().GetResult()
            if ($child.FirstLine) { $out = "$($child.FirstLine)`n$out" }
            [IO.File]::WriteAllText((Join-Path $Output "$($child.Name).stdout.log"), $out)
            [IO.File]::WriteAllText((Join-Path $Output "$($child.Name).stderr.log"), $child.Stderr.GetAwaiter().GetResult())
        } catch {
            $cleanupErrors.Add("$($child.Name): $_")
        } finally {
            $child.Process.Dispose()
        }
    }
    if ($cleanupErrors.Count -gt 0) { throw ($cleanupErrors -join [Environment]::NewLine) }
}
$receipt | Tee-Object -FilePath (Join-Path $Output 'result.txt')
