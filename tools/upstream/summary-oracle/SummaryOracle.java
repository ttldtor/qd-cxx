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
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.Summary;
import com.dxfeed.event.market.SummaryDelegate;
import com.dxfeed.event.market.impl.SummaryMapping;
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

/** Captures actual production Summary mappings, delegates, physical slots and business getters. */
public final class SummaryOracle {
  private static final int[] NUMERIC = {2, 4, 8, 24, 104, 120};
  private static final int[] RAW = {2, 4, 8, 24, 40, 56, 72, 88, 104, 120, 152};

  private static boolean numeric(String name) {
    return Arrays.asList(
            "DayOpen.Price",
            "DayHigh.Price",
            "DayLow.Price",
            "DayClose.Price",
            "PrevDayClose.Price",
            "PrevDayVolume")
        .contains(name);
  }

  private static boolean raw(String name) {
    return Arrays.asList("DayId", "PrevDayId", "Flags").contains(name);
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
    if (type == 24) {
      long[] result = new long[128 * 5];
      int p = 0;
      for (int high : new int[] {Integer.MIN_VALUE, -128, 0, 128, 0x7fffff80})
        for (int low = 0; low < 128; ++low) result[p++] = high | low;
      return result;
    }
    if (type == 104) {
      List<Long> values =
          new ArrayList<>(Arrays.asList(Long.MIN_VALUE, Long.MAX_VALUE, 0L, -1L, 1L));
      long min = -(1L << 55), max = (1L << 55) - 1;
      for (long base :
          new long[] {
            min,
            max,
            min * 10,
            max * 10,
            min * 100,
            max * 100,
            min * 10 - 5,
            max * 10 + 5,
            min * 100 - 50,
            max * 100 + 50
          }) for (int delta = -6; delta <= 6; ++delta) values.add(base + delta);
      for (long base :
          new long[] {
            -9007199254740992L, 9007199254740992L, Long.MIN_VALUE + 1000, Long.MAX_VALUE - 1000
          })
        for (int delta : new int[] {-501, -500, -499, -50, -5, 0, 5, 50, 499, 500, 501})
          values.add(base + delta);
      long[] result = new long[values.size()];
      for (int i = 0; i < result.length; ++i) result[i] = values.get(i);
      return result;
    }
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

  /** Whether this exact key is required by the production Summary mapping. */
  private static boolean required(String key) {
    return Arrays.asList(
            "DayId",
            "DayOpen.Price",
            "DayHigh.Price",
            "DayLow.Price",
            "PrevDayId",
            "PrevDayClose.Price")
        .contains(key);
  }

  /**
   * Finite integer-rounding, rank, narrowing and overflow probes for the production business-int
   * accessor.
   */
  private static long[] interestSamples(int type) {
    if (type != 120) return samples(type);
    List<Long> values = new ArrayList<>();
    for (int rank = 0; rank < 256; ++rank)
      for (long significand : new long[] {-1, 0, 1}) values.add((significand << 8) | rank);
    for (int rank :
        new int[] {0, 1, 109, 110, 111, 118, 119, 127, 128, 129, 130, 145, 146, 147, 254, 255})
      for (long significand :
          new long[] {
            -(1L << 55),
            -3602879701896397L,
            -2147483649L,
            -2147483648L,
            -15,
            -10,
            -9,
            -5,
            -1,
            0,
            1,
            5,
            9,
            10,
            15,
            2147483647L,
            2147483648L,
            2147483649L,
            (1L << 55) - 1
          }) values.add((significand << 8) | rank);
    long[] result = new long[values.size()];
    for (int i = 0; i < result.length; ++i) result[i] = values.get(i);
    return result;
  }

  /**
   * Clone complete physical storage, retaining long tails while independently permuting logical
   * fields.
   */
  private static DataRecord variant(DataRecord source, int id, String family, int type) {
    List<DataField> logical = fields(source);
    Collections.reverse(logical);
    List<DataIntField> ints = new ArrayList<>();
    for (DataField field : logical) {
      String local = field.getLocalName();
      if (family.equals("missing") && !required(local)) continue;
      if (family.startsWith("missing-") && local.equals(family.substring(8))) continue;
      if (family.startsWith("required-") && local.equals(family.substring(9))) continue;
      SerialFieldType selected = field.getSerialType();
      if (family.equals("numeric") && numeric(local)
          || family.equals("raw") && raw(local)
          || (family.equals("interest") || family.equals("bad-interest"))
              && local.equals("OpenInterest")
          || family.equals("bad") && numeric(local)) selected = type(type);
      ints.add(selected.createDefaultIntInstance(ints.size(), source.getName() + "." + local));
      if (selected.isLong())
        ints.add(new VoidIntField(ints.size(), source.getName() + "." + local + "$tail"));
    }
    return new DefaultRecord(
        id, source.getName(), false, ints.toArray(new DataIntField[0]), new DataObjField[0]);
  }

  /**
   * Select the actual production stream delegate; no event or conversion implementation is
   * substituted.
   */
  private static EventDelegate<?> delegate(DataRecord record) {
    for (EventDelegate<?> value : new MarketFactoryImpl().createStreamOnlyDelegates(record))
      if (value instanceof SummaryDelegate) return value;
    throw new IllegalArgumentException("No production Summary delegate");
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
    SummaryMapping mapping = record.getMapping(SummaryMapping.class);
    Method flags = Summary.class.getDeclaredMethod("getFlags");
    flags.setAccessible(true);
    long[] values =
        family.equals("flags")
            ? new long[32]
            : family.equals("interest") ? interestSamples(type) : samples(type);
    for (int sample = 0; sample < values.length; ++sample) {
      String symbol =
          family.equals("symbols")
              ? new String[] {"AAPL", "AAPL&N", "AAPL{x=y}", "AAPL&N{x=y}", "AAPL{x=y}&Q"}
                  [sample % 5]
              : "AAPL";
      RecordMode mode =
          family.equals("no-metadata") ? RecordMode.DATA : RecordMode.DATA.withEventTimeSequence();
      RecordCursor cursor = new RecordBuffer(mode).add(record, 0, symbol);
      if (cursor.hasEventTimeSequence())
        cursor.setEventTimeSequence(((long) (sample - 3) << 32) | 0x7fffffffL);
      for (DataField field : logical) {
        String name = field.getLocalName();
        long value = 0;
        if (numeric(name)) value = family.equals("numeric") ? values[sample] : 0x180;
        if (name.equals("OpenInterest"))
          value = family.equals("interest") ? values[sample] : 0x123456789abcdef0L;
        if (raw(name))
          value =
              family.equals("raw")
                  ? values[sample]
                  : name.equals("Flags")
                      ? (sample < 16 ? sample : 0x76543210L | sample)
                      : sample - 3;
        if (field.getSerialType().isLong()) cursor.setLong(field.getIndex(), value);
        else cursor.setInt(field.getIndex(), (int) value);
      }
      try {
        Summary event = (Summary) delegate.createEvent(cursor);
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
            "summary",
            scenario,
            record.getId(),
            sample,
            units(event.getEventSymbol()),
            event.getEventTime(),
            event.getDayId(),
            event.getPrevDayId(),
            event.getOpenInterest(),
            flags.invoke(event),
            event.getDayClosePriceType().getCode(),
            event.getPrevDayClosePriceType().getCode(),
            mapping.getDayOpenPriceWideDecimal(cursor),
            mapping.getDayHighPriceWideDecimal(cursor),
            mapping.getDayLowPriceWideDecimal(cursor),
            mapping.getDayClosePriceWideDecimal(cursor),
            mapping.getPrevDayClosePriceWideDecimal(cursor),
            mapping.getPrevDayVolumeWideDecimal(cursor),
            Double.doubleToRawLongBits(event.getDayOpenPrice()),
            Double.doubleToRawLongBits(event.getDayHighPrice()),
            Double.doubleToRawLongBits(event.getDayLowPrice()),
            Double.doubleToRawLongBits(event.getDayClosePrice()),
            Double.doubleToRawLongBits(event.getPrevDayClosePrice()),
            Double.doubleToRawLongBits(event.getPrevDayVolume()));
        if (family.equals("interest"))
          row(
              out,
              "interest",
              scenario,
              record.getId(),
              sample,
              mapping.getOpenInterest(cursor),
              mapping.getOpenInterestLong(cursor),
              Double.doubleToRawLongBits(mapping.getOpenInterestDouble(cursor)),
              mapping.getOpenInterestWideDecimal(cursor));
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

  /**
   * Takes a fresh output TSV and fixed profile selector; never opens a feed or changes the
   * reference.
   */
  public static void main(String[] args) throws Exception {
    if (args.length != 2 || !Arrays.asList("default", "tiny", "int").contains(args[1]))
      throw new IllegalArgumentException("Expected TSV and default/tiny/int");
    DataScheme source = DXFeedScheme.getInstance();
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8))) {
      String[] families =
          args[1].equals("default")
              ? new String[] {
                "default",
                "numeric",
                "raw",
                "interest",
                "missing",
                "missing-DayClose.Price",
                "missing-PrevDayVolume",
                "missing-OpenInterest",
                "missing-Flags",
                "required-DayId",
                "required-DayOpen.Price",
                "required-DayHigh.Price",
                "required-DayLow.Price",
                "required-PrevDayId",
                "required-PrevDayClose.Price",
                "flags",
                "symbols",
                "no-metadata",
                "bad-interest",
                "bad"
              }
              : new String[] {"scheme-" + args[1]};
      for (String family : families) {
        int[] types =
            family.equals("numeric") || family.equals("interest")
                ? NUMERIC
                : family.equals("raw")
                    ? RAW
                    : (family.equals("bad") || family.equals("bad-interest"))
                        ? new int[] {40, 152}
                        : new int[] {120};
        for (int type : types) {
          String scenario = family + type;
          DataRecord[] records = {
            variant(source.findRecordByName("Summary"), 0, family, type),
            variant(source.findRecordByName("Summary&Q"), 1, family, type)
          };
          new DefaultScheme(PentaCodec.INSTANCE, records);
          for (DataRecord record : records) observe(out, record, scenario, family, type);
        }
      }
      if (out.checkError()) throw new IOException("Cannot write Summary fixture");
    }
  }
}
