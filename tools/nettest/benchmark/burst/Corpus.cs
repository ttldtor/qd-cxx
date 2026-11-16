// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
using System;
using System.Buffers.Binary;
using System.IO;
using System.Text;

public sealed class BurstSummary {
    public long Datagrams { get; set; }
    public long PayloadBytes { get; set; }
    public long MessageBytes { get; set; }
    public long FileBytes { get; set; }
    public int SmallestPayloadBytes { get; set; }
    public int LargestPayloadBytes { get; set; }
}

public static class BurstCorpusV1 {
    private const int DatagramHeader = 16;
    private static readonly byte[] Magic = Encoding.ASCII.GetBytes("QBURST01");
    private static readonly int[] Sizes = { 24, 40, 64, 80 };

    private static void FillMessage(Span<byte> destination, uint seed, ulong ordinal) {
        BinaryPrimitives.WriteUInt16LittleEndian(destination, checked((ushort)destination.Length));
        BinaryPrimitives.WriteUInt64LittleEndian(destination.Slice(2), ordinal);
        uint state = seed ^ (uint)ordinal ^ (uint)(ordinal >> 32) ^ 0x9e3779b9U;
        for (int i = 10; i < destination.Length; ++i) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            destination[i] = (byte)state;
        }
    }

    private static ulong Offset(long ordinal, long messages, long windowNanos) {
        return checked((ulong)(ordinal * windowNanos / messages));
    }

    private static BurstSummary Traverse(string file, int messages, int windowMs, int limit, uint seed, bool validate) {
        var summary = new BurstSummary { SmallestPayloadBytes = int.MaxValue };
        long windowNanos = checked((long)windowMs * 1000000L);
        using var stream = new FileStream(file, validate ? FileMode.Open : FileMode.CreateNew,
            validate ? FileAccess.Read : FileAccess.Write, FileShare.Read, 1024 * 1024, FileOptions.SequentialScan);
        using var writer = validate ? null : new BinaryWriter(stream, Encoding.UTF8, true);
        using var reader = validate ? new BinaryReader(stream, Encoding.UTF8, true) : null;

        if (validate) {
            if (!reader.ReadBytes(8).AsSpan().SequenceEqual(Magic) || reader.ReadUInt32() != (uint)messages ||
                reader.ReadUInt32() != (uint)limit)
                throw new InvalidDataException("Invalid corpus header.");
        } else {
            writer.Write(Magic);
            writer.Write((uint)messages);
            writer.Write((uint)limit);
        }

        var payload = new byte[limit];
        var actual = validate ? new byte[limit] : null;
        long ordinal = 0;
        uint packet = 0;
        while (ordinal < messages) {
            long first = ordinal;
            BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(0, 4), 0x31504451U);
            BinaryPrimitives.WriteUInt32LittleEndian(payload.AsSpan(4, 4), packet);
            BinaryPrimitives.WriteUInt64LittleEndian(payload.AsSpan(8, 8), (ulong)first);
            int length = DatagramHeader;
            int count = 0;
            while (ordinal < messages) {
                int messageLength = Sizes[(int)((ordinal + seed) % (uint)Sizes.Length)];
                if (length + messageLength > limit)
                    break;
                FillMessage(payload.AsSpan(length, messageLength), seed, (ulong)ordinal);
                length += messageLength;
                summary.MessageBytes += messageLength;
                ++count;
                ++ordinal;
            }
            if (count == 0)
                throw new InvalidDataException("Payload limit cannot hold a message.");
            ulong offset = Offset(first, messages, windowNanos);
            if (validate) {
                if (reader.ReadUInt64() != offset || reader.ReadUInt32() != (uint)length ||
                    reader.ReadUInt32() != (uint)count)
                    throw new InvalidDataException("Invalid frame header or schedule at datagram " + packet);
                int read = 0;
                while (read < length) {
                    int got = reader.Read(actual, read, length - read);
                    if (got == 0)
                        throw new EndOfStreamException("Truncated payload at datagram " + packet);
                    read += got;
                }
                if (!actual.AsSpan(0, length).SequenceEqual(payload.AsSpan(0, length)))
                    throw new InvalidDataException("Invalid message payload at datagram " + packet);
            } else {
                writer.Write(offset);
                writer.Write((uint)length);
                writer.Write((uint)count);
                writer.Write(payload, 0, length);
            }
            summary.PayloadBytes += length;
            summary.SmallestPayloadBytes = Math.Min(summary.SmallestPayloadBytes, length);
            summary.LargestPayloadBytes = Math.Max(summary.LargestPayloadBytes, length);
            ++summary.Datagrams;
            ++packet;
        }
        if (validate && stream.Position != stream.Length)
            throw new InvalidDataException("Trailing corpus bytes.");
        if (!validate)
            writer.Flush();
        summary.FileBytes = stream.Length;
        return summary;
    }

    public static BurstSummary Generate(string file, int messages, int windowMs, int limit, uint seed) {
        return Traverse(file, messages, windowMs, limit, seed, false);
    }

    public static BurstSummary Validate(string file, int messages, int windowMs, int limit, uint seed) {
        return Traverse(file, messages, windowMs, limit, seed, true);
    }
}
