package com.dxfeed.event.market;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
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
import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.impl.DXEndpointImpl;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.scheme.DXScheme;
import com.dxfeed.scheme.SchemeLoadingOptions;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.TreeSet;

/** Bounded production delegate/file observations; package access records exact Profile flags. */
public final class EventMappingOracle {
  private static void require(boolean condition, String message) {
    if (!condition) {
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

  private static String units(String value) {
    if (value == null) {
      return "null";
    }
    if (value.isEmpty()) {
      return "-";
    }
    StringBuilder result = new StringBuilder();
    for (char c : value.toCharArray()) {
      result.append(String.format("%04x", (int) c));
    }
    return result.toString();
  }

  private static String bits(double value) {
    return String.format("%016x", Double.doubleToLongBits(value));
  }

  private static EventDelegate<?> delegate(DataRecord record) {
    for (EventDelegate<?> d : new MarketFactoryImpl().createStreamOnlyDelegates(record)) {
      if (d instanceof QuoteDelegate || d instanceof ProfileDelegate) {
        return d;
      }
    }
    throw new IllegalArgumentException("no event delegate: " + record.getName());
  }

  private static void event(PrintWriter out, String phase, int index, MarketEvent value) {
    if (value instanceof Quote) {
      Quote q = (Quote) value;
      row(
          out,
          "quote",
          phase,
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
          q.getEventTime(),
          q.getTimeMillisSequence());
    } else {
      Profile p = (Profile) value;
      row(
          out,
          "profile",
          phase,
          index,
          units(p.getEventSymbol()),
          bits(p.getBeta()),
          bits(p.getEarningsPerShare()),
          bits(p.getDividendFrequency()),
          bits(p.getExDividendAmount()),
          p.getExDividendDayId(),
          bits(p.getHigh52WeekPrice()),
          bits(p.getLow52WeekPrice()),
          bits(p.getShares()),
          bits(p.getFreeFloat()),
          bits(p.getHighLimitPrice()),
          bits(p.getLowLimitPrice()),
          p.getHaltStartTime(),
          p.getHaltEndTime(),
          p.getFlags(),
          units(p.getDescription()),
          units(p.getStatusReason()),
          p.getTradingStatus().getCode(),
          p.getShortSaleRestriction().getCode(),
          p.getEventTime());
    }
  }

  private static void schema(PrintWriter out, String id, DataScheme scheme) {
    for (String name : new String[] {"Quote", "Profile"}) {
      DataRecord r = scheme.findRecordByName(name);
      row(
          out,
          "record",
          id,
          name,
          r.getId(),
          r.hasTime(),
          r.getIntFieldCount(),
          r.getObjFieldCount());
      for (int i = 0; i < r.getIntFieldCount(); i++) {
        DataIntField f = r.getIntField(i);
        row(
            out,
            "schema",
            id,
            name,
            "int",
            i,
            f.getSerialType().getId(),
            units(f.getLocalName()),
            units(f.getPropertyName()));
      }
      for (int i = 0; i < r.getObjFieldCount(); i++) {
        DataObjField f = r.getObjField(i);
        row(
            out,
            "schema",
            id,
            name,
            "obj",
            i,
            f.getSerialType().getId(),
            units(f.getLocalName()),
            units(f.getPropertyName()));
      }
      for (String key :
          new String[] {
            "Bid.Time",
            "BidTime",
            "Bid.Exchange",
            "BidExchangeCode",
            "Bid.Price",
            "BidPrice",
            "52High.Price",
            "HighPrice52",
            "High52WeekPrice",
            "Description"
          }) {
        DataField f = r.findFieldByName(key);
        row(out, "lookup", id, name, key, f == null ? "absent" : units(f.getLocalName()));
      }
    }
  }

  private static void cursor(PrintWriter out, String phase, int index, RecordCursor c) {
    DataRecord r = c.getRecord();
    row(out, "cursor", phase, index, r.getName(), c.getEventFlags(), c.getEventTimeSequence());
    for (int i = 0; i < r.getIntFieldCount(); i++) {
      DataIntField f = r.getIntField(i);
      if (f.getSerialType().getId() == 0) {
        continue;
      }
      row(
          out,
          "value",
          phase,
          index,
          units(f.getLocalName()),
          f.getSerialType().getId(),
          f.getSerialType().isLong() ? c.getLong(i) : c.getInt(i),
          "null");
    }
    for (int i = 0; i < r.getObjFieldCount(); i++) {
      DataObjField f = r.getObjField(i);
      row(
          out,
          "value",
          phase,
          index,
          units(f.getLocalName()),
          f.getSerialType().getId(),
          0,
          units((String) c.getObj(i)));
    }
    event(out, phase, index, (MarketEvent) delegate(r).createEvent(c));
  }

  private static MarketEvent input(String name, int i) {
    String[] texts = {"A\u0000\ud800X\udfff\ud83d\ude00", null, ""};
    if (name.equals("Quote")) {
      Quote q = new Quote(i == 0 ? "IBM" : "\u0394TEST");
      q.setBidTime(new long[] {-2001, 1700000000123L, -2147483648000L}[i]);
      q.setAskTime(new long[] {-1001, 1700000001456L, 2147483647000L}[i]);
      q.setBidExchangeCode('N');
      q.setAskExchangeCode('Q');
      q.setBidPrice(i == 2 ? Double.NaN : 101.25 + i);
      q.setAskPrice(i == 2 ? Double.POSITIVE_INFINITY : 102.5 + i);
      q.setBidSizeAsDouble(i == 2 ? Double.NEGATIVE_INFINITY : 13.75 + i);
      q.setAskSizeAsDouble(i == 2 ? -0.0 : 17.125 + i);
      q.setSequence(17 + i);
      q.setTimeNanoPart(987654 - i);
      q.setEventTime(1600000000123L);
      return q;
    }
    Profile p = new Profile(i == 0 ? "IBM" : "\u0394TEST");
    p.setBeta(i == 1 ? Double.NaN : 1.25);
    p.setEarningsPerShare(i == 1 ? Double.POSITIVE_INFINITY : -2.5);
    p.setDividendFrequency(i == 1 ? Double.NEGATIVE_INFINITY : 3.75);
    p.setExDividendAmount(i == 1 ? -0.0 : 4.125);
    p.setExDividendDayId(new int[] {-17, 0, Integer.MAX_VALUE}[i]);
    p.setHigh52WeekPrice(105.5);
    p.setLow52WeekPrice(6.25);
    p.setShares(700.5);
    p.setFreeFloat(800.75);
    p.setHighLimitPrice(109.125);
    p.setLowLimitPrice(10.875);
    p.setHaltStartTime(new long[] {-2001, -2147483648000L, 0}[i]);
    p.setHaltEndTime(new long[] {3001, 2147483647000L, 0}[i]);
    p.setFlags(new int[] {0x76543216, -1, 0}[i]);
    p.setDescription(texts[i]);
    p.setStatusReason(texts[(i + 1) % 3]);
    p.setEventTime(1600000000123L);
    return p;
  }

  private static RecordCursor put(EventDelegate<?> d, MarketEvent e, RecordBuffer b) {
    return e instanceof Quote
        ? ((QuoteDelegate) d).putEvent((Quote) e, b)
        : ((ProfileDelegate) d).putEvent((Profile) e, b);
  }

  private static void roundTrip(
      Path root, PrintWriter out, DataScheme scheme, String name, boolean raw) throws Exception {
    String id = name + (raw ? "-raw" : "-stream");
    DataRecord record = scheme.findRecordByName(name);
    EventDelegate<?> d = delegate(record);
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(raw ? TimestampsType.MESSAGE : TimestampsType.NONE);
    writer.setOpt("");
    writer.setSaveAs(raw ? MessageType.RAW_DATA : MessageType.STREAM_DATA);
    try (FileWriter opened = writer.open(root.resolve(id + ".qtp").toString(), scheme)) {
      for (int i = 0; i < 3; i++) {
        MarketEvent value = input(name, i);
        event(out, id + "-input", i, value);
        RecordBuffer b = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
        RecordCursor c = put(d, value, b);
        cursor(out, id + "-put", i, c);
        require(!opened.visitData(b, MessageType.STREAM_DATA), "writer capacity");
      }
    }
    replay(root.resolve(id + ".qtp"), out, scheme, id, raw, 3);
  }

  private static void replay(
      Path path, PrintWriter out, DataScheme scheme, String id, boolean raw, int expected)
      throws Exception {
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    final int[] counts = {0, 0, 0};
    FileReader reader = new FileReader(path.toString(), new ConnectionStats(), params);
    reader.setScheme(scheme);
    try {
      reader.readInto(
          new MessageConsumerAdapter() {
            @Override
            protected void processData(DataIterator iterator, MessageType type) {
              RecordCursor c;
              while ((c = ((RecordSource) iterator).next()) != null) {
                require(counts[2]++ < 64, "finite callback limit");
                if (type == MessageType.STREAM_DATA) {
                  cursor(out, id + "-read", counts[0]++, c);
                }
              }
            }

            @Override
            public void handleCorruptedStream() {
              counts[1]++;
            }

            @Override
            public void handleCorruptedMessage(int type) {
              counts[1]++;
            }
          });
    } finally {
      reader.close();
    }
    row(out, "replay", id, counts[0], counts[1], counts[2]);
    require(counts[0] == expected && counts[1] == 0, "production replay " + id);
  }

  private static DataRecord cloneRecord(DataRecord source, int id, String mode) {
    List<DataField> fields = new ArrayList<>();
    for (int i = 0; i < source.getIntFieldCount(); i++) {
      if (source.getIntField(i).getSerialType().getId() != 0) {
        fields.add(source.getIntField(i));
      }
    }
    for (int i = 0; i < source.getObjFieldCount(); i++) {
      fields.add(source.getObjField(i));
    }
    if (mode.equals("reorder")) {
      Collections.reverse(fields);
    }
    List<DataIntField> ints = new ArrayList<>();
    List<DataObjField> objs = new ArrayList<>();
    for (DataField f : fields) {
      boolean required =
          source.getName().equals("Profile")
              ? f.getLocalName().equals("Description")
              : f.getLocalName().matches("(Bid|Ask)\\.(Price|Size)");
      if (mode.equals("minimal") && !required || mode.equals("missing-required") && required) {
        continue;
      }
      SerialFieldType type = f.getSerialType();
      if (mode.equals("numeric-int") && type.getId() == 120) {
        type = SerialFieldType.COMPACT_INT;
      }
      if (f instanceof DataObjField) {
        objs.add(type.createDefaultObjInstance(objs.size(), f.getName()));
      } else {
        ints.add(type.createDefaultIntInstance(ints.size(), f.getName()));
        if (type.isLong()) {
          ints.add(new VoidIntField(ints.size(), f.getName() + "$VoidTail"));
        }
      }
    }
    if (mode.equals("reorder")) {
      ints.add(
          SerialFieldType.COMPACT_INT.createDefaultIntInstance(
              ints.size(), source.getName() + ".Extra"));
    }
    return new DefaultRecord(
        id,
        source.getName(),
        false,
        ints.toArray(new DataIntField[0]),
        objs.toArray(new DataObjField[0]));
  }

  private static DataScheme variant(DataScheme source, String mode) {
    return new DefaultScheme(
        PentaCodec.INSTANCE,
        cloneRecord(source.findRecordByName("Profile"), 0, mode),
        cloneRecord(source.findRecordByName("Quote"), 1, mode));
  }

  private static byte[] bytes(ByteArrayOutput out) {
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  private static void frame(ByteArrayOutput tape, int type, ByteArrayOutput payload)
      throws Exception {
    tape.writeCompactInt(payload.getPosition() + 1);
    tape.writeCompactInt(type);
    tape.write(bytes(payload));
  }

  private static void omissions(Path root, PrintWriter out, DataScheme target) throws Exception {
    for (String name : new String[] {"Profile", "Quote"}) {
      byte[] original = Files.readAllBytes(root.resolve(name + "-stream.qtp"));
      ByteArrayInput in = new ByteArrayInput(original);
      int length = in.readCompactInt();
      ByteArrayOutput tape = new ByteArrayOutput();
      tape.write(original, 0, in.getPosition() + length);
      ByteArrayOutput desc = new ByteArrayOutput();
      desc.writeCompactInt(77);
      desc.writeUTFString(name);
      desc.writeCompactInt(0);
      frame(tape, 2, desc);
      ByteArrayOutput data = new ByteArrayOutput();
      com.devexperts.qd.SymbolCodec.Writer symbol = target.getCodec().createWriter();
      symbol.reset(ProtocolOption.EMPTY_SET);
      symbol.writeSymbol(data, target.getCodec().encode("IBM"), null, 0);
      data.writeCompactInt(77);
      frame(tape, 15, data);
      tape.writeByte(0);
      Path path = root.resolve(name + "-wire-omitted.qtp");
      Files.write(path, bytes(tape));
      replay(path, out, target, name + "-wire-omitted", false, 1);
    }
  }

  private static void sequenceBoundaries(PrintWriter out, DataScheme scheme) {
    DataRecord record = scheme.findRecordByName("Quote");
    DataField seq = record.findFieldByName("Sequence");
    DataField nano = record.findFieldByName("TimeNanoPart");
    if (seq == null || nano == null) {
      return;
    }
    int index = 0;
    for (int value : new int[] {Integer.MIN_VALUE, -1, 0, 1, 4194303, 4194304, Integer.MAX_VALUE}) {
      RecordBuffer buffer = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
      RecordCursor c = put(delegate(record), input("Quote", 0), buffer);
      c.setInt(seq.getIndex(), value);
      c.setInt(nano.getIndex(), value);
      cursor(out, "direct-sequence-boundary", index++, c);
    }
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 3, "output, scenario and inputs required");
    Path root = Paths.get(args[0]);
    Files.createDirectory(root);
    String scenario = args[1];
    Path inputs = Paths.get(args[2]);
    if (scenario.equals("set-before")) {
      System.setProperty("dxscheme.nanoTime", "true");
    }
    DXEndpoint endpoint = null;
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(root.resolve("observations.tsv"), StandardCharsets.UTF_8))) {
      row(out, "case", scenario);
      for (String key : new TreeSet<>(System.getProperties().stringPropertyNames())) {
        if (key.startsWith("dxscheme.")) {
          row(out, "startup", key, System.getProperty(key));
        }
      }
      DataScheme scheme;
      if (scenario.startsWith("xml")) {
        DXScheme.Loader loader = DXScheme.newLoader().fromSpecification("resource:dxlink.xml");
        if (scenario.equals("xml-extension")) {
          loader.fromURL(inputs.resolve("extension.xml").toUri().toURL());
        }
        if (scenario.equals("xml-unsupported")) {
          loader.fromURL(inputs.resolve("unsupported-extension.xml").toUri().toURL());
        }
        if (scenario.equals("xml-properties")) {
          SchemeLoadingOptions options = new SchemeLoadingOptions();
          options.setDXFeedDefaults();
          loader.withOptions(options);
          loader.withProperty("dxscheme.nanoTime", "false");
          loader.withProperty("dxscheme.enabled.Sequence", "Quote");
        }
        scheme = loader.load();
      } else if (scenario.startsWith("endpoint")) {
        DXEndpoint.Builder builder =
            DXEndpoint.newBuilder()
                .withRole(DXEndpoint.Role.FEED)
                .withProperty(
                    "dxfeed.properties", inputs.resolve("conflict.properties").toString());
        if (scenario.equals("endpoint-api")) {
          builder
              .withProperty("dxscheme.nanoTime", "true")
              .withProperty("dxscheme.enabled.Sequence", "Profile");
        }
        endpoint = builder.build();
        scheme = ((DXEndpointImpl) endpoint).getQDEndpoint().getScheme();
      } else {
        scheme = DXFeedScheme.getInstance();
      }
      schema(out, "initial", scheme);
      if (scenario.equals("mutation")) {
        System.setProperty("dxscheme.nanoTime", "true");
        schema(out, "singleton-after", DXFeedScheme.getInstance());
        endpoint =
            DXEndpoint.newBuilder()
                .withRole(DXEndpoint.Role.FEED)
                .withProperty("dxfeed.properties", inputs.resolve("conflict.properties").toString())
                .withProperty("dxscheme.nanoTime", "true")
                .withProperty("dxscheme.enabled.Sequence", "Quote")
                .build();
        schema(out, "endpoint-after", ((DXEndpointImpl) endpoint).getQDEndpoint().getScheme());
      }
      if (scenario.equals("reorder")
          || scenario.equals("minimal")
          || scenario.equals("numeric-int")) {
        scheme = variant(scheme, scenario);
        schema(out, "variant", scheme);
      }
      if (scenario.equals("rejected")) {
        DefaultRecord aliasRecord =
            (DefaultRecord) cloneRecord(scheme.findRecordByName("Quote"), 0, "reorder");
        aliasRecord.addFieldAlias("Bid.Price", "ChosenPrice");
        row(
            out,
            "lookup",
            "alias",
            "Quote",
            "ChosenPrice",
            units(aliasRecord.findFieldByName("ChosenPrice").getLocalName()));
        try {
          aliasRecord.addFieldAlias("Bid.Time", "BidPrice");
          row(out, "reject", "alias-conflict", "accepted", "-");
        } catch (IllegalArgumentException e) {
          row(out, "reject", "alias-conflict", e.getClass().getSimpleName(), units(e.getMessage()));
        }
      }
      if (scenario.equals("xml-unsupported")) {
        row(
            out,
            "unsupported",
            "Profile.ExtraText",
            scheme
                .findRecordByName("Profile")
                .findFieldByName("ExtraText")
                .getSerialType()
                .getId());
      } else if (scenario.equals("rejected")) {
        for (String name : new String[] {"Quote", "Profile"}) {
          try {
            delegate(cloneRecord(scheme.findRecordByName(name), 0, "missing-required"));
            row(out, "reject", name, "accepted", "-");
          } catch (IllegalArgumentException e) {
            row(out, "reject", name, e.getClass().getSimpleName(), units(e.getMessage()));
          }
        }
      } else {
        for (String name : new String[] {"Quote", "Profile"}) {
          roundTrip(root, out, scheme, name, false);
          roundTrip(root, out, scheme, name, true);
        }
      }
      if (!scenario.equals("rejected") && !scenario.equals("xml-unsupported")) {
        omissions(root, out, scheme);
        sequenceBoundaries(out, scheme);
      }
      row(out, "complete", scenario);
      require(!out.checkError(), "output failure");
    } finally {
      if (endpoint != null) {
        endpoint.close();
      }
    }
    System.out.println("CAPTURE COMPLETE: " + scenario);
  }
}
