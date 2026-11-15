// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

using System;
using System.Diagnostics;
using System.IO;
using System.Threading;

/// <summary>Preloads a validated synthetic schedule and measures its release times without network I/O.</summary>
public static class BurstClockV1
{
    private const int FileHeaderBytes = 16;
    private const int MaxDatagrams = 2000000;
    private const int MaxPayloadBytes = 1472;
    private const long NanosPerSecond = 1000000000L;

    public sealed class Result
    {
        public string Scope { get; set; } = "clock-only";
        public long ClockFrequencyHz { get; set; }
        public long WindowNanoseconds { get; set; }
        public long ScheduledDatagrams { get; set; }
        public long ScheduledMessages { get; set; }
        public long ReleasedDatagramsInWindow { get; set; }
        public long ReleasedMessagesInWindow { get; set; }
        public long ReleasedDatagramsTotal { get; set; }
        public long ReleasedMessagesTotal { get; set; }
        public long ActualDurationNanoseconds { get; set; }
        public long LatenessP99Nanoseconds { get; set; }
        public long LatenessMaxNanoseconds { get; set; }
    }

    private readonly struct Entry
    {
        public readonly long DueTicks;
        public readonly uint Messages;

        public Entry(long dueTicks, uint messages)
        {
            DueTicks = dueTicks;
            Messages = messages;
        }
    }

    private static long ToTicksCeiling(ulong nanoseconds, long frequency)
    {
        return checked((long)Math.Ceiling((decimal)nanoseconds * frequency / NanosPerSecond));
    }

    private static long ToNanoseconds(long ticks, long frequency)
    {
        return checked((long)((decimal)ticks * NanosPerSecond / frequency));
    }

    /// <summary>Measure all releases with one monotonic clock after loading the complete schedule.</summary>
    /// <param name="path">Path to a corpus validated by the caller.</param>
    /// <param name="datagramCount">Expected number of scheduled datagrams.</param>
    /// <param name="messageCount">Expected number of messages represented by the schedule.</param>
    /// <param name="windowMilliseconds">Offered window duration in milliseconds.</param>
    /// <returns>Counts and lateness in clock-derived nanoseconds.</returns>
    public static Result Run(string path, int datagramCount, int messageCount, int windowMilliseconds)
    {
        if (datagramCount < 1 || datagramCount > MaxDatagrams || messageCount < 1 ||
            windowMilliseconds < 1 || windowMilliseconds > 600000)
            throw new InvalidDataException("Schedule parameters are outside supported bounds.");

        long frequency = Stopwatch.Frequency;
        long windowNanos = checked((long)windowMilliseconds * 1000000L);
        long windowTicks = ToTicksCeiling((ulong)windowNanos, frequency);
        var entries = new Entry[datagramCount];
        long loadedMessages = 0;
        using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (var reader = new BinaryReader(stream))
        {
            if (stream.Length < FileHeaderBytes)
                throw new InvalidDataException("Corpus header is truncated.");

            stream.Position = FileHeaderBytes;
            ulong previousOffset = 0;
            for (int i = 0; i < entries.Length; ++i)
            {
                if (stream.Length - stream.Position < FileHeaderBytes)
                    throw new InvalidDataException("Datagram header is truncated.");

                ulong offset = reader.ReadUInt64();
                uint bytes = reader.ReadUInt32();
                uint messages = reader.ReadUInt32();
                if (offset >= (ulong)windowNanos || (i > 0 && offset < previousOffset) ||
                    bytes < FileHeaderBytes || bytes > MaxPayloadBytes || messages == 0 ||
                    bytes > stream.Length - stream.Position)
                    throw new InvalidDataException("Invalid schedule entry.");

                entries[i] = new Entry(ToTicksCeiling(offset, frequency), messages);
                previousOffset = offset;
                loadedMessages = checked(loadedMessages + messages);
                stream.Position += bytes;
            }
            if (stream.Position != stream.Length || loadedMessages != messageCount)
                throw new InvalidDataException("Schedule totals do not match the manifest.");
        }

        var lateness = new long[entries.Length];
        long inWindowDatagrams = 0;
        long inWindowMessages = 0;
        long totalMessages = 0;
        long start = Stopwatch.GetTimestamp();
        long end = start;

        for (int i = 0; i < entries.Length; ++i)
        {
            Entry entry = entries[i];
            long elapsed;
            do
            {
                elapsed = Stopwatch.GetTimestamp() - start;
                if (elapsed < entry.DueTicks)
                    Thread.SpinWait(32);
            } while (elapsed < entry.DueTicks);

            lateness[i] = elapsed - entry.DueTicks;
            totalMessages += entry.Messages;
            if (elapsed < windowTicks)
            {
                ++inWindowDatagrams;
                inWindowMessages += entry.Messages;
            }
            end = elapsed;
        }

        Array.Sort(lateness);
        int percentile = checked((int)(((long)lateness.Length * 99 + 99) / 100) - 1);
        return new Result
        {
            ClockFrequencyHz = frequency,
            WindowNanoseconds = windowNanos,
            ScheduledDatagrams = entries.Length,
            ScheduledMessages = loadedMessages,
            ReleasedDatagramsInWindow = inWindowDatagrams,
            ReleasedMessagesInWindow = inWindowMessages,
            ReleasedDatagramsTotal = entries.Length,
            ReleasedMessagesTotal = totalMessages,
            ActualDurationNanoseconds = ToNanoseconds(end, frequency),
            LatenessP99Nanoseconds = ToNanoseconds(lateness[percentile], frequency),
            LatenessMaxNanoseconds = ToNanoseconds(lateness[lateness.Length - 1], frequency)
        };
    }
}
