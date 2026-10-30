package qdcxx.oracle;

import com.devexperts.io.BufferedInput;
import com.devexperts.io.BufferedInputPart;
import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.IOUtil;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.SymbolCodec;
import com.devexperts.qd.kit.CompactCharField;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DateField;
import com.devexperts.qd.kit.DecimalField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.LongField;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.PlainIntField;
import com.devexperts.qd.kit.SequenceField;
import com.devexperts.qd.kit.ShortStringField;
import com.devexperts.qd.kit.StringField;
import com.devexperts.qd.kit.TimeMillisField;
import com.devexperts.qd.kit.TimeSecondsField;
import com.devexperts.qd.kit.VoidIntField;
import com.devexperts.qd.kit.WideDecimalField;
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
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.transport.stats.ConnectionStats;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.SchemeProperties;
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

/** Exact field observations through production primitives, composer/parser, and file paths. */
public final class FieldTypesOracle {
  private static final DataRecord MIXED =
      new DefaultRecord(
          0,
          "FieldTypes",
          false,
          new DataIntField[] {
            new ShortStringField(0, "FieldTypes.Short"),
            new SequenceField(1, "FieldTypes.Sequence"),
            new DateField(2, "FieldTypes.Date"),
            new CompactIntField(3, "FieldTypes.Count"),
            new CompactCharField(4, "FieldTypes.Char"),
            new TimeSecondsField(5, "FieldTypes.Seconds"),
            new WideDecimalField(6, "FieldTypes.Wide"),
            new VoidIntField(7, "FieldTypes.Wide$VoidTail")
          },
          new DataObjField[] {
            new StringField(0, "FieldTypes.Text"), new StringField(1, "FieldTypes.Tail")
          });
  private static final DataScheme SYNTH = new DefaultScheme(PentaCodec.INSTANCE, MIXED);
  private static final DataScheme STANDARD = DXFeedScheme.getInstance();
  private static final DataRecord PROFILE = STANDARD.findRecordByName("Profile");
  private static final int[] TYPES = {10, 40, 72, 88, 8, 2, 56, 120, 10};
  private static final String[] NAMES = {
    "Text", "Short", "Sequence", "Date", "Count", "Char", "Seconds", "Wide", "Tail"
  };

  private static void require(boolean value, String message) {
    if (!value) {
      throw new IllegalStateException(message);
    }
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

  private static PrintWriter output(Path path) throws Exception {
    return new PrintWriter(Files.newBufferedWriter(path, StandardCharsets.UTF_8));
  }

  private static String units(String s) {
    if (s == null) {
      return "null";
    }
    StringBuilder b = new StringBuilder();
    for (char c : s.toCharArray()) {
      b.append(String.format("%04x", (int) c));
    }
    return b.length() == 0 ? "-" : b.toString();
  }

  private static String hex(byte[] bytes) {
    StringBuilder b = new StringBuilder();
    for (byte v : bytes) {
      b.append(String.format("%02x", v & 255));
    }
    return b.length() == 0 ? "-" : b.toString();
  }

  private static byte[] unhex(String s) {
    byte[] b = new byte[s.length() / 2];
    for (int i = 0; i < b.length; i++) {
      b[i] = (byte) Integer.parseInt(s.substring(2 * i, 2 * i + 2), 16);
    }
    return b;
  }

  private static byte[] bytes(ByteArrayOutput out) {
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  private static byte[] compact(long value) throws Exception {
    ByteArrayOutput o = new ByteArrayOutput();
    o.writeCompactLong(value);
    return bytes(o);
  }

  private static byte[] text(String value) throws Exception {
    ByteArrayOutput o = new ByteArrayOutput();
    MIXED.getObjField(0).writeObj(o, value);
    return bytes(o);
  }

  private static byte[] concat(byte[]... values) throws Exception {
    ByteArrayOutput o = new ByteArrayOutput();
    for (byte[] v : values) {
      o.write(v);
    }
    return bytes(o);
  }

  private static byte[] frame(int type, byte[] payload) throws Exception {
    return concat(compact(payload.length + 1), new byte[] {(byte) type}, payload);
  }

  private static void readString(PrintWriter out, String id, byte[] b, boolean plain)
      throws Exception {
    ByteArrayInput in = new ByteArrayInput(b);
    ByteArrayInputStream stream = new ByteArrayInputStream(b);
    String value = "-", status = "ok";
    try {
      value =
          units(
              plain
                  ? IOUtil.readCharArrayString(new DataInputStream(stream))
                  : (String) MIXED.getObjField(0).readObj(in));
    } catch (Exception e) {
      status = e.getClass().getSimpleName();
    }
    row(
        out,
        "string-read",
        id,
        plain ? "data-input" : "buffered",
        hex(b),
        value,
        status,
        plain ? b.length - stream.available() : in.getPosition());
  }

  private static void readPart(PrintWriter out, String id, byte[] b) throws Exception {
    ByteArrayInput underlying = new ByteArrayInput(b);
    underlying.mark();
    BufferedInputPart in = new BufferedInputPart(underlying, b.length);
    String value = "-", status = "ok";
    try {
      value = units((String) MIXED.getObjField(0).readObj(in));
    } catch (Exception e) {
      status = e.getClass().getSimpleName();
    }
    row(out, "string-read", id, "message-part", hex(b), value, status, in.totalPosition());
  }

  private static void primitives(Path root) throws Exception {
    try (PrintWriter out = output(root.resolve("primitives.tsv"))) {
      row(out, "case", "primitives");
      String[] strings = {
        null,
        "",
        "A",
        "A\u0000B",
        "\u0080\u07ff\u0800\uffff",
        "\ud83d\ude00",
        "\ud800",
        "\udfff",
        "\t\n\r\u001b"
      };
      for (int i = 0; i < strings.length; i++) {
        byte[] b = text(strings[i]);
        row(out, "string-write", "value-" + i, units(strings[i]), hex(b));
        for (int n = 0; n <= b.length; n++) {
          readString(out, "value-" + i + "-prefix-" + n, Arrays.copyOf(b, n), false);
          readPart(out, "value-" + i + "-prefix-" + n, Arrays.copyOf(b, n));
          readString(out, "value-" + i + "-prefix-" + n, Arrays.copyOf(b, n), true);
        }
      }
      String[] malformed = {
        "7e",
        "017f",
        "0180",
        "01ff",
        "01c080",
        "01c1bf",
        "01e08080",
        "01f0808080",
        "01f0908080",
        "01f48fbfbf",
        "01f4908080",
        "01f880808080",
        "01c200",
        "01e00080",
        "01e08000",
        "01f0008080",
        "01f0800080",
        "01f0808000",
        "0241",
        "0041",
        "800141",
        "c0000141",
        "f00000000141",
        "f080000000",
        "ff0000000080000000",
        "ff7fffffffffffffff"
      };
      for (int i = 0; i < malformed.length; i++) {
        byte[] b = unhex(malformed[i]);
        readString(out, "probe-" + i, b, false);
        readPart(out, "probe-" + i, b);
        // DataInput has no remaining-input preflight; do not allocate hostile declared lengths.
        if (i < 22) {
          readString(out, "probe-" + i, b, true);
        }
      }
      readPart(out, "valid-max-count", compact(Integer.MAX_VALUE));
      long[] values = {
        0,
        -1,
        1,
        -64,
        63,
        -65,
        64,
        -8192,
        8191,
        -8193,
        8192,
        Integer.MIN_VALUE,
        Integer.MAX_VALUE,
        2147483648L,
        4294967295L,
        4294967296L,
        Long.MIN_VALUE,
        Long.MAX_VALUE
      };
      for (int t = 0; t < 3; t++) {
        DataIntField f = MIXED.getIntField(t);
        for (int i = 0; i < values.length; i++) {
          byte[] b = compact(values[i]);
          ByteArrayInput in = new ByteArrayInput(b);
          int value = f.readInt(in);
          row(
              out,
              "int-read",
              f.getSerialType().getId(),
              "value-" + i,
              hex(b),
              value,
              "ok",
              in.getPosition());
        }
        String[] redundant = {
          "8000",
          "bfff",
          "c00000",
          "dfffff",
          "e0000000",
          "efffffff",
          "f000000000",
          "f7ffffffff",
          "f80000000000",
          "fc000000000000",
          "fe00000000000000",
          "ff0000000000000000",
          "ffffffffffffffffff",
          "ff0000000080000000"
        };
        for (int i = 0; i < redundant.length; i++) {
          byte[] b = unhex(redundant[i]);
          for (int n = 0; n <= b.length; n++) {
            ByteArrayInput in = new ByteArrayInput(Arrays.copyOf(b, n));
            String value = "-", status = "ok";
            try {
              value = Integer.toString(f.readInt(in));
            } catch (Exception e) {
              status = e.getClass().getSimpleName();
            }
            row(
                out,
                "int-read",
                f.getSerialType().getId(),
                "wide-" + i + "-prefix-" + n,
                hex(Arrays.copyOf(b, n)),
                value,
                status,
                in.getPosition());
          }
        }
      }
      row(out, "complete", "primitives");
      require(!out.checkError(), "primitives output");
    }
  }

  private static void schema(Path root) throws Exception {
    require(
        new SchemeProperties(System.getProperties()).getProperties().isEmpty(), "scheme overrides");
    require(!PROFILE.hasTime(), "indexed Profile gate");
    try (PrintWriter out = output(root.resolve("schema.tsv"))) {
      row(out, "case", "schema");
      row(out, "settings", STANDARD.getRecordCount(), "overrides=none");
      for (DataRecord r : new DataRecord[] {MIXED, PROFILE}) {
        row(
            out,
            "schema",
            units(r.getName()),
            r.getId(),
            r.hasTime(),
            r.getIntFieldCount(),
            r.getObjFieldCount());
        for (int i = 0; i < r.getIntFieldCount(); i++) {
          DataIntField f = r.getIntField(i);
          int type = f.getSerialType().getId();
          require(
              type == 0
                  || type == 2
                  || type == 8
                  || type == 40
                  || type == 56
                  || type == 72
                  || type == 88
                  || type == 120,
              "excluded Profile type " + type);
          row(
              out,
              "int-slot",
              units(r.getName()),
              i,
              units(f.getLocalName()),
              units(f.getPropertyName()),
              type);
        }
        for (int i = 0; i < r.getObjFieldCount(); i++) {
          DataObjField f = r.getObjField(i);
          require(f.getSerialType().getId() == 10, "excluded object type");
          row(
              out,
              "obj-slot",
              units(r.getName()),
              i,
              units(f.getLocalName()),
              units(f.getPropertyName()),
              10);
        }
      }
      for (int i = 0; i < NAMES.length; i++) {
        row(
            out,
            "logical",
            i,
            units(NAMES[i]),
            TYPES[i],
            i == 0 ? "obj" : i == 8 ? "obj" : "int",
            i == 0 ? 0 : i == 8 ? 1 : i - 1);
      }
      row(out, "complete", "schema");
    }
  }

  private static void cursor(PrintWriter out, int index, int message, RecordCursor c) {
    DataRecord r = c.getRecord();
    row(
        out,
        "record",
        index,
        message,
        r.getId(),
        units(r.getName()),
        units(r.getScheme().getCodec().decode(c.getCipher(), c.getSymbol())),
        c.getEventFlags(),
        c.getEventTimeSequence());
    if (r == MIXED) {
      for (int i = 0; i < TYPES.length; i++) {
        int t = TYPES[i];
        row(
            out,
            "field",
            index,
            i,
            units(NAMES[i]),
            t,
            t == 10 ? 0 : t == 120 ? c.getLong(i - 1) : c.getInt(i - 1),
            t == 10 ? units((String) c.getObj(i == 0 ? 0 : 1)) : "-");
      }
    } else {
      int logical = 0;
      for (int i = 0; i < r.getIntFieldCount(); i++) {
        DataIntField f = r.getIntField(i);
        int t = f.getSerialType().getId();
        if (t == 0) {
          continue;
        }
        row(
            out,
            "field",
            index,
            logical++,
            units(f.getLocalName()),
            t,
            t == 120 ? c.getLong(i) : c.getInt(i),
            "-");
      }
      for (int i = 0; i < r.getObjFieldCount(); i++) {
        row(
            out,
            "field",
            index,
            logical++,
            units(r.getObjField(i).getLocalName()),
            10,
            0,
            units((String) c.getObj(i)));
      }
    }
  }

  private static final class Observer extends MessageConsumerAdapter {
    final PrintWriter out;
    int count, errors;

    Observer(PrintWriter out) {
      this.out = out;
    }

    @Override
    protected void processData(DataIterator it, MessageType type) {
      RecordCursor c;
      while ((c = ((RecordSource) it).next()) != null) {
        cursor(out, count++, type.getId(), c);
      }
    }

    @Override
    public void handleCorruptedStream() {
      row(out, "error", "stream", -1);
      errors++;
    }

    @Override
    public void handleCorruptedMessage(int type) {
      row(out, "error", "message", type);
      errors++;
    }

    @Override
    public void handleUnknownMessage(int type) {
      row(out, "error", "unknown", type);
      errors++;
    }
  }

  private static void replay(Path root, String name, DataScheme scheme) throws Exception {
    FileReaderParams.Default p = new FileReaderParams.Default();
    p.setSpeed(FileReaderParams.MAX_SPEED);
    p.setCycle(false);
    FileReader reader =
        new FileReader(root.resolve(name + ".qtp").toString(), new ConnectionStats(), p);
    reader.setScheme(scheme);
    try (PrintWriter out = output(root.resolve(name + ".tsv"))) {
      row(out, "case", name);
      Observer o = new Observer(out);
      reader.readInto(o);
      row(out, "end", o.count, o.errors);
      row(out, "complete", name);
      require(!out.checkError(), "replay output");
    } finally {
      reader.close();
    }
  }

  private static RecordBuffer values(DataRecord r, int sample) {
    RecordBuffer buffer = new RecordBuffer(RecordMode.TIMESTAMPED_DATA);
    RecordCursor c = buffer.add(r, r.getScheme().getCodec().encode("IBM"), null);
    for (int i = 0; i < r.getIntFieldCount(); i++) {
      int t = r.getIntField(i).getSerialType().getId();
      if (t == 0) {
        continue;
      }
      if (t == 120) {
        c.setLong(i, 10000000000L + sample * 1000 + i);
      } else {
        c.setInt(
            i,
            t == 2
                ? 0xd800 + sample
                : sample == 1 && r == MIXED && i < 3
                    ? -(sample * 100 + i + 1)
                    : sample * 100 + i + 1);
      }
    }
    c.setObj(0, sample == 0 ? null : sample == 1 ? "" : "A\u0000\ud83d\ude00\ud800\n");
    c.setObj(1, sample == 0 ? "tail" : sample == 1 ? null : "");
    c.setEventTimeSequence((1600000000L << 32) + (123L << 22));
    return buffer;
  }

  private static void production(Path root, String name, DataRecord r, boolean raw)
      throws Exception {
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(raw ? TimestampsType.MESSAGE : TimestampsType.NONE);
    writer.setOpt("");
    writer.setSaveAs(raw ? MessageType.RAW_DATA : MessageType.STREAM_DATA);
    try (FileWriter opened = writer.open(root.resolve(name + ".qtp").toString(), r.getScheme())) {
      for (int i = 0; i < 3; i++) {
        opened.visitData(values(r, i), MessageType.STREAM_DATA);
      }
    }
    ByteArrayInput frameInput = new ByteArrayInput(Files.readAllBytes(root.resolve(name + ".qtp")));
    int dataFrames = 0;
    while (frameInput.available() > 0) {
      int length = frameInput.readCompactInt();
      int end = frameInput.getPosition() + length;
      if (length > 0) {
        int type = frameInput.readCompactInt();
        if (type == (raw ? 5 : 15)) {
          dataFrames++;
        }
      }
      frameInput.skipBytes(end - frameInput.getPosition());
    }
    require(dataFrames == 3, "one singleton visit per data frame " + name);
    replay(root, name, r.getScheme());
  }

  private static void composer(Path root) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    BinaryQTPComposer composer = new BinaryQTPComposer(SYNTH, true);
    composer.setOutput(out);
    composer.setOptSet(ProtocolOption.EMPTY_SET);
    for (int i = 0; i < 3; i++) {
      composer.visitData(values(MIXED, i), MessageType.STREAM_DATA);
    }
    Files.write(root.resolve("composer.qtp"), bytes(out));
    try (PrintWriter log = output(root.resolve("composer.tsv"))) {
      row(log, "case", "composer");
      ByteArrayInput in = new ByteArrayInput(bytes(out));
      BinaryQTPParser parser = new BinaryQTPParser(SYNTH);
      parser.setInput(in);
      Observer o = new Observer(log);
      parser.parse(o);
      row(log, "consumed", in.getPosition(), in.available());
      row(log, "end", o.count, o.errors);
      row(log, "complete", "composer");
      require(o.count == 3 && o.errors == 0, "composer replay");
    }
  }

  private static byte[] header(Path root) throws Exception {
    byte[] b = Files.readAllBytes(root.resolve("mixed-stream.qtp"));
    ByteArrayInput in = new ByteArrayInput(b);
    int n = in.readCompactInt();
    return Arrays.copyOf(b, in.getPosition() + n);
  }

  private static void diagnostic(
      Path root,
      String name,
      String record,
      String[] fields,
      int[] types,
      byte[][] values,
      boolean append)
      throws Exception {
    ByteArrayOutput desc = new ByteArrayOutput();
    desc.writeCompactInt(77);
    desc.writeUTFString(record);
    desc.writeCompactInt(fields.length);
    for (int i = 0; i < fields.length; i++) {
      desc.writeUTFString(fields[i]);
      desc.writeCompactInt(types[i]);
    }
    ByteArrayOutput data = new ByteArrayOutput();
    SymbolCodec.Writer symbol = SYNTH.getCodec().createWriter();
    symbol.reset(ProtocolOption.EMPTY_SET);
    symbol.writeSymbol(data, SYNTH.getCodec().encode("IBM"), null, 0);
    data.writeCompactInt(77);
    for (byte[] v : values) {
      data.write(v);
    }
    byte[] tape = concat(header(root), frame(2, bytes(desc)), frame(15, bytes(data)));
    if (append) {
      ByteArrayOutput next = new ByteArrayOutput();
      next.writeCompactInt(78);
      next.writeUTFString("FieldTypes");
      next.writeCompactInt(1);
      next.writeUTFString("Count");
      next.writeCompactInt(8);
      ByteArrayOutput d = new ByteArrayOutput();
      symbol.reset(ProtocolOption.EMPTY_SET);
      symbol.writeSymbol(d, SYNTH.getCodec().encode("NEXT"), null, 0);
      d.writeCompactInt(78);
      d.writeCompactInt(913);
      tape = concat(tape, frame(2, bytes(next)), frame(15, bytes(d)));
    }
    Files.write(root.resolve(name + ".qtp"), concat(tape, new byte[] {0}));
    replay(root, name, SYNTH);
  }

  private static void diagnostics(Path root) throws Exception {
    diagnostic(root, "missing", "FieldTypes", new String[0], new int[0], new byte[0][], true);
    for (int i = 0; i < 3; i++) {
      String v = i == 0 ? null : i == 1 ? "" : "last";
      diagnostic(
          root,
          "repeat-" + i,
          "FieldTypes",
          new String[] {"Text", "Count", "Text"},
          new int[] {10, 8, 10},
          new byte[][] {text("first"), compact(71), text(v)},
          true);
    }
    diagnostic(
        root,
        "mixed-order",
        "FieldTypes",
        NAMES,
        TYPES,
        new byte[][] {
          text("mixed"),
          compact(-1),
          compact(12345),
          compact(-700),
          compact(42),
          unhex("eda080"),
          compact(-9),
          compact(Long.MIN_VALUE),
          text("")
        },
        true);
    diagnostic(
        root,
        "unknown-field",
        "FieldTypes",
        new String[] {"Count", "Unknown", "Date"},
        new int[] {8, 10, 88},
        new byte[][] {compact(41), text("ignored\udfff"), compact(-9)},
        true);
    diagnostic(
        root,
        "unknown-record",
        "Unknown",
        new String[] {"Text", "Count"},
        new int[] {10, 8},
        new byte[][] {text("ignored"), compact(41)},
        true);
    diagnostic(
        root,
        "malformed-skipped",
        "FieldTypes",
        new String[] {"Count", "Unknown", "Date"},
        new int[] {8, 10, 88},
        new byte[][] {compact(41), unhex("01f0908080"), compact(-9)},
        true);
    diagnostic(
        root,
        "malformed-unknown-record",
        "Unknown",
        new String[] {"Text"},
        new int[] {10},
        new byte[][] {unhex("01f0908080")},
        true);
    diagnostic(
        root,
        "mismatch-int",
        "FieldTypes",
        new String[] {"Short", "Sequence", "Date"},
        new int[] {8, 40, 72},
        new byte[][] {compact(65), compact(66), compact(67)},
        true);
    diagnostic(
        root,
        "mismatch-object",
        "FieldTypes",
        new String[] {"Text", "Count"},
        new int[] {8, 8},
        new byte[][] {compact(5), compact(6)},
        true);
    diagnostic(
        root,
        "oversized-skipped",
        "FieldTypes",
        new String[] {"Unknown"},
        new int[] {10},
        new byte[][] {compact(Integer.MAX_VALUE)},
        true);
    String[] probes = {
      "7f",
      "00",
      "0141",
      "01c080",
      "01e08080",
      "01f0808080",
      "01f0908080",
      "01c200",
      "02eda0bdedb880",
      "7e",
      "800141"
    };
    for (int i = 0; i < probes.length; i++) {
      diagnostic(
          root,
          "string-wire-" + i,
          "FieldTypes",
          new String[] {"Text", "Count"},
          new int[] {10, 8},
          new byte[][] {unhex(probes[i]), compact(93)},
          true);
    }
    for (int t : new int[] {40, 72, 88}) {
      for (long v : new long[] {2147483648L, 4294967295L, Long.MIN_VALUE, Long.MAX_VALUE}) {
        diagnostic(
            root,
            "wide-" + t + "-" + v,
            "FieldTypes",
            new String[] {t == 40 ? "Short" : t == 72 ? "Sequence" : "Date", "Count"},
            new int[] {t, 8},
            new byte[][] {compact(v), compact(94)},
            true);
      }
    }
  }

  public static void main(String[] args) throws Exception {
    require(
        args.length == 1
            || (args.length == 2
                && (args[1].equals("scalar-only")
                    || args[1].equals("object-only")
                    || args[1].equals("indexed-only")
                    || args[1].equals("flags-only"))),
        "new output directory required");
    Path root = Paths.get(args[0]);
    Files.createDirectory(root);
    if (args.length == 2) {
      if (args[1].equals("scalar-only")) {
        Scalars.capture(root);
      } else if (args[1].equals("object-only")) {
        ObjectFieldsOracle.capture(root);
      } else if (args[1].equals("indexed-only")) {
        IndexedSchemasOracle.capture(root);
      } else {
        FlagsOracle.capture(root);
      }
      return;
    }
    schema(root);
    primitives(root);
    production(root, "mixed-stream", MIXED, false);
    production(root, "mixed-raw", MIXED, true);
    production(root, "profile-stream", PROFILE, false);
    production(root, "profile-raw", PROFILE, true);
    composer(root);
    diagnostics(root);
    Scalars.capture(Files.createDirectory(root.resolve("scalars")));
    ObjectFieldsOracle.capture(Files.createDirectory(root.resolve("objects")));
    IndexedSchemasOracle.capture(Files.createDirectory(root.resolve("indexed")));
    FlagsOracle.capture(Files.createDirectory(root.resolve("flags")));
    System.out.println(
        "CAPTURE COMPLETE: exact field types, original Profile and production paths");
  }

  /** Requested wire formats and actual cursor slots are deliberately separate observations. */
  private static final class Scalars {
    static final int[] TYPES = {0, 1, 3, 4, 24, 104, 152};
    static final long SENTINEL = 913;

    static DataRecord record(int type) {
      DataIntField value;
      switch (type) {
        case 0:
          value = new VoidIntField(0, "Scalar.Value");
          break;
        case 1:
        case 3:
          value = new CompactIntField(0, "Scalar.Value");
          break;
        case 4:
          value = new PlainIntField(0, "Scalar.Value");
          break;
        case 24:
          value = new DecimalField(0, "Scalar.Value");
          break;
        case 104:
          value = new LongField(0, "Scalar.Value");
          break;
        case 152:
          value = new TimeMillisField(0, "Scalar.Value");
          break;
        default:
          throw new IllegalArgumentException("scalar type " + type);
      }
      require(value != null, "production consumer field");
      boolean wide = type == 104 || type == 152;
      DataIntField[] fields =
          wide
              ? new DataIntField[] {
                value,
                new VoidIntField(1, "Scalar.Value$VoidTail"),
                new CompactIntField(2, "Scalar.Sentinel")
              }
              : new DataIntField[] {value, new CompactIntField(1, "Scalar.Sentinel")};
      DataRecord record = new DefaultRecord(0, "Scalar", false, fields, new DataObjField[0]);
      new DefaultScheme(PentaCodec.INSTANCE, record);
      return record;
    }

    static final class Requested extends BinaryRecordDesc {
      Requested(DataRecord record, int type) throws Exception {
        super(record, 2, new String[] {"Value", "Sentinel"}, new int[] {type, 8}, false, DIR_READ);
      }

      int count() {
        return nDesc;
      }
    }

    static final class Trace extends BinaryRecordDesc {
      long examined;

      Trace(BinaryRecordDesc source) {
        super(source);
      }

      @Override
      public RecordCursor readRecord(
          BufferedInput input, RecordBuffer buffer, int cipher, String symbol, int flags)
          throws IOException {
        long start = input.totalPosition();
        try {
          return super.readRecord(input, buffer, cipher, symbol, flags);
        } finally {
          examined = input.totalPosition() - start;
        }
      }
    }

    static final class Seen extends MessageConsumerAdapter {
      int records, errors, streamRecords;
      long value, sentinel, callbackValue, callbackSentinel;

      @Override
      protected void processData(DataIterator iterator, MessageType type) {
        RecordCursor cursor;
        while ((cursor = ((RecordSource) iterator).next()) != null) {
          DataRecord record = cursor.getRecord();
          boolean wide = record.getIntField(0).getSerialType().isLong();
          long raw = wide ? cursor.getLong(0) : cursor.getInt(0);
          long tail = cursor.getInt(wide ? 2 : 1);
          require(
              records == 0 || (raw == callbackValue && tail == callbackSentinel),
              "RAW callback values agree");
          callbackValue = raw;
          callbackSentinel = tail;
          records++;
          if (type == MessageType.STREAM_DATA) {
            value = raw;
            sentinel = tail;
            streamRecords++;
          }
        }
      }

      @Override
      public void handleCorruptedStream() {
        errors++;
      }

      @Override
      public void handleCorruptedMessage(int type) {
        errors++;
      }

      @Override
      public void handleUnknownMessage(int type) {
        errors++;
      }
    }

    static byte[] description(int type) throws Exception {
      ByteArrayOutput out = new ByteArrayOutput();
      out.writeCompactInt(0);
      out.writeUTFString("Scalar");
      out.writeCompactInt(2);
      out.writeUTFString("Value");
      out.writeCompactInt(type);
      out.writeUTFString("Sentinel");
      out.writeCompactInt(8);
      return frame(2, bytes(out));
    }

    static byte[] payload(byte[] body) throws Exception {
      ByteArrayOutput out = new ByteArrayOutput();
      SymbolCodec.Writer symbols = PentaCodec.INSTANCE.createWriter();
      symbols.reset(ProtocolOption.EMPTY_SET);
      symbols.writeSymbol(out, PentaCodec.INSTANCE.encode("IBM"), null, 0);
      out.writeCompactInt(0);
      out.write(body);
      return bytes(out);
    }

    static byte[] physical(int type, long value) throws Exception {
      ByteArrayOutput out = new ByteArrayOutput();
      switch (type) {
        case 0:
          break;
        case 1:
          out.writeByte((int) value);
          break;
        case 3:
          out.writeShort((int) value);
          break;
        case 4:
          out.writeInt((int) value);
          break;
        default:
          out.writeCompactLong(value);
      }
      return bytes(out);
    }

    static Seen observe(DataRecord record, byte[] wire, long[] examined) {
      final Trace[] trace = new Trace[1];
      BinaryQTPParser parser =
          new BinaryQTPParser(record.getScheme()) {
            @Override
            protected BinaryRecordDesc wrapRecordDesc(BinaryRecordDesc source) {
              return trace[0] = new Trace(source);
            }
          };
      parser.setInput(new ByteArrayInput(wire));
      Seen seen = new Seen();
      parser.parse(seen);
      require(seen.streamRecords == (seen.records == 0 ? 0 : 1), "one STREAM callback projection");
      examined[0] = trace[0] == null ? -1 : trace[0].examined;
      return seen;
    }

    static void sample(PrintWriter out, String id, int type, byte[] body, boolean sentinel)
        throws Exception {
      DataRecord record = record(type);
      byte[] data = payload(sentinel ? concat(body, compact(SENTINEL)) : body);
      for (int family : new int[] {15, 5}) {
        long[] examined = new long[1];
        Seen seen = observe(record, concat(description(type), frame(family, data)), examined);
        row(
            out,
            "sample",
            id,
            type,
            family,
            hex(data),
            seen.records,
            seen.errors,
            seen.records == 0 ? "-" : Long.toString(seen.value),
            seen.records == 0 ? "-" : Long.toString(seen.sentinel),
            examined[0]);
      }
    }

    static void composed(PrintWriter out, Path root, int type) throws Exception {
      final DataRecord record = record(type);
      final Requested requested = new Requested(record, type);
      row(out, "binding", type, record.getIntField(0).getSerialType().getId(), requested.count());
      RecordBuffer buffer = new RecordBuffer(RecordMode.TIMESTAMPED_DATA);
      RecordCursor cursor = buffer.add(record, record.getScheme().getCodec().encode("IBM"), null);
      boolean wide = type == 104 || type == 152;
      if (wide) {
        cursor.setLong(0, Long.MIN_VALUE);
      } else {
        cursor.setInt(0, -1);
      }
      cursor.setInt(wide ? 2 : 1, (int) SENTINEL);
      cursor.setEventTimeSequence((1600000000L << 32) + (123L << 22));
      for (boolean raw : new boolean[] {false, true}) {
        MessageType family = raw ? MessageType.RAW_DATA : MessageType.STREAM_DATA;
        String suffix = raw ? "-raw" : "";
        ByteArrayOutput encoded = new ByteArrayOutput();
        BinaryQTPComposer composer =
            new BinaryQTPComposer(record.getScheme(), true) {
              @Override
              protected BinaryRecordDesc getRequestedRecordDesc(DataRecord ignored) {
                return requested;
              }
            };
        composer.setOptSet(ProtocolOption.EMPTY_SET);
        composer.setOutput(encoded);
        buffer.rewind();
        composer.visitData(buffer, family);
        byte[] wire = bytes(encoded);
        Files.write(root.resolve("composer-" + type + suffix + ".qtp"), wire);
        long[] examined = new long[1];
        Seen seen = observe(record, wire, examined);
        require(
            seen.records == (raw ? 3 : 1) && seen.errors == 0 && seen.sentinel == SENTINEL,
            "requested composer callback");
        row(out, "composer", type, family.getId(), seen.value, seen.sentinel, examined[0]);
        if (type == 1 || type == 3) {
          continue;
        }
        FileWriter writer = new FileWriter();
        writer.setFormat(FileFormat.BINARY);
        writer.setCompression(StreamCompression.NONE);
        writer.setTime(raw ? TimestampsType.MESSAGE : TimestampsType.NONE);
        writer.setSaveAs(family);
        writer.setOpt("");
        Path path = root.resolve("file-" + type + suffix + ".qtp");
        try (FileWriter opened = writer.open(path.toString(), record.getScheme())) {
          buffer.rewind();
          opened.visitData(buffer, MessageType.STREAM_DATA);
        }
        FileReaderParams.Default params = new FileReaderParams.Default();
        params.setSpeed(FileReaderParams.MAX_SPEED);
        params.setCycle(false);
        FileReader reader = new FileReader(path.toString(), new ConnectionStats(), params);
        reader.setScheme(record.getScheme());
        Seen delivered = new Seen();
        try {
          reader.readInto(delivered);
        } finally {
          reader.close();
        }
        require(
            delivered.records == (raw ? 3 : 1)
                && delivered.streamRecords == 1
                && delivered.errors == 0
                && delivered.sentinel == SENTINEL,
            "FileWriter/FileReader callback");
        row(out, "file", type, family.getId(), delivered.value, delivered.sentinel);
      }
    }

    static void capture(Path root) throws Exception {
      try (PrintWriter out = output(root.resolve("samples.tsv"))) {
        row(out, "case", "scalar-samples");
        require(
            SerialFieldType.BYTE.createDefaultIntInstance(0, "Scalar.Value") == null,
            "BYTE default absent");
        require(
            SerialFieldType.SHORT.createDefaultIntInstance(0, "Scalar.Value") == null,
            "SHORT default absent");
        row(out, "factory", 1, "null");
        row(out, "factory", 3, "null");
        for (int type : TYPES) {
          composed(out, root, type);
          long[] values =
              type == 0
                  ? new long[] {0}
                  : type == 1
                      ? new long[] {-128, -1, 0, 127}
                      : type == 3
                          ? new long[] {-32768, -1, 0, 32767}
                          : type == 4
                              ? new long[] {Integer.MIN_VALUE, -1, 0, Integer.MAX_VALUE}
                              : new long[] {
                                Long.MIN_VALUE,
                                Integer.MIN_VALUE,
                                -1,
                                0,
                                Integer.MAX_VALUE,
                                2147483648L,
                                4294967295L,
                                4294967296L,
                                Long.MAX_VALUE
                              };
          for (int i = 0; i < values.length; ++i) {
            byte[] body = physical(type, values[i]);
            sample(out, "value-" + i, type, body, true);
            for (int n = 0; n < body.length; ++n) {
              sample(out, "value-" + i + "-prefix-" + n, type, Arrays.copyOf(body, n), false);
            }
            sample(out, "value-" + i + "-sentinel-prefix-0", type, body, false);
            sample(
                out,
                "value-" + i + "-sentinel-prefix-1",
                type,
                concat(body, new byte[] {(byte) 0x83}),
                false);
          }
          if (type == 24 || type == 104 || type == 152) {
            String[] forms = {
              "8000",
              "bfff",
              "c00000",
              "dfffff",
              "e0000000",
              "efffffff",
              "f000000000",
              "f7ffffffff",
              "f80000000000",
              "fc000000000000",
              "fe00000000000000",
              "ff0000000000000000",
              "ffffffffffffffffff",
              "ff0000000080000000"
            };
            for (int i = 0; i < forms.length; ++i) {
              byte[] body = unhex(forms[i]);
              sample(out, "overlong-" + i, type, body, true);
              for (int n = 0; n < body.length; ++n) {
                sample(out, "overlong-" + i + "-prefix-" + n, type, Arrays.copyOf(body, n), false);
              }
            }
          }
        }
        row(out, "complete", "scalar-samples");
        require(!out.checkError(), "scalar samples output");
      }
    }
  }
}
