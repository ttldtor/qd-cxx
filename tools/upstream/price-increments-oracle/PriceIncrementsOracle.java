// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.glossary.PriceIncrements;
import java.io.BufferedWriter;
import java.math.RoundingMode;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/** Invoked by Invoke-Oracle.ps1; all expectations call pinned production PriceIncrements. */
public final class PriceIncrementsOracle {
  private PriceIncrementsOracle() {}

  private static String hex(String text) {
    if (text == null) return "~";
    if (text.isEmpty()) return "-";
    StringBuilder out = new StringBuilder();
    for (char ch : text.toCharArray()) {
      for (int shift = 12; shift >= 0; shift -= 4)
        out.append(Character.forDigit((ch >> shift) & 15, 16));
    }
    return out.toString();
  }

  private static String bits(double value) {
    return Long.toHexString(Double.doubleToLongBits(value));
  }

  private static String array(double[] values) {
    StringBuilder out = new StringBuilder();
    for (double value : values) {
      if (out.length() != 0) out.append(',');
      out.append(bits(value));
    }
    return out.length() == 0 ? "-" : out.toString();
  }

  private static void row(BufferedWriter out, Object... cells) throws Exception {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) out.write('\t');
      out.write(cells[i].toString());
    }
    out.write('\n');
  }

  private static String describe(PriceIncrements value) {
    return hex(value.getText())
        + "|"
        + value.hashCode()
        + "|"
        + array(value.getPriceIncrements())
        + "|"
        + value.getPricePrecision();
  }

  private static String error(Exception e) {
    return e instanceof ArithmeticException
        ? "ARITHMETIC"
        : e instanceof IllegalArgumentException
            ? "INVALID"
            : "UNEXPECTED:" + e.getClass().getName();
  }

  private static void parse(BufferedWriter out, String text) throws Exception {
    try {
      row(out, "P", hex(text), describe(PriceIncrements.valueOf(text)));
    } catch (Exception e) {
      row(out, "P", hex(text), error(e));
    }
  }

  private static void scalar(BufferedWriter out, double number) throws Exception {
    try {
      row(out, "S", bits(number), describe(PriceIncrements.valueOf(number)));
    } catch (Exception e) {
      row(out, "S", bits(number), error(e));
    }
  }

  private static void vector(BufferedWriter out, double[] values) throws Exception {
    try {
      row(
          out,
          "A",
          values == null ? "~" : array(values),
          describe(PriceIncrements.valueOf(values)));
    } catch (Exception e) {
      row(out, "A", values == null ? "~" : array(values), error(e));
    }
  }

  private static void query(
      BufferedWriter out, String text, double price, int direction, double step) throws Exception {
    PriceIncrements value = PriceIncrements.valueOf(text);
    StringBuilder modes = new StringBuilder();
    for (RoundingMode mode : RoundingMode.values()) {
      if (modes.length() != 0) modes.append(',');
      try {
        modes.append(bits(value.roundPrice(price, mode)));
      } catch (Exception e) {
        modes.append(error(e));
      }
    }
    String advanced;
    try {
      advanced = bits(value.incrementPrice(price, direction, step));
    } catch (Exception e) {
      advanced = error(e);
    }
    row(
        out,
        "Q",
        hex(text),
        bits(price),
        direction,
        bits(step),
        bits(value.getPriceIncrement(price, direction)),
        value.getPricePrecision(price),
        bits(value.roundPrice(price, direction)),
        modes,
        advanced);
  }

  public static void main(String[] args) throws Exception {
    try (BufferedWriter out = Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8)) {
      String[] texts = {
        null,
        "",
        "0",
        "-0.0",
        "+.0",
        "0.",
        "000",
        "+",
        "-",
        ".",
        " ",
        "0 ",
        " 0",
        "1E",
        "1E+",
        "1e-",
        ".01",
        "0.0100",
        "1e-19",
        "1E-400",
        "-1e-400",
        "1e2147483648",
        "1e4294967296",
        "1e2147483647",
        "1e-2147483648",
        "Infinity",
        "NaN",
        "0.01 1; 0.05",
        "0.01 1; 0.005",
        "0.01 1; 0.05 10; 0.1",
        "0 1; 0.1",
        "0.1 0; 1",
        "0.1 2; 0.2 1; 1",
        "0.1 1;0.2",
        "0.1 1; ",
        "0.1 1; 0.2 ",
        "0.1  1; 0.2",
        "0.1\t1; 0.2",
        "0.1 1 ; 0.2",
        "1e-18 1e-18; 1e-18",
        "1e-18 1e-18; 1e-18 3e-18; 1e-18",
        "1e-18 1e-18; 1e-18 3.000000000000001e-18; 1e-18",
        "1e309",
        "-0.000",
        "+000.",
        "0e0",
        "-0e0",
        "0x1p0",
        "\ud800"
      };
      for (String text : texts) parse(out, text);
      for (String text :
          new String[] {
            ";",
            "; ",
            " ; ",
            "0;",
            "0; ",
            "0; 0",
            "0 1;",
            "0 1; ",
            "0 1; 0",
            "1 1; 1; 1",
            "1 1; 1 2;",
            "1 1; 1 2; "
          }) {
        parse(out, text);
      }
      Random random = new Random(220);
      for (int i = 0; i < 2048; ++i) scalar(out, Double.longBitsToDouble(random.nextLong()));
      for (double number :
          new double[] {
            0,
            -0.0,
            Double.NaN,
            Double.POSITIVE_INFINITY,
            Double.NEGATIVE_INFINITY,
            Double.MIN_VALUE,
            Double.MIN_NORMAL,
            Double.MAX_VALUE,
            1e-18,
            1e-19,
            1e23,
            1.9947337614746448e16,
            1.994733761474645e16
          }) scalar(out, number);
      vector(out, null);
      vector(out, new double[] {});
      double[][] arrays = {
        {0},
        {-0.0},
        {1e-19},
        {Double.MIN_VALUE},
        {Double.NaN},
        {.01, 1, .05},
        {.01, 1, .005},
        {.1, 1, .2, 2, .3},
        {.1, 1},
        {.1, Double.NaN, .2},
        {.1, 1, .2, 1, .3},
        {.1, 1, .2, Math.nextUp(1.0), .3},
        {.1, 1e-18, .2, 3e-18, .3},
        {.1, 1e-18, .2, Math.nextUp(3e-18), .3}
      };
      for (double[] values : arrays) vector(out, values);
      String[] rules = {
        "",
        "-0e0",
        "1e-19",
        "0.01",
        "0.05",
        "0.1",
        "0.25",
        "1",
        "2.5",
        "1e-18",
        "1e10",
        "1e100",
        "1e308",
        "0.01 1; 0.05",
        "0.05 1; 0.01",
        "0.01 1; 0.05 10; 0.1",
        "1e-18 1e-10; 1e-10",
        "0.1 1e100; 1e100"
      };
      List<Double> prices = new ArrayList<>();
      for (double number :
          new double[] {
            0,
            -0.0,
            Double.NaN,
            Double.POSITIVE_INFINITY,
            Double.NEGATIVE_INFINITY,
            Double.MIN_VALUE,
            -Double.MIN_VALUE,
            Double.MIN_NORMAL,
            Double.MAX_VALUE,
            -Double.MAX_VALUE,
            1e23,
            -1e23,
            1.9947337614746448e16,
            -3.461273582384885e16
          }) prices.add(number);
      for (double center :
          new double[] {
            1e-18,
            1e-10,
            .005,
            .015,
            .025,
            .05,
            .075,
            .125,
            .5,
            1,
            1.00000001,
            2.5,
            9.95,
            10,
            1e10,
            1e100
          }) {
        for (double value : new double[] {Math.nextDown(center), center, Math.nextUp(center)}) {
          prices.add(value);
          prices.add(-value);
        }
      }
      for (int i = 0; i < 32; ++i) prices.add(Double.longBitsToDouble(random.nextLong()));
      int[] directions = {Integer.MIN_VALUE, -1, 0, 1, Integer.MAX_VALUE};
      for (String rule : rules)
        for (double price : prices)
          for (int direction : directions) query(out, rule, price, direction, 0);
      for (int i = 0; i < 8192; ++i) {
        double price = Double.longBitsToDouble(random.nextLong());
        for (double increment : new double[] {1e-18, 1, .05})
          row(
              out,
              "D",
              bits(price),
              bits(increment),
              bits(PriceIncrements.valueOf(increment).roundPrice(price)));
      }
      for (int exponent = 15; exponent <= 18; ++exponent) {
        double step = Math.pow(10, exponent - 14);
        for (int sample = 0; sample < 64; ++sample) {
          long center =
              Double.doubleToRawLongBits((100000000000000L + sample * 13743769123456L + .5) * step);
          for (int adjacent = -2; adjacent <= 2; ++adjacent)
            for (long sign : new long[] {0, Long.MIN_VALUE}) {
              double price = Double.longBitsToDouble((center + adjacent) | sign);
              row(
                  out,
                  "D",
                  bits(price),
                  bits(1.0),
                  bits(PriceIncrements.valueOf(1).roundPrice(price)));
            }
        }
      }
      for (String rule : rules)
        for (double step : new double[] {-.1, Double.NaN, Double.POSITIVE_INFINITY, -0.0, .03, 1})
          for (int direction : directions) query(out, rule, .025, direction, step);
    }
  }
}
