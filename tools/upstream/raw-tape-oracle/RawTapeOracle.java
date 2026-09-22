package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.HeartbeatPayload;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.ProtocolOption;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.qd.util.TimeSequenceUtil;
import com.devexperts.transport.stats.ConnectionStats;
import com.dxfeed.api.DXEndpoint;
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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/** Production RAW file observations, with separately identified diagnostic wire mutations. */
public final class RawTapeOracle {
  private static final DataScheme SYNTH =
      new DefaultScheme(
          PentaCodec.INSTANCE, record(0, "S", "v"), record(1, "R", "y", "x", "z"), record(2, "E"));
  private static final DXFeedScheme QUOTE = DXFeedScheme.getInstance();
  private static final int[] SLOTS = {0, 1, 2, 4, 6, 7, 8, 10};
  private static final QuoteDelegate DELEGATE = delegate();

  private static DefaultRecord record(int id, String name, String... fields) {
    DataIntField[] ints = new DataIntField[fields.length];
    for (int i = 0; i < fields.length; i++) {
      ints[i] = new CompactIntField(i, name + "." + fields[i]);
    }
    return new DefaultRecord(id, name, false, ints, new DataObjField[0]);
  }

  private static QuoteDelegate delegate() {
    for (EventDelegate<?> d :
        new MarketFactoryImpl().createStreamOnlyDelegates(QUOTE.findRecordByName("Quote"))) {
      if (d instanceof QuoteDelegate) {
        return (QuoteDelegate) d;
      }
    }
    throw new IllegalStateException("Quote delegate");
  }

  private static void require(boolean ok, String message) {
    if (!ok) {
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

  private static PrintWriter output(Path p) throws Exception {
    return new PrintWriter(Files.newBufferedWriter(p, StandardCharsets.UTF_8));
  }

  private static String units(String s) {
    if (s == null) {
      return "null";
    }
    if (s.isEmpty()) {
      return "-";
    }
    StringBuilder b = new StringBuilder();
    for (char c : s.toCharArray()) {
      b.append(String.format("%04x", (int) c));
    }
    return b.toString();
  }

  private static String bits(double d) {
    return String.format("%016x", Double.doubleToLongBits(d));
  }

  private static byte[] bytes(ByteArrayOutput out) {
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  private static byte[] concat(byte[]... parts) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    for (byte[] p : parts) {
      out.write(p);
    }
    return bytes(out);
  }

  private static byte[] frame(int type, byte[] payload) throws Exception {
    ByteArrayOutput body = new ByteArrayOutput();
    body.writeCompactInt(type);
    body.write(payload);
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactInt(body.getPosition());
    out.write(bytes(body));
    return bytes(out);
  }

  private static byte[] heartbeat(int flags, long... values) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactInt(flags);
    int i = 0;
    if ((flags & 1) != 0) {
      out.writeCompactLong(values[i++]);
    }
    for (int mask : new int[] {2, 4, 8}) {
      if ((flags & mask) != 0) {
        out.writeCompactInt((int) values[i++]);
      }
    }
    return bytes(out);
  }

  private static byte[] header(String opt, String time, int send) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    out.write(new byte[] {68, 88, 80, 51});
    out.writeCompactInt(1 + (opt.equals("absent") ? 0 : 1) + (time.equals("absent") ? 0 : 1));
    out.writeUTFString("type");
    out.writeUTFString("tape");
    if (!opt.equals("absent")) {
      out.writeUTFString("opt");
      out.writeUTFString(opt.equals("null") ? null : opt);
    }
    if (!time.equals("absent")) {
      out.writeUTFString("time");
      out.writeUTFString(time);
    }
    out.writeCompactInt(send == -1 ? 0 : 1);
    if (send != -1) {
      out.writeCompactInt(send);
      out.writeUTFString(MessageType.findById(send).toString());
      out.writeCompactInt(0);
    }
    out.writeCompactInt(0);
    return frame(1, bytes(out));
  }

  private static final class Observer extends MessageConsumerAdapter {
    final PrintWriter out;
    final DataScheme scheme;
    int records;
    int errors;
    int calls;

    Observer(PrintWriter out, DataScheme scheme) {
      this.out = out;
      this.scheme = scheme;
    }

    @Override
    public void processDescribeProtocol(ProtocolDescriptor d, boolean log) {
      row(
          out,
          "descriptor",
          calls++,
          d.getSendSet().toString(),
          d.getReceiveSet().toString(),
          units(d.getProperty("opt")),
          units(ProtocolOption.parseProtocolOptions(d.getProperty("opt")).toString()));
      for (Map.Entry<String, String> e : new TreeMap<>(d.getProperties()).entrySet()) {
        row(out, "property", units(e.getKey()), units(e.getValue()));
      }
    }

    @Override
    public void processHeartbeat(HeartbeatPayload h) {
      row(
          out,
          "heartbeat",
          calls++,
          h.hasTimeMillis() ? h.getTimeMillis() : "-",
          h.hasTimeMark() ? h.getTimeMark() : "-",
          h.hasDeltaMark() ? h.getDeltaMark() : "-",
          h.hasLagMark() ? h.getLagMark() : "-",
          h.isEmpty());
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      row(out, "callback", calls++, type.getId());
      RecordCursor c;
      while ((c = ((RecordSource) iterator).next()) != null) {
        int index = records++;
        row(
            out,
            "record",
            index,
            type.getId(),
            c.getRecord().getId(),
            units(c.getRecord().getName()),
            units(scheme.getCodec().decode(c.getCipher(), c.getSymbol())),
            c.getEventFlags(),
            c.getEventTimeSequence());
        if (scheme == QUOTE) {
          for (int i = 0; i < SLOTS.length; i++) {
            int slot = SLOTS[i];
            int t = c.getRecord().getIntField(slot).getSerialType().getId();
            row(out, "field", index, i, t, t == 120 ? c.getLong(slot) : c.getInt(slot));
          }
          Quote q = DELEGATE.createEvent(c);
          row(
              out,
              "mapped",
              index,
              q.getBidTime(),
              q.getAskTime(),
              (int) q.getBidExchangeCode(),
              (int) q.getAskExchangeCode(),
              bits(q.getBidPrice()),
              bits(q.getBidSizeAsDouble()),
              bits(q.getAskPrice()),
              bits(q.getAskSizeAsDouble()),
              q.getEventTime(),
              q.getSequence(),
              q.getTimeNanoPart());
        } else {
          for (int i = 0; i < c.getIntCount(); i++) {
            row(out, "field", index, i, 8, c.getInt(i));
          }
        }
      }
    }

    @Override
    public void handleCorruptedStream() {
      row(out, "error", calls++, "stream", -1);
      errors++;
    }

    @Override
    public void handleCorruptedMessage(int type) {
      row(out, "error", calls++, "message", type);
      errors++;
    }

    @Override
    public void handleUnknownMessage(int type) {
      row(out, "error", calls++, "unknown", type);
      errors++;
    }
  }

  private static void replay(Path root, String name, DataScheme scheme, String origin)
      throws Exception {
    FileReaderParams.Default p = new FileReaderParams.Default();
    p.setSpeed(FileReaderParams.MAX_SPEED);
    p.setCycle(false);
    FileReader r = new FileReader(root.resolve(name + ".qtp").toString(), new ConnectionStats(), p);
    r.setScheme(scheme);
    try (PrintWriter out = output(root.resolve(name + ".tsv"))) {
      row(out, "case", name, origin);
      Observer o = new Observer(out, scheme);
      r.readInto(o);
      row(out, "end", o.records, o.errors, o.calls);
      row(out, "complete", name);
      require(!out.checkError(), "output");
    } finally {
      r.close();
    }
    try (PrintWriter out = output(root.resolve(name + "-frames.tsv"))) {
      byte[] b = Files.readAllBytes(root.resolve(name + ".qtp"));
      ByteArrayInput in = new ByteArrayInput(b);
      int n = 0;
      while (in.available() > 0) {
        int start = in.getPosition();
        try {
          int length = in.readCompactInt();
          int end = in.getPosition() + length;
          require(length >= 0 && end <= b.length, "framing");
          int type = length == 0 ? -1 : in.readCompactInt();
          row(out, "frame", n++, start, end, type);
          in.skipBytes(end - in.getPosition());
        } catch (Exception e) {
          row(out, "invalid", start, b.length);
          break;
        }
      }
      row(out, "complete", "frames", n, b.length);
    }
  }

  private static Quote quote(int i) {
    Quote q = new Quote(i == 1 ? "MSFT" : "IBM");
    q.setBidTime(1700000000000L + i * 2000L);
    q.setAskTime(1700000001000L + i * 2000L);
    q.setBidExchangeCode('N');
    q.setAskExchangeCode('Q');
    q.setBidPrice(100.25 + i);
    q.setAskPrice(100.5 + i);
    q.setBidSizeAsDouble(10.5 + i);
    q.setAskSizeAsDouble(12.25 + i);
    q.setEventTime(1600000000123L + i * 1000L);
    return q;
  }

  private static void production(
      Path root, String name, DataScheme scheme, String opt, MessageType save) throws Exception {
    FileWriter w = new FileWriter();
    w.setFormat(FileFormat.BINARY);
    w.setCompression(StreamCompression.NONE);
    w.setTime(TimestampsType.MESSAGE);
    w.setSaveAs(save);
    w.setOpt(opt);
    try (FileWriter opened = w.open(root.resolve(name + ".qtp").toString(), scheme)) {
      for (int i = 0; i < 3; i++) {
        HeartbeatPayload h = new HeartbeatPayload();
        h.setTimeMillis(1600000000123L + i * 1000L);
        opened.visitHeartbeat(h);
        RecordBuffer b = new RecordBuffer(RecordMode.FLAGGED_DATA);
        if (scheme == QUOTE) {
          DELEGATE.putEvent(quote(i), b);
        } else {
          DataRecord rec = SYNTH.getRecord(i);
          RecordCursor c = b.add(rec, SYNTH.getCodec().encode("ABC"), null);
          for (int j = 0; j < rec.getIntFieldCount(); j++) {
            c.setInt(j, i * 10 + j);
          }
        }
        require(!opened.visitData(b, MessageType.STREAM_DATA), "capacity");
      }
    }
    replay(root, name, scheme, "FileWriter");
  }

  private static void connector(Path root, boolean defaultOptions) throws Exception {
    Path file = root.resolve(defaultOptions ? "connector-default.qtp" : "connector-quote.qtp");
    String relative =
        Paths.get("")
            .toAbsolutePath()
            .relativize(file.toAbsolutePath())
            .toString()
            .replace('\\', '/');
    DXEndpoint endpoint =
        DXEndpoint.newBuilder()
            .withRole(DXEndpoint.Role.PUBLISHER)
            .withProperty(DXEndpoint.DXFEED_WILDCARD_ENABLE_PROPERTY, "true")
            .withProperty(DXEndpoint.DXENDPOINT_EVENT_TIME_PROPERTY, "true")
            .build();
    try {
      endpoint.connect(
          "tape:"
              + relative
              + (defaultOptions
                  ? "[format=binary,time=message]"
                  : "[format=binary,time=message,opt=]"));
      for (int i = 0; i < 3; i++) {
        endpoint.getPublisher().publishEvents(Arrays.asList(quote(i)));
        endpoint.awaitProcessed();
      }
    } finally {
      endpoint.closeAndAwaitTermination();
    }
    replay(
        root,
        defaultOptions ? "connector-default" : "connector-quote",
        QUOTE,
        "DXEndpoint-TapeConnector-FileWriterHandler");
  }

  private static List<byte[]> split(byte[] b) throws Exception {
    List<byte[]> result = new ArrayList<>();
    ByteArrayInput in = new ByteArrayInput(b);
    while (in.available() > 0) {
      int start = in.getPosition();
      int length = in.readCompactInt();
      in.skipBytes(length);
      result.add(Arrays.copyOfRange(b, start, in.getPosition()));
    }
    return result;
  }

  private static void probe(Path root, String name, byte[]... parts) throws Exception {
    Files.write(root.resolve(name + ".qtp"), concat(parts));
    replay(root, name, SYNTH, "diagnostic-wire");
  }

  private static void focused(Path root) throws Exception {
    List<byte[]> frames = split(Files.readAllBytes(root.resolve("raw-synthetic.qtp")));
    byte[] desc = frames.get(2), data = frames.get(3), descR = frames.get(5), dataR = frames.get(6);
    byte[] h = header("", "MESSAGE", 5);
    for (String opt : new String[] {"absent", "", "null", "hs", "unknown", "hs,unknown"}) {
      probe(
          root,
          "opt-" + (opt.isEmpty() ? "empty" : opt.replace(',', '-')),
          header(opt, "MESSAGE", 5),
          desc,
          frame(0, heartbeat(1, 123)),
          data);
    }
    for (String time : new String[] {"absent", "NONE", "MESSAGE", "FIELD"}) {
      for (int send : new int[] {-1, 5, 15}) {
        for (int actual : new int[] {5, 15}) {
          ByteArrayInput d = new ByteArrayInput(data);
          int len = d.readCompactInt();
          d.readCompactInt();
          byte[] payload = Arrays.copyOfRange(data, d.getPosition(), data.length);
          probe(
              root,
              "matrix-" + time + "-" + send + "-" + actual,
              header("", time, send),
              desc,
              frame(0, heartbeat(1, 123)),
              frame(actual, payload));
        }
      }
    }
    probe(
        root,
        "timing-sequence",
        h,
        desc,
        data,
        frame(0, heartbeat(1, 0)),
        data,
        frame(0, heartbeat(1, 1234)),
        data,
        frame(0, heartbeat(1, 1234)),
        data,
        frame(0, heartbeat(1, -1)),
        data,
        frame(0, heartbeat(1, 99)),
        frame(0, heartbeat(1, 77)),
        descR,
        dataR,
        dataR,
        frame(0, heartbeat(1, 999)));
    for (long time :
        new long[] {
          Long.MIN_VALUE,
          -2147483648001L,
          -2147483648000L,
          -1001,
          -1000,
          -999,
          -1,
          0,
          1,
          999,
          1000,
          2147483647999L,
          2147483648000L,
          Long.MAX_VALUE
        }) {
      probe(root, "boundary-" + time, h, desc, frame(0, heartbeat(1, time)), data);
    }
    probe(root, "empty-envelope", h, desc, new byte[] {0}, data);
    probe(root, "explicit-zero-empty", h, desc, frame(0, new byte[0]), data);
    probe(root, "heartbeat-absent", h, desc, frame(0, heartbeat(0)), data);
    probe(root, "heartbeat-unknown-bit", h, desc, frame(0, heartbeat(16)), data);
    probe(
        root,
        "heartbeat-extension",
        h,
        desc,
        frame(0, concat(heartbeat(1, 5), new byte[] {42})),
        data);
    probe(
        root,
        "heartbeat-fields",
        h,
        desc,
        frame(0, heartbeat(15, 5, Integer.MIN_VALUE, Integer.MAX_VALUE, -1)),
        data);
    probe(root, "heartbeat-marks-no-time", h, desc, frame(0, heartbeat(14, 7, 8, 9)), data);
    byte[] full = heartbeat(15, Long.MAX_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE, -1);
    for (int n = 0; n < full.length; n++) {
      probe(
          root, "heartbeat-truncated-" + n, h, desc, data, frame(0, Arrays.copyOf(full, n)), data);
    }
    byte[] timestampOnly = heartbeat(1, Long.MAX_VALUE);
    for (int n = 1; n < timestampOnly.length; n++) {
      probe(
          root,
          "timestamp-truncated-" + n,
          h,
          desc,
          data,
          frame(0, Arrays.copyOf(timestampOnly, n)),
          data);
    }
    probe(
        root,
        "heartbeat-preserves-time",
        h,
        desc,
        frame(0, heartbeat(1, 123)),
        frame(0, heartbeat(0)),
        data);
    probe(root, "heartbeat-negative-bits", h, desc, frame(0, heartbeat(-1, 5, 1, 2, 3)), data);
    ByteArrayInput raw = new ByteArrayInput(data);
    raw.readCompactInt();
    raw.readCompactInt();
    probe(
        root,
        "mixed-raw-stream",
        h,
        desc,
        data,
        frame(15, Arrays.copyOfRange(data, raw.getPosition(), data.length)));
    probe(root, "unsupported-message", h, desc, frame(99, new byte[] {1}), data);
    ByteArrayOutput flags = new ByteArrayOutput();
    com.devexperts.qd.SymbolCodec.Writer sw = SYNTH.getCodec().createWriter();
    sw.reset(ProtocolOption.SUPPORTED_SET);
    sw.writeSymbol(flags, SYNTH.getCodec().encode("ABC"), null, 1);
    flags.writeCompactInt(0);
    flags.writeCompactInt(42);
    probe(root, "nonzero-flags", header("hs", "MESSAGE", 5), desc, frame(5, bytes(flags)));
    probe(root, "nonzero-flags-empty-opt", h, desc, frame(5, bytes(flags)));
    try (PrintWriter out = output(root.resolve("packing.tsv"))) {
      for (long t :
          new long[] {
            Long.MIN_VALUE,
            -2147483648001L,
            -2147483648000L,
            -1,
            0,
            1,
            2147483647999L,
            2147483648000L,
            Long.MAX_VALUE
          }) {
        row(out, "packing", t, TimeSequenceUtil.getTimeSequenceFromTimeMillis(t));
      }
      row(out, "complete", "packing");
    }
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 1, "new output directory required");
    Path root = Paths.get(args[0]);
    Files.createDirectory(root);
    require(
        new SchemeProperties(System.getProperties()).getProperties().isEmpty(), "scheme overrides");
    try (PrintWriter out = output(root.resolve("settings.tsv"))) {
      row(
          out,
          "settings",
          "BINARY",
          "NONE",
          "MESSAGE",
          "RAW_DATA",
          "MAX_SPEED",
          "cycle=false",
          "overrides=none");
      row(
          out,
          "schema",
          QUOTE.getClass().getName(),
          QUOTE.getRecordCount(),
          QUOTE.findRecordByName("Quote").getIntFieldCount(),
          QUOTE.findRecordByName("Quote").getObjFieldCount());
      for (int i = 0; i < SLOTS.length; i++) {
        DataIntField field = QUOTE.findRecordByName("Quote").getIntField(SLOTS[i]);
        row(
            out,
            "schema-field",
            i,
            SLOTS[i],
            field.getSerialType().getId(),
            units(field.getPropertyName()));
      }
      for (int i = 0; i < 3; i++) {
        Quote q = quote(i);
        row(
            out,
            "input",
            i,
            units(q.getEventSymbol()),
            q.getEventTime(),
            q.getBidTime(),
            q.getAskTime(),
            bits(q.getBidPrice()),
            bits(q.getBidSizeAsDouble()),
            bits(q.getAskPrice()),
            bits(q.getAskSizeAsDouble()));
      }
      row(out, "complete", "settings");
    }
    production(root, "raw-quote", QUOTE, "", MessageType.RAW_DATA);
    production(root, "raw-synthetic", SYNTH, "", MessageType.RAW_DATA);
    production(root, "raw-hs", SYNTH, "hs", MessageType.RAW_DATA);
    production(root, "stream-synthetic", SYNTH, "", MessageType.STREAM_DATA);
    connector(root, false);
    connector(root, true);
    focused(root);
    System.out.println("CAPTURE COMPLETE: RAW production and diagnostic observations");
  }
}
