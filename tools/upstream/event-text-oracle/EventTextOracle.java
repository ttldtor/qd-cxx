// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.util.TimeFormat;
import com.devexperts.util.WideDecimal;
import com.dxfeed.event.IndexedEventSource;
import com.dxfeed.event.impl.EventUtil;
import com.dxfeed.event.market.Direction;
import com.dxfeed.event.market.OrderSource;
import com.dxfeed.event.market.PriceType;
import com.dxfeed.event.market.Profile;
import com.dxfeed.event.market.Quote;
import com.dxfeed.event.market.Summary;
import com.dxfeed.event.market.Trade;
import com.dxfeed.event.market.TradeBase;
import com.dxfeed.event.market.TradeETH;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.SimpleTimeZone;
import java.util.TimeZone;

/**
 * Internal assessment caller: javac, then Java with explicit timezone/language/country. Input:
 * fresh output filename and exact zone/fixed:offsetMillis. Output: ASCII scalar/UTF-16 hex TSV.
 * Errors propagate and fail the run. Maintained launcher captures bounded sequential processes.
 */
public final class EventTextOracle {
  private static BufferedWriter output;

  private static String units(String text) {
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < text.length(); ++i) {
      if (i != 0) {
        result.append(',');
      }
      String hex = Integer.toHexString(text.charAt(i));
      for (int j = hex.length(); j < 4; ++j) {
        result.append('0');
      }
      result.append(hex);
    }
    return result.toString();
  }

  private static void row(String kind, String input, String text) throws Exception {
    boolean ascii =
        kind.equals("double")
            || kind.equals("wide")
            || kind.equals("char")
            || kind.equals("flags")
            || kind.equals("enum")
            || kind.equals("millis")
            || kind.equals("seconds")
            || kind.equals("secondsFresh");
    output.write(kind + "\t" + input + "\t" + (ascii ? text : units(text)) + "\n");
  }

  private static void number(long bits) throws Exception {
    double value = Double.longBitsToDouble(bits);
    row("double", Long.toHexString(bits), Double.toString(value));
    row("wide", Long.toHexString(bits), WideDecimal.toString(WideDecimal.composeWide(value)));
  }

  private static void fillTrade(TradeBase trade) {
    trade.setEventTime(-1);
    trade.setTime(-1);
    trade.setTimeNanoPart(-17);
    trade.setSequence(4194303);
    trade.setExchangeCode('\udfff');
    trade.setTradeId(Long.MIN_VALUE);
    trade.setPrice(-0.0);
    trade.setChange(Double.MIN_VALUE);
    trade.setSizeAsDouble(Double.POSITIVE_INFINITY);
    trade.setDayId(Integer.MIN_VALUE);
    trade.setDayVolumeAsDouble(1234567890123456.5);
    trade.setDayTurnover(Double.NaN);
    trade.setTickDirection(Direction.ZERO_UP);
    trade.setExtendedTradingHours(true);
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 2) {
      throw new IllegalArgumentException("Output filename and explicit timezone are required");
    }
    String zoneId = args[1];
    if (zoneId.startsWith("fixed:")) {
      int offset = Integer.parseInt(zoneId.substring(6));
      TimeZone.setDefault(new SimpleTimeZone(offset, zoneId));
    } else {
      TimeZone zone = TimeZone.getTimeZone(zoneId);
      if (!zone.getID().equals(zoneId)) {
        throw new IllegalArgumentException("Exact timezone unavailable: " + zoneId);
      }
      TimeZone.setDefault(zone);
    }
    try (BufferedWriter writer =
        Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8)) {
      output = writer;
      if (zoneId.equals("UTC")) {
        long[] bits = {
          0L,
          Long.MIN_VALUE,
          1L,
          Long.MIN_VALUE | 1L,
          2L,
          0x000fffffffffffffL,
          0x0010000000000000L,
          0x0010000000000001L,
          0x3f50624dd2f1a9fbL,
          0x3f50624dd2f1a9fcL,
          0x3f50624dd2f1a9fdL,
          0x416312cfffffffffL,
          0x416312d000000000L,
          0x416312d000000001L,
          0x3ff0000000000000L,
          0x3ff0000000000001L,
          0x4340000000000000L,
          0x43e0000000000000L,
          0x7fefffffffffffffL,
          0x7ff0000000000000L,
          0xfff0000000000000L,
          0x7ff8000000000000L,
          0xfff8000000000001L,
          0x44b52d02c7e14af6L,
          0x3eb0c6f7a0b5ed8dL
        };
        for (long value : bits) {
          number(value);
        }
        long state = 0x123456789abcdefL;
        for (int i = 0; i < 32768; ++i) {
          state ^= state << 13;
          state ^= state >>> 7;
          state ^= state << 17;
          number(state);
        }
        for (int exponent = 0; exponent < 2047; ++exponent) {
          for (long fraction : new long[] {0L, 1L, (1L << 51) - 1, (1L << 52) - 1}) {
            long value = ((long) exponent << 52) | fraction;
            number(value);
            number(value | Long.MIN_VALUE);
          }
        }
        for (int exponent = -323; exponent <= 308; ++exponent) {
          long center = Double.doubleToRawLongBits(Double.parseDouble("1e" + exponent));
          for (long step = -2; step <= 2; ++step) {
            long value = center + step;
            if (value > 0 && value < 0x7ff0000000000000L) {
              number(value);
              number(value | Long.MIN_VALUE);
            }
          }
        }
        for (long value = 1; value <= 1024; ++value) {
          number(value);
          number(value | Long.MIN_VALUE);
        }
        for (int flag :
            new int[] {0, 1, 2, 4, 8, 16, 64, 127, 255, Integer.MIN_VALUE, Integer.MAX_VALUE, -1}) {
          row("flags", Integer.toString(flag), Integer.toHexString(flag));
        }
        for (String type :
            new String[] {
              "Direction",
              "TradingStatus",
              "ShortSaleRestriction",
              "Side",
              "Scope",
              "OrderAction",
              "IcebergType",
              "PriceType",
              "OtcMarketsPriceType",
              "TimeAndSaleType",
              "AuctionType",
              "ImbalanceSide"
            }) {
          Class<?> enumClass = Class.forName("com.dxfeed.event.market." + type);
          Object[] values = enumClass.getEnumConstants();
          for (int i = 0; i < values.length; ++i) {
            row("enum", type + ":" + i, values[i].toString());
          }
        }
        for (int unit = 0; unit <= 65535; ++unit) {
          row("char", Integer.toString(unit), EventUtil.encodeChar((char) unit));
        }
        for (String name :
            new String[] {
              "DEFAULT",
              "COMPOSITE_BID",
              "COMPOSITE_ASK",
              "REGIONAL_BID",
              "REGIONAL_ASK",
              "AGGREGATE_BID",
              "AGGREGATE_ASK",
              "COMPOSITE",
              "REGIONAL",
              "AGGREGATE",
              "A",
              "ABE",
              "NTV",
              "ntv",
              "1234"
            }) {
          row("orderSource", name, OrderSource.valueOf(name).toString());
        }
        row("source", "null", String.valueOf(new IndexedEventSource(-1, null).toString()));
        row("source", "empty", new IndexedEventSource(Integer.MIN_VALUE, "").toString());
        row("source", "literal", new IndexedEventSource(7, "null").toString());
        row("source", "units", new IndexedEventSource(7, "A\u0000\ud800'{}").toString());
      }
      long[] times = {
        Long.MIN_VALUE,
        -62135683200001L,
        -62135683200000L,
        -62135683199999L,
        -62135596800000L,
        -2208988800001L,
        -2208988800000L,
        -1L,
        0L,
        1L,
        999L,
        1000L,
        86400000L,
        1583650800000L,
        1604210400000L,
        253402300799999L,
        253402387199999L,
        253402387200000L,
        253402387200001L,
        253402473600000L,
        253402473600001L,
        Long.MAX_VALUE
      };
      for (long time : times) {
        row("millis", Long.toString(time), TimeFormat.DEFAULT.withMillis().format(time));
        row("seconds", Long.toString(time), TimeFormat.DEFAULT.format(time));
        row(
            "secondsFresh",
            Long.toString(time),
            TimeFormat.getInstance(TimeZone.getDefault()).format(time));
      }
      row("summary", "default", new Summary("").toString());
      Summary summary = new Summary("S\u0000\ud800'{}");
      summary.setEventTime(-1);
      summary.setDayId(Integer.MIN_VALUE);
      summary.setPrevDayId(Integer.MAX_VALUE);
      summary.setDayOpenPrice(-0.0);
      summary.setDayHighPrice(Double.POSITIVE_INFINITY);
      summary.setDayLowPrice(Double.NEGATIVE_INFINITY);
      summary.setDayClosePrice(Double.MIN_VALUE);
      summary.setDayClosePriceType(PriceType.FINAL);
      summary.setPrevDayClosePrice(1e7);
      summary.setPrevDayClosePriceType(PriceType.PRELIMINARY);
      summary.setPrevDayVolume(1234567890123456.5);
      summary.setOpenInterest(3000000000L);
      row("summary", "filled", summary.toString());
      for (long interest : new long[] {0L, 1L, -1L, 3000000000L, Long.MIN_VALUE, Long.MAX_VALUE}) {
        Summary interestSummary = new Summary("interest");
        interestSummary.setOpenInterest(interest);
        row("summaryInterest", Long.toString(interest), interestSummary.toString());
      }
      row("quote", "default", new Quote("").toString());
      row("profile", "default", new Profile("").toString());
      row("trade", "default", new Trade("").toString());
      row("tradeETH", "default", new TradeETH("").toString());
      Quote quote = new Quote("A\u0000\ud800'{}");
      quote.setEventTime(-1);
      quote.setBidTime(1234567890123L);
      quote.setAskTime(1234567890999L);
      quote.setSequence(4194303);
      quote.setTimeNanoPart(-17);
      quote.setBidExchangeCode('\u0000');
      quote.setAskExchangeCode('\udfff');
      quote.setBidPrice(-0.0);
      quote.setBidSizeAsDouble(0.001);
      quote.setAskPrice(Double.POSITIVE_INFINITY);
      quote.setAskSizeAsDouble(Double.NaN);
      row("quote", "filled", quote.toString());
      Profile profile = new Profile("P'\udfff");
      profile.setEventTime(1);
      profile.setDescription("quotes'\n\u0000");
      profile.setStatusReason(null);
      profile.setHaltStartTime(-1);
      profile.setHaltEndTime(1234567890123L);
      profile.setBeta(-0.0);
      profile.setEarningsPerShare(Double.NEGATIVE_INFINITY);
      profile.setDividendFrequency(1e7);
      profile.setExDividendAmount(Double.MIN_VALUE);
      profile.setExDividendDayId(-1);
      profile.setShares(1234567890123456.5);
      profile.setFreeFloat(Double.NaN);
      row("profile", "filled", profile.toString());
      Trade trade = new Trade("T\u0000\ud800");
      fillTrade(trade);
      row("trade", "filled", trade.toString());
      TradeETH tradeETH = new TradeETH("T\u0000\ud800");
      fillTrade(tradeETH);
      row("tradeETH", "filled", tradeETH.toString());
      java.lang.reflect.Method profileFlags =
          Profile.class.getDeclaredMethod("setFlags", int.class);
      profileFlags.setAccessible(true);
      java.lang.reflect.Method summaryFlags =
          Summary.class.getDeclaredMethod("setFlags", int.class);
      summaryFlags.setAccessible(true);
      java.lang.reflect.Method tradeFlags =
          TradeBase.class.getDeclaredMethod("setFlags", int.class);
      tradeFlags.setAccessible(true);
      for (int raw :
          new int[] {
            0,
            1,
            2,
            3,
            4,
            5,
            6,
            7,
            8,
            9,
            10,
            11,
            12,
            13,
            14,
            15,
            Integer.MIN_VALUE,
            Integer.MAX_VALUE,
            -1,
            0x10203040
          }) {
        Summary flagSummary = new Summary("flags");
        summaryFlags.invoke(flagSummary, raw);
        row("summaryFlags", Integer.toString(raw), flagSummary.toString());
        Profile flagProfile = new Profile("flags");
        profileFlags.invoke(flagProfile, raw);
        row("profileFlags", Integer.toString(raw), flagProfile.toString());
        Trade flagTrade = new Trade("flags");
        tradeFlags.invoke(flagTrade, raw);
        row("tradeFlags", Integer.toString(raw), flagTrade.toString());
        TradeETH flagTradeETH = new TradeETH("flags");
        tradeFlags.invoke(flagTradeETH, raw);
        row("tradeETHFlags", Integer.toString(raw), flagTradeETH.toString());
      }
    }
  }
}
