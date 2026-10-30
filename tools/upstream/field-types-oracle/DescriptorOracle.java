package qdcxx.oracle;

import com.devexperts.connector.proto.EndpointId;
import com.devexperts.io.BufferedInput;
import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.RawDataConsumer;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.transport.stats.ConnectionStats;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Production descriptor companion invoked by FieldTypesOracle and its governed launcher. Reads an
 * explicit pinned classpath and fresh output directory; writes finite synthetic tapes and
 * observations. Headerless mode runs in a separate child before FileWriter initialization.
 * Exceptions or output errors fail capture; no production class or source is modified.
 */
public final class DescriptorOracle {
  static PrintWriter observations;
  static PrintWriter uidRanges;
  static byte[] endpointUid;
  static int endpointUidOffset;
  static int endpointUidBodyOffset;
  static int uidRangeCount;
  static String active;
  static Path dir;
  static final DefaultRecord A = FlagsOracle.record(0, "ProbeA", false, 1);
  static final DefaultRecord B = FlagsOracle.record(1, "ProbeB", false, 1);
  static final DataScheme SCHEME = new DefaultScheme(PentaCodec.INSTANCE, A, B);
  static final MessageType[] FAMILIES = FlagsOracle.FAMILIES;

  static void require(boolean test, String reason) {
    if (!test) throw new AssertionError(active + ": " + reason);
  }

  static byte[] frame(int id, byte[] body) throws Exception {
    ByteArrayOutput payload = new ByteArrayOutput();
    payload.writeCompactInt(id);
    payload.write(body);
    ByteArrayOutput output = new ByteArrayOutput();
    output.writeCompactInt(payload.getPosition());
    output.write(FlagsOracle.bytes(payload));
    return FlagsOracle.bytes(output);
  }

  static byte[] descriptor(ProtocolDescriptor desc) throws Exception {
    ByteArrayOutput output = new ByteArrayOutput();
    desc.composeTo(output);
    return frame(1, FlagsOracle.bytes(output));
  }

  static ProtocolDescriptor desc(String time, String opt, MessageType... sends) {
    ProtocolDescriptor result = ProtocolDescriptor.newEmptyProtocolDescriptor();
    result.setProperty("type", "tape");
    if (time != null) result.setProperty("time", time);
    if (opt != null) result.setProperty("opt", opt);
    for (MessageType send : sends) result.addSend(result.newMessageDescriptor(send));
    return result;
  }

  static final class Seen extends MessageConsumerAdapter implements RawDataConsumer {
    final List<String> rows = new ArrayList<>();
    int errors;
    int protocols;
    int others;

    @Override
    public void processData(DataIterator source, MessageType type) {
      RecordCursor cursor;
      while ((cursor = ((RecordSource) source).next()) != null) {
        String row =
            type.getId()
                + "/"
                + cursor.getRecord().getId()
                + "/"
                + cursor.getInt(0)
                + "/"
                + cursor.getEventTimeSequence();
        rows.add(row);
        observations.println("record\t" + active + "\t" + row);
      }
    }

    @Override
    public void processDescribeProtocol(ProtocolDescriptor desc, boolean log) {
      protocols++;
      observations.println(
          "descriptor\t"
              + active
              + "\t"
              + protocols
              + "\t"
              + desc.getProperty("version")
              + "\t"
              + desc.getProperty("opt")
              + "\t"
              + desc.getProperty("time")
              + "\t"
              + desc.getProperty("unknown")
              + "\t"
              + desc.getSendSet()
              + "\t"
              + desc.getReceiveSet()
              + "\t"
              + (desc.getEndpointId() == null ? "absent" : "present"));
    }

    @Override
    public void handleCorruptedMessage(int type) {
      errors++;
    }

    @Override
    public void handleCorruptedStream() {
      errors++;
    }

    @Override
    public void handleUnknownMessage(int type) {
      others++;
    }

    @Override
    public void processOtherMessage(int type, BufferedInput data, int length) {
      others++;
      observations.println("other\t" + active + "\t" + type + "\t" + length);
    }
  }

  static Seen file(String name, byte[] input, boolean known, MessageType as, TimestampsType time)
      throws Exception {
    active = name;
    Path path = dir.resolve(name + ".qtp");
    Files.write(path, input);
    randomRange(name, input);
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setFormat(FileFormat.BINARY);
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    params.setSchemeKnown(known);
    params.setReadAs(as);
    params.setTime(time);
    params.setResyncOn(null);
    FileReader reader = new FileReader(path.toString(), new ConnectionStats(), params);
    reader.setScheme(SCHEME);
    Seen seen = new Seen();
    try {
      reader.readInto(seen);
    } finally {
      reader.close();
    }
    observations.println(
        "file\t"
            + name
            + "\tbytes="
            + input.length
            + "\trecords="
            + seen.rows.size()
            + "\terrors="
            + seen.errors
            + "\tprotocols="
            + seen.protocols
            + "\tothers="
            + seen.others);
    return seen;
  }

  static byte[] data(DataRecord record, MessageType family) throws Exception {
    return FlagsOracle.compose(SCHEME, record, family, "", new int[] {0});
  }

  /**
   * Locate the last protocol payload by actual compact envelope boundaries, including a partial
   * body.
   */
  static int lastProtocolPayload(byte[] input) throws Exception {
    ByteArrayInput framed = new ByteArrayInput(input);
    int start = 0;
    int payload = -1;
    while (start < input.length) {
      framed.setPosition(start);
      int length = framed.readCompactInt();
      int body = framed.getPosition();
      require(length >= 0, "nonnegative designed envelope");
      if (length > 0 && framed.readCompactInt() == 1) payload = framed.getPosition();
      long next = (long) body + length;
      if (next >= input.length) break;
      require(next > start, "forward envelope boundary");
      start = (int) next;
    }
    return payload;
  }

  /** Record only the source-identified UID field, never property/header/prefix bytes. */
  static void randomRange(String name, byte[] input) throws Exception {
    if (uidRanges == null || endpointUid == null) return;
    int offset;
    if (name.startsWith("physical-endpoint-cut-")) {
      offset = endpointUidOffset;
    } else if (name.startsWith("complete-endpoint-cut-")
        || name.equals("endpoint-valid")
        || name.equals("endpoint-extra-byte")
        || name.equals("bad-magic")
        || name.equals("late-bad-descriptor")
        || name.equals("late-partial-descriptor")) {
      int payload = lastProtocolPayload(input);
      require(payload >= 0, "designed endpoint envelope");
      offset = payload + endpointUidBodyOffset;
    } else {
      return;
    }
    if (offset < input.length) {
      int size = Math.min(endpointUid.length, input.length - offset);
      require(offset >= 0 && size > 0 && size <= 16, "bounded actual UID field");
      for (int i = 0; i < size; i++)
        require(input[offset + i] == endpointUid[i], "actual UID field prefix");
      uidRanges.println(name + ".qtp\t" + offset + "\t" + size);
      uidRangeCount++;
    }
  }

  static void direct(String name, byte[] input) throws Exception {
    active = name;
    ByteArrayInput bytes = new ByteArrayInput(input);
    BinaryQTPParser parser = new BinaryQTPParser(SCHEME);
    parser.setInput(bytes);
    Seen seen = new Seen();
    parser.parse(seen);
    observations.println(
        "direct\t"
            + name
            + "\trecords="
            + seen.rows.size()
            + "\terrors="
            + seen.errors
            + "\tposition="
            + bytes.getPosition()
            + "\tlength="
            + input.length);
  }

  static void writer(boolean headerless) throws Exception {
    for (TimestampsType time : new TimestampsType[] {TimestampsType.NONE, TimestampsType.MESSAGE}) {
      for (MessageType family : FAMILIES) {
        active = "writer-" + time + "-" + family.getId();
        Path path = dir.resolve(active + ".qtp");
        FileWriter config = new FileWriter();
        config.setFormat(FileFormat.BINARY);
        config.setCompression(StreamCompression.NONE);
        config.setTime(time);
        config.setSaveAs(family);
        config.setOpt("");
        try (FileWriter opened = config.open(path.toString(), SCHEME)) {
          RecordBuffer records = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
          RecordCursor current = records.add(A, SCHEME.getCodec().encode("FLAGS"), null);
          current.setInt(0, 913);
          current.setEventTimeSequence(0x1234567800000000L);
          opened.visitData(records, family);
        }
        byte[] tape = Files.readAllBytes(path);
        ByteArrayInput in = new ByteArrayInput(tape);
        in.readCompactInt();
        int first = in.readCompactInt();
        require(headerless ? first != 1 : first == 1, "actual writer header setting");
        Seen seen = file(active + "-reader", tape, false, null, time);
        require(seen.errors == 0 && seen.rows.size() == 1, "writer/file reader");
        require(seen.rows.get(0).startsWith(family.getId() + "/0/913/"), "family/value");
        observations.println(
            "writer\t" + active + "\tfirst=" + first + "\theaderless=" + headerless);
      }
    }
  }

  static void designed() throws Exception {
    byte[] a = data(A, MessageType.STREAM_DATA);
    byte[] b = data(B, MessageType.HISTORY_DATA);
    byte[] header = descriptor(desc("NONE", "", FAMILIES));
    byte[] descriptions = FlagsOracle.descriptions(a);
    byte[] bare = Arrays.copyOfRange(a, descriptions.length, a.length);
    Seen known = file("scheme-known-no-descriptions", bare, true, null, TimestampsType.NONE);
    require(known.errors == 0 && known.rows.size() == 1, "schemeKnown actual FileReader");
    Seen unknown = file("no-descriptions-default", bare, false, null, TimestampsType.NONE);
    require(unknown.errors > 0 && unknown.rows.isEmpty(), "description required by default");
    Seen readAs = file("read-as-ticker", a, false, MessageType.TICKER_DATA, TimestampsType.NONE);
    require(readAs.errors == 0 && readAs.rows.get(0).startsWith("10/"), "actual readAs");
    file("headerless-described", a, false, null, TimestampsType.NONE);
    file(
        "late-interleaved",
        FlagsOracle.concat(header, a, b, bare),
        false,
        null,
        TimestampsType.NONE);
    ProtocolDescriptor delta = ProtocolDescriptor.newEmptyProtocolDescriptor();
    delta.setProperty("version", "delta");
    file(
        "repeat-version-only",
        FlagsOracle.concat(header, a, descriptor(delta), bare),
        false,
        null,
        TimestampsType.NONE);
    file(
        "late-initial-descriptor",
        FlagsOracle.concat(a, header, bare),
        false,
        null,
        TimestampsType.NONE);
    ProtocolDescriptor unknownSend = desc(null, null);
    unknownSend.appendFromTextTokens(Arrays.asList("+UNKNOWN_NAME", "per-message=value"), 0);
    file(
        "unknown-send",
        FlagsOracle.concat(descriptor(unknownSend), a),
        false,
        null,
        TimestampsType.NONE);
    file(
        "absent-time-opt-send",
        FlagsOracle.concat(descriptor(desc(null, null)), a),
        false,
        null,
        TimestampsType.NONE);
    ProtocolDescriptor wrongType = desc("NONE", "", FAMILIES);
    wrongType.setProperty("type", "other");
    file(
        "other-type",
        FlagsOracle.concat(descriptor(wrongType), a),
        false,
        null,
        TimestampsType.NONE);
    for (String version : new String[] {"", "3.355", "not-a-version"}) {
      ProtocolDescriptor current = desc("NONE", "", FAMILIES);
      current.setProperty("version", version);
      file(
          "version-" + version.replace('.', '_'),
          FlagsOracle.concat(descriptor(current), a),
          false,
          null,
          TimestampsType.NONE);
    }
    ProtocolDescriptor extras = desc("NONE", "hs", MessageType.TICKER_DATA);
    extras.setProperty("unknown", "value");
    extras.addReceive(extras.newMessageDescriptor(MessageType.HISTORY_DATA));
    extras.getSend(MessageType.TICKER_DATA).setProperty("per-message", "value");
    file(
        "unknown-property-send-mismatch",
        FlagsOracle.concat(descriptor(extras), a),
        false,
        null,
        TimestampsType.NONE);
    ProtocolDescriptor endpoint = desc("NONE", "", FAMILIES);
    byte[] endpointPrefix = descriptor(endpoint);
    EndpointId actualEndpoint = EndpointId.newEndpointId("synthetic");
    endpoint.setEndpointId(actualEndpoint);
    byte[] endpointHeader = descriptor(endpoint);
    endpointUid =
        Arrays.copyOf(
            actualEndpoint.getJVMId().getUID(), actualEndpoint.getJVMId().getUID().length);
    ByteArrayInput plain = new ByteArrayInput(endpointPrefix);
    plain.readCompactInt();
    require(plain.readCompactInt() == 1, "plain protocol type");
    int plainPayloadStart = plain.getPosition();
    ByteArrayInput endpointFrame = new ByteArrayInput(endpointHeader);
    endpointFrame.readCompactInt();
    require(endpointFrame.readCompactInt() == 1, "endpoint protocol type");
    int endpointPayloadStart = endpointFrame.getPosition();
    for (int i = plainPayloadStart; i < endpointPrefix.length; i++) {
      require(
          endpointFrame.readByte() == endpointPrefix[i],
          "unchanged properties and send/receive prefix");
    }
    long uidLength = endpointFrame.readCompactLong();
    require(uidLength == 16 && uidLength == endpointUid.length, "actual UUID byte-array length");
    endpointUidOffset = endpointFrame.getPosition();
    endpointUidBodyOffset = endpointUidOffset - endpointPayloadStart;
    for (byte value : endpointUid)
      require(endpointFrame.readByte() == value, "serialized endpoint UID bytes");
    require(endpointFrame.readUTFString().equals("synthetic"), "endpoint name after UID field");
    require(endpointFrame.readCompactLong() == actualEndpoint.getId(), "endpoint ID after name");
    require(endpointFrame.getPosition() == endpointHeader.length, "exact endpoint structural tail");
    file("endpoint-valid", FlagsOracle.concat(endpointHeader, a), false, null, TimestampsType.NONE);
    byte[] repeated = FlagsOracle.concat(header, a, descriptor(extras), bare);
    file("repeated-delta", repeated, false, null, TimestampsType.NONE);
    for (String option : new String[] {"", "hs", "unknown", "hs,unknown", "hs,hs", " hs "}) {
      file(
          "option-" + option.replace(' ', '_').replace(',', '-'),
          FlagsOracle.concat(descriptor(desc("NONE", option, FAMILIES)), a),
          false,
          null,
          TimestampsType.NONE);
    }
    for (String time : new String[] {"NONE", "MESSAGE", "message", "FIELD", "unknown"}) {
      String timeCase = time.equals("message") ? "lower-message" : time;
      file(
          "time-" + timeCase,
          FlagsOracle.concat(descriptor(desc(time, "", FAMILIES)), a),
          false,
          null,
          TimestampsType.NONE);
      file(
          "time-auto-" + timeCase,
          FlagsOracle.concat(descriptor(desc(time, "", FAMILIES)), a),
          false,
          null,
          null);
    }
    ProtocolDescriptor empty = ProtocolDescriptor.newEmptyProtocolDescriptor();
    file(
        "descriptor-empty",
        FlagsOracle.concat(descriptor(empty), a),
        false,
        null,
        TimestampsType.NONE);
    for (int type : new int[] {3, 4, 11, 49, 61, 70, 999}) {
      file(
          "control-" + type,
          FlagsOracle.concat(header, a, frame(type, new byte[] {0}), bare),
          false,
          null,
          TimestampsType.NONE);
    }
    // Designed complete frames and physical truncations are separate observations.
    for (int length = 0; length < endpointHeader.length; length++) {
      byte[] partial = Arrays.copyOf(endpointHeader, length);
      file("physical-endpoint-cut-" + length, partial, false, null, TimestampsType.NONE);
      direct("physical-endpoint-cut-" + length, partial);
    }
    ByteArrayInput endpointIn = new ByteArrayInput(endpointHeader);
    endpointIn.readCompactInt();
    endpointIn.readCompactInt();
    byte[] endpointBody =
        Arrays.copyOfRange(endpointHeader, endpointIn.getPosition(), endpointHeader.length);
    for (int length = 0; length < endpointBody.length; length++) {
      file(
          "complete-endpoint-cut-" + length,
          frame(1, Arrays.copyOf(endpointBody, length)),
          false,
          null,
          TimestampsType.NONE);
      direct("complete-endpoint-cut-" + length, frame(1, Arrays.copyOf(endpointBody, length)));
    }
    file(
        "endpoint-extra-byte",
        FlagsOracle.concat(frame(1, FlagsOracle.concat(endpointBody, new byte[] {0})), a),
        false,
        null,
        TimestampsType.NONE);
    byte[] wrongMagic = Arrays.copyOf(endpointBody, endpointBody.length);
    wrongMagic[0] = 'X';
    file(
        "bad-magic", FlagsOracle.concat(frame(1, wrongMagic), a), false, null, TimestampsType.NONE);
    ByteArrayOutput negative = new ByteArrayOutput();
    negative.write(new byte[] {'D', 'X', 'P', '3'});
    negative.writeCompactLong(-1);
    file(
        "negative-properties",
        frame(1, FlagsOracle.bytes(negative)),
        false,
        null,
        TimestampsType.NONE);
    file(
        "late-bad-descriptor",
        FlagsOracle.concat(header, a, frame(1, wrongMagic)),
        false,
        null,
        TimestampsType.NONE);
    file(
        "late-partial-descriptor",
        FlagsOracle.concat(header, a, Arrays.copyOf(endpointHeader, endpointHeader.length - 1)),
        false,
        null,
        TimestampsType.NONE);
  }

  static byte[] designedDescriptor(String[] keys, String[] values, int[] ids, String[] names)
      throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    out.write(new byte[] {'D', 'X', 'P', '3'});
    out.writeCompactLong(keys.length);
    for (int i = 0; i < keys.length; i++) {
      out.writeUTFString(keys[i]);
      out.writeUTFString(values[i]);
    }
    out.writeCompactInt(ids.length);
    for (int i = 0; i < ids.length; i++) {
      out.writeCompactInt(ids[i]);
      out.writeUTFString(names[i]);
      out.writeCompactLong(0);
    }
    out.writeCompactInt(0);
    return FlagsOracle.bytes(out);
  }

  static void additional() throws Exception {
    byte[] a = data(A, MessageType.STREAM_DATA);
    byte[] minimal =
        designedDescriptor(new String[] {"type"}, new String[] {"tape"}, new int[0], new String[0]);
    byte[] duplicates =
        designedDescriptor(
            new String[] {"type", "version", "version"},
            new String[] {"tape", "first", "last"},
            new int[0],
            new String[0]);
    file(
        "duplicate-top-key",
        FlagsOracle.concat(frame(1, duplicates), a),
        false,
        null,
        TimestampsType.NONE);
    for (int[] ids : new int[][] {new int[] {15, 15}, new int[] {-1}, new int[] {15}}) {
      String[] names =
          ids.length == 2
              ? new String[] {"STREAM_DATA", "STREAM_DATA"}
              : ids[0] == -1 ? new String[] {"STREAM_DATA"} : new String[] {"TICKER_DATA"};
      String name =
          ids.length == 2
              ? "duplicate-send-id"
              : ids[0] == -1 ? "send-name-alias" : "send-id-name-mismatch";
      byte[] body = designedDescriptor(new String[] {"type"}, new String[] {"tape"}, ids, names);
      file(name, FlagsOracle.concat(frame(1, body), a), false, null, TimestampsType.NONE);
    }
    ByteArrayOutput tail = new ByteArrayOutput();
    tail.writeCompactLong(-1);
    file(
        "endpoint-null",
        FlagsOracle.concat(frame(1, FlagsOracle.concat(minimal, FlagsOracle.bytes(tail))), a),
        false,
        null,
        TimestampsType.NONE);
    tail = new ByteArrayOutput();
    tail.writeCompactLong(0);
    tail.writeUTFString(null);
    tail.writeCompactLong(0);
    file(
        "endpoint-empty-uid-null-name",
        FlagsOracle.concat(frame(1, FlagsOracle.concat(minimal, FlagsOracle.bytes(tail))), a),
        false,
        null,
        TimestampsType.NONE);
    ByteArrayOutput malformed = new ByteArrayOutput();
    malformed.write(new byte[] {'D', 'X', 'P', '3'});
    malformed.writeCompactLong(0);
    malformed.writeCompactInt(-1);
    file(
        "negative-message-count",
        frame(1, FlagsOracle.bytes(malformed)),
        false,
        null,
        TimestampsType.NONE);
    malformed = new ByteArrayOutput();
    malformed.write(new byte[] {'D', 'X', 'P', '3'});
    malformed.writeCompactLong(1);
    malformed.writeCompactLong(1);
    malformed.writeByte(0xff);
    file(
        "bad-property-utf",
        frame(1, FlagsOracle.bytes(malformed)),
        false,
        null,
        TimestampsType.NONE);
    file("empty-file", new byte[0], false, null, TimestampsType.NONE);
    file("only-empty-marker", new byte[] {0}, false, null, TimestampsType.NONE);
    byte[] actualMessage = Files.readAllBytes(dir.resolve("writer-MESSAGE-15.qtp"));
    ProtocolDescriptor none = ProtocolDescriptor.newEmptyProtocolDescriptor();
    none.setProperty("time", "NONE");
    file(
        "message-then-none-delta",
        FlagsOracle.concat(actualMessage, descriptor(none), a),
        false,
        null,
        TimestampsType.MESSAGE);
  }

  static void capture(Path target) throws Exception {
    capture(target, false);
  }

  static void capture(Path target, boolean headerless) throws Exception {
    dir = Files.createDirectory(target);
    endpointUid = null;
    uidRangeCount = 0;
    try (PrintWriter output =
            new PrintWriter(
                Files.newBufferedWriter(dir.resolve("observations.tsv"), StandardCharsets.UTF_8));
        PrintWriter ranges =
            headerless
                ? null
                : new PrintWriter(
                    Files.newBufferedWriter(
                        dir.resolve("uid-ranges.tsv"), StandardCharsets.UTF_8))) {
      observations = output;
      uidRanges = ranges;
      if (ranges != null) ranges.println("file\tstart\tlength");
      writer(headerless);
      if (!headerless) {
        designed();
        additional();
      }
      observations.println("all\tPASS");
      require(!observations.checkError(), "descriptor observations output");
      if (ranges != null) {
        ranges.println("complete\t" + uidRangeCount);
        require(uidRangeCount == 57 && !ranges.checkError(), "finite random UID ranges");
      }
    }
  }

  /**
   * Internal child entry point; Invoke-Oracle.ps1 supplies a fresh directory and headerless mode.
   */
  public static void main(String[] args) throws Exception {
    if (args.length != 2 || !args[1].equals("headerless")) {
      throw new IllegalArgumentException("fresh output directory and headerless mode required");
    }
    capture(Paths.get(args[0]), true);
  }
}
