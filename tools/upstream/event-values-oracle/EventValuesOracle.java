// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.util.DayUtil;
import com.devexperts.util.TimeUtil;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.event.IndexedEvent;
import com.dxfeed.event.impl.EventUtil;
import com.dxfeed.event.impl.TimeNanosUtil;
import com.dxfeed.event.market.AuctionType;
import com.dxfeed.event.market.Direction;
import com.dxfeed.event.market.IcebergType;
import com.dxfeed.event.market.ImbalanceSide;
import com.dxfeed.event.market.OrderAction;
import com.dxfeed.event.market.OtcMarketsPriceType;
import com.dxfeed.event.market.PriceType;
import com.dxfeed.event.market.Scope;
import com.dxfeed.event.market.ShortSaleRestriction;
import com.dxfeed.event.market.Side;
import com.dxfeed.event.market.TimeAndSaleType;
import com.dxfeed.event.market.Trade;
import com.dxfeed.event.market.TradingStatus;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

/**
 * Observes production value utilities and the existing standard schema without a runtime endpoint.
 */
public final class EventValuesOracle {
  private static final int[] INTS = {
    Integer.MIN_VALUE,
    Integer.MIN_VALUE + 1,
    -1000001,
    -1000,
    -1,
    0,
    1,
    999,
    1000,
    1000000,
    0x12345678,
    Integer.MAX_VALUE
  };
  private static final long[] LONGS = {
    Long.MIN_VALUE,
    Long.MIN_VALUE + 1,
    -2147483648001L,
    -1000001,
    -1000,
    -1,
    0,
    1,
    999,
    1000,
    1000000,
    2147483647999L,
    0x123456789abcdef0L,
    Long.MAX_VALUE
  };

  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) {
        out.print('\t');
      }
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static String units(String text) {
    if (text == null) {
      return "null";
    }
    if (text.isEmpty()) {
      return "-";
    }
    StringBuilder result = new StringBuilder();
    for (char value : text.toCharArray()) {
      result.append(String.format("%04x", (int) value));
    }
    return result.toString();
  }

  private static void enums(PrintWriter out) throws Exception {
    Class<?>[] types = {
      Direction.class,
      TradingStatus.class,
      ShortSaleRestriction.class,
      Side.class,
      Scope.class,
      OrderAction.class,
      IcebergType.class,
      PriceType.class,
      OtcMarketsPriceType.class,
      TimeAndSaleType.class,
      AuctionType.class,
      ImbalanceSide.class
    };
    for (Class<?> type : types) {
      Method valueOf = type.getMethod("valueOf", int.class);
      Method getCode = type.getMethod("getCode");
      for (int code = -1; code <= 17; ++code) {
        try {
          row(out, "enum", type.getSimpleName(), code, getCode.invoke(valueOf.invoke(null, code)));
        } catch (InvocationTargetException failure) {
          if (!(failure.getCause() instanceof ArrayIndexOutOfBoundsException)) {
            throw failure;
          }
          row(out, "enum", type.getSimpleName(), code, "error");
        }
      }
    }
    row(
        out,
        "flags",
        IndexedEvent.TX_PENDING,
        IndexedEvent.REMOVE_EVENT,
        IndexedEvent.SNAPSHOT_BEGIN,
        IndexedEvent.SNAPSHOT_END,
        IndexedEvent.SNAPSHOT_SNIP,
        IndexedEvent.SNAPSHOT_MODE);
  }

  private static void bits(PrintWriter out) {
    int[] masks32 = {0, 1, 3, 7, -1, Integer.MIN_VALUE, Integer.MAX_VALUE, 0x55aa55aa};
    long[] masks64 = {0, 1, 3, 7, -1, Long.MIN_VALUE, Long.MAX_VALUE, 0x55aa55aa55aa55aaL};
    int[] shifts = {
      Integer.MIN_VALUE, -65, -64, -33, -32, -1, 0, 1, 22, 31, 32, 63, 64, 65, Integer.MAX_VALUE
    };
    for (int flags : INTS) {
      for (int mask : masks32) {
        for (int shift : shifts) {
          for (int value : new int[] {0, -1, Integer.MIN_VALUE, 0x12345678}) {
            row(
                out,
                "bits32",
                flags,
                mask,
                shift,
                value,
                EventUtil.getBits(flags, mask, shift),
                EventUtil.setBits(flags, mask, shift, value));
          }
        }
      }
    }
    for (long flags : LONGS) {
      for (long mask : masks64) {
        for (int shift : shifts) {
          for (long value : new long[] {0, -1, Long.MIN_VALUE, 0x123456789abcdef0L}) {
            row(
                out,
                "bits64",
                flags,
                mask,
                shift,
                value,
                EventUtil.getBits(flags, mask, shift),
                EventUtil.setBits(flags, mask, shift, value));
          }
        }
      }
    }
  }

  private static void time(PrintWriter out) {
    Trade event = new Trade();
    for (long value : LONGS) {
      row(
          out,
          "nanos",
          value,
          TimeNanosUtil.getMillisFromNanos(value),
          TimeNanosUtil.getNanoPartFromNanos(value));
      row(
          out,
          "time",
          value,
          TimeUtil.getSecondsFromTime(value),
          TimeUtil.getMillisFromTime(value));
      event.setTimeSequence(value);
      row(out, "packed", value, event.getTime(), event.getSequence());
      for (int part : new int[] {Integer.MIN_VALUE, -1, 0, 1, 999999, 1000000, Integer.MAX_VALUE}) {
        row(out, "combine", value, part, TimeNanosUtil.getNanosFromMillisAndNanoPart(value, part));
      }
      for (long millis : LONGS) {
        event.setTimeSequence(value);
        event.setTime(millis);
        row(out, "settime", value, millis, event.getTimeSequence());
      }
      for (int sequence :
          new int[] {
            Integer.MIN_VALUE,
            -1,
            0,
            1,
            Trade.MAX_SEQUENCE,
            Trade.MAX_SEQUENCE + 1,
            Integer.MAX_VALUE
          }) {
        event.setTimeSequence(value);
        try {
          event.setSequence(sequence);
          row(out, "setseq", value, sequence, event.getTimeSequence());
        } catch (IllegalArgumentException failure) {
          row(out, "setseq", value, sequence, "error");
        }
      }
    }
  }

  private static void day(PrintWriter out, int year, int month, int day) {
    try {
      row(out, "day", year, month, day, DayUtil.getDayIdByYearMonthDay(year, month, day));
    } catch (IllegalArgumentException failure) {
      row(out, "day", year, month, day, "error");
    }
  }

  private static void date(PrintWriter out, int value) {
    try {
      row(out, "date", value, DayUtil.getDayIdByYearMonthDay(value));
    } catch (IllegalArgumentException failure) {
      row(out, "date", value, "error");
    }
    row(out, "calendar", value, DayUtil.getYearMonthDayByDayId(value));
  }

  private static void days(PrintWriter out) {
    for (int year :
        new int[] {
          Integer.MIN_VALUE,
          Integer.MIN_VALUE + 1,
          -10000,
          -4800,
          -400,
          -100,
          -4,
          -1,
          0,
          1,
          4,
          100,
          400,
          1600,
          1900,
          1969,
          1970,
          2000,
          2024,
          2100,
          10000,
          Integer.MAX_VALUE
        }) {
      for (int month :
          new int[] {-1, 0, 1, 2, 3, 4, 12, 13, Integer.MIN_VALUE, Integer.MAX_VALUE}) {
        for (int day :
            new int[] {
              Integer.MIN_VALUE, -366, -1, 0, 1, 28, 29, 30, 31, 365, 366, Integer.MAX_VALUE
            }) {
          day(out, year, month, day);
        }
      }
    }
    for (int value : INTS) {
      date(out, value);
    }
    for (int value :
        new int[] {
          -20000229,
          -10101,
          0,
          10101,
          19000229,
          19700101,
          20000229,
          20240229,
          21000229,
          20240000,
          20241301,
          Integer.MAX_VALUE - 2472632,
          Integer.MAX_VALUE - 2472631
        }) {
      date(out, value);
    }
    int random = 0x12345678;
    for (int i = 0; i < 1024; ++i) {
      random = random * 1664525 + 1013904223;
      date(out, random);
      day(out, random, i % 12 + 1, random ^ 0x55aa55aa);
    }
  }

  private static void strings(PrintWriter out) {
    String[] texts = {
      null,
      "",
      "A",
      "ABCDEFGH",
      "ABCDEFGHI",
      "\0",
      "A\0B",
      "\0\0\0\0\0\0\0\0",
      "\u00ffABC",
      "\u0080ABC",
      "\u00ff",
      "\u0100",
      "\ud800",
      "\u00ffABCDEFG",
      "\u0100ABCDEFGH"
    };
    DataRecord record = DXFeedScheme.getInstance().findRecordByName("TimeAndSale");
    DataIntField field =
        (DataIntField) record.findFieldByName("TimeAndSale.ExchangeSaleConditions");
    if (field == null
        || !field.getSerialType().hasSameRepresentationAs(SerialFieldType.SHORT_STRING)) {
      throw new IllegalStateException("Actual TimeAndSale SHORT_STRING field is missing");
    }
    row(out, "schema", record.getName(), field.getName(), field.getSerialType().getId());
    for (String text : texts) {
      try {
        long code = EventUtil.encodeShortString(text);
        row(out, "encode", units(text), code, units(EventUtil.decodeShortString(code)));
        row(
            out,
            "field",
            units(text),
            field.parseString(text),
            units(field.toString(field.parseString(text))));
      } catch (IllegalArgumentException failure) {
        row(out, "encode", units(text), "error", "error");
      }
    }
    for (long code : LONGS) {
      row(out, "decode", code, units(EventUtil.decodeShortString(code)));
    }
    for (int value = 1; value <= 255; ++value) {
      for (int shift = 0; shift < 64; shift += 8) {
        long code = (long) value << shift;
        row(out, "decode", code, units(EventUtil.decodeShortString(code)));
      }
    }
    row(
        out,
        "decode",
        0x4100420043004400L,
        units(EventUtil.decodeShortString(0x4100420043004400L)));
  }

  /** Takes output TSV and the fixed all selector; does not open endpoints or change schemas. */
  public static void main(String[] args) throws Exception {
    if (args.length != 2 || !args[1].equals("all")) {
      throw new IllegalArgumentException("Expected output TSV and all selector");
    }
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8))) {
      enums(out);
      bits(out);
      time(out);
      days(out);
      strings(out);
      if (out.checkError()) {
        throw new IOException("Cannot write event value observations");
      }
    }
  }
}
