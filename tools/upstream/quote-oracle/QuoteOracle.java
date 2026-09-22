package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.SymbolCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolOption;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.transport.stats.ConnectionStats;
import com.devexperts.util.WideDecimal;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.api.impl.SchemeProperties;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.Quote;
import com.dxfeed.event.market.QuoteDelegate;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

/** Real unchanged default Quote round trips and bounded focused production-parser observations. */
public final class QuoteOracle {
  private static final DXFeedScheme SCHEME = DXFeedScheme.getInstance();
  private static final DataRecord RECORD = SCHEME.findRecordByName("Quote");
  private static final int[] SLOTS = {0, 1, 2, 4, 6, 7, 8, 10};
  private static final int[] TYPES = {56, 2, 120, 120, 56, 2, 120, 120};
  private static final String[] NAMES = {
    "BidTime",
    "BidExchangeCode",
    "BidPrice",
    "BidSize",
    "AskTime",
    "AskExchangeCode",
    "AskPrice",
    "AskSize"
  };
  private static final QuoteDelegate DELEGATE = delegate();

  private static QuoteDelegate delegate() {
    for (EventDelegate<?> d : new MarketFactoryImpl().createStreamOnlyDelegates(RECORD)) {
      if (d instanceof QuoteDelegate) {
        return (QuoteDelegate) d;
      }
    }
    throw new IllegalStateException("No production Quote delegate");
  }

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

  private static String units(String s) {
    StringBuilder result = new StringBuilder();
    for (char c : s.toCharArray()) {
      result.append(String.format("%04x", (int) c));
    }
    return result.length() == 0 ? "-" : result.toString();
  }

  private static String hex(byte[] bytes) {
    StringBuilder result = new StringBuilder();
    for (byte b : bytes) {
      result.append(String.format("%02x", b & 255));
    }
    return result.length() == 0 ? "-" : result.toString();
  }

  private static byte[] bytes(ByteArrayOutput out) {
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  private static String bits(double value) {
    return String.format("%016x", Double.doubleToLongBits(value));
  }

  private static PrintWriter output(Path file) throws Exception {
    return new PrintWriter(Files.newBufferedWriter(file, StandardCharsets.UTF_8));
  }

  private static void schema(Path root) throws Exception {
    require(
        RECORD.getIntFieldCount() == 12 && RECORD.getObjFieldCount() == 0 && !RECORD.hasTime(),
        "layout");
    require(
        new SchemeProperties(System.getProperties()).getProperties().isEmpty(),
        "No scheme overrides allowed");
    try (PrintWriter out = output(root.resolve("schema.tsv"))) {
      row(
          out,
          "schema",
          SCHEME.getClass().getName(),
          SCHEME.getRecordCount(),
          RECORD.getId(),
          SCHEME.getCodec().getClass().getName());
      row(
          out,
          "settings",
          "overrides=none",
          "wide=true",
          "regional=unchanged",
          "sequence=disabled",
          "nano=disabled");
      for (int i = 0; i < SCHEME.getRecordCount(); i++) {
        DataRecord r = SCHEME.getRecord(i);
        row(
            out,
            "scheme-record",
            r.getId(),
            units(r.getName()),
            r.getIntFieldCount(),
            r.getObjFieldCount(),
            r.hasTime());
        for (int j = 0; j < r.getIntFieldCount(); j++) {
          DataIntField f = r.getIntField(j);
          row(
              out,
              "scheme-int",
              i,
              j,
              units(f.getLocalName()),
              units(f.getPropertyName()),
              f.getSerialType().getId());
        }
        for (int j = 0; j < r.getObjFieldCount(); j++) {
          row(
              out,
              "scheme-obj",
              i,
              j,
              units(r.getObjField(j).getLocalName()),
              units(r.getObjField(j).getPropertyName()),
              r.getObjField(j).getSerialType().getId());
        }
      }
      for (int i = 0; i < SLOTS.length; i++) {
        DataIntField f = RECORD.getIntField(SLOTS[i]);
        require(
            f.getSerialType().getId() == TYPES[i] && f.getPropertyName().equals(NAMES[i]),
            "identity");
        row(
            out,
            "quote-field",
            i,
            SLOTS[i],
            units(f.getLocalName()),
            units(f.getPropertyName()),
            TYPES[i]);
      }
      row(out, "complete", "schema");
    }
  }

  private static void event(PrintWriter out, String kind, int index, Quote q) {
    row(
        out,
        kind,
        index,
        units(q.getEventSymbol()),
        q.getBidTime(),
        (int) q.getBidExchangeCode(),
        bits(q.getBidPrice()),
        bits(q.getBidSizeAsDouble()),
        q.getAskTime(),
        (int) q.getAskExchangeCode(),
        bits(q.getAskPrice()),
        bits(q.getAskSizeAsDouble()),
        q.getTime(),
        q.getSequence(),
        q.getTimeNanoPart(),
        q.getEventTime());
  }

  private static void cursor(PrintWriter out, int index, RecordCursor c) {
    row(
        out,
        "record",
        index,
        c.getRecord().getId(),
        units(SCHEME.getCodec().decode(c.getCipher(), c.getSymbol())),
        c.getEventFlags(),
        c.getEventTimeSequence());
    for (int i = 0; i < SLOTS.length; i++) {
      long raw = TYPES[i] == 120 ? c.getLong(SLOTS[i]) : c.getInt(SLOTS[i]);
      row(
          out,
          "field",
          index,
          i,
          TYPES[i],
          raw,
          TYPES[i] == 120 ? WideDecimal.toString(raw) : "-",
          TYPES[i] == 120 ? bits(WideDecimal.toDouble(raw)) : "-");
    }
    event(out, "mapped", index, DELEGATE.createEvent(c));
  }

  private static final class Observer extends MessageConsumerAdapter {
    private final PrintWriter out;
    private int count;
    private int errors;

    Observer(PrintWriter out) {
      this.out = out;
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      require(type == MessageType.STREAM_DATA, "message type");
      RecordCursor c;
      while ((c = ((RecordSource) iterator).next()) != null) {
        require(
            c.getRecord() == RECORD && c.getEventFlags() == 0 && c.getEventTimeSequence() == 0,
            "record/metadata");
        cursor(out, count++, c);
      }
    }

    @Override
    public void handleCorruptedStream() {
      errors++;
      row(out, "error", "stream");
    }

    @Override
    public void handleCorruptedMessage(int type) {
      errors++;
      row(out, "error", "message", type);
    }
  }

  private static void replay(Path root, String name, int expected) throws Exception {
    FileReaderParams.Default p = new FileReaderParams.Default();
    p.setSpeed(FileReaderParams.MAX_SPEED);
    p.setCycle(false);
    FileReader reader =
        new FileReader(root.resolve(name + ".qtp").toString(), new ConnectionStats(), p);
    reader.setScheme(SCHEME);
    try (PrintWriter out = output(root.resolve(name + ".tsv"))) {
      row(out, "case", name);
      Observer observer = new Observer(out);
      reader.readInto(observer);
      row(out, "end", observer.count, observer.errors);
      row(out, "complete", name);
      if (expected >= 0) {
        require(observer.count == expected && observer.errors == 0, "replay " + name);
      }
      require(!out.checkError(), "output");
    } finally {
      reader.close();
    }
  }

  private static void production(Path root) throws Exception {
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(TimestampsType.NONE);
    writer.setOpt("");
    writer.setSaveAs(MessageType.STREAM_DATA);
    double[][] values = {
      {100.25, 10.5, 100.5, 12.25},
      {200.125, 0.125, 200.25, 1.5},
      {101.5, 11.75, 102.0, 13.5},
      {-0.0, Double.NaN, Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY},
      {0.1, 1.2345678901234567, -1234567.890125, Double.MIN_VALUE}
    };
    try (FileWriter opened = writer.open(root.resolve("default-quote.qtp").toString(), SCHEME);
        PrintWriter out = output(root.resolve("inputs.tsv"));
        PrintWriter raw = output(root.resolve("put.tsv"))) {
      row(out, "case", "inputs");
      row(raw, "case", "put");
      for (int i = 0; i < values.length; i++) {
        Quote q = new Quote(i == 1 ? "MSFT" : i == 3 ? "SPECIAL" : "IBM");
        q.setBidTime(1700000000000L + i * 2000L + (i == 4 ? 123 : 0));
        q.setAskTime(1700000001000L + i * 2000L + (i == 4 ? 456 : 0));
        q.setBidExchangeCode('N');
        q.setAskExchangeCode('Q');
        q.setBidPrice(values[i][0]);
        q.setBidSizeAsDouble(values[i][1]);
        q.setAskPrice(values[i][2]);
        q.setAskSizeAsDouble(values[i][3]);
        q.setSequence(i == 4 ? 17 : 0);
        q.setTimeNanoPart(i == 4 ? 987654 : 0);
        event(out, "input", i, q);
        RecordBuffer buffer = new RecordBuffer(RecordMode.FLAGGED_DATA);
        RecordCursor c = DELEGATE.putEvent(q, buffer);
        cursor(raw, i, c);
        require(!opened.visitData(buffer, MessageType.STREAM_DATA), "capacity");
      }
      row(out, "complete", "inputs");
      row(raw, "end", values.length, 0);
      row(raw, "complete", "put");
    }
    replay(root, "default-quote", values.length);
  }

  private static byte[] frame(int type, byte[] payload) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    ByteArrayOutput body = new ByteArrayOutput();
    body.writeCompactInt(type);
    body.write(payload);
    out.writeCompactInt(body.getPosition());
    out.write(bytes(body));
    return bytes(out);
  }

  private static byte[] header(Path root) throws Exception {
    byte[] original = Files.readAllBytes(root.resolve("default-quote.qtp"));
    ByteArrayInput in = new ByteArrayInput(original);
    int length = in.readCompactInt();
    return Arrays.copyOf(original, in.getPosition() + length);
  }

  private static void probeTape(
      Path root,
      String name,
      String recordName,
      int[] fields,
      long[] values,
      byte[] rawChar,
      int expected)
      throws Exception {
    ByteArrayOutput desc = new ByteArrayOutput();
    desc.writeCompactInt(77);
    desc.writeUTFString(recordName);
    desc.writeCompactInt(fields.length);
    for (int f : fields) {
      desc.writeUTFString(f >= 8 ? "Unknown" + f : NAMES[f]);
      desc.writeCompactInt(TYPES[f % 8]);
    }
    ByteArrayOutput data = new ByteArrayOutput();
    SymbolCodec.Writer symbol = SCHEME.getCodec().createWriter();
    symbol.reset(ProtocolOption.EMPTY_SET);
    symbol.writeSymbol(data, SCHEME.getCodec().encode("IBM"), null, 0);
    data.writeCompactInt(77);
    for (int i = 0; i < fields.length; i++) {
      int type = TYPES[fields[i] % 8];
      if (type == 120) {
        data.writeCompactLong(values[i]);
      } else if (type == 2) {
        if (rawChar != null) {
          data.write(rawChar);
        } else {
          data.writeUTFChar((int) values[i]);
        }
      } else {
        data.writeCompactInt((int) values[i]);
      }
    }
    ByteArrayOutput tape = new ByteArrayOutput();
    tape.write(header(root));
    tape.write(frame(2, bytes(desc)));
    tape.write(frame(15, bytes(data)));
    tape.writeByte(0);
    Files.write(root.resolve(name + ".qtp"), bytes(tape));
    replay(root, name, expected);
  }

  private static void focused(Path root) throws Exception {
    try (PrintWriter out = output(root.resolve("primitives.tsv"))) {
      row(out, "case", "primitives");
      int[] chars = {
        -1, 0, 0x7f, 0x80, 0x7ff, 0x800, 0xd7ff, 0xd800, 0xdfff, 0xe000, 0xffff, 0x10000, 0x10ffff,
        0x110000
      };
      for (int c : chars) {
        ByteArrayOutput bytes = new ByteArrayOutput();
        String status = "ok";
        try {
          RECORD.getIntField(1).writeInt(bytes, c);
        } catch (Exception e) {
          status = e.getClass().getSimpleName();
        }
        row(out, "char-write", c, hex(bytes(bytes)), status, bytes.getPosition());
      }
      String[] encodings = {
        "",
        "00",
        "7f",
        "c080",
        "c1bf",
        "e08080",
        "eda080",
        "edbfbf",
        "efbfbf",
        "f0808080",
        "f0908080",
        "f48fbfbf",
        "f4908080",
        "80",
        "ff",
        "c2",
        "c200",
        "e0",
        "e080",
        "e00080",
        "e08000",
        "f0",
        "f080",
        "f08080",
        "f0008080",
        "f0800080",
        "f0808000"
      };
      for (int i = 0; i < encodings.length; i++) {
        String h = encodings[i];
        byte[] b = new byte[h.length() / 2];
        for (int j = 0; j < b.length; j++) {
          b[j] = (byte) Integer.parseInt(h.substring(j * 2, j * 2 + 2), 16);
        }
        ByteArrayInput in = new ByteArrayInput(b);
        String value = "-";
        String status = "ok";
        try {
          value = Integer.toString(RECORD.getIntField(1).readInt(in));
        } catch (Exception e) {
          status = e.getClass().getSimpleName();
        }
        row(out, "char-read", i, hex(b), value, status, in.getPosition());
        if (status.equals("ok")) {
          probeTape(root, "char-" + i, "Quote", new int[] {1}, new long[] {0}, b, 1);
        }
      }
      for (int value : new int[] {Integer.MIN_VALUE, -1, 0, 1, Integer.MAX_VALUE}) {
        ByteArrayOutput b = new ByteArrayOutput();
        RECORD.getIntField(0).writeInt(b, value);
        row(out, "time", value, hex(bytes(b)), ((long) value) * 1000);
        probeTape(
            root, "time-" + value, "Quote", new int[] {0, 4}, new long[] {value, value}, null, 1);
      }
      long[] raw = {
        0,
        128,
        1,
        255,
        256,
        -256,
        512,
        -512,
        384,
        -128,
        257,
        511,
        -255,
        -1,
        Long.MIN_VALUE,
        Long.MAX_VALUE,
        (9007199254740991L << 8) | 150,
        (9007199254740993L << 8) | 151,
        (36028797018963967L << 8) | 255
      };
      for (int i = 0; i < raw.length; i++) {
        long value = raw[i];
        ByteArrayOutput b = new ByteArrayOutput();
        b.writeCompactLong(value);
        row(
            out,
            "decimal-raw",
            i,
            value,
            hex(bytes(b)),
            WideDecimal.toString(value),
            bits(WideDecimal.toDouble(value)));
        probeTape(
            root,
            "decimal-" + i,
            "Quote",
            new int[] {2, 3, 6, 7},
            new long[] {value, value, value, value},
            null,
            1);
      }
      double[] doubles = {
        0.0,
        -0.0,
        Double.NaN,
        Double.POSITIVE_INFINITY,
        Double.NEGATIVE_INFINITY,
        0.1,
        10.5,
        12.25,
        1.2345678901234567,
        Double.MIN_VALUE,
        Double.MAX_VALUE,
        1e-127,
        1e127
      };
      for (int i = 0; i < doubles.length; i++) {
        long value = WideDecimal.composeWide(doubles[i]);
        row(
            out,
            "decimal-compose",
            i,
            bits(doubles[i]),
            value,
            WideDecimal.toString(value),
            bits(WideDecimal.toDouble(value)));
      }
      row(out, "complete", "primitives");
    }
    probeTape(root, "missing", "Quote", new int[0], new long[0], null, 1);
    probeTape(
        root,
        "reordered",
        "Quote",
        new int[] {7, 6, 5, 4, 3, 2, 1, 0},
        new long[] {32642, 25728, 81, 7, 27009, 25857, 78, -3},
        null,
        1);
    probeTape(root, "duplicate", "Quote", new int[] {2, 2}, new long[] {384, 640}, null, 1);
    probeTape(
        root,
        "unknown-fields",
        "Quote",
        new int[] {8, 9, 10, 2},
        new long[] {-9, 0x10000, Long.MIN_VALUE, 384},
        null,
        1);
    probeTape(
        root,
        "unknown-record",
        "UnknownQuote",
        new int[] {0, 1, 2},
        new long[] {-9, 0x10000, Long.MIN_VALUE},
        null,
        0);
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 1, "new output directory required");
    Path root = Paths.get(args[0]);
    Files.createDirectory(root);
    schema(root);
    production(root);
    focused(root);
    System.out.println("CAPTURE COMPLETE: full default Quote and focused production observations");
  }
}
