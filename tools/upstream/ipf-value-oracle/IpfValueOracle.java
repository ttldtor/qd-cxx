// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.ipf.InstrumentProfile;
import com.dxfeed.ipf.InstrumentProfileField;
import java.io.File;
import java.io.PrintWriter;
import java.util.Random;

/** Deterministic IPF value observations against the pinned production classpath. */
public final class IpfValueOracle {
  private static String hex(String value) {
    StringBuilder text = new StringBuilder();
    for (int i = 0; i < value.length(); i++) {
      String unit = Integer.toHexString(value.charAt(i));
      for (int padding = unit.length(); padding < 4; padding++) {
        text.append('0');
      }
      text.append(unit);
    }
    return text.toString();
  }

  private static String bits(double value) {
    return Long.toHexString(Double.doubleToLongBits(value));
  }

  private static void observe(PrintWriter out, String operation, String name, String input) {
    InstrumentProfile profile = new InstrumentProfile();
    String result;
    try {
      if (operation.equals("format")) {
        profile.setNumericField(name, Double.longBitsToDouble(Long.parseUnsignedLong(input, 16)));
        result = hex(profile.getField(name));
      } else if (operation.equals("date-format")) {
        profile.setDateField(name, Integer.parseInt(input));
        result = hex(profile.getField(name));
      } else {
        profile.setField(name, input);
        if (operation.equals("number")) {
          result = bits(profile.getNumericField(name));
        } else if (operation.equals("date")) {
          result = Integer.toString(profile.getDateField(name));
        } else {
          result = hex(profile.getField(name));
        }
      }
    } catch (IllegalArgumentException e) {
      result = "ERROR";
    }
    out.println(
        operation
            + "\t"
            + hex(name)
            + "\t"
            + (operation.endsWith("format") ? input : hex(input))
            + "\t"
            + result);
  }

  /** Write observations; takes one output filename and starts no network work. */
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output TSV filename");
    }
    try (PrintWriter out = new PrintWriter(new File(args[0]), "UTF-8")) {
      String[] numbers = {
        "",
        "0",
        "-0",
        " 1.25 ",
        " ",
        "+1",
        ".5",
        "5.",
        "1e23",
        "NaN",
        "-NaN",
        "Infinity",
        "+Infinity",
        "-Infinity",
        "nan",
        "inf",
        "0x1.8p2",
        "-0X1.fp-3",
        "0x1",
        "1f",
        "1d",
        "1_0",
        "1,000",
        "1e999999",
        "1e-999999",
        "-2147483649",
        "2147483648",
        "2.9",
        "1.234567890123456789",
        "-0.0",
        "\u0000\t12\r\n",
        "\uff11",
        "1e",
        ".",
        "--1",
        "1ff"
      };
      for (String value : numbers) {
        for (String name :
            new String[] {"ICB", "SIC", "MULTIPLIER", "SPC", "STRIKE", "custom", "DESCRIPTION"}) {
          observe(out, "text", name, value);
          observe(out, "number", name, value);
          observe(out, "date", name, value);
        }
      }
      String[] dates = {
        "",
        "1970-01-01",
        "1970-01-02",
        "1969-12-31",
        "2024-02-30",
        "2024-13-01",
        "2024-0-0",
        "2024-1-2",
        "2024-01-02junk",
        "2024-01-02T23:00:00-09:00",
        " 2024-01-02",
        "2024/01/02",
        "not-date",
        "0000-01-01",
        "0001-01-01",
        "-0001-01-01",
        "1582-10-04",
        "1582-10-10",
        "1582-10-15",
        "99999-01-01",
        "2024--1-01",
        "2024-01--1",
        "2024- 1- 2",
        "+2024-01-01",
        "\u0662\u0660\u0662\u0664-\u0660\u0661-\u0660\u0662",
        "\uff12\uff10\uff12\uff14-\uff10\uff11-\uff10\uff12",
        "2147483647-01-01",
        "2147483648-01-01",
        "999999999999999999-01-01"
      };
      for (String value : dates) {
        for (String name : new String[] {"EXPIRATION", "LAST_TRADE", "custom"}) {
          observe(out, "text", name, value);
          observe(out, "date", name, value);
          observe(out, "number", name, value);
        }
      }
      double[] special = {
        0,
        -0.0,
        1e-9,
        Math.nextUp(1e-9),
        1e-10,
        1e12,
        Math.nextDown(1e12),
        1e23,
        Double.NaN,
        Double.POSITIVE_INFINITY,
        Double.NEGATIVE_INFINITY,
        Double.MIN_VALUE,
        Double.MAX_VALUE,
        0.12345678901234568,
        2147483648.5,
        9223372036854775808.0
      };
      for (double value : special) {
        for (String name : new String[] {"STRIKE", "ICB", "EXPIRATION", "custom"}) {
          observe(out, "format", name, bits(value));
        }
      }
      for (String value :
          new String[] {"1583-01--100", "1582-09-40", "1582-01-365", "1581-12-400"}) {
        observe(out, "date", "EXPIRATION", value);
      }
      observe(out, "format", "STRIKE", "3ee9e4093026776e");
      Random random = new Random(208);
      for (int index = 0; index < 16384; index++) {
        observe(out, "format", "STRIKE", bits(Double.longBitsToDouble(random.nextLong())));
      }
      for (int exponent = -324; exponent <= 309; exponent++) {
        double power = Math.pow(10, exponent);
        for (double value :
            new double[] {power, Math.nextDown(power), Math.nextUp(power), -power}) {
          observe(out, "format", "STRIKE", bits(value));
        }
      }
      for (long raw = 1; raw <= 64; raw++) {
        observe(out, "format", "STRIKE", Long.toHexString(raw));
      }
      for (int exponent = 0; exponent < 2047; exponent++) {
        observe(
            out, "format", "STRIKE", Long.toHexString(((long) exponent << 52) | 0x123456789abcdL));
      }
      for (int start :
          new int[] {
            0x30, 0x660, 0x6f0, 0x7c0, 0x966, 0x9e6, 0xa66, 0xae6, 0xb66, 0xbe6, 0xc66, 0xce6,
            0xd66, 0xde6, 0xe50, 0xed0, 0xf20, 0x1040, 0x1090, 0x17e0, 0x1810, 0x1946, 0x19d0,
            0x1a80, 0x1a90, 0x1b50, 0x1bb0, 0x1c40, 0x1c50, 0xa620, 0xa8d0, 0xa900, 0xa9d0, 0xa9f0,
            0xaa50, 0xabf0, 0xff10
          }) {
        String unicode =
            new String(
                new char[] {
                  (char) (start + 2),
                  (char) start,
                  (char) (start + 2),
                  (char) (start + 4),
                  '-',
                  (char) start,
                  (char) (start + 1),
                  '-',
                  (char) start,
                  (char) (start + 2)
                });
        observe(out, "date", "EXPIRATION", unicode);
      }
      String[] oddDates = {
        "2024-01-01.5",
        "2024.5-01-01",
        "2024-01-01,000",
        "2024-01-01-2",
        "2024-\t01-\t02",
        "\n2024-01-01",
        "2024-2147483647-01",
        "2024-2147483648-01",
        "2024-01-2147483647",
        "2024-01-2147483648",
        "9223372036854775807-01-01",
        "9223372036854775808-01-01",
        "-9223372036854775808-01-01",
        "-9999999999999999999-01-01"
      };
      for (String value : oddDates) {
        observe(out, "date", "EXPIRATION", value);
      }
      for (int index = 0; index < 512; index++) {
        String value =
            (random.nextInt(10000) - 5000)
                + "-"
                + (random.nextInt(60) - 20)
                + "-"
                + (random.nextInt(100) - 20);
        observe(out, "date", "EXPIRATION", value);
      }
      for (int day :
          new int[] {
            0, 1, -1, Integer.MIN_VALUE, Integer.MAX_VALUE, -141428, -141427, -719530, -719529
          }) {
        for (String name : new String[] {"EXPIRATION", "custom", "STRIKE"}) {
          observe(out, "date-format", name, Integer.toString(day));
        }
      }
      for (InstrumentProfileField field : InstrumentProfileField.values()) {
        observe(out, "text", field.name(), "");
      }
      if (out.checkError()) {
        throw new IllegalStateException("Cannot write oracle observations");
      }
    }
  }
}
