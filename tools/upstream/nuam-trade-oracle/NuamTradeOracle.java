// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
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
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.custom.CustomFactoryImpl;
import com.dxfeed.event.custom.NuamTrade;
import com.dxfeed.event.custom.NuamTradeDelegate;
import com.dxfeed.event.custom.impl.NuamTradeMapping;
import com.dxfeed.event.market.TradeBase;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Captures actual production NuamTrade mappings, delegates, physical slots and business getters.
 */
public final class NuamTradeOracle {
  private static final int[] NUMERIC = {2, 4, 8, 24, 104, 120};
  private static final int[] RAW = {2, 4, 8, 24, 40, 56, 72, 88, 104, 120, 152};

  private static boolean numeric(String name) {
    return Arrays.asList(
            "Last.Price",
            "Last.Size",
            "Last.Change",
            "Volume",
            "DayTurnover",
            "LastSignificantPrice",
            "LastPriceForAll",
            "VWAP")
        .contains(name);
  }

  private static boolean raw(String name) {
    return Arrays.asList(
            "Last.Time",
            "Last.Sequence",
            "Last.TimeNanoPart",
            "Last.Exchange",
            "Last.Tick",
            "DayId",
            "NumberOfTrades",
            "Last.Flags")
        .contains(name);
  }

  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) out.print('\t');
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static String units(String text) {
    if (text == null) return "null";
    if (text.isEmpty()) return "-";
    StringBuilder result = new StringBuilder();
    for (char value : text.toCharArray()) result.append(String.format("%04x", (int) value));
    return result.toString();
  }

  private static SerialFieldType type(int id) {
    for (SerialFieldType value :
        new SerialFieldType[] {
          SerialFieldType.BYTE,
          SerialFieldType.UTF_CHAR,
          SerialFieldType.SHORT,
          SerialFieldType.INT,
          SerialFieldType.COMPACT_INT,
          SerialFieldType.DECIMAL,
          SerialFieldType.SHORT_STRING,
          SerialFieldType.TIME_SECONDS,
          SerialFieldType.SEQUENCE,
          SerialFieldType.DATE,
          SerialFieldType.LONG,
          SerialFieldType.WIDE_DECIMAL,
          SerialFieldType.TIME_MILLIS,
          SerialFieldType.UTF_CHAR_ARRAY,
          SerialFieldType.STRING,
          SerialFieldType.BYTE_ARRAY,
          SerialFieldType.CUSTOM_OBJECT,
          SerialFieldType.SERIAL_OBJECT
        }) if (value.getId() == id) return value;
    throw new IllegalArgumentException("Unknown fixture type " + id);
  }

  private static long[] samples(int type) {
    if (type == 1) return new long[] {-128, -1, 0, 1, 127};
    if (type == 2) return new long[] {0, 65, 0xd800, 0xffff, 0x10ffff};
    if (type == 3) return new long[] {-32768, -1, 0, 1, 32767};
    if (type == 24)
      return new long[] {
        Integer.MIN_VALUE, -1, 0, 1, 0x10, 0x20, 0x40, 0x60, 0x70, 0x7f, 128, Integer.MAX_VALUE
      };
    if (type == 104)
      return new long[] {
        Long.MIN_VALUE,
        Long.MAX_VALUE,
        -(1L << 55),
        (1L << 55) - 1,
        -9007199254740992L,
        9007199254740992L,
        -256,
        -1,
        0,
        1,
        128,
        256
      };
    if (type == 120 || type == 152)
      return new long[] {
        Long.MIN_VALUE, Long.MAX_VALUE, 0, 128, 256, -256, 0x123456789abcdef0L, -1
      };
    return new long[] {Integer.MIN_VALUE, Integer.MAX_VALUE, -1, 0, 1, 17, 0x12345678};
  }

  private static List<DataField> fields(DataRecord record) {
    List<DataField> fields = new ArrayList<>();
    for (int i = 0; i < record.getIntFieldCount(); ++i)
      if (record.getIntField(i).getSerialType().getId() != 0) fields.add(record.getIntField(i));
    for (int i = 0; i < record.getObjFieldCount(); ++i) fields.add(record.getObjField(i));
    return fields;
  }

  private static DataRecord variant(DataRecord source, int id, String family, int type) {
    List<DataField> logical = fields(source);
    Collections.reverse(logical);
    List<DataIntField> ints = new ArrayList<>();
    for (DataField field : logical) {
      String local = field.getLocalName();
      if (family.equals("missing") && !local.equals("Last.Price") && !local.equals("Last.Size"))
        continue;
      if (family.equals("required") && local.equals("Last.Price")
          || family.equals("required-size") && local.equals("Last.Size")) continue;
      if (family.startsWith("missing-") && local.equals(family.substring("missing-".length())))
        continue;
      SerialFieldType selected = field.getSerialType();
      if (family.equals("numeric") && numeric(local)
          || family.equals("raw") && raw(local)
          || family.startsWith("id") && local.equals("TradeId")
          || family.equals("bad") && numeric(local)) selected = type(type);
      if (selected.isObject())
        throw new IllegalArgumentException("NuamTrade oracle scalar profiles only");
      ints.add(selected.createDefaultIntInstance(ints.size(), source.getName() + "." + local));
      if (selected.isLong())
        ints.add(new VoidIntField(ints.size(), source.getName() + "." + local + "$tail"));
      if (family.equals("id-one") && local.equals("TradeId"))
        ints.add(
            SerialFieldType.INT.createDefaultIntInstance(
                ints.size(), source.getName() + ".Neighbor"));
    }
    return new DefaultRecord(
        id, source.getName(), false, ints.toArray(new DataIntField[0]), new DataObjField[0]);
  }

  private static EventDelegate<?> delegate(DataRecord record) {
    for (EventDelegate<?> value : new CustomFactoryImpl().createStreamOnlyDelegates(record))
      if (value instanceof NuamTradeDelegate) return value;
    throw new IllegalArgumentException("No production NuamTrade delegate");
  }

  private static void observe(
      PrintWriter out, DataRecord record, String scenario, String family, int type)
      throws Exception {
    row(out, "record", scenario, record.getId(), record.getName());
    List<DataField> logical = fields(record);
    for (int i = 0; i < logical.size(); ++i) {
      DataField field = logical.get(i);
      row(
          out,
          "field",
          scenario,
          record.getId(),
          i,
          field.getSerialType().getId(),
          field.getLocalName(),
          field.getPropertyName());
    }
    EventDelegate<?> delegate;
    try {
      delegate = delegate(record);
    } catch (IllegalArgumentException failure) {
      row(out, "reject", scenario, record.getId(), failure.getClass().getSimpleName());
      return;
    }
    if (family.equals("id-one"))
      row(out, "native-reject", scenario, record.getId(), "INCOMPATIBLE_TYPE");
    NuamTradeMapping mapping = record.getMapping(NuamTradeMapping.class);
    Method flags = TradeBase.class.getDeclaredMethod("getFlags");
    flags.setAccessible(true);
    long[] values = family.equals("flags") ? new long[40] : samples(type);
    int[] ticks = {-1, 0, 1, 2, 3};
    for (int sample = 0; sample < values.length; ++sample) {
      String symbol =
          family.equals("symbols")
              ? new String[] {"AAPL", "AAPL&N", "AAPL{x=y}", "AAPL&N{x=y}", "AAPL{x=y}&Q"}
                  [sample % 5]
              : "AAPL";
      RecordCursor cursor =
          new RecordBuffer(RecordMode.DATA.withEventTimeSequence()).add(record, 0, symbol);
      cursor.setEventTimeSequence(((long) (sample - 3) << 32) | 0x7fffffffL);
      for (DataField field : logical) {
        String name = field.getLocalName();
        long value = 0;
        if (numeric(name)) value = family.equals("numeric") ? values[sample] : 0x180;
        if (raw(name)) {
          if (family.equals("raw")) value = values[sample];
          else if (name.equals("Last.Time"))
            value = new int[] {Integer.MIN_VALUE, -1, 0, 1, Integer.MAX_VALUE}[sample % 5];
          else if (name.equals("Last.Sequence"))
            value = new int[] {-1, 0, 1, 0x7fffffff, 0x003fffff}[sample % 5];
          else if (name.equals("Last.TimeNanoPart"))
            value =
                new int[] {Integer.MIN_VALUE, -1, 999999, 1000000, Integer.MAX_VALUE}[sample % 5];
          else if (name.equals("Last.Exchange")) value = 0x10ffff;
          else if (name.equals("Last.Tick")) value = ticks[(sample / 8) % ticks.length];
          else if (name.equals("Last.Flags")) value = family.equals("flags") ? sample % 32 : sample;
          else if (name.equals("DayId") || name.equals("NumberOfTrades")) value = sample - 3;
        }
        if (name.equals("TradeStatTime"))
          value = new long[] {Long.MIN_VALUE, -1L, 0L, 1L, Long.MAX_VALUE}[sample % 5];
        if (name.equals("TradeId"))
          value = family.startsWith("id") ? values[sample] : 0x123456789abcdef0L;
        if (name.equals("Neighbor")) value = 0x89abcdef;
        if (field.getSerialType().isLong()) cursor.setLong(field.getIndex(), value);
        else cursor.setInt(field.getIndex(), (int) value);
      }
      try {
        NuamTrade event = (NuamTrade) delegate.createEvent(cursor);
        row(
            out,
            "symbol",
            scenario,
            record.getId(),
            sample,
            units(symbol),
            cursor.getEventTimeSequence());
        for (int i = 0; i < logical.size(); ++i) {
          DataField field = logical.get(i);
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
            "trade",
            scenario,
            record.getId(),
            sample,
            units(event.getEventSymbol()),
            event.getEventTime(),
            event.getTimeSequence(),
            event.getTime(),
            event.getTimeNanos(),
            event.getSequence(),
            event.getTimeNanoPart(),
            (int) event.getExchangeCode(),
            event.getTradeId(),
            event.getDayId(),
            flags.invoke(event),
            event.getTickDirection().getCode(),
            event.isExtendedTradingHours() ? 1 : 0,
            mapping.getPriceWideDecimal(cursor),
            mapping.getSizeWideDecimal(cursor),
            mapping.getChangeWideDecimal(cursor),
            mapping.getDayVolumeWideDecimal(cursor),
            mapping.getDayTurnoverWideDecimal(cursor),
            Double.doubleToRawLongBits(event.getPrice()),
            Double.doubleToRawLongBits(event.getSizeAsDouble()),
            Double.doubleToRawLongBits(event.getChange()),
            Double.doubleToRawLongBits(event.getDayVolumeAsDouble()),
            Double.doubleToRawLongBits(event.getDayTurnover()),
            event.getSize(),
            event.getDayVolume(),
            event.getTradeStatTime(),
            event.getNumberOfTrades(),
            mapping.getLastSignificantPriceWideDecimal(cursor),
            mapping.getLastPriceForAllWideDecimal(cursor),
            mapping.getVWAPWideDecimal(cursor),
            Double.doubleToRawLongBits(event.getLastSignificantPrice()),
            Double.doubleToRawLongBits(event.getLastPriceForAll()),
            Double.doubleToRawLongBits(event.getVWAP()));
      } catch (IllegalArgumentException failure) {
        row(
            out,
            "conversion-reject",
            scenario,
            record.getId(),
            sample,
            failure.getClass().getSimpleName());
        break;
      }
    }
  }

  /** Takes output TSV and the fixed all selector; performs no endpoint/network work. */
  public static void main(String[] args) throws Exception {
    if (args.length != 2 || !args[1].equals("all"))
      throw new IllegalArgumentException("Expected output TSV and all");
    DataScheme source = DXFeedScheme.getInstance();
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8))) {
      for (String family :
          new String[] {
            "default",
            "numeric",
            "raw",
            "id",
            "id-one",
            "missing",
            "required",
            "required-size",
            "missing-TradeStatTime",
            "missing-LastSignificantPrice",
            "missing-LastPriceForAll",
            "missing-NumberOfTrades",
            "missing-VWAP",
            "flags",
            "symbols",
            "bad"
          }) {
        int[] types =
            family.equals("numeric")
                ? NUMERIC
                : family.equals("raw")
                    ? RAW
                    : family.equals("id")
                        ? new int[] {104, 120, 152}
                        : family.equals("id-one")
                            ? new int[] {8}
                            : family.equals("bad") ? new int[] {40, 152} : new int[] {120};
        for (int type : types) {
          String scenario = family + type;
          DataRecord[] records = {
            variant(source.findRecordByName("NuamTrade"), 0, family, type),
            variant(source.findRecordByName("NuamTrade&Q"), 1, family, type)
          };
          new DefaultScheme(PentaCodec.INSTANCE, records);
          for (DataRecord record : records) observe(out, record, scenario, family, type);
        }
      }
      if (out.checkError()) throw new IOException("Cannot write NuamTrade fixture");
    }
  }
}
