# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
#requires -Version 7.2
# Internal finite workload launched only by Test-ProcessMetrics.ps1. stdin GO starts two
# named worker threads, stdout DONE marks their completion; stdin STOP permits exit.
# The caller owns process cleanup and output capture. Invalid input exits unsuccessfully.
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @"
using System;
using System.Diagnostics;
using System.Threading;
using System.Runtime.InteropServices;
public static class ResourceWorkload {
    [DllImport("kernel32.dll")] private static extern IntPtr GetCurrentThread();
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] private static extern int SetThreadDescription(IntPtr thread, string name);
    private static void NameThread(string name) {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows)) { SetThreadDescription(GetCurrentThread(), name); }
    }
    public static void Run() {
        byte[] memory = new byte[8 * 1024 * 1024];
        for (int i = 0; i < memory.Length; i += 4096) { memory[i] = 1; }
        Console.WriteLine("READY");
        if (Console.ReadLine() != "GO") { throw new InvalidOperationException("Expected GO"); }
        var busy = new Thread(() => {
            NameThread("metrics-busy");
            var timer = Stopwatch.StartNew();
            double value = 1;
            while (timer.ElapsedMilliseconds < 1800) { value = Math.Sqrt(value + 1); }
            GC.KeepAlive(value);
        });
        busy.Name = "metrics-busy";
        var idle = new Thread(() => { NameThread("metrics-idle"); Thread.Sleep(1800); });
        idle.Name = "metrics-idle";
        busy.Start();
        idle.Start();
        busy.Join();
        idle.Join();
        Console.WriteLine("DONE");
        if (Console.ReadLine() != "STOP") { throw new InvalidOperationException("Expected STOP"); }
        GC.KeepAlive(memory);
    }
}
"@
[ResourceWorkload]::Run()
