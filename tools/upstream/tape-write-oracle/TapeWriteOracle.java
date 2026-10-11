package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.CompactCharField;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DateField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.SequenceField;
import com.devexperts.qd.kit.ShortStringField;
import com.devexperts.qd.kit.StringField;
import com.devexperts.qd.kit.TimeSecondsField;
import com.devexperts.qd.kit.VoidIntField;
import com.devexperts.qd.kit.WideDecimalField;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.HeartbeatPayload;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.transport.stats.ConnectionStats;
import com.dxfeed.api.impl.DXFeedScheme;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/** Finite production file observations with separately labelled diagnostic envelope scans. */
public final class TapeWriteOracle {
  private static final long BASE_TIME = 1600000000123L;
  private static final int SAMPLE_COUNT = 8;
  private static final int CALLBACK_LIMIT = 256;
  private static final long[] RAW = {
    0, -1, Integer.MIN_VALUE, Integer.MAX_VALUE, -64, 63, -8193, 8192
  };
  private static final long[] WIDE = {
    0, -1, Long.MIN_VALUE, Long.MAX_VALUE, 2147483648L, 4294967295L, -8193, 8192
  };
  private static final int[] CHARS = {0, 0xffff, 0xd800, 0xdfff, 0x10ffff, 0x80, 0x7ff, 0x800};
  private static final String[] SYMBOLS = {
    "IBM", "IBM", "", "\u0394\u0000\ud800\udfff", "\u0394\u0000\ud800\udfff", "LONGSYMBOL", "*", "A"
  };
  private static final Object[] TEXTS = {
    null,
    "",
    new char[0],
    new char[] {'A', 0, '\ud800', '\udfff'},
    "\ud83d\ude00",
    "\u0080\u07ff\u0800\uffff",
    "\t\n\r",
    new char[] {'Z'}
  };

  private static void require(boolean value, String message) {
    if (!value) {
      throw new IllegalStateException(message);
    }
  }

  private static String units(String text) {
    if (text == null) {
      return "null";
    }
    StringBuilder result = new StringBuilder();
    for (char c : text.toCharArray()) {
      result.append(String.format("%04x", (int) c));
    }
    return result.length() == 0 ? "-" : result.toString();
  }

  private static String text(Object value) {
    return units(value instanceof char[] ? new String((char[]) value) : (String) value);
  }

  private static String textKind(Object value) {
    return value == null ? "null" : value instanceof char[] ? "char-array" : "string";
  }

  private static void validateInputs(Path path) throws Exception {
    List<String> lines = Files.readAllLines(path, StandardCharsets.UTF_8);
    require(lines.size() == SAMPLE_COUNT + 1, "finite reviewed input table");
    require(
        lines.get(0).equals("sample\traw32\traw64\tchar\tsymbol-units\ttext-kind\ttext-units"),
        "input header");
    for (int i = 0; i < SAMPLE_COUNT; i++) {
      require(
          lines
              .get(i + 1)
              .equals(
                  i
                      + "\t"
                      + RAW[i]
                      + "\t"
                      + WIDE[i]
                      + "\t"
                      + CHARS[i]
                      + "\t"
                      + units(SYMBOLS[i])
                      + "\t"
                      + textKind(TEXTS[i])
                      + "\t"
                      + text(TEXTS[i])),
          "reviewed input sample " + i);
    }
  }

  private static PrintWriter output(Path path) throws Exception {
    return new PrintWriter(Files.newBufferedWriter(path, StandardCharsets.UTF_8));
  }

  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; i++) {
      if (i != 0) {
        out.print('\t');
      }
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static DataScheme custom(boolean unsupported) {
    if (unsupported) {
      return new DefaultScheme(
          PentaCodec.INSTANCE,
          new DefaultRecord(
              0,
              "Unsupported",
              false,
              new DataIntField[] {
                SerialFieldType.DECIMAL.createDefaultIntInstance(0, "Unsupported.Price")
              },
              new DataObjField[] {new StringField(0, "Unsupported.Text", true)}));
    }
    DefaultRecord record =
        new DefaultRecord(
            0,
            "Writer.Extras",
            false,
            new DataIntField[] {
              new CompactCharField(0, "Writer.Extras.Char.Unit"),
              new CompactIntField(1, "Writer.Extras.Raw.Count"),
              new ShortStringField(2, "Writer.Extras.Short.Text"),
              new TimeSecondsField(3, "Writer.Extras.Time.Seconds"),
              new SequenceField(4, "Writer.Extras.Raw.Sequence"),
              new DateField(5, "Writer.Extras.Raw.Date"),
              new WideDecimalField(6, "Writer.Extras.Raw.Wide"),
              new VoidIntField(7, "Writer.Extras.Raw.Wide$VoidTail")
            },
            new DataObjField[] {
              new StringField(0, "Writer.Extras.Nullable.Text"),
              new StringField(1, "Writer.Extras.Tail.Text")
            });
    record.addFieldAlias("Raw.Count", "FirstLookup");
    record.addFieldAlias("Raw.Count", "OutgoingCount");
    return new DefaultScheme(PentaCodec.INSTANCE, record);
  }

  private static void schema(PrintWriter out, DataRecord record) {
    row(
        out,
        "schema",
        record.getId(),
        units(record.getName()),
        record.hasTime(),
        record.getIntFieldCount(),
        record.getObjFieldCount());
    for (int i = 0; i < record.getIntFieldCount(); i++) {
      field(out, "int", i, record.getIntField(i));
    }
    for (int i = 0; i < record.getObjFieldCount(); i++) {
      field(out, "obj", i, record.getObjField(i));
    }
    if (record.getName().equals("Writer.Extras")) {
      for (String key : new String[] {"Raw.Count", "RawCount", "FirstLookup", "OutgoingCount"}) {
        row(out, "lookup", units(key), units(record.findFieldByName(key).getLocalName()));
      }
    }
  }

  private static void field(PrintWriter out, String storage, int index, DataField field) {
    row(
        out,
        "slot",
        storage,
        index,
        field.getSerialType().getId(),
        units(field.getLocalName()),
        units(field.getPropertyName()));
  }

  private static RecordCursor append(
      RecordBuffer buffer, DataRecord record, int sample, boolean rawSymbol) {
    String symbol = SYMBOLS[sample];
    int cipher = rawSymbol ? 0 : record.getScheme().getCodec().encode(symbol);
    RecordCursor cursor = buffer.add(record, cipher, cipher == 0 ? symbol : null);
    for (int i = 0; i < record.getIntFieldCount(); i++) {
      int type = record.getIntField(i).getSerialType().getId();
      if (type == 0) {
        continue;
      }
      if (type == 120) {
        cursor.setLong(i, WIDE[sample]);
      } else {
        cursor.setInt(i, type == 2 ? CHARS[sample] : (int) RAW[sample]);
      }
    }
    for (int i = 0; i < record.getObjFieldCount(); i++) {
      cursor.setObj(i, TEXTS[(sample + i) % SAMPLE_COUNT]);
    }
    cursor.setEventTimeSequence(((BASE_TIME / 1000 + sample / 3) << 32) | (123L << 22) | sample);
    return cursor;
  }

  private static void cursor(
      PrintWriter out, String phase, int index, int message, RecordCursor cursor) {
    DataRecord record = cursor.getRecord();
    row(
        out,
        "record",
        phase,
        index,
        message,
        record.getId(),
        units(record.getName()),
        cursor.getCipher(),
        units(cursor.getSymbol()),
        units(record.getScheme().getCodec().decode(cursor.getCipher(), cursor.getSymbol())),
        cursor.getEventFlags(),
        cursor.getEventTimeSequence());
    for (int i = 0; i < record.getIntFieldCount(); i++) {
      DataIntField field = record.getIntField(i);
      int type = field.getSerialType().getId();
      if (type != 0) {
        row(
            out,
            "value",
            phase,
            index,
            "int",
            i,
            type,
            field.getSerialType().isLong() ? cursor.getLong(i) : cursor.getInt(i),
            "null");
      }
    }
    for (int i = 0; i < record.getObjFieldCount(); i++) {
      row(
          out,
          "value",
          phase,
          index,
          "obj",
          i,
          record.getObjField(i).getSerialType().getId(),
          0,
          text(cursor.getObj(i)));
    }
  }

  private static void replay(Path path, DataScheme scheme, PrintWriter out, int expected)
      throws Exception {
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    final int[] counts = new int[3];
    FileReader reader = new FileReader(path.toString(), new ConnectionStats(), params);
    reader.setScheme(scheme);
    try {
      reader.readInto(
          new MessageConsumerAdapter() {
            @Override
            public void processDescribeProtocol(ProtocolDescriptor descriptor, boolean log) {
              row(
                  out,
                  "descriptor",
                  units(descriptor.getProperty("opt")),
                  units(descriptor.getProperty("time")));
              for (Map.Entry<String, String> entry :
                  new TreeMap<>(descriptor.getProperties()).entrySet()) {
                row(out, "property", units(entry.getKey()), units(entry.getValue()));
              }
            }

            @Override
            public void processHeartbeat(HeartbeatPayload heartbeat) {
              row(
                  out,
                  "heartbeat",
                  heartbeat.hasTimeMillis() ? Long.toString(heartbeat.getTimeMillis()) : "-",
                  heartbeat.isEmpty());
            }

            @Override
            protected void processData(DataIterator iterator, MessageType type) {
              row(out, "callback", type.getId());
              RecordCursor current;
              while ((current = ((RecordSource) iterator).next()) != null) {
                require(counts[2]++ < CALLBACK_LIMIT, "finite callback limit");
                if (type == MessageType.STREAM_DATA) {
                  cursor(out, "read", counts[0]++, type.getId(), current);
                }
              }
            }

            @Override
            public void handleCorruptedStream() {
              row(out, "error", "stream");
              counts[1]++;
            }

            @Override
            public void handleCorruptedMessage(int type) {
              row(out, "error", "message", type);
              counts[1]++;
            }
          });
    } finally {
      reader.close();
    }
    row(out, "end", counts[0], counts[1], counts[2]);
    require(counts[0] == expected && counts[1] == 0, "production FileReader acceptance");
  }

  private static FileWriter writer(TimestampsType time, MessageType save, String options) {
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(time);
    writer.setSaveAs(save);
    writer.setOpt(options);
    return writer;
  }

  private static void production(
      Path root,
      String name,
      DataScheme scheme,
      DataRecord record,
      TimestampsType time,
      MessageType save,
      String options,
      boolean batch,
      boolean rawSymbol,
      boolean timestamped)
      throws Exception {
    Path tape = root.resolve(name + ".qtp");
    try (PrintWriter out = output(root.resolve(name + ".tsv"))) {
      row(out, "case", name);
      row(
          out,
          "settings",
          "BINARY",
          "NONE",
          time,
          save.getId(),
          units(options),
          batch,
          rawSymbol,
          timestamped);
      schema(out, record);
      for (int i = 0; i < TEXTS.length; i++) {
        row(out, "text-input", i, textKind(TEXTS[i]), text(TEXTS[i]));
      }
      try (FileWriter opened = writer(time, save, options).open(tape.toString(), scheme)) {
        HeartbeatPayload heartbeat = new HeartbeatPayload();
        heartbeat.setTimeMillis(BASE_TIME);
        opened.visitHeartbeat(heartbeat);
        RecordMode mode = timestamped ? RecordMode.TIMESTAMPED_DATA : RecordMode.FLAGGED_DATA;
        if (batch) {
          RecordBuffer buffer = new RecordBuffer(mode);
          for (int i = 0; i < SAMPLE_COUNT; i++) {
            cursor(out, "input", i, 15, append(buffer, record, i, rawSymbol));
          }
          require(!opened.visitData(buffer, MessageType.STREAM_DATA), "writer capacity");
        } else {
          for (int i = 0; i < SAMPLE_COUNT; i++) {
            RecordBuffer buffer = new RecordBuffer(mode);
            cursor(out, "input", i, 15, append(buffer, record, i, rawSymbol));
            require(!opened.visitData(buffer, MessageType.STREAM_DATA), "writer capacity");
          }
        }
      }
      row(out, "writer-close", name);
      replay(tape, scheme, out, SAMPLE_COUNT);
      row(out, "complete", name);
      require(!out.checkError(), "observation output");
    }
    scan(tape, root.resolve(name + "-wire.tsv"), name);
  }

  private static void scan(Path path, Path output, String name) throws Exception {
    ByteArrayInput input = new ByteArrayInput(Files.readAllBytes(path));
    try (PrintWriter out = output(output)) {
      row(out, "case", name + "-wire");
      row(out, "diagnostic", "envelope-scan-only");
      int index = 0;
      while (input.available() > 0) {
        int start = input.getPosition();
        int length = input.readCompactInt();
        int end = input.getPosition() + length;
        require(length >= 0 && end <= input.getLimit(), "finite envelope");
        int message = length == 0 ? -1 : input.readCompactInt();
        row(out, "frame", index++, start, end, length, message);
        if (message == 2) {
          while (input.getPosition() < end) {
            int id = input.readCompactInt();
            String record = input.readUTFString();
            int fields = input.readCompactInt();
            row(out, "wire-record", id, units(record), fields);
            for (int i = 0; i < fields; i++) {
              row(out, "wire-field", id, i, units(input.readUTFString()), input.readCompactInt());
            }
          }
        }
        require(input.getPosition() <= end, "descriptor frame boundary");
        input.skipBytes(end - input.getPosition());
      }
      row(out, "complete", name + "-wire");
      require(!out.checkError(), "wire output");
    }
  }

  private static void failures(Path root, DataScheme scheme) throws Exception {
    try (PrintWriter out = output(root.resolve("failures.tsv"))) {
      row(out, "case", "failures");
      Path nullTape = root.resolve("null-symbol.qtp");
      String status = "accepted";
      try (FileWriter opened =
          writer(TimestampsType.MESSAGE, MessageType.RAW_DATA, "")
              .open(nullTape.toString(), scheme)) {
        RecordBuffer buffer = new RecordBuffer(RecordMode.TIMESTAMPED_DATA);
        RecordCursor c = buffer.add(scheme.getRecord(0), 0, null);
        c.setEventTimeSequence((BASE_TIME / 1000 << 32) | (123L << 22));
        opened.visitData(buffer, MessageType.STREAM_DATA);
      } catch (IllegalArgumentException e) {
        status = e.getClass().getSimpleName();
      }
      row(out, "failure", "null-symbol", status);
      require(!status.equals("accepted"), "null symbol rejected");
      String badTime = "accepted";
      try (FileWriter opened =
          writer(TimestampsType.TEXT, MessageType.RAW_DATA, "")
              .open(root.resolve("invalid.time").toString(), scheme)) {
        // Opening is the tested operation; no records are supplied.
      } catch (Exception e) {
        badTime = e.getClass().getSimpleName();
      }
      row(out, "failure", "time-extension", badTime);
      require(!badTime.equals("accepted"), "time-file extension rejected");
      Path emptyTape = root.resolve("empty-writer.qtp");
      try (FileWriter opened =
          writer(TimestampsType.MESSAGE, MessageType.RAW_DATA, "")
              .open(emptyTape.toString(), scheme)) {
        // Closing a writer without data is a separate observation, not EOF authentication.
      }
      row(out, "empty-writer", Files.exists(emptyTape));
      row(out, "complete", "failures");
      require(!out.checkError(), "failure output");
    }
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 2, "fresh output directory and reviewed input table required");
    validateInputs(Paths.get(args[1]));
    Path root = Paths.get(args[0]);
    Files.createDirectory(root);
    DataScheme extras = custom(false);
    DataRecord record = extras.getRecord(0);
    production(
        root,
        "extras-raw-message",
        extras,
        record,
        TimestampsType.MESSAGE,
        MessageType.RAW_DATA,
        "",
        true,
        false,
        true);
    production(
        root,
        "extras-raw-none",
        extras,
        record,
        TimestampsType.NONE,
        MessageType.RAW_DATA,
        "",
        true,
        true,
        true);
    production(
        root,
        "extras-stream-none",
        extras,
        record,
        TimestampsType.NONE,
        MessageType.STREAM_DATA,
        "",
        true,
        false,
        true);
    production(
        root,
        "extras-raw-singletons",
        extras,
        record,
        TimestampsType.MESSAGE,
        MessageType.RAW_DATA,
        "",
        false,
        false,
        true);
    production(
        root,
        "extras-heartbeat-regular",
        extras,
        record,
        TimestampsType.MESSAGE,
        MessageType.RAW_DATA,
        "",
        true,
        false,
        false);
    production(
        root,
        "extras-raw-hs",
        extras,
        record,
        TimestampsType.MESSAGE,
        MessageType.RAW_DATA,
        "hs",
        true,
        false,
        true);
    production(
        root,
        "extras-raw-default-options",
        extras,
        record,
        TimestampsType.MESSAGE,
        MessageType.RAW_DATA,
        null,
        true,
        false,
        true);
    production(
        root,
        "extras-raw-field-time",
        extras,
        record,
        TimestampsType.FIELD,
        MessageType.RAW_DATA,
        "",
        true,
        false,
        true);
    DataScheme standard = DXFeedScheme.getInstance();
    for (String name : Arrays.asList("Quote", "Profile")) {
      production(
          root,
          name.toLowerCase() + "-raw",
          standard,
          standard.findRecordByName(name),
          TimestampsType.MESSAGE,
          MessageType.RAW_DATA,
          "",
          true,
          false,
          true);
    }
    DataScheme unsupported = custom(true);
    production(
        root,
        "unsupported-full-identities",
        unsupported,
        unsupported.getRecord(0),
        TimestampsType.MESSAGE,
        MessageType.RAW_DATA,
        "",
        true,
        false,
        true);
    failures(root, extras);
    System.out.println(
        "CAPTURE COMPLETE: production FileWriter/FileReader; diagnostic scans separate");
  }
}
