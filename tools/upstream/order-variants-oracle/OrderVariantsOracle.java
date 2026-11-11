// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Oracle caller: Invoke-Oracle.ps1. Inputs: fresh output TSV and explicit scheme profile.
// Outputs: exact TSV and bounded QTP frames through production Java classes; errors throw.
package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.*;
import com.devexperts.qd.kit.*;
import com.devexperts.qd.ng.*;
import com.devexperts.qd.qtp.*;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.market.*;
import java.io.*;
import java.lang.reflect.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

public final class OrderVariantsOracle {
  private static final long[] SAMPLES = {
    Long.MIN_VALUE,
    Long.MAX_VALUE,
    Integer.MIN_VALUE,
    Integer.MAX_VALUE,
    -1,
    0,
    1,
    0x100000001L,
    -385,
    -384,
    -383,
    383,
    384,
    385,
    0x7fffffffffffff80L,
    0x8000000000000080L,
    0x7fffffffffffff00L,
    Long.MIN_VALUE
  };
  private static final String[] TEXT = {
    null,
    "",
    "SPREAD",
    "A" + (char) 0 + "B",
    "\u00e9\ud83d\ude42",
    "\ud800",
    "\udfff",
    "\ud800\udc00"
  };

  private static void row(PrintWriter out, Object... values) {
    for (int i = 0; i < values.length; i++) {
      if (i != 0) out.print('\t');
      out.print(values[i]);
    }
    out.print('\n');
  }

  private static String units(String s) {
    if (s == null) return "null";
    if (s.isEmpty()) return "-";
    StringBuilder b = new StringBuilder();
    for (char c : s.toCharArray()) b.append(String.format("%04x", (int) c));
    return b.toString();
  }

  private static boolean required(String name) {
    return Arrays.asList("Index", "Time", "Sequence", "Price", "Size", "Flags", "SpreadSymbol")
        .contains(name);
  }

  private static boolean numeric(String name) {
    return Arrays.asList(
            "Price",
            "Size",
            "ExecutedSize",
            "Count",
            "TradePrice",
            "TradeSize",
            "IcebergPeakSize",
            "IcebergHiddenSize",
            "IcebergExecutedSize")
        .contains(name);
  }

  private static boolean raw(String name) {
    return Arrays.asList("Time", "Sequence", "TimeNanoPart", "Flags", "MMID", "IcebergFlags")
        .contains(name);
  }

  private static SerialFieldType type(int id) {
    for (SerialFieldType t :
        new SerialFieldType[] {
          SerialFieldType.UTF_CHAR,
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
        }) if (t.getId() == id) return t;
    throw new IllegalArgumentException("Unsupported private field type");
  }

  private static List<DataIntField> fields(DataRecord r) {
    List<DataIntField> result = new ArrayList<>();
    for (int i = 0; i < r.getIntFieldCount(); i++)
      if (r.getIntField(i).getSerialType().getId() != 0) result.add(r.getIntField(i));
    return result;
  }

  private static DataRecord variant(
      DataRecord original, int id, String family, int selected, String name) {
    Map<String, SerialFieldType> kinds = new LinkedHashMap<>();
    for (DataIntField f : fields(original)) kinds.put(f.getLocalName(), f.getSerialType());
    if (!family.equals("default") && !family.startsWith("scheme-")) {
      for (String field :
          new String[] {
            "TimeNanoPart",
            "ActionTime",
            "OrderId",
            "AuxOrderId",
            "ExecutedSize",
            "Count",
            "TradeId",
            "TradePrice",
            "TradeSize"
          })
        if (!kinds.containsKey(field))
          kinds.put(
              field,
              field.endsWith("Id")
                  ? SerialFieldType.LONG
                  : field.equals("ActionTime")
                      ? SerialFieldType.TIME_MILLIS
                      : field.equals("TimeNanoPart")
                          ? SerialFieldType.COMPACT_INT
                          : SerialFieldType.WIDE_DECIMAL);
      if (name.startsWith("AnalyticOrder")) {
        kinds.put("MMID", SerialFieldType.SHORT_STRING);
        kinds.put("IcebergPeakSize", SerialFieldType.WIDE_DECIMAL);
        kinds.put("IcebergHiddenSize", SerialFieldType.WIDE_DECIMAL);
        kinds.put("IcebergExecutedSize", SerialFieldType.WIDE_DECIMAL);
        kinds.put("IcebergFlags", SerialFieldType.COMPACT_INT);
      }
    }
    List<DataIntField> ints = new ArrayList<>();
    ints.add(new VoidIntField(0, name + ".Void"));
    for (Map.Entry<String, SerialFieldType> e : kinds.entrySet()) {
      String field = e.getKey();
      if (family.equals("missing") && !required(field)) continue;
      if (family.equals("required-" + field)) continue;
      SerialFieldType t = e.getValue();
      if (family.equals("numeric") && numeric(field) || family.equals("raw") && raw(field))
        t = type(selected);
      ints.add(t.createDefaultIntInstance(ints.size(), name + "." + field));
      if (t.isLong()) ints.add(new VoidIntField(ints.size(), name + "." + field + "$VoidTail"));
    }
    List<DataObjField> objs = new ArrayList<>();
    if (name.startsWith("SpreadOrder") && !family.equals("required-SpreadSymbol"))
      objs.add(SerialFieldType.UTF_CHAR_ARRAY.createDefaultObjInstance(0, name + ".SpreadSymbol"));
    return new DefaultRecord(
        id, name, true, ints.toArray(new DataIntField[0]), objs.toArray(new DataObjField[0]));
  }

  private static EventDelegate<?> delegate(DataRecord r) {
    for (EventDelegate<?> d : new MarketFactoryImpl().createDelegates(r))
      if ((d instanceof SpreadOrderDelegate || d instanceof AnalyticOrderDelegate)
          && d.getContract().toString().equals("History")) return d;
    throw new IllegalArgumentException("No actual HISTORY variant delegate");
  }

  private static Object encode(Object value) throws Exception {
    if (value instanceof String) return units((String) value);
    if (value instanceof Double) return "bits:" + Double.doubleToRawLongBits((Double) value);
    if (value instanceof Character) return (int) (Character) value;
    if (value instanceof OrderSource) return ((OrderSource) value).id();
    if (value instanceof Enum) return value.getClass().getMethod("getCode").invoke(value);
    return value;
  }

  private static void observed(PrintWriter out, String scenario, int sample, RecordCursor cursor)
      throws Exception {
    DataRecord r = cursor.getRecord();
    EventDelegate<?> d = delegate(r);
    OrderBase event = (OrderBase) d.createEvent(cursor);
    Object mapping = d.getMapping();
    row(
        out,
        "symbol",
        scenario,
        r.getId(),
        sample,
        units(cursor.getDecodedSymbol()),
        cursor.getEventTimeSequence());
    for (DataIntField f : fields(r))
      row(
          out,
          "value",
          scenario,
          r.getId(),
          sample,
          f.getLocalName(),
          f.getSerialType().getId(),
          f.getSerialType().isLong() ? cursor.getLong(f.getIndex()) : cursor.getInt(f.getIndex()));
    for (int i = 0; i < r.getObjFieldCount(); i++)
      row(
          out,
          "object",
          scenario,
          r.getId(),
          sample,
          r.getObjField(i).getLocalName(),
          units((String) cursor.getObj(i)));
    for (String getter :
        new String[] {
          "getEventSymbol",
          "getEventTime",
          "getIndex",
          "getSource",
          "getEventFlags",
          "getTimeSequence",
          "getTime",
          "getTimeNanos",
          "getSequence",
          "getTimeNanoPart",
          "getActionTime",
          "getOrderId",
          "getAuxOrderId",
          "getCount",
          "getTradeId",
          "getExchangeCode",
          "getOrderSide",
          "getScope",
          "getAction",
          "getSize",
          "hasSize",
          "getPrice",
          "getSizeAsDouble",
          "getExecutedSize",
          "getTradePrice",
          "getTradeSize"
        })
      row(
          out,
          "event",
          scenario,
          r.getId(),
          sample,
          getter,
          encode(event.getClass().getMethod(getter).invoke(event)));
    Method flags = OrderBase.class.getDeclaredMethod("getFlags");
    flags.setAccessible(true);
    row(out, "event", scenario, r.getId(), sample, "rawFlags", flags.invoke(event));
    for (String field : new String[] {"Price", "Size", "ExecutedSize", "TradePrice", "TradeSize"})
      row(
          out,
          "wide",
          scenario,
          r.getId(),
          sample,
          field,
          mapping
              .getClass()
              .getMethod("get" + field + "WideDecimal", RecordCursor.class)
              .invoke(mapping, cursor));
    if (event instanceof SpreadOrder)
      row(
          out,
          "event",
          scenario,
          r.getId(),
          sample,
          "getSpreadSymbol",
          units(((SpreadOrder) event).getSpreadSymbol()));
    if (event instanceof AnalyticOrder) {
      AnalyticOrder a = (AnalyticOrder) event;
      row(out, "event", scenario, r.getId(), sample, "getMarketMaker", units(a.getMarketMaker()));
      for (String field :
          new String[] {"IcebergPeakSize", "IcebergHiddenSize", "IcebergExecutedSize"}) {
        row(
            out,
            "event",
            scenario,
            r.getId(),
            sample,
            "get" + field,
            encode(a.getClass().getMethod("get" + field).invoke(a)));
        row(
            out,
            "wide",
            scenario,
            r.getId(),
            sample,
            field,
            mapping
                .getClass()
                .getMethod("get" + field + "WideDecimal", RecordCursor.class)
                .invoke(mapping, cursor));
      }
      Method f = AnalyticOrder.class.getDeclaredMethod("getIcebergFlags");
      f.setAccessible(true);
      row(out, "event", scenario, r.getId(), sample, "rawIcebergFlags", f.invoke(a));
      row(
          out,
          "event",
          scenario,
          r.getId(),
          sample,
          "getIcebergType",
          a.getIcebergType().getCode());
    }
    row(out, "collectorTime", scenario, r.getId(), sample, cursor.getTime());
  }

  private static void capture(
      PrintWriter out, Path root, DataScheme scheme, String scenario, String family, int selected)
      throws Exception {
    for (int id = 0; id < scheme.getRecordCount(); id++) {
      DataRecord r = scheme.getRecord(id);
      row(out, "record", scenario, id, r.getName());
      for (int i = 0; i < r.getIntFieldCount(); i++) {
        DataIntField f = r.getIntField(i);
        row(out, "physical", scenario, id, i, f.getSerialType().getId(), f.getLocalName());
      }
      for (int i = 0; i < r.getObjFieldCount(); i++) {
        DataObjField f = r.getObjField(i);
        row(out, "objfield", scenario, id, i, f.getSerialType().getId(), f.getLocalName());
      }
      try {
        delegate(r);
      } catch (IllegalArgumentException e) {
        row(out, "reject", scenario, id, e.getClass().getSimpleName());
        continue;
      }
      RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
      long[] samples = selected == 2 ? new long[] {0, 65, 0xd800, 0xffff, 0x10ffff} : SAMPLES;
      int count = family.equals("flags") ? 32 : samples.length;
      for (int sample = 0; sample < count; sample++) {
        RecordCursor c = input.add(r, 0, "AAPL");
        c.setEventTimeSequence(((long) (sample - 3) << 32) | 0x7fffffffL);
        for (DataIntField f : fields(r)) {
          String name = f.getLocalName();
          long v = numeric(name) ? 0x180 : 0;
          if (family.equals("numeric") && numeric(name) || family.equals("raw") && raw(name))
            v = samples[sample];
          else if (name.equals("Index"))
            v = new int[] {0, -1, Integer.MIN_VALUE, Integer.MAX_VALUE}[sample % 4];
          else if (name.equals("Time")) v = sample - 3;
          else if (name.equals("Sequence")) v = sample % 2 == 0 ? -1 : Integer.MAX_VALUE;
          else if (name.equals("Flags"))
            v = family.equals("flags") ? 0x87650000L | sample * 0x421 : 0x87654321L;
          else if (name.equals("IcebergFlags"))
            v = family.equals("flags") ? 0x87650000L | sample : 0x87654323L;
          else if (name.equals("MMID")) v = sample % 3 == 0 ? -1 : sample % 3 == 1 ? 0 : 0x41420043;
          else if (f.getSerialType().isLong()) v = samples[sample % samples.length];
          if (f.getSerialType().isLong()) c.setLong(f.getIndex(), v);
          else c.setInt(f.getIndex(), (int) v);
        }
        for (int i = 0; i < r.getObjFieldCount(); i++) c.setObj(i, TEXT[sample % TEXT.length]);
        c.setEventFlags(family.equals("remove") ? 2 : family.equals("flags") ? sample : sample % 2);
        if (family.equals("remove"))
          c.setTime(new long[] {0, -1, Long.MIN_VALUE, 0x100000005L, Long.MAX_VALUE}[sample % 5]);
      }
      BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
      ByteArrayOutput output = new ByteArrayOutput();
      composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
      composer.setOutput(output);
      composer.visitData(input, MessageType.HISTORY_DATA);
      byte[] wire = Arrays.copyOf(output.getBuffer(), output.getPosition());
      Files.write(root.resolve(scenario + "-" + id + ".qtp"), wire);
      final int[] seen = {0, 0};
      BinaryQTPParser parser = new BinaryQTPParser(scheme);
      parser.setInput(new ByteArrayInput(wire));
      parser.parse(
          new MessageConsumerAdapter() {
            protected void processData(DataIterator iterator, MessageType kind) {
              RecordCursor c;
              while ((c = ((RecordSource) iterator).next()) != null) {
                try {
                  observed(out, scenario, seen[0]++, c);
                } catch (Exception e) {
                  throw new IllegalStateException(e);
                }
              }
            }

            public void handleCorruptedStream() {
              seen[1]++;
            }

            public void handleCorruptedMessage(int t) {
              seen[1]++;
            }

            public void handleUnknownMessage(int t) {
              seen[1]++;
            }
          });
      if (seen[0] != count || seen[1] != 0)
        throw new IllegalStateException("Incomplete production parser/delegate observations");
      row(out, "parsed", scenario, id, seen[0], seen[1]);
    }
  }

  @SuppressWarnings({"rawtypes", "unchecked"})
  private static void capabilities(PrintWriter out) throws Exception {
    List<Field> fields = new ArrayList<>();
    for (Field f : OrderSource.class.getFields())
      if (f.getType() == OrderSource.class) fields.add(f);
    Collections.sort(fields, Comparator.comparing(Field::getName));
    for (Field f : fields) {
      OrderSource s = (OrderSource) f.get(null);
      for (Class c : new Class[] {Order.class, SpreadOrder.class, AnalyticOrder.class})
        row(
            out,
            "capability",
            f.getName(),
            s.id(),
            c.getSimpleName(),
            s.isPublishable(c),
            s.isFullOrderBook());
    }
    Method set = AnalyticOrder.class.getDeclaredMethod("setIcebergFlags", int.class);
    set.setAccessible(true);
    Method get = AnalyticOrder.class.getDeclaredMethod("getIcebergFlags");
    get.setAccessible(true);
    for (int raw : new int[] {0, 1, 2, 3, -1, Integer.MIN_VALUE, Integer.MAX_VALUE, 0x12345678})
      for (IcebergType t : IcebergType.values()) {
        AnalyticOrder e = new AnalyticOrder();
        set.invoke(e, raw);
        e.setIcebergType(t);
        row(out, "icebergSetter", raw, t.getCode(), get.invoke(e), e.getIcebergType().getCode());
      }
  }

  public static void main(String[] args) throws Exception {
    // Named-record source parsing is independent of direct publication capabilities.
    if (args.length != 2 || !Arrays.asList("default", "tiny", "int", "fob").contains(args[1]))
      throw new IllegalArgumentException("Output and profile required");
    Path root = Paths.get(args[0] + ".wire");
    Files.createDirectory(root);
    DataScheme standard = DXFeedScheme.getInstance();
    PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8));
    try {
      if (args[1].equals("default")) capabilities(out);
      String[] families =
          args[1].equals("default")
              ? new String[] {
                "default",
                "full",
                "numeric",
                "raw",
                "missing",
                "required-Index",
                "required-Time",
                "required-Sequence",
                "required-Price",
                "required-Size",
                "required-Flags",
                "required-SpreadSymbol",
                "flags",
                "remove"
              }
              : new String[] {"scheme-" + args[1]};
      for (String kind : new String[] {"SpreadOrder", "AnalyticOrder"})
        for (String family : families) {
          if (kind.equals("AnalyticOrder") && family.equals("required-SpreadSymbol")) continue;
          int[] types =
              family.equals("numeric")
                  ? new int[] {2, 4, 8, 24, 104, 120}
                  : family.equals("raw")
                      ? new int[] {2, 4, 8, 24, 40, 56, 72, 88, 104, 120, 152}
                      : new int[] {120};
          for (int selected : types) {
            DataRecord base = standard.findRecordByName(kind);
            if (base == null) throw new IllegalStateException("Standard variant absent: " + kind);
            DataRecord[] records = new DataRecord[4];
            String[] suffix = {"", "#ISE", "#NTV", "#Z9x"};
            for (int id = 0; id < records.length; id++) {
              String name = kind + suffix[id];
              row(out, "standardAvailable", args[1], name, standard.findRecordByName(name) != null);
              records[id] =
                  variant(
                      standard.findRecordByName(name) != null
                          ? standard.findRecordByName(name)
                          : base,
                      id,
                      family,
                      selected,
                      name);
            }
            capture(
                out,
                root,
                new DefaultScheme(PentaCodec.INSTANCE, records),
                kind + "-" + family + selected,
                family,
                selected);
          }
        }
      out.flush();
      if (out.checkError()) throw new IOException("Cannot flush variant observations");
    } finally {
      out.close();
    }
    if (out.checkError()) throw new IOException("Cannot close variant observations");
  }
}
