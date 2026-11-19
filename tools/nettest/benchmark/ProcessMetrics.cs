// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

// Loaded by ProcessMetrics.ps1 with Add-Type. Reads one live owned child's counters;
// failures are returned as unavailable observations, never as zero CPU or memory.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;

namespace Qdcxx.Benchmark {
    public sealed class ResourceSnapshot {
        public long StartTicks;
        public long EndTicks;
        public string Identity = "";
        public double? UserCpuMs;
        public double? SystemCpuMs;
        public long? WorkingSetBytes;
        public long? PrivateBytes;
        public long? OsPeakWorkingSetBytes;
        public string Status = "ok";
        public string PrivateBytesStatus = "ok";
        public string Affinity = "";
        public string AffinityStatus = "unavailable: platform has no reader";
        public List<ThreadSnapshot> Threads = new List<ThreadSnapshot>();
    }

    public sealed class ThreadSnapshot {
        public int Tid;
        public long StartTicks;
        public long EndTicks;
        public string Identity = "";
        public string Name = "";
        public double? UserCpuMs;
        public double? SystemCpuMs;
        public string Status = "ok";
    }

    public sealed class ResourceReader {
        private readonly Process process;
        private string identity;
        public string Identity { get { return identity; } }
        private readonly bool windows = RuntimeInformation.IsOSPlatform(OSPlatform.Windows);
        private readonly bool linux = RuntimeInformation.IsOSPlatform(OSPlatform.Linux);
        private readonly double clockTicks;

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenThread(uint access, bool inherit, uint tid);
        [DllImport("kernel32.dll")]
        private static extern uint GetProcessIdOfThread(IntPtr thread);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetThreadTimes(IntPtr thread, out long created, out long exited,
            out long kernel, out long user);
        [DllImport("kernel32.dll")]
        private static extern bool CloseHandle(IntPtr handle);
        [DllImport("kernel32.dll")]
        private static extern int GetThreadDescription(IntPtr thread, out IntPtr description);
        [DllImport("kernel32.dll")]
        private static extern IntPtr LocalFree(IntPtr memory);
        [DllImport("libc", SetLastError = true)]
        private static extern long sysconf(int name);

        public ResourceReader(Process ownedProcess) {
            process = ownedProcess;
            // Linux _SC_CLK_TCK is 2; do not assume a 100 Hz kernel counter.
            clockTicks = linux ? sysconf(2) : 0;
            if (linux && clockTicks <= 0) {
                throw new InvalidOperationException("sysconf(_SC_CLK_TCK) failed");
            }
            identity = windows ? process.StartTime.ToUniversalTime().Ticks.ToString(CultureInfo.InvariantCulture) : "";
            if (linux) {
                identity = ReadStat("/proc/" + process.Id + "/stat")[19];
            }
        }

        private static string[] ReadStat(string path) {
            string stat = File.ReadAllText(path);
            int end = stat.LastIndexOf(')');
            if (end < 0) {
                throw new InvalidDataException("Malformed proc stat");
            }
            // The command name may itself contain spaces and parentheses. Index 0 is field 3.
            return stat.Substring(end + 2).Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
        }

        private static long Number(string value) {
            return long.Parse(value, CultureInfo.InvariantCulture);
        }

        private ThreadSnapshot ReadWindowsThread(int tid) {
            var sample = new ThreadSnapshot { Tid = tid, StartTicks = Stopwatch.GetTimestamp() };
            const uint QUERY_LIMITED_INFORMATION = 0x0800;
            IntPtr handle = OpenThread(QUERY_LIMITED_INFORMATION, false, (uint)tid);
            if (handle == IntPtr.Zero) {
                sample.Status = "unavailable: OpenThread error " + Marshal.GetLastWin32Error();
                if (sample.EndTicks == 0) { sample.EndTicks = Stopwatch.GetTimestamp(); }
                return sample;
            }
            try {
                if (GetProcessIdOfThread(handle) != (uint)process.Id) {
                    sample.Status = "unavailable: TID owner changed";
                    if (sample.EndTicks == 0) { sample.EndTicks = Stopwatch.GetTimestamp(); }
                    return sample;
                }
                long created, exited, kernel, user;
                if (!GetThreadTimes(handle, out created, out exited, out kernel, out user)) {
                    sample.Status = "unavailable: GetThreadTimes error " + Marshal.GetLastWin32Error();
                    if (sample.EndTicks == 0) { sample.EndTicks = Stopwatch.GetTimestamp(); }
                    return sample;
                }
                sample.Identity = created.ToString(CultureInfo.InvariantCulture);
                sample.UserCpuMs = user / 10000.0;
                sample.SystemCpuMs = kernel / 10000.0;
                IntPtr description;
                if (GetThreadDescription(handle, out description) >= 0 && description != IntPtr.Zero) {
                    try { sample.Name = Marshal.PtrToStringUni(description) ?? ""; }
                    finally { LocalFree(description); }
                }
                if (sample.EndTicks == 0) { sample.EndTicks = Stopwatch.GetTimestamp(); }
                return sample;
            } finally {
                CloseHandle(handle);
                sample.EndTicks = Stopwatch.GetTimestamp();
            }
        }

        public ResourceSnapshot Read() {
            var sample = new ResourceSnapshot { Identity = identity, StartTicks = Stopwatch.GetTimestamp() };
            try {
                if (windows) {
                    process.Refresh();
                    if (process.HasExited) {
                        sample.Status = "unavailable: process exited";
                        if (sample.EndTicks == 0) { sample.EndTicks = Stopwatch.GetTimestamp(); }
                        return sample;
                    }
                    sample.UserCpuMs = process.UserProcessorTime.TotalMilliseconds;
                    sample.SystemCpuMs = process.PrivilegedProcessorTime.TotalMilliseconds;
                    sample.WorkingSetBytes = process.WorkingSet64;
                    sample.PrivateBytes = process.PrivateMemorySize64;
                    sample.OsPeakWorkingSetBytes = process.PeakWorkingSet64;
                    try {
                        sample.Affinity = unchecked((ulong)process.ProcessorAffinity.ToInt64()).ToString("X", CultureInfo.InvariantCulture);
                        sample.AffinityStatus = "sampled Windows process affinity mask (hex); processor groups may limit its scope";
                    } catch (System.ComponentModel.Win32Exception error) {
                        sample.AffinityStatus = "unavailable: " + error.Message;
                    }
                    sample.EndTicks = Stopwatch.GetTimestamp();
                    foreach (ProcessThread thread in process.Threads) {
                        try { sample.Threads.Add(ReadWindowsThread(thread.Id)); }
                        finally { thread.Dispose(); }
                    }
                } else if (linux) {
                    string root = "/proc/" + process.Id;
                    string[] stat = ReadStat(root + "/stat");
                    if (stat[19] != identity) {
                        sample.Status = "unavailable: PID identity changed";
                        if (sample.EndTicks == 0) { sample.EndTicks = Stopwatch.GetTimestamp(); }
                        return sample;
                    }
                    sample.UserCpuMs = Number(stat[11]) * 1000.0 / clockTicks;
                    sample.SystemCpuMs = Number(stat[12]) * 1000.0 / clockTicks;
                    foreach (string line in File.ReadLines(root + "/status")) {
                        string[] parts = line.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                        if (parts[0] == "Cpus_allowed_list:") {
                            sample.Affinity = parts[1];
                            sample.AffinityStatus = "sampled Linux Cpus_allowed_list";
                        }
                        if (parts[0] == "VmRSS:") { sample.WorkingSetBytes = Number(parts[1]) * 1024; }
                        if (parts[0] == "VmHWM:") { sample.OsPeakWorkingSetBytes = Number(parts[1]) * 1024; }
                    }
                    sample.PrivateBytesStatus = "unavailable: Linux private commitment has no Windows-equivalent counter";
                    sample.EndTicks = Stopwatch.GetTimestamp();
                    foreach (string directory in Directory.GetDirectories(root + "/task")) {
                        var thread = new ThreadSnapshot { Tid = int.Parse(Path.GetFileName(directory), CultureInfo.InvariantCulture), StartTicks = Stopwatch.GetTimestamp() };
                        try {
                            string[] taskStat = ReadStat(directory + "/stat");
                            thread.Identity = taskStat[19];
                            thread.Name = File.ReadAllText(directory + "/comm").TrimEnd('\n');
                            thread.UserCpuMs = Number(taskStat[11]) * 1000.0 / clockTicks;
                            thread.SystemCpuMs = Number(taskStat[12]) * 1000.0 / clockTicks;
                        } catch (IOException) { thread.Status = "unavailable: thread exited during sample"; }
                        catch (UnauthorizedAccessException) { thread.Status = "unavailable: thread access denied"; }
                        thread.EndTicks = Stopwatch.GetTimestamp();
                        sample.Threads.Add(thread);
                    }
                    if (ReadStat(root + "/stat")[19] != identity) {
                        sample.Status = "unavailable: PID identity changed during sample";
                    }
                } else {
                    sample.Status = "unavailable: supported platforms are Windows and Linux";
                }
            } catch (Exception error) when (error is IOException || error is InvalidOperationException ||
                error is System.ComponentModel.Win32Exception || error is UnauthorizedAccessException) {
                sample.Status = "unavailable: " + error.GetType().Name + ": " + error.Message;
            }
            if (sample.EndTicks == 0) { sample.EndTicks = Stopwatch.GetTimestamp(); }
            return sample;
        }
    }
}
