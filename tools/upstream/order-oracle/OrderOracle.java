// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
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
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolOption;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.Order;
import com.dxfeed.event.market.OrderBase;
import com.dxfeed.event.market.OrderDelegate;
import com.dxfeed.event.market.impl.OrderMapping;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Captures original physical slots and real binary parser/delegate Order observations offline. */
public final class OrderOracle {
  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) out.print('\t');
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static String units(String value) {
    if (value == null) return "null";
    if (value.isEmpty()) return "-";
    StringBuilder out = new StringBuilder();
    for (char unit : value.toCharArray()) out.append(String.format("%04x", (int) unit));
    return out.toString();
  }

  private static boolean numeric(String name) {
    return Arrays.asList("Price", "Size", "ExecutedSize", "Count", "TradePrice", "TradeSize")
        .contains(name);
  }

  private static boolean raw(String name) {
    return Arrays.asList("Time", "Sequence", "TimeNanoPart", "Flags", "MMID").contains(name);
  }

  private static boolean required(String name) {
    return Arrays.asList("Index", "Time", "Sequence", "Price", "Size", "Flags").contains(name);
  }

  private static SerialFieldType type(int id) {
    for (SerialFieldType value :
        new SerialFieldType[] {
          SerialFieldType.UTF_CHAR, SerialFieldType.INT, SerialFieldType.COMPACT_INT,
          SerialFieldType.DECIMAL, SerialFieldType.SHORT_STRING, SerialFieldType.TIME_SECONDS,
          SerialFieldType.SEQUENCE, SerialFieldType.DATE, SerialFieldType.LONG,
          SerialFieldType.WIDE_DECIMAL, SerialFieldType.TIME_MILLIS, SerialFieldType.UTF_CHAR_ARRAY
        }) if (value.getId() == id) return value;
    throw new IllegalArgumentException("Unsupported fixture type");
  }

  private static List<DataIntField> fields(DataRecord record) {
    List<DataIntField> result = new ArrayList<>();
    for (int i = 0; i < record.getIntFieldCount(); ++i) {
      DataIntField field = record.getIntField(i);
      if (field.getSerialType().getId() != 0) result.add(field);
    }
    return result;
  }

  private static DataRecord variant(DataRecord original, int id, String family, int selected) {
    List<DataIntField> result = new ArrayList<>();
    result.add(new VoidIntField(0, original.getName() + ".Void"));
    for (DataIntField field : fields(original)) {
      String name = field.getLocalName();
      if (family.equals("missing") && !required(name)) continue;
      if (family.startsWith("required-") && name.equals(family.substring(9))) continue;
      SerialFieldType kind = field.getSerialType();
      if (family.equals("numeric") && numeric(name) || family.equals("raw") && raw(name)) {
        kind = type(selected);
      }
      result.add(kind.createDefaultIntInstance(result.size(), original.getName() + "." + name));
      if (kind.isLong()) {
        result.add(new VoidIntField(result.size(), original.getName() + "." + name + "$VoidTail"));
      }
    }
    return new DefaultRecord(
        id, original.getName(), true, result.toArray(new DataIntField[0]), new DataObjField[0]);
  }

  private static OrderDelegate delegate(DataRecord record) {
    for (EventDelegate<?> value : new MarketFactoryImpl().createDelegates(record)) {
      if (value instanceof OrderDelegate && value.getContract().toString().equals("History")) {
        return (OrderDelegate) value;
      }
    }
    throw new IllegalArgumentException("No production HISTORY Order delegate");
  }

  private static final long[] SAMPLES = {
    Long.MIN_VALUE,
    Long.MAX_VALUE,
    Integer.MIN_VALUE,
    Integer.MAX_VALUE,
    -1,
    0,
    1,
    0x0000000100000001L,
    -385,
    -384,
    -383,
    383,
    384,
    385,
    0x7fffffffffffff80L,
    0x8000000000000080L,
    0x7fffffffffffff00L,
    0x8000000000000000L
  };

  private static void observed(PrintWriter out, String scenario, int sample, RecordCursor cursor)
      throws Exception {
    DataRecord record = cursor.getRecord();
    OrderDelegate delegate = delegate(record);
    OrderMapping mapping = delegate.getMapping();
    Order event = delegate.createEvent(cursor);
    Method flags = OrderBase.class.getDeclaredMethod("getFlags");
    flags.setAccessible(true);
    row(
        out,
        "symbol",
        scenario,
        record.getId(),
        sample,
        units(cursor.getDecodedSymbol()),
        cursor.getEventTimeSequence());
    List<DataIntField> logical = fields(record);
    for (int i = 0; i < logical.size(); ++i) {
      DataIntField field = logical.get(i);
      row(
          out,
          "value",
          scenario,
          record.getId(),
          sample,
          i,
          field.getSerialType().isLong()
              ? cursor.getLong(field.getIndex())
              : cursor.getInt(field.getIndex()),
          "null");
    }
    row(
        out,
        "order",
        scenario,
        record.getId(),
        sample,
        units(event.getEventSymbol()),
        event.getEventTime(),
        event.getIndex(),
        event.getSource().id(),
        event.getEventFlags(),
        event.getTimeSequence(),
        event.getTime(),
        event.getTimeNanos(),
        event.getSequence(),
        event.getTimeNanoPart(),
        event.getActionTime(),
        event.getOrderId(),
        event.getAuxOrderId(),
        event.getCount(),
        flags.invoke(event),
        event.getTradeId(),
        units(event.getMarketMaker()),
        (int) event.getExchangeCode(),
        event.getOrderSide().getCode(),
        event.getScope().getCode(),
        event.getAction().getCode(),
        event.getSize(),
        event.hasSize(),
        mapping.getPriceWideDecimal(cursor),
        mapping.getSizeWideDecimal(cursor),
        mapping.getExecutedSizeWideDecimal(cursor),
        mapping.getTradePriceWideDecimal(cursor),
        mapping.getTradeSizeWideDecimal(cursor),
        Double.doubleToRawLongBits(event.getPrice()),
        Double.doubleToRawLongBits(event.getSizeAsDouble()),
        Double.doubleToRawLongBits(event.getExecutedSize()),
        Double.doubleToRawLongBits(event.getTradePrice()),
        Double.doubleToRawLongBits(event.getTradeSize()),
        cursor.getTime());
  }

  private static void capture(
      PrintWriter out, Path root, DataScheme scheme, String scenario, String family, int selected)
      throws Exception {
    for (int id = 0; id < scheme.getRecordCount(); ++id) {
      DataRecord record = scheme.getRecord(id);
      row(out, "record", scenario, id, record.getName());
      List<DataIntField> logical = fields(record);
      for (int i = 0; i < record.getIntFieldCount(); ++i) {
        DataIntField field = record.getIntField(i);
        row(out, "physical", scenario, id, i, field.getSerialType().getId(), field.getLocalName());
      }
      for (int i = 0; i < logical.size(); ++i) {
        DataField field = logical.get(i);
        row(
            out,
            "field",
            scenario,
            id,
            i,
            field.getSerialType().getId(),
            field.getLocalName(),
            field.getPropertyName());
      }
      try {
        delegate(record);
      } catch (IllegalArgumentException failure) {
        row(out, "reject", scenario, id, failure.getClass().getSimpleName());
        continue;
      }
      RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
      long[] samples = selected == 2 ? new long[] {0, 65, 0xd800, 0xffff, 0x10ffff} : SAMPLES;
      int count = family.equals("flags") ? 32 : samples.length;
      for (int sample = 0; sample < count; ++sample) {
        RecordCursor cursor = input.add(record, 0, "AAPL");
        cursor.setEventTimeSequence(((long) (sample - 3) << 32) | 0x7fffffffL);
        for (DataIntField field : logical) {
          String name = field.getLocalName();
          long value = numeric(name) ? 0x180 : 0;
          if (family.equals("numeric") && numeric(name) || family.equals("raw") && raw(name))
            value = samples[sample];
          else if (name.equals("Index"))
            value = new int[] {0, -1, Integer.MIN_VALUE, Integer.MAX_VALUE}[sample % 4];
          else if (name.equals("Time")) value = sample - 3;
          else if (name.equals("Sequence")) value = sample % 2 == 0 ? -1 : 0x7fffffff;
          else if (name.equals("Flags"))
            value = family.equals("flags") ? (0x87650000L | sample * 0x421) : 0x87654321L;
          else if (name.equals("MMID"))
            value = sample % 3 == 0 ? -1 : sample % 3 == 1 ? 0 : 0x41420043;
          else if (field.getSerialType().isLong()) value = samples[sample % samples.length];
          if (field.getSerialType().isLong()) cursor.setLong(field.getIndex(), value);
          else cursor.setInt(field.getIndex(), (int) value);
        }
        cursor.setEventFlags(
            family.equals("remove") ? 2 : family.equals("flags") ? sample : sample % 2);
        if (family.equals("remove"))
          cursor.setTime(
              new long[] {0, -1, Long.MIN_VALUE, (1L << 32) | 5, Long.MAX_VALUE}[sample % 5]);
      }
      BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
      ByteArrayOutput output = new ByteArrayOutput();
      composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
      composer.setOutput(output);
      composer.visitData(input, MessageType.HISTORY_DATA);
      byte[] wire = Arrays.copyOf(output.getBuffer(), output.getPosition());
      Files.write(root.resolve(scenario + "-" + id + ".qtp"), wire);
      final int expectedCount = count;
      final int[] seen = new int[2];
      BinaryQTPParser parser = new BinaryQTPParser(scheme);
      parser.setInput(new ByteArrayInput(wire));
      parser.parse(
          new MessageConsumerAdapter() {
            protected void processData(DataIterator iterator, MessageType messageType) {
              RecordCursor cursor;
              while ((cursor = ((RecordSource) iterator).next()) != null) {
                if (family.equals("flags") && cursor.getEventFlags() != seen[0])
                  throw new IllegalStateException(
                      "HISTORY hs flags were masked in binary round trip");
                try {
                  observed(out, scenario, seen[0]++, cursor);
                } catch (Exception failure) {
                  throw new IllegalStateException(failure);
                }
              }
            }

            public void handleCorruptedStream() {
              ++seen[1];
            }

            public void handleCorruptedMessage(int type) {
              ++seen[1];
            }

            public void handleUnknownMessage(int type) {
              ++seen[1];
            }
          });
      if (seen[0] != expectedCount || seen[1] != 0)
        throw new IllegalStateException("Parser observation incomplete");
      row(out, "parsed", scenario, id, seen[0], seen[1]);
    }
  }

  /** Inputs: one fresh output TSV and explicit default/tiny/int/fob child scheme profile. */
  public static void main(String[] args) throws Exception {
    if (args.length != 2 || !Arrays.asList("default", "tiny", "int", "fob").contains(args[1])) {
      throw new IllegalArgumentException("Expected output TSV and scheme profile");
    }
    Path root = Paths.get(args[0] + ".wire");
    Files.createDirectory(root);
    DataScheme standard = DXFeedScheme.getInstance();
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8))) {
      String[] families =
          args[1].equals("default")
              ? new String[] {
                "default",
                "numeric",
                "raw",
                "missing",
                "required-Index",
                "required-Time",
                "required-Sequence",
                "required-Price",
                "required-Size",
                "required-Flags",
                "flags",
                "remove"
              }
              : new String[] {"scheme-" + args[1]};
      for (String family : families) {
        int[] types =
            family.equals("numeric")
                ? new int[] {2, 4, 8, 24, 104, 120}
                : family.equals("raw")
                    ? new int[] {2, 4, 8, 24, 40, 56, 72, 88, 104, 120, 152}
                    : new int[] {120};
        for (int selected : types) {
          DataRecord[] records = new DataRecord[3];
          for (int id = 0; id < records.length; ++id) {
            String name = new String[] {"Order", "Order#NTV", "Order#BZX"}[id];
            DataRecord original = standard.findRecordByName(name);
            if (original == null) throw new IllegalStateException("Standard Order source missing");
            records[id] = variant(original, id, family, selected);
          }
          capture(
              out,
              root,
              new DefaultScheme(PentaCodec.INSTANCE, records),
              family + selected,
              family,
              selected);
        }
      }
      if (out.checkError()) throw new IOException("Cannot write Order observations");
    }
  }
}
