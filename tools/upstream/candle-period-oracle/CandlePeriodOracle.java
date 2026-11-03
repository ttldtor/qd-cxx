// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.event.candle.CandlePeriod;
import com.dxfeed.event.candle.CandleType;
import java.io.File;
import java.io.PrintWriter;
import java.util.Locale;
import java.util.Random;

/** Deterministic observations from the pinned production Candle value classes. */
public final class CandlePeriodOracle {
  private static String hex(String text) {
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < text.length(); ++i) {
      String unit = Integer.toHexString(text.charAt(i));
      for (int n = unit.length(); n < 4; ++n) {
        result.append('0');
      }
      result.append(unit);
    }
    return result.toString();
  }

  private static void observe(
      PrintWriter out, String operation, String input, int type, long bits) {
    String prefix = operation + "\t" + hex(input) + "\t" + type + "\t" + Long.toHexString(bits);
    try {
      if (operation.equals("type")) {
        CandleType result = CandleType.parse(input);
        out.println(
            prefix
                + "\t"
                + hex(result.toString())
                + "\t"
                + result.ordinal()
                + "\t-\t"
                + result.getPeriodIntervalMillis());
      } else {
        CandlePeriod result =
            operation.equals("parse")
                ? CandlePeriod.parse(input)
                : CandlePeriod.valueOf(Double.longBitsToDouble(bits), CandleType.values()[type]);
        out.println(
            prefix
                + "\t"
                + hex(result.toString())
                + "\t"
                + result.getType().ordinal()
                + "\t"
                + Long.toHexString(Double.doubleToRawLongBits(result.getValue()))
                + "\t"
                + result.getPeriodIntervalMillis());
      }
    } catch (IllegalArgumentException exception) {
      out.println(prefix + "\tERROR\t-\t-\t-");
    }
  }

  /** Write one TSV output; the caller bounds process time and heap, no network work occurs. */
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output TSV filename");
    }
    try (PrintWriter out = new PrintWriter(new File(args[0]), "UTF-8")) {
      for (CandleType type : CandleType.values()) {
        String name = type.name();
        for (int i = 1; i <= name.length(); ++i) {
          String prefix = name.substring(0, i);
          observe(out, "type", prefix, -1, 0);
          observe(out, "type", prefix.toLowerCase(Locale.ROOT), -1, 0);
          observe(out, "parse", "2.5" + prefix, -1, 0);
        }
        for (String suffix : new String[] {"s", "S", "ss"}) {
          observe(out, "type", name + suffix, -1, 0);
          observe(out, "type", name.toLowerCase(Locale.ROOT) + suffix, -1, 0);
        }
        observe(out, "type", type.toString(), -1, 0);
        for (String number :
            new String[] {
              "",
              "1",
              "+1",
              "-1",
              "0",
              "-0",
              "+0",
              ".5",
              "5.",
              "1e3",
              "1E-3",
              "1e309",
              "-1e309",
              "1e-9999",
              "-1e-9999",
              "0e9999",
              "NaN",
              "Infinity",
              "-Infinity",
              "0x1p2",
              "1f",
              "1d",
              "1_0",
              " 1",
              "1 ",
              ".",
              "+",
              "e",
              "1e",
              "1e+",
              "1..2",
              "--1",
              "1+-2"
            }) {
          observe(out, "parse", number + type.toString(), -1, 0);
        }
        long[] edges = {
          0L,
          Long.MIN_VALUE,
          0x3ff0000000000000L,
          0xbff0000000000000L,
          1L,
          Long.MIN_VALUE | 1L,
          0x0010000000000000L,
          0x7fefffffffffffffL,
          0xffefffffffffffffL,
          0x7ff0000000000000L,
          0xfff0000000000000L,
          0x7ff8000000000001L,
          0xfff0000000000001L,
          0x43e0000000000000L,
          0xc3e0000000000000L,
          0x43dfffffffffffffL,
          0x43e0000000000001L
        };
        for (long bits : edges) {
          observe(out, "value", "", type.ordinal(), bits);
        }
        for (double value :
            new double[] {
              1e-3,
              1e-4,
              1e7,
              1e8,
              1e23,
              1.2345678901234567,
              9007199254740992.0,
              0.1,
              0.0009999999999999998
            }) {
          for (double neighbor : new double[] {Math.nextDown(value), value, Math.nextUp(value)}) {
            observe(out, "value", "", type.ordinal(), Double.doubleToRawLongBits(neighbor));
          }
        }
        Random random = new Random(203);
        for (int i = 0; i < 512; ++i) {
          observe(out, "value", "", type.ordinal(), random.nextLong());
        }
      }
      // Every UTF-16 code unit tests Java's exact ASCII-name case equivalence and invalid inputs.
      for (int unit = 0; unit <= 0xffff; ++unit) {
        observe(out, "type", Character.toString((char) unit), -1, 0);
      }
      for (String text :
          new String[] {
            "",
            " pm",
            "PM",
            "Pr",
            "moS",
            "minutes ",
            "\u0131CK",
            "t\u0130ck",
            "\u017fecond",
            "tic\u212a",
            "price momentum",
            "price-renko",
            "1",
            "1e2"
          }) {
        observe(out, "type", text, -1, 0);
        observe(out, "parse", text, -1, 0);
      }
      if (!CandlePeriod.valueOf(Double.NaN, CandleType.DAY)
              .equals(
                  CandlePeriod.valueOf(
                      Double.longBitsToDouble(0xfff8000000000001L), CandleType.DAY))
          || CandlePeriod.valueOf(0.0, CandleType.DAY)
              .equals(CandlePeriod.valueOf(-0.0, CandleType.DAY))) {
        throw new AssertionError("NaN/signed-zero equality changed");
      }
    }
  }
}
