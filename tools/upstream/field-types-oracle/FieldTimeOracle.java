package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.BinaryRecordDesc;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolOption;
import com.devexperts.qd.qtp.RawDataConsumer;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.qd.util.TimeSequenceUtil;
import com.devexperts.transport.stats.ConnectionStats;
import com.dxfeed.api.impl.DXFeedScheme;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

/**
 * FIELD companion called by FieldTypesOracle.capture with a fresh output directory and its pinned
 * production classpath. Owns finite synthetic files and observations; uses actual FileWriter,
 * FileReader, BinaryQTPComposer and BinaryQTPParser. Designed physical inputs are labelled
 * separately from observed cursor projections. Exceptions fail the caller; production files are
 * never edited.
 */
public final class FieldTimeOracle {
  private static final long TIME = 0x1234567889abcdefL;
  private static final long[] PACKED = {TIME, 0, -1, Long.MIN_VALUE, Long.MAX_VALUE};
  private static final long[] SECONDS = {
    0, -1, Long.MIN_VALUE, Long.MAX_VALUE, 0x1234567889abcdefL
  };
  private static final long[] SEQUENCES = {0, -1, 0x100000000L, Long.MIN_VALUE, Long.MAX_VALUE};
  private static final String[] BUSINESS = {
    "Time", "Value", "EventPrice", "EventSequence", "EventTime"
  };
  private static final DataRecord RECORD = record();
  private static final DataScheme SCHEME = new DefaultScheme(PentaCodec.INSTANCE, RECORD);
  private static PrintWriter observations;
  private static PrintWriter canonical;

  private static DataRecord record() {
    DataIntField[] fields = new DataIntField[BUSINESS.length];
    for (int i = 0; i < fields.length; i++) {
      fields[i] =
          i == 0
              ? SerialFieldType.TIME_SECONDS.createDefaultIntInstance(i, "Sample." + BUSINESS[i])
              : new CompactIntField(i, "Sample." + BUSINESS[i], SerialFieldType.COMPACT_INT);
    }
    return new DefaultRecord(0, "Sample", false, fields, new DataObjField[0]);
  }

  private static void require(boolean value, String message) {
    if (!value) throw new AssertionError(message);
  }

  private static final class Seen extends MessageConsumerAdapter implements RawDataConsumer {
    final String id;
    final String route;
    final long[][] raw;
    int records;
    int errors;

    Seen(String id, String route, long[][] raw) {
      this.id = id;
      this.route = route;
      this.raw = raw;
    }

    @Override
    public void processData(DataIterator input, MessageType type) {
      RecordCursor cursor;
      while ((cursor = ((RecordSource) input).next()) != null) {
        require(records < 16, "finite FIELD callback count " + id);
        StringBuilder values = new StringBuilder();
        for (int i = 0; i < BUSINESS.length; i++) {
          values.append('\t').append(cursor.getInt(i));
        }
        observations.println(
            "record\t"
                + id
                + "\t"
                + route
                + "\t"
                + type.getId()
                + "\t"
                + records
                + "\t"
                + cursor.getEventFlags()
                + "\t"
                + cursor.getEventTimeSequence()
                + "\t"
                + TimeSequenceUtil.getTimeMillisFromTimeSequence(cursor.getEventTimeSequence())
                + values);
        if (route.equals("canonical")) {
          long[] words = raw[records];
          canonical.println(
              "record\t"
                  + id
                  + "\t"
                  + type.getId()
                  + "\t"
                  + records
                  + "\t"
                  + cursor.getEventFlags()
                  + "\t"
                  + (words.length == 0 ? "absent" : Long.toString(words[0]))
                  + "\t"
                  + (words.length < 2 ? "absent" : Long.toString(words[1]))
                  + "\t"
                  + cursor.getEventTimeSequence()
                  + "\t"
                  + TimeSequenceUtil.getTimeMillisFromTimeSequence(cursor.getEventTimeSequence())
                  + values);
        }
        records++;
      }
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
      errors++;
    }
  }

  private static void parse(
      String id,
      String route,
      byte[] bytes,
      boolean read,
      long external,
      long[][] raw,
      int expected)
      throws Exception {
    BinaryQTPParser parser = new BinaryQTPParser(SCHEME);
    parser.setReadEventTimeSequence(read);
    parser.setEventTimeSequence(external);
    parser.setInput(new ByteArrayInput(bytes));
    Seen seen = new Seen(id, route, raw);
    parser.parse(seen);
    require(
        seen.records == expected && seen.errors == 0,
        "FIELD parser completion " + id + "/" + route);
    observations.println("parse\t" + id + "\t" + route + "\t" + seen.records + "\t" + seen.errors);
  }

  private static void readFile(Path file, String id, TimestampsType time, long[][] raw, int count)
      throws Exception {
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    params.setResyncOn(null);
    params.setTime(time);
    FileReader reader = new FileReader(file.toString(), new ConnectionStats(), params);
    reader.setScheme(SCHEME);
    Seen seen = new Seen(id, "file-" + time, raw);
    try {
      reader.readInto(seen);
    } finally {
      reader.close();
    }
    require(seen.records == count && seen.errors == 0, "FIELD FileReader completion " + id);
    observations.println("file\t" + id + "\t" + time + "\t" + seen.records + "\t" + seen.errors);
  }

  private static long[][] words(int count, boolean pair) {
    long[][] result = new long[count][];
    for (int i = 0; i < count; i++) {
      result[i] = pair ? new long[] {(int) (PACKED[i] >> 32), (int) PACKED[i]} : new long[0];
    }
    return result;
  }

  private static void fileWriter(Path dir, MessageType family) throws Exception {
    String id = "writer-" + family.getId();
    Path file = dir.resolve(id + ".qtp");
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(TimestampsType.FIELD);
    writer.setOpt("");
    writer.setSaveAs(family);
    RecordBuffer input = new RecordBuffer(RecordMode.TIMESTAMPED_DATA);
    for (int i = 0; i < PACKED.length; i++) {
      RecordCursor cursor = input.add(RECORD, SCHEME.getCodec().encode("TEST"), null);
      cursor.setEventTimeSequence(PACKED[i]);
      for (int j = 0; j < BUSINESS.length; j++) cursor.setInt(j, 700 + 100 * j + i);
    }
    try (FileWriter opened = writer.open(file.toString(), SCHEME)) {
      require(!opened.visitData(input, family), "FIELD writer capacity");
    }
    byte[] bytes = Files.readAllBytes(file);
    require(bytes.length < 65536, "FIELD file bound");
    long[][] raw = words(PACKED.length, true);
    parse(id, "canonical", bytes, true, 0, raw, PACKED.length);
    for (boolean read : new boolean[] {false, true}) {
      for (long external : new long[] {0, 0x1111111122222222L}) {
        parse(id, "parser-" + read + "-" + external, bytes, read, external, raw, PACKED.length);
      }
    }
    for (TimestampsType time :
        new TimestampsType[] {
          null, TimestampsType.NONE, TimestampsType.FIELD, TimestampsType.MESSAGE
        }) {
      readFile(file, id, time, raw, PACKED.length);
    }
  }

  private static void mixedTiming(Path dir, String time) throws Exception {
    String id = "heartbeat-" + time;
    byte[] desc =
        description(
            new String[] {"EventTime", "EventSequence", "Time", "Value"},
            new int[] {56, 72, 56, 8});
    ByteArrayOutput whole = new ByteArrayOutput();
    whole.write(
        DescriptorOracle.descriptor(DescriptorOracle.desc(time, "", MessageType.STREAM_DATA)));
    whole.write(desc);
    long[][] raw = new long[5][];
    long[] stamps = {0, 1234567, -1, Long.MIN_VALUE, Long.MAX_VALUE};
    for (int i = 0; i < stamps.length; i++) {
      ByteArrayOutput heartbeat = new ByteArrayOutput();
      heartbeat.writeCompactInt(1);
      heartbeat.writeCompactLong(stamps[i]);
      whole.write(DescriptorOracle.frame(0, FlagsOracle.bytes(heartbeat)));
      ByteArrayOutput data = new ByteArrayOutput();
      data.writeByte(0xfe);
      data.writeCompactInt(91);
      data.writeCompactLong(SECONDS[i]);
      data.writeCompactLong(SEQUENCES[i]);
      data.writeCompactLong(700 + i);
      data.writeCompactLong(900 + i);
      whole.write(DescriptorOracle.frame(15, FlagsOracle.bytes(data)));
      raw[i] = new long[] {SECONDS[i], SEQUENCES[i]};
    }
    byte[] bytes = FlagsOracle.bytes(whole);
    Path file = dir.resolve(id + ".qtp");
    Files.write(file, bytes);
    parse(id, "canonical", bytes, true, 0, raw, 5);
    for (TimestampsType selected :
        new TimestampsType[] {
          null, TimestampsType.NONE, TimestampsType.FIELD, TimestampsType.MESSAGE
        }) {
      readFile(file, id, selected, raw, 5);
    }
  }

  private static byte[] description(String[] names, int[] types) throws Exception {
    ByteArrayOutput output = new ByteArrayOutput();
    output.writeCompactInt(91);
    output.writeUTFString("Sample");
    output.writeCompactInt(names.length);
    for (int i = 0; i < names.length; i++) {
      output.writeUTFString(names[i]);
      output.writeCompactInt(types[i]);
    }
    return DescriptorOracle.frame(2, FlagsOracle.bytes(output));
  }

  private static void designed(Path dir, String id, String[] names, int[] types, int metadata)
      throws Exception {
    ByteArrayOutput payload = new ByteArrayOutput();
    long[][] raw = new long[SECONDS.length][];
    for (int row = 0; row < SECONDS.length; row++) {
      payload.writeByte(0xfe);
      payload.writeCompactInt(91);
      int physical = 0;
      for (int field = 0; field < names.length; field++) {
        if (types[field] == 0) continue;
        long value =
            physical == 0 ? SECONDS[row] : physical == 1 ? SEQUENCES[row] : 900 + physical + row;
        payload.writeCompactLong(value);
        physical++;
      }
      raw[row] =
          metadata == 0
              ? new long[0]
              : metadata == 1
                  ? new long[] {SECONDS[row]}
                  : new long[] {SECONDS[row], SEQUENCES[row]};
      observations.println(
          "input\t"
              + id
              + "\t"
              + row
              + "\t"
              + SECONDS[row]
              + "\t"
              + SEQUENCES[row]
              + "\t"
              + metadata);
    }
    byte[] bytes =
        FlagsOracle.concat(
            description(names, types), DescriptorOracle.frame(15, FlagsOracle.bytes(payload)));
    Files.write(dir.resolve(id + ".qtp"), bytes);
    parse(id, "canonical", bytes, true, 0, raw, SECONDS.length);
    parse(id, "disabled", bytes, false, 0, raw, SECONDS.length);
    parse(id, "external", bytes, true, TIME, raw, SECONDS.length);
  }

  private static final class Requested extends BinaryRecordDesc {
    Requested(String[] names, int[] types) throws Exception {
      super(RECORD, names.length, names, types, true, DIR_READ);
    }
  }

  private static void removal(Path dir, String id, String[] names, int[] types, int metadata)
      throws Exception {
    final Requested requested = new Requested(names, types);
    BinaryQTPComposer composer =
        new BinaryQTPComposer(SCHEME, true) {
          @Override
          protected BinaryRecordDesc getRequestedRecordDesc(DataRecord record) {
            return requested;
          }
        };
    composer.setWriteEventTimeSequence(true);
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    RecordBuffer input = new RecordBuffer(RecordMode.TIMESTAMPED_DATA.withEventFlags());
    for (int flags : new int[] {0, 2, 0}) {
      RecordCursor cursor = input.add(RECORD, SCHEME.getCodec().encode("TEST"), null);
      cursor.setEventFlags(flags);
      cursor.setEventTimeSequence(TIME);
      for (int i = 0; i < BUSINESS.length; i++) cursor.setInt(i, 900 + i);
    }
    require(!composer.visitData(input, MessageType.STREAM_DATA), "FIELD composer capacity");
    byte[] bytes = Arrays.copyOf(output.getBuffer(), output.getPosition());
    Files.write(dir.resolve(id + ".qtp"), bytes);
    long[][] raw = new long[3][];
    for (int i = 0; i < 3; i++)
      raw[i] =
          metadata == 0
              ? new long[0]
              : metadata == 1
                  ? new long[] {(int) (TIME >> 32)}
                  : new long[] {(int) (TIME >> 32), (int) TIME};
    parse(id, "canonical", bytes, true, 0, raw, 3);
    if (id.equals("remove-pair")) {
      BinaryQTPParser unknown =
          new BinaryQTPParser(new DefaultScheme(PentaCodec.INSTANCE, new DataRecord[0]));
      unknown.setReadEventTimeSequence(true);
      unknown.setInput(new ByteArrayInput(bytes));
      Seen seen = new Seen(id, "unknown", raw);
      String failure = "none";
      try {
        unknown.parse(seen);
      } catch (RuntimeException error) {
        failure = error.getClass().getName();
      }
      require(
          failure.equals("java.lang.NullPointerException") && seen.records == 0 && seen.errors == 0,
          "measured unknown FIELD behavior");
      observations.println("unknown\t" + id + "\t" + failure + "\t0\t0");
    }
  }

  private static void overlongBoundaries(Path dir) throws Exception {
    String id = "overlong-boundaries";
    long[] seconds = {0, 0, -1, Integer.MAX_VALUE, Integer.MIN_VALUE, 0, 0};
    long[] sequences = {
      0x3fffffL, 0x400000L, 0x7fffffffL, 0x80000000L, 0xffc00000L, 0x100000000L, Long.MAX_VALUE
    };
    long[][] raw = new long[seconds.length][];
    ByteArrayOutput data = new ByteArrayOutput();
    for (int i = 0; i < seconds.length; i++) {
      data.writeByte(0xfe);
      data.writeCompactInt(91);
      // Full-width compact escapes are deliberately retained even for small values.
      data.writeByte(0xff);
      data.writeLong(seconds[i]);
      data.writeByte(0xff);
      data.writeLong(sequences[i]);
      data.writeCompactLong(700 + i);
      data.writeCompactLong(900 + i);
      raw[i] = new long[] {seconds[i], sequences[i]};
      observations.println(
          "input\t" + id + "\t" + i + "\t" + seconds[i] + "\t" + sequences[i] + "\t2");
    }
    byte[] bytes =
        FlagsOracle.concat(
            description(
                new String[] {"EventTime", "EventSequence", "Time", "Value"},
                new int[] {56, 72, 56, 8}),
            DescriptorOracle.frame(15, FlagsOracle.bytes(data)));
    Files.write(dir.resolve(id + ".qtp"), bytes);
    parse(id, "canonical", bytes, true, 0, raw, seconds.length);
    parse(id, "disabled", bytes, false, 0, raw, seconds.length);
    parse(id, "external", bytes, true, TIME, raw, seconds.length);
  }

  private static void standardIndexed(Path dir) throws Exception {
    DataScheme scheme = DXFeedScheme.getInstance();
    final DataRecord record = scheme.findRecordByName("TimeAndSale");
    require(
        record != null
            && record.hasTime()
            && record.getIntFieldCount() == 15
            && record.getObjFieldCount() == 0,
        "finite standard indexed shape");
    Path file = dir.resolve("standard-indexed.qtp");
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(TimestampsType.FIELD);
    writer.setOpt("");
    writer.setSaveAs(MessageType.STREAM_DATA);
    RecordBuffer input = new RecordBuffer(RecordMode.TIMESTAMPED_DATA);
    for (int i = 0; i < PACKED.length; i++) {
      RecordCursor cursor = input.add(record, scheme.getCodec().encode("TEST"), null);
      cursor.setEventTimeSequence(PACKED[i]);
      for (int slot = 0; slot < record.getIntFieldCount(); slot++)
        cursor.setInt(slot, 100 + slot + i);
    }
    try (FileWriter opened = writer.open(file.toString(), scheme)) {
      require(!opened.visitData(input, MessageType.STREAM_DATA), "standard FIELD writer capacity");
    }
    try (final PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(dir.resolve("standard.tsv"), StandardCharsets.UTF_8))) {
      out.println("schema\tTimeAndSale\ttrue\t15\t0");
      for (int slot = 0; slot < record.getIntFieldCount(); slot++) {
        DataIntField field = record.getIntField(slot);
        out.println(
            "slot\t"
                + slot
                + "\t"
                + field.getSerialType().getId()
                + "\t"
                + field.getSerialType().isLong()
                + "\t"
                + field.getPropertyName());
      }
      for (final String route : new String[] {"parser", "file"}) {
        final int[] count = {0};
        MessageConsumerAdapter seen =
            new MessageConsumerAdapter() {
              @Override
              protected void processData(DataIterator source, MessageType type) {
                RecordCursor cursor;
                while ((cursor = ((RecordSource) source).next()) != null) {
                  require(
                      type == MessageType.STREAM_DATA && count[0] < 5,
                      "standard FIELD bounded callbacks");
                  StringBuilder row =
                      new StringBuilder(
                          "record\t"
                              + route
                              + "\t"
                              + count[0]
                              + "\t"
                              + cursor.getEventTimeSequence()
                              + "\t"
                              + TimeSequenceUtil.getTimeMillisFromTimeSequence(
                                  cursor.getEventTimeSequence())
                              + "\t"
                              + cursor.getTime());
                  require(
                      cursor.getEventTimeSequence() == PACKED[count[0]],
                      "standard FIELD time projection");
                  for (int slot = 0; slot < record.getIntFieldCount(); slot++)
                    row.append('\t').append(cursor.getInt(slot));
                  out.println(row);
                  count[0]++;
                }
              }

              @Override
              public void handleCorruptedMessage(int type) {
                throw new AssertionError("standard FIELD corruption");
              }

              @Override
              public void handleCorruptedStream() {
                throw new AssertionError("standard FIELD corruption");
              }
            };
        if (route.equals("parser")) {
          BinaryQTPParser parser = new BinaryQTPParser(scheme);
          parser.setReadEventTimeSequence(true);
          parser.setInput(new ByteArrayInput(Files.readAllBytes(file)));
          parser.parse(seen);
        } else {
          FileReaderParams.Default params = new FileReaderParams.Default();
          params.setSpeed(FileReaderParams.MAX_SPEED);
          params.setCycle(false);
          params.setResyncOn(null);
          FileReader reader = new FileReader(file.toString(), new ConnectionStats(), params);
          reader.setScheme(scheme);
          try {
            reader.readInto(seen);
          } finally {
            reader.close();
          }
        }
        require(count[0] == 5, "standard FIELD completion");
      }
      out.println("all\tPASS");
      require(!out.checkError(), "standard FIELD observations");
    }
  }

  public static void capture(Path dir) throws Exception {
    Files.createDirectories(dir);
    try (PrintWriter out =
            new PrintWriter(
                Files.newBufferedWriter(dir.resolve("observations.tsv"), StandardCharsets.UTF_8));
        PrintWriter expected =
            new PrintWriter(
                Files.newBufferedWriter(dir.resolve("canonical.tsv"), StandardCharsets.UTF_8))) {
      observations = out;
      canonical = expected;
      for (MessageType family : FlagsOracle.FAMILIES) fileWriter(dir, family);
      for (String time : new String[] {"NONE", "MESSAGE", "FIELD"}) mixedTiming(dir, time);
      overlongBoundaries(dir);
      standardIndexed(dir);
      designed(
          dir,
          "pair",
          new String[] {"EventTime", "EventSequence", "Time", "Value"},
          new int[] {56, 72, 56, 8},
          2);
      designed(
          dir, "first-only", new String[] {"EventTime", "Time", "Value"}, new int[] {56, 56, 8}, 1);
      designed(dir, "absent", new String[] {"Time", "Value"}, new int[] {56, 8}, 0);
      designed(
          dir,
          "wrong-time",
          new String[] {"EventTime", "EventSequence", "Time", "Value"},
          new int[] {8, 8, 56, 8},
          0);
      designed(
          dir,
          "wrong-sequence",
          new String[] {"EventTime", "EventSequence", "Time", "Value"},
          new int[] {56, 8, 56, 8},
          1);
      designed(
          dir,
          "reordered",
          new String[] {"EventSequence", "EventTime", "Time", "Value"},
          new int[] {72, 56, 56, 8},
          0);
      designed(
          dir,
          "lowercase",
          new String[] {"eventTime", "EventSequence", "Time", "Value"},
          new int[] {56, 72, 56, 8},
          0);
      designed(
          dir,
          "alias",
          new String[] {"OtherTime", "EventSequence", "Time", "Value"},
          new int[] {56, 72, 56, 8},
          0);
      designed(
          dir,
          "void-before",
          new String[] {"Void", "EventTime", "EventSequence", "Time", "Value"},
          new int[] {0, 56, 72, 56, 8},
          0);
      designed(
          dir,
          "void-between",
          new String[] {"EventTime", "Void", "EventSequence", "Time", "Value"},
          new int[] {56, 0, 72, 56, 8},
          1);
      designed(
          dir,
          "void-after",
          new String[] {"EventTime", "EventSequence", "Void", "Time", "Value"},
          new int[] {56, 72, 0, 56, 8},
          2);
      removal(
          dir,
          "remove-pair",
          new String[] {"EventTime", "EventSequence", "EventPrice", "Value"},
          new int[] {56, 72, 8, 8},
          2);
      removal(
          dir,
          "remove-wrong-sequence",
          new String[] {"EventTime", "EventSequence", "EventPrice", "Value"},
          new int[] {56, 8, 8, 8},
          1);
      removal(
          dir,
          "remove-wrong-time",
          new String[] {"EventTime", "EventSequence", "EventPrice", "Value"},
          new int[] {8, 8, 8, 8},
          0);
      removal(
          dir,
          "remove-void-before",
          new String[] {"Void", "EventTime", "EventSequence", "EventPrice", "Value"},
          new int[] {0, 56, 72, 8, 8},
          0);
      removal(
          dir,
          "remove-void-between",
          new String[] {"EventTime", "Void", "EventSequence", "EventPrice", "Value"},
          new int[] {56, 0, 72, 8, 8},
          1);
      removal(
          dir,
          "remove-void-after",
          new String[] {"EventTime", "EventSequence", "Void", "EventPrice", "Value"},
          new int[] {56, 72, 0, 8, 8},
          2);
      out.println("all\tPASS");
      expected.println("all\tPASS");
      require(!out.checkError() && !expected.checkError(), "FIELD observation output");
    }
  }
}
