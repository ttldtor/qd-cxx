package qdcxx.oracle;

import com.devexperts.io.BufferedInput;
import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.SymbolCodec;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
import com.devexperts.qd.ng.EventFlag;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.BinaryRecordDesc;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.ProtocolOption;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.transport.stats.ConnectionStats;
import com.dxfeed.api.impl.DXFeedScheme;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Production flags companion invoked by FieldTypesOracle; writes finite binary fixtures and
 * observations. Inputs are an empty capture directory and an explicitly pinned production
 * classpath. Files and records are owned here; exceptions fail the calling oracle rather than
 * publishing success.
 */
public final class FlagsOracle {
  static PrintWriter observations;
  static final long TIME = 0x1234567889abcdefL;
  static final int SENTINEL = 913;
  static final MessageType[] FAMILIES = {
    MessageType.RAW_DATA, MessageType.TICKER_DATA, MessageType.STREAM_DATA, MessageType.HISTORY_DATA
  };
  static String active;
  static int readTimes;

  static void require(boolean condition, String message) {
    if (!condition) {
      throw new AssertionError(active + ": " + message);
    }
  }

  static byte[] bytes(ByteArrayOutput out) {
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  static byte[] concat(byte[]... parts) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    for (byte[] part : parts) {
      out.write(part);
    }
    return bytes(out);
  }

  static byte[] frame(MessageType type, byte[] body) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactInt(body.length + 1);
    out.writeCompactInt(type.getId());
    out.write(body);
    return bytes(out);
  }

  static DefaultRecord record(int id, String name, boolean time, int count) {
    DataIntField[] fields = new DataIntField[count];
    for (int i = 0; i < count; i++) {
      fields[i] = new CompactIntField(i, name + ".F" + i);
    }
    return new DefaultRecord(id, name, time, fields, new DataObjField[0]);
  }

  static RecordBuffer input(DataScheme scheme, DataRecord record, int[] flags) {
    RecordBuffer buffer = new RecordBuffer(RecordMode.FLAGGED_DATA);
    for (int flag : flags) {
      RecordCursor cur = buffer.add(record, scheme.getCodec().encode("FLAGS"), null);
      cur.setEventFlags(flag);
      for (int i = 0; i < record.getIntFieldCount(); i++) {
        cur.setInt(i, SENTINEL + i);
      }
      if (record.hasTime()) {
        cur.setTime(TIME);
      }
    }
    return buffer;
  }

  static byte[] compose(
      DataScheme scheme, DataRecord record, MessageType family, String opt, int[] flags)
      throws Exception {
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    if (opt != null) {
      composer.setOptSet(ProtocolOption.parseProtocolOptions(opt));
    }
    ByteArrayOutput out = new ByteArrayOutput();
    composer.setOutput(out);
    require(!composer.visitData(input(scheme, record, flags), family), "composer capacity");
    return bytes(out);
  }

  static byte[] descriptions(byte[] wire) throws Exception {
    ByteArrayInput in = new ByteArrayInput(wire);
    int size = in.readCompactInt();
    int end = in.getPosition() + size;
    require(in.readCompactInt() == 2, "initial description");
    return Arrays.copyOf(wire, end);
  }

  static final class Seen extends MessageConsumerAdapter {
    final List<String> stream = new ArrayList<>();
    final List<String> ticker = new ArrayList<>();
    final List<String> history = new ArrayList<>();
    int errors;
    String option;
    int descriptors;

    @Override
    public void processDescribeProtocol(ProtocolDescriptor descriptor, boolean logDescriptor) {
      descriptors++;
      option = descriptor.getProperty(ProtocolDescriptor.OPT_PROPERTY);
      observations.println(
          "descriptor-option\t"
              + active
              + "\t"
              + (option == null ? "ABSENT" : option.isEmpty() ? "EMPTY" : option));
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      RecordCursor cur;
      while ((cur = ((RecordSource) iterator).next()) != null) {
        StringBuilder value = new StringBuilder(cur.getRecord().getName());
        value.append('|').append(cur.getEventFlags()).append('|').append(cur.getTime());
        value.append('|').append(cur.getEventTimeSequence());
        for (int i = 0; i < cur.getRecord().getIntFieldCount(); i++) {
          value.append('|').append(cur.getInt(i));
        }
        String row = value.toString();
        observations.println("callback\t" + active + "\t" + type.getId() + "\t" + row);
        if (type == MessageType.STREAM_DATA) {
          stream.add(row);
        } else if (type == MessageType.TICKER_DATA) {
          ticker.add(row);
        } else if (type == MessageType.HISTORY_DATA) {
          history.add(row);
        } else {
          throw new AssertionError("unlabelled callback");
        }
      }
    }

    @Override
    public void handleCorruptedMessage(int type) {
      errors++;
      observations.println("corrupt\t" + active + "\t" + type);
    }

    @Override
    public void handleCorruptedStream() {
      errors++;
      observations.println("corrupt-stream\t" + active);
    }

    @Override
    public void handleUnknownMessage(int type) {
      errors++;
    }

    List<String> projection(MessageType family) {
      if (family == MessageType.RAW_DATA) {
        require(ticker.equals(stream) && history.equals(stream), "RAW callback equality");
        observations.println(
            "projection\t"
                + active
                + "\tSTREAM\t"
                + stream.size()
                + "\tcallbacks="
                + (ticker.size() + stream.size() + history.size()));
        return stream;
      }
      return family == MessageType.STREAM_DATA
          ? stream
          : family == MessageType.TICKER_DATA ? ticker : history;
    }
  }

  static final class ReadDesc extends BinaryRecordDesc {
    ReadDesc(BinaryRecordDesc source) {
      super(source);
    }

    @Override
    protected long readTime(BufferedInput in) throws IOException {
      long before = in.totalPosition();
      long value = super.readTime(in);
      readTimes++;
      observations.println(
          "raw-removal-time\t"
              + active
              + "\t"
              + (record == null ? "unknown" : record.getName())
              + "\t"
              + value
              + "\t"
              + before
              + "\t"
              + in.totalPosition());
      return value;
    }
  }

  static final class Parser extends BinaryQTPParser {
    Parser(DataScheme scheme) {
      super(scheme);
    }

    @Override
    protected BinaryRecordDesc wrapRecordDesc(BinaryRecordDesc desc) {
      return new ReadDesc(desc);
    }
  }

  static Seen parse(String name, DataScheme scheme, byte[] wire, Path dir) throws Exception {
    active = name;
    Files.write(dir.resolve(name + ".qtp"), wire);
    Parser parser = new Parser(scheme);
    ByteArrayInput in = new ByteArrayInput(wire);
    parser.setInput(in);
    Seen seen = new Seen();
    parser.parse(seen);
    observations.println(
        "parse\t"
            + name
            + "\tbytes="
            + wire.length
            + "\tconsumed="
            + in.getPosition()
            + "\terrors="
            + seen.errors);
    return seen;
  }

  static int[] subsets() {
    int[] flags = new int[64];
    for (int i = 0; i < flags.length; i++) {
      flags[i] = (i & 31) | ((i & 32) << 1);
    }
    return flags;
  }

  static byte[] designed(DataScheme scheme, int[] flags, int sender, boolean ordinary, long time)
      throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    SymbolCodec.Writer writer = scheme.getCodec().createWriter();
    writer.reset(ProtocolOption.EMPTY_SET);
    for (int flag : flags) {
      writer.writeSymbol(out, scheme.getCodec().encode("FLAGS"), null, flag);
      out.writeCompactInt(sender);
      if ((flag & 2) != 0) {
        out.writeCompactLong(time);
      } else if (ordinary) {
        out.writeCompactInt((int) (TIME >>> 32));
        out.writeCompactInt((int) TIME);
        out.writeCompactInt(SENTINEL + 2);
      }
    }
    return bytes(out);
  }

  static void verifyRows(
      List<String> rows,
      int[] flags,
      int mask,
      String name,
      boolean indexed,
      int count,
      long time) {
    require(rows.size() == flags.length, "wire projection count");
    for (int i = 0; i < flags.length; i++) {
      int emitted = flags[i] & mask;
      boolean remove = (emitted & 2) != 0;
      StringBuilder expected =
          new StringBuilder(name + "|" + emitted + "|" + (indexed ? time : 0) + "|0");
      for (int j = 0; j < count; j++) {
        int value = remove ? 0 : SENTINEL + j;
        if (indexed && j < 2) {
          value = j == 0 ? (int) (time >>> 32) : (int) time;
        }
        expected.append('|').append(value);
      }
      require(
          rows.get(i).equals(expected.toString()),
          "flags/time/default projection "
              + i
              + " actual="
              + rows.get(i)
              + " expected="
              + expected);
    }
  }

  static void matrix(Path dir, DataScheme scheme, DataRecord pair) throws Exception {
    int[] flags = subsets();
    for (MessageType family : FAMILIES) {
      for (String option : new String[] {null, "", "hs", "unknown", "hs,hs", "hs,unknown"}) {
        String label =
            option == null ? "default" : option.isEmpty() ? "empty" : option.replace(',', '-');
        String name = "composer-" + family.getId() + "-" + label;
        active = name;
        int mask =
            EventFlag.getSupportedEventFlags(
                option == null
                    ? ProtocolOption.EMPTY_SET
                    : ProtocolOption.parseProtocolOptions(option),
                family);
        observations.println("requested-composer\t" + name + "\tinput=all64\tmask=" + mask);
        Seen seen = parse(name, scheme, compose(scheme, pair, family, option, flags), dir);
        require(seen.errors == 0, "composer parse");
        verifyRows(seen.projection(family), flags, mask, pair.getName(), true, 3, TIME);
      }
      byte[] desc = descriptions(compose(scheme, pair, family, "hs", new int[] {0}));
      String name = "designed-" + family.getId() + "-all64";
      Seen seen =
          parse(
              name,
              scheme,
              concat(desc, frame(family, designed(scheme, flags, pair.getId(), true, TIME))),
              dir);
      require(seen.errors == 0, "designed parse");
      verifyRows(seen.projection(family), flags, -1, pair.getName(), true, 3, TIME);
      int[] unsupported = {0x20, 0x80, 0x100, Integer.MIN_VALUE, -1};
      for (String option : new String[] {"", "hs"}) {
        String unknownName =
            "composer-unknown-" + family.getId() + "-" + (option.isEmpty() ? "empty" : "hs");
        Seen unknown =
            parse(unknownName, scheme, compose(scheme, pair, family, option, unsupported), dir);
        require(unknown.errors == 0, "composer masks unknown/reserved bits");
        verifyRows(
            unknown.projection(family),
            unsupported,
            option.isEmpty() ? 0 : 95,
            pair.getName(),
            true,
            3,
            TIME);
      }
    }
  }

  static void removal(Path dir, DataRecord unused) throws Exception {
    DataRecord pair = record(0, "Pair", true, 3);
    DataRecord plain = record(1, "Plain", false, 3);
    DataRecord empty = record(2, "Empty", false, 0);
    DataScheme scheme = new DefaultScheme(PentaCodec.INSTANCE, pair, plain, empty);
    DataScheme unknownConsumer =
        new DefaultScheme(
            PentaCodec.INSTANCE, record(0, "Plain", false, 3), record(1, "Empty", false, 0));
    for (DataRecord source : new DataRecord[] {pair, plain, empty}) {
      byte[] desc =
          descriptions(compose(scheme, source, MessageType.STREAM_DATA, "hs", new int[] {0}));
      for (long time : new long[] {Long.MIN_VALUE, -1, 0, TIME, Long.MAX_VALUE}) {
        String name = "remove-" + source.getName() + "-" + Long.toHexString(time);
        int before = readTimes;
        Seen seen =
            parse(
                name,
                scheme,
                concat(
                    desc,
                    frame(
                        MessageType.STREAM_DATA,
                        designed(scheme, new int[] {2, 2}, source.getId(), false, time))),
                dir);
        require(seen.errors == 0 && readTimes - before == 2, "two compact-long removals");
        verifyRows(
            seen.stream,
            new int[] {2, 2},
            -1,
            source.getName(),
            source.hasTime(),
            source.getIntFieldCount(),
            time);
        if (source == pair) {
          before = readTimes;
          Seen skipped =
              parse(
                  name + "-unknown",
                  unknownConsumer,
                  concat(
                      desc,
                      frame(
                          MessageType.STREAM_DATA,
                          designed(scheme, new int[] {2, 2}, source.getId(), false, time))),
                  dir);
          require(
              skipped.errors == 0 && skipped.stream.isEmpty() && readTimes - before == 2,
              "unknown record consumes exact removal time");
        }
      }
      byte[] sentinelDesc =
          descriptions(compose(scheme, pair, MessageType.STREAM_DATA, "hs", new int[] {0}));
      Seen sentinel =
          parse(
              "remove-followed-sentinel-" + source.getName(),
              scheme,
              concat(
                  desc,
                  sentinelDesc,
                  frame(
                      MessageType.STREAM_DATA,
                      concat(
                          designed(scheme, new int[] {2}, source.getId(), false, TIME),
                          designed(scheme, new int[] {0}, pair.getId(), true, TIME)))),
              dir);
      require(sentinel.errors == 0 && sentinel.stream.size() == 2, "following sentinel count");
      verifyRows(sentinel.stream.subList(1, 2), new int[] {0}, -1, pair.getName(), true, 3, TIME);
    }
    for (SerialFieldType serial :
        new SerialFieldType[] {
          SerialFieldType.INT,
          SerialFieldType.COMPACT_INT,
          SerialFieldType.DECIMAL,
          SerialFieldType.SHORT_STRING,
          SerialFieldType.TIME_SECONDS,
          SerialFieldType.SEQUENCE,
          SerialFieldType.DATE,
          SerialFieldType.LONG,
          SerialFieldType.WIDE_DECIMAL,
          SerialFieldType.TIME_MILLIS
        }) {
      int full = serial.getId();
      String name = "Layout" + full;
      DataIntField first = serial.createDefaultIntInstance(0, name + ".First");
      DataIntField second =
          serial.isLong()
              ? new VoidIntField(1, name + ".First$VoidTail")
              : serial.createDefaultIntInstance(1, name + ".Second");
      DataRecord source =
          new DefaultRecord(
              0,
              name,
              true,
              new DataIntField[] {first, second, new CompactIntField(2, name + ".Sentinel")},
              new DataObjField[0]);
      DataScheme layoutScheme = new DefaultScheme(PentaCodec.INSTANCE, source);
      Seen seen =
          parse(
              "remove-layout-" + full,
              layoutScheme,
              compose(layoutScheme, source, MessageType.STREAM_DATA, "hs", new int[] {2, 0}),
              dir);
      require(seen.errors == 0, "layout production removal");
      verifyRows(seen.stream, new int[] {2, 0}, -1, name, true, 3, TIME);
      observations.println(
          "layout\t"
              + full
              + "\t"
              + (serial.isLong() ? "SINGLE64" : "PAIR32")
              + "\tphysical-slots=0,1\tlogical-values="
              + (serial.isLong() ? 2 : 3));
    }
  }

  static void options(Path dir, DataScheme scheme, DataRecord pair) throws Exception {
    byte[] descriptions =
        descriptions(compose(scheme, pair, MessageType.STREAM_DATA, "hs", new int[] {0}));
    for (String option : new String[] {null, "", "hs", "unknown", "hs,hs", "hs,unknown", " hs"}) {
      String label =
          option == null
              ? "absent"
              : option.isEmpty() ? "empty" : option.replace(',', '-').replace(' ', '_');
      ProtocolDescriptor descriptor = ProtocolDescriptor.newPeerProtocolDescriptor(null);
      descriptor.setProperty(ProtocolDescriptor.TYPE_PROPERTY, "tape");
      if (option != null) {
        descriptor.setProperty(ProtocolDescriptor.OPT_PROPERTY, option);
      }
      ByteArrayOutput out = new ByteArrayOutput();
      BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
      composer.setOutput(out);
      composer.composeDescribeProtocol(descriptor);
      Seen seen =
          parse(
              "designed-descriptor-" + label,
              scheme,
              concat(
                  bytes(out),
                  descriptions,
                  frame(MessageType.STREAM_DATA, designed(scheme, subsets(), 0, true, TIME))),
              dir);
      require(
          seen.descriptors == 1
              && (option == null ? seen.option == null : option.equals(seen.option)),
          "exact descriptor option presence");
      require(seen.errors == 0, "option does not mask parser flags");
      verifyRows(seen.stream, subsets(), -1, pair.getName(), true, 3, TIME);
    }
  }

  static void symbolCase(String name, byte[] wire, int[] expected, int errorAt) throws Exception {
    active = "primitive-" + name;
    SymbolCodec.Reader reader = PentaCodec.INSTANCE.createReader();
    reader.reset(ProtocolOption.EMPTY_SET);
    ByteArrayInput in = new ByteArrayInput(wire);
    for (int i = 0; i < expected.length; i++) {
      int before = in.getPosition();
      try {
        reader.readSymbol(in, null);
        require(i != errorAt && reader.getEventFlags() == expected[i], "primitive flag");
        observations.println(
            "primitive\t"
                + name
                + "\t"
                + i
                + "\t"
                + reader.getEventFlags()
                + "\t"
                + reader.getEventFlagsBytes()
                + "\t"
                + before
                + "\t"
                + in.getPosition());
      } catch (IOException exception) {
        require(i == errorAt, "unexpected primitive error");
        observations.println(
            "primitive-error\t"
                + name
                + "\t"
                + i
                + "\t"
                + exception.getClass().getSimpleName()
                + "\t"
                + in.getPosition());
        break;
      }
    }
  }

  static void symbols(Path dir, DataScheme scheme, DataRecord pair) throws Exception {
    symbolCase("initial-mru", new byte[] {(byte) 0xf8, (byte) 0xfe}, new int[] {1}, -1);
    symbolCase(
        "new-reuse-zero-repeat",
        new byte[] {
          (byte) 0xf9,
          4,
          (byte) 0xfe,
          (byte) 0xf8,
          (byte) 0xff,
          (byte) 0xff,
          (byte) 0xf9,
          0,
          (byte) 0xff
        },
        new int[] {4, 4, 0, 0},
        -1);
    symbolCase("undefined-repeat", new byte[] {(byte) 0xff}, new int[] {0}, 0);
    symbolCase("duplicate", new byte[] {(byte) 0xf8, (byte) 0xf9}, new int[] {0}, 0);
    symbolCase("midflags", new byte[] {(byte) 0xf9, (byte) 0x80}, new int[] {0}, 0);
    byte[] desc = descriptions(compose(scheme, pair, MessageType.STREAM_DATA, "hs", new int[] {0}));
    for (int flags : new int[] {0x20, 0x80, 0x100, Integer.MIN_VALUE, -1}) {
      ByteArrayOutput out = new ByteArrayOutput();
      out.write(designed(scheme, new int[] {flags}, 0, true, TIME));
      String name = "designed-unknown-" + Integer.toHexString(flags);
      Seen seen =
          parse(name, scheme, concat(desc, frame(MessageType.STREAM_DATA, bytes(out))), dir);
      require(seen.errors == 0 && seen.stream.size() == 1, "Java accepts arbitrary raw flags");
      verifyRows(seen.stream, new int[] {flags}, -1, pair.getName(), true, 3, TIME);
    }
    ByteArrayOutput wide = new ByteArrayOutput();
    wide.writeByte(0xf9);
    wide.writeCompactLong(0x100000004L);
    wide.writeByte(0xfe);
    symbolCase("wide-low32", bytes(wide), new int[] {4}, -1);
    wide.writeCompactInt(0);
    wide.writeCompactInt((int) (TIME >>> 32));
    wide.writeCompactInt((int) TIME);
    wide.writeCompactInt(SENTINEL + 2);
    Seen overlong =
        parse(
            "designed-wide-flags",
            scheme,
            concat(desc, frame(MessageType.STREAM_DATA, bytes(wide))),
            dir);
    require(overlong.errors == 0, "overlong low32 flag production parse");
    verifyRows(overlong.stream, new int[] {4}, -1, pair.getName(), true, 3, TIME);
    for (byte[] suffix :
        new byte[][] {
          new byte[] {(byte) 0xf9},
          new byte[] {(byte) 0xf9, (byte) 0x80},
          new byte[] {(byte) 0xf8, (byte) 0xf9},
          new byte[] {(byte) 0xff}
        }) {
      String name = "designed-malformed-" + suffix.length + "-" + (suffix[0] & 255);
      Seen seen = parse(name, scheme, concat(desc, frame(MessageType.STREAM_DATA, suffix)), dir);
      require(seen.errors > 0, "production malformed flags/repeat");
    }
    byte[] first = frame(MessageType.STREAM_DATA, designed(scheme, new int[] {4}, 0, true, TIME));
    Seen reset =
        parse(
            "message-repeat-reset",
            scheme,
            concat(desc, first, frame(MessageType.STREAM_DATA, new byte[] {(byte) 0xff})),
            dir);
    require(reset.errors == 1 && reset.stream.size() == 1, "message boundary symbol reset/prefix");
    Seen empty =
        parse(
            "empty-message",
            scheme,
            concat(desc, frame(MessageType.STREAM_DATA, new byte[0]), first),
            dir);
    require(empty.errors == 0 && empty.stream.size() == 1, "empty message");
    byte[] initialMru =
        concat(new byte[] {(byte) 0xf8}, designed(scheme, new int[] {0}, 0, true, TIME));
    Seen mru =
        parse(
            "message-mru-reset",
            scheme,
            concat(desc, first, frame(MessageType.STREAM_DATA, initialMru)),
            dir);
    require(mru.errors == 0 && mru.stream.size() == 2, "message MRU reset count");
    verifyRows(mru.stream.subList(1, 2), new int[] {1}, -1, pair.getName(), true, 3, TIME);
    Parser resetParser = new Parser(scheme);
    resetParser.setInput(new ByteArrayInput(concat(desc, first)));
    active = "parser-reset-session-before";
    Seen beforeReset = new Seen();
    resetParser.parse(beforeReset);
    resetParser.resetSession();
    active = "parser-reset-session-after";
    resetParser.setInput(new ByteArrayInput(first));
    Seen afterReset = new Seen();
    resetParser.parse(afterReset);
    require(
        afterReset.errors == 1 && afterReset.stream.isEmpty(), "resetSession removes descriptions");
    Parser parser = new Parser(scheme);
    byte[] complete = concat(desc, first);
    parser.setInput(new ByteArrayInput(Arrays.copyOf(complete, complete.length - 1)));
    Seen physical = new Seen();
    active = "physical-frame-truncation";
    parser.parse(physical);
    require(physical.errors == 0 && physical.stream.isEmpty(), "partial frame waits");
    observations.println("partial-frame\tWAIT_FOR_INPUT\tno-eof-claim");
    ByteArrayOutput timeBytes = new ByteArrayOutput();
    timeBytes.writeCompactLong(TIME);
    byte[] removal = designed(scheme, new int[] {2}, 0, false, TIME);
    for (int cut = 0; cut < timeBytes.getPosition(); cut++) {
      byte[] body =
          concat(
              designed(scheme, new int[] {0}, 0, true, TIME),
              Arrays.copyOf(removal, removal.length - timeBytes.getPosition() + cut));
      Seen truncated =
          parse(
              "remove-truncated-" + cut,
              scheme,
              concat(desc, frame(MessageType.STREAM_DATA, body)),
              dir);
      require(
          truncated.errors == 1 && truncated.stream.isEmpty(),
          "complete frame removal payload truncation");
      observations.println(
          "negative\tremove-truncated-"
              + cut
              + "\tcompleted="
              + truncated.stream.size()
              + "\terrors="
              + truncated.errors);
    }
  }

  static void file(Path dir, DataScheme scheme, DataRecord record, String label) throws Exception {
    for (String opt : new String[] {null, "", "hs"}) {
      active =
          "file-" + label + "-" + (opt == null ? "absent-setting" : opt.isEmpty() ? "empty" : opt);
      Path path = dir.resolve(active + ".qtp");
      FileWriter writer = new FileWriter();
      writer.setFormat(FileFormat.BINARY);
      writer.setCompression(StreamCompression.NONE);
      writer.setTime(TimestampsType.NONE);
      writer.setSaveAs(MessageType.STREAM_DATA);
      if (opt != null) {
        writer.setOpt(opt);
      }
      try (FileWriter opened = writer.open(path.toString(), scheme)) {
        opened.visitData(
            input(scheme, record, new int[] {1, 2, 4, 8, 16, 64, 95}), MessageType.STREAM_DATA);
      }
      FileReaderParams.Default params = new FileReaderParams.Default();
      params.setSpeed(FileReaderParams.MAX_SPEED);
      params.setCycle(false);
      params.setResyncOn(null);
      FileReader reader = new FileReader(path.toString(), new ConnectionStats(), params);
      reader.setScheme(scheme);
      Seen seen = new Seen();
      try {
        reader.readInto(seen);
      } finally {
        reader.close();
      }
      require(seen.errors == 0 && seen.stream.size() == 7, "actual FileWriter/FileReader");
      verifyRows(
          seen.stream,
          new int[] {1, 2, 4, 8, 16, 64, 95},
          opt != null && opt.isEmpty() ? 0 : 95,
          record.getName(),
          record.hasTime(),
          record.getIntFieldCount(),
          TIME);
      observations.println("production-file\t" + active + "\t" + Files.size(path) + "\t7");
    }
  }

  public static void capture(Path dir) throws Exception {
    try (PrintWriter writer =
        new PrintWriter(
            Files.newBufferedWriter(dir.resolve("observations.tsv"), StandardCharsets.UTF_8))) {
      observations = writer;
      DefaultRecord pair = record(0, "Pair", true, 3);
      DataScheme scheme = new DefaultScheme(PentaCodec.INSTANCE, pair);
      for (String opt : new String[] {"", "hs", "hs,hs", "unknown", "hs,unknown", " hs"}) {
        observations.println("option\t" + opt + "\t" + ProtocolOption.parseProtocolOptions(opt));
      }
      matrix(dir, scheme, pair);
      options(dir, scheme, pair);
      removal(dir, pair);
      symbols(dir, scheme, pair);
      file(dir, scheme, pair, "pair");
      DataScheme standard = DXFeedScheme.getInstance();
      file(dir, standard, standard.findRecordByName("TimeAndSale"), "time-and-sale");
      observations.println("all\tPASS");
      if (writer.checkError()) {
        throw new IOException("flags observation write failed");
      }
    } finally {
      observations = null;
    }
  }
}
