// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

using System;
using System.Buffers.Binary;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Threading;

/// <summary>Replay and check a bounded prevalidated corpus on an IPv4 multicast group.</summary>
public static class BurstMulticastV1
{
    private const int HeaderBytes = 16;
    private const int MaximumPayloadBytes = 1472;
    private const int MaximumMessages = 2000000;
    private const int LinuxBatchSize = 32;
    private const ulong LinuxBatchDueSpanNanoseconds = 100000;
    private const int LinuxDontWait = 0x40;
    private const int LinuxInterrupted = 4;
    private const int LinuxWouldBlock = 11;
    private const long NanosPerSecond = 1000000000L;
    private static readonly byte[] CorpusMagic = { 81, 66, 85, 82, 83, 84, 48, 49 };
    private static readonly byte[] DatagramMagic = { 81, 68, 80, 49 };

    public sealed class Entry
    {
        public long DueTicks;
        public int Messages;
        public byte[] Payload = Array.Empty<byte>();
    }

    public sealed class Corpus
    {
        public Entry[] Entries = Array.Empty<Entry>();
        public int Messages;
        public long WindowTicks;
        public long WindowNanoseconds;
    }

    public sealed class SenderResult
    {
        public long ClockFrequencyHz { get; set; }
        public long WindowNanoseconds { get; set; }
        public long PacingWindowNanoseconds { get; set; }
        public int ScheduledDatagrams { get; set; }
        public int ScheduledMessages { get; set; }
        public int AttemptedDatagrams { get; set; }
        public int SentDatagrams { get; set; }
        public int SentMessages { get; set; }
        public int SentDatagramsInWindow { get; set; }
        public int SentMessagesInWindow { get; set; }
        public long PayloadBytesSent { get; set; }
        public long SendErrors { get; set; }
        public long LatenessP99Nanoseconds { get; set; }
        public long LatenessMaxNanoseconds { get; set; }
        public long ElapsedNanoseconds { get; set; }
    }

    public sealed class ReceiverResult
    {
        public int ExpectedDatagrams { get; set; }
        public int ExpectedMessages { get; set; }
        public int UniqueDatagrams { get; set; }
        public int UniqueMessages { get; set; }
        public int UniqueDatagramsInWindow { get; set; }
        public int UniqueMessagesInWindow { get; set; }
        public long ReceiveSpanNanoseconds { get; set; }
        public int MissingDatagrams { get; set; }
        public int MissingMessages { get; set; }
        public int DuplicateDatagrams { get; set; }
        public int OutOfOrderDatagrams { get; set; }
        public int CorruptDatagrams { get; set; }
        public int UnknownDatagrams { get; set; }
        public long PayloadBytesReceived { get; set; }
        public int SocketReceiveBufferBytes { get; set; }
        public bool Completed { get; set; }
    }

    private static long ToTicks(ulong nanos)
    {
        return checked((long)Math.Ceiling((decimal)nanos * Stopwatch.Frequency / NanosPerSecond));
    }

    private static long ToNanos(long ticks)
    {
        return checked((long)((decimal)ticks * NanosPerSecond / Stopwatch.Frequency));
    }

    /// <summary>Read the complete validated corpus before any network measurement.</summary>
    public static Corpus Load(string path, int count, int messages, int windowMs, string expectedHash)
    {
        if (count < 1 || count > MaximumMessages || messages < 1 || messages > MaximumMessages ||
            windowMs < 1 || windowMs > 600000)
            throw new InvalidDataException("Invalid corpus dimensions.");

        var result = new Corpus {
            Entries = new Entry[count],
            Messages = messages,
            WindowNanoseconds = checked((long)windowMs * 1000000L)
        };
        result.WindowTicks = ToTicks((ulong)result.WindowNanoseconds);
        using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (var reader = new BinaryReader(stream))
        {
            byte[] magic = reader.ReadBytes(8);
            if (!magic.AsSpan().SequenceEqual(CorpusMagic) || reader.ReadUInt32() != messages)
                throw new InvalidDataException("Invalid corpus header.");
            uint limit = reader.ReadUInt32();
            if (limit < 128 || limit > MaximumPayloadBytes)
                throw new InvalidDataException("Invalid corpus payload limit.");

            long total = 0;
            ulong previous = 0;
            for (int i = 0; i < count; ++i)
            {
                ulong offset = reader.ReadUInt64();
                uint size = reader.ReadUInt32();
                uint entryMessages = reader.ReadUInt32();
                if (offset >= (ulong)result.WindowNanoseconds || (i != 0 && offset < previous) ||
                    size < HeaderBytes || size > limit || entryMessages == 0 || entryMessages > messages)
                    throw new InvalidDataException("Invalid corpus entry.");

                byte[] payload = reader.ReadBytes((int)size);
                if (payload.Length != size || !payload.AsSpan(0, 4).SequenceEqual(DatagramMagic) ||
                    BinaryPrimitives.ReadUInt32LittleEndian(payload.AsSpan(4, 4)) != i || BinaryPrimitives.ReadUInt64LittleEndian(payload.AsSpan(8, 8)) != (ulong)total)
                    throw new InvalidDataException("Invalid datagram header.");

                result.Entries[i] = new Entry { DueTicks = ToTicks(offset), Messages = (int)entryMessages, Payload = payload };
                previous = offset;
                total = checked(total + entryMessages);
            }
            if (stream.Position != stream.Length || total != messages)
                throw new InvalidDataException("Corpus totals differ from the manifest.");
            stream.Position = 0;
            using var sha = SHA256.Create();
            string hash = Convert.ToHexString(sha.ComputeHash(stream));
            if (!hash.Equals(expectedHash, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Corpus changed before it was loaded.");
        }
        return result;
    }

    private static void AwaitDue(long start, long due)
    {
        while (Stopwatch.GetTimestamp() - start < due)
            Thread.SpinWait(32);
    }

    private static void RecordSend(Corpus corpus, SenderResult result, Entry entry, int sent, long elapsed)
    {
        ++result.AttemptedDatagrams;
        if (sent != entry.Payload.Length) {
            ++result.SendErrors;
            return;
        }

        ++result.SentDatagrams;
        result.SentMessages += entry.Messages;
        result.PayloadBytesSent += sent;
        if (elapsed < corpus.WindowTicks) {
            ++result.SentDatagramsInWindow;
            result.SentMessagesInWindow += entry.Messages;
        }
    }

    private static int SendOne(Socket socket, Entry entry)
    {
        try { return socket.Send(entry.Payload); }
        catch (SocketException) { return -1; }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct LinuxIoVector
    {
        public IntPtr Base;
        public UIntPtr Length;
    }

    // Separate msghdr from msg_len so native tail padding remains part of msghdr.
    [StructLayout(LayoutKind.Sequential)]
    private struct LinuxMessageHeader
    {
        public IntPtr Name;
        public uint NameLength;
        public IntPtr Vectors;
        public UIntPtr VectorCount;
        public IntPtr Control;
        public UIntPtr ControlLength;
        public int Flags;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct LinuxMessage
    {
        public LinuxMessageHeader Header;
        public uint Length;
    }

    [DllImport("libc", EntryPoint = "sendmmsg", SetLastError = true)]
    private static extern int SendMessages(SafeSocketHandle socket, IntPtr messages, uint count, int flags);

    private static long SendLinuxBatches(Socket socket, Corpus corpus, long[] due, long[] lateness, SenderResult result)
    {
        var storage = new byte[LinuxBatchSize * MaximumPayloadBytes];
        var vectors = new LinuxIoVector[LinuxBatchSize];
        var messages = new LinuxMessage[LinuxBatchSize];
        GCHandle storagePin = default;
        GCHandle vectorPin = default;
        GCHandle messagePin = default;
        try {
            storagePin = GCHandle.Alloc(storage, GCHandleType.Pinned);
            vectorPin = GCHandle.Alloc(vectors, GCHandleType.Pinned);
            messagePin = GCHandle.Alloc(messages, GCHandleType.Pinned);
            IntPtr messageBase = messagePin.AddrOfPinnedObject();
            int messageSize = Marshal.SizeOf<LinuxMessage>();
            int vectorSize = Marshal.SizeOf<LinuxIoVector>();
            for (int i = 0; i < LinuxBatchSize; ++i) {
                vectors[i].Base = IntPtr.Add(storagePin.AddrOfPinnedObject(), i * MaximumPayloadBytes);
                messages[i].Header.Vectors = IntPtr.Add(vectorPin.AddrOfPinnedObject(), i * vectorSize);
                messages[i].Header.VectorCount = (UIntPtr)1;
            }

            long batchDueSpan = ToTicks(LinuxBatchDueSpanNanoseconds);
            SafeSocketHandle handle = socket.SafeHandle;
            long start = Stopwatch.GetTimestamp();
            for (int first = 0; first < corpus.Entries.Length;) {
                int limit = Math.Min(LinuxBatchSize, corpus.Entries.Length - first);
                int count = 1;
                while (count < limit && due[first + count] - due[first] <= batchDueSpan)
                    ++count;
                for (int i = 0; i < count; ++i) {
                    byte[] payload = corpus.Entries[first + i].Payload;
                    Buffer.BlockCopy(payload, 0, storage, i * MaximumPayloadBytes, payload.Length);
                    vectors[i].Length = (UIntPtr)payload.Length;
                }

                // Submit after the last datagram becomes due. Timestamp the batch at return.
                AwaitDue(start, due[first + count - 1]);
                int offset = 0;
                while (offset < count) {
                    int sent = SendMessages(handle, IntPtr.Add(messageBase, offset * messageSize),
                        (uint)(count - offset), LinuxDontWait);
                    if (sent <= 0) {
                        int error = Marshal.GetLastPInvokeError();
                        Entry entry = corpus.Entries[first + offset];
                        int length = error is LinuxInterrupted or LinuxWouldBlock ? SendOne(socket, entry) : -1;
                        long elapsed = Stopwatch.GetTimestamp() - start;
                        RecordSend(corpus, result, entry, length, elapsed);
                        lateness[first + offset] = Math.Max(0, elapsed - due[first + offset]);
                        ++offset;
                        continue;
                    }

                    long completed = Stopwatch.GetTimestamp() - start;
                    // A partial return owns only the prefix. Submit the remaining suffix on the next call.
                    for (int i = offset; i < offset + sent; ++i) {
                        int index = first + i;
                        RecordSend(corpus, result, corpus.Entries[index], (int)messages[i].Length, completed);
                        lateness[index] = Math.Max(0, completed - due[index]);
                    }
                    offset += sent;
                }
                first += count;
            }
            return Stopwatch.GetTimestamp() - start;
        } finally {
            if (messagePin.IsAllocated) messagePin.Free();
            if (vectorPin.IsAllocated) vectorPin.Free();
            if (storagePin.IsAllocated) storagePin.Free();
        }
    }

    /// <summary>Send preloaded datagrams and record kernel send completion times on a monotonic clock.</summary>
    public static SenderResult Send(Corpus corpus, IPAddress group, IPAddress local, int port, int pacingWindowMs = 0)
    {
        long pacingWindowNanoseconds = pacingWindowMs == 0 ? corpus.WindowNanoseconds : pacingWindowMs * 1000000L;
        if (pacingWindowNanoseconds <= 0 || pacingWindowNanoseconds > corpus.WindowNanoseconds)
            throw new ArgumentOutOfRangeException(nameof(pacingWindowMs));

        long[] due = null;
        if (pacingWindowNanoseconds < corpus.WindowNanoseconds) {
            due = new long[corpus.Entries.Length];
            decimal scale = (decimal)pacingWindowNanoseconds / corpus.WindowNanoseconds;
            for (int i = 0; i < due.Length; ++i)
                due[i] = (long)Math.Ceiling(corpus.Entries[i].DueTicks * scale);
        }

        var result = new SenderResult {
            ClockFrequencyHz = Stopwatch.Frequency,
            WindowNanoseconds = corpus.WindowNanoseconds,
            PacingWindowNanoseconds = pacingWindowNanoseconds,
            ScheduledDatagrams = corpus.Entries.Length,
            ScheduledMessages = corpus.Messages
        };
        var lateness = new long[corpus.Entries.Length];
        using (var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp))
        {
            socket.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.MulticastInterface, local.GetAddressBytes());
            socket.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.MulticastTimeToLive, 1);
            socket.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.MulticastLoopback, true);
            socket.SendTimeout = 100;
            socket.Connect(new IPEndPoint(group, port));
            if (due != null && OperatingSystem.IsLinux() && BitConverter.IsLittleEndian &&
                RuntimeInformation.ProcessArchitecture is Architecture.X64 or Architecture.Arm64) {
                result.ElapsedNanoseconds = ToNanos(SendLinuxBatches(socket, corpus, due, lateness, result));
            } else {
                long start = Stopwatch.GetTimestamp();
                for (int i = 0; i < corpus.Entries.Length; ++i) {
                    Entry entry = corpus.Entries[i];
                    long deadline = due == null ? entry.DueTicks : due[i];
                    AwaitDue(start, deadline);
                    int sent = SendOne(socket, entry);
                    long completed = Stopwatch.GetTimestamp() - start;
                    RecordSend(corpus, result, entry, sent, completed);
                    lateness[i] = Math.Max(0, completed - deadline);
                }
                result.ElapsedNanoseconds = ToNanos(Stopwatch.GetTimestamp() - start);
            }
        }
        Array.Sort(lateness);
        int p99 = checked((int)(((long)lateness.Length * 99 + 99) / 100) - 1);
        result.LatenessP99Nanoseconds = ToNanos(lateness[p99]);
        result.LatenessMaxNanoseconds = ToNanos(lateness[lateness.Length - 1]);
        return result;
    }

    /// <summary>Join the group and compare received datagrams with their exact prepared bytes.</summary>
    public static ReceiverResult Receive(Corpus corpus, IPAddress group, IPAddress local, int port, int timeoutMs)
    {
        var result = new ReceiverResult { ExpectedDatagrams = corpus.Entries.Length, ExpectedMessages = corpus.Messages };
        var seen = new bool[corpus.Entries.Length];
        // A valid UDP datagram can exceed this corpus profile. Count it as corrupt without truncation.
        var buffer = new byte[65535];
        using (var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp))
        {
            socket.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
            socket.Bind(new IPEndPoint(IPAddress.Any, port));
            socket.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.AddMembership, new MulticastOption(group, local));
            socket.ReceiveBufferSize = 4 * 1024 * 1024;
            result.SocketReceiveBufferBytes = socket.ReceiveBufferSize;
            socket.Blocking = false;
            Console.WriteLine("READY");
            Console.Out.Flush();
            long started = Stopwatch.GetTimestamp();
            long timeout = checked((long)Math.Ceiling((decimal)timeoutMs * Stopwatch.Frequency / 1000));
            int high = -1;
            long firstValid = -1;
            long lastValid = -1;
            while (Stopwatch.GetTimestamp() - started < timeout)
            {
                if (!socket.Poll(1000, SelectMode.SelectRead)) continue;
                int count;
                try { count = socket.Receive(buffer); }
                catch (SocketException failure) when (failure.SocketErrorCode == SocketError.WouldBlock) { continue; }
                result.PayloadBytesReceived += count;
                if (count < HeaderBytes || !buffer.AsSpan(0, 4).SequenceEqual(DatagramMagic)) {
                    ++result.UnknownDatagrams;
                    continue;
                }
                uint number = BinaryPrimitives.ReadUInt32LittleEndian(buffer.AsSpan(4, 4));
                if (number >= corpus.Entries.Length) {
                    ++result.UnknownDatagrams;
                    continue;
                }
                Entry expected = corpus.Entries[number];
                if (count != expected.Payload.Length || !buffer.AsSpan(0, count).SequenceEqual(expected.Payload)) {
                    ++result.CorruptDatagrams;
                    continue;
                }
                if (seen[number]) {
                    ++result.DuplicateDatagrams;
                    continue;
                }
                seen[number] = true;
                long arrival = Stopwatch.GetTimestamp();
                if (firstValid < 0) firstValid = arrival;
                lastValid = arrival;
                if (arrival - firstValid < corpus.WindowTicks) {
                    ++result.UniqueDatagramsInWindow;
                    result.UniqueMessagesInWindow += expected.Messages;
                }
                ++result.UniqueDatagrams;
                result.UniqueMessages += expected.Messages;
                if (number < high) ++result.OutOfOrderDatagrams;
                high = Math.Max(high, (int)number);
            }
            if (firstValid >= 0) result.ReceiveSpanNanoseconds = ToNanos(lastValid - firstValid);
            socket.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.DropMembership, new MulticastOption(group, local));
        }
        result.MissingDatagrams = result.ExpectedDatagrams - result.UniqueDatagrams;
        result.MissingMessages = result.ExpectedMessages - result.UniqueMessages;
        result.Completed = result.MissingDatagrams == 0 && result.CorruptDatagrams == 0 && result.UnknownDatagrams == 0 &&
            result.DuplicateDatagrams == 0 && result.OutOfOrderDatagrams == 0;
        return result;
    }
}
