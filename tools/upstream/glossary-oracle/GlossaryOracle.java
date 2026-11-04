// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.glossary.AdditionalUnderlyings;
import com.dxfeed.glossary.CFI;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.TreeMap;

/**
 * Invoked by Invoke-Oracle.ps1 with an output TSV; all observations call production glossary APIs.
 */
public final class GlossaryOracle {
  private GlossaryOracle() {}

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

  private static void row(BufferedWriter out, Object... cells) throws Exception {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) out.write('\t');
      out.write(cells[i].toString());
    }
    out.write('\n');
  }

  private static String values(CFI.Attribute attribute) {
    StringBuilder out = new StringBuilder();
    for (CFI.Value value : attribute.getValues()) {
      if (out.length() != 0) out.append('|');
      out.append(value.getCode())
          .append(',')
          .append(hex(value.getName()))
          .append(',')
          .append(hex(value.getDescription()));
    }
    return out.toString();
  }

  private static String projection(CFI cfi) {
    StringBuilder out =
        new StringBuilder(cfi.getCode())
            .append('|')
            .append(cfi.getIntCode())
            .append('|')
            .append(cfi.isEquity())
            .append(',')
            .append(cfi.isDebtInstrument())
            .append(',')
            .append(cfi.isEntitlement())
            .append(',')
            .append(cfi.isOption())
            .append(',')
            .append(cfi.isFuture())
            .append(',')
            .append(cfi.isOther())
            .append('|')
            .append(hex(cfi.describe()));
    for (CFI.Value value : cfi.decipher()) {
      out.append('|')
          .append(value.getCode())
          .append(',')
          .append(hex(value.getName()))
          .append(',')
          .append(hex(value.getDescription()))
          .append(',')
          .append(hex(value.getAttribute().getName()))
          .append(',')
          .append(hex(value.getAttribute().getDescription()));
    }
    return out.toString();
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) throw new IllegalArgumentException("output TSV required");
    try (BufferedWriter out = Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8)) {
      Set<String> codes = new LinkedHashSet<>();
      Map<CFI.Attribute, Boolean> seen = new IdentityHashMap<>();
      for (char category = 'A'; category <= 'Z'; ++category) {
        for (char group = 'A'; group <= 'Z'; ++group) {
          String base = "" + category + group + "XXXX";
          codes.add(base);
          CFI.Value[] decoded = CFI.valueOf(base).decipher();
          for (int pos = 0; pos < 6; ++pos) {
            CFI.Attribute attribute = decoded[pos].getAttribute();
            if (seen.put(attribute, true) == null)
              row(
                  out,
                  "D",
                  base,
                  pos,
                  hex(attribute.getName()),
                  hex(attribute.getDescription()),
                  values(attribute));
            for (CFI.Value value : attribute.getValues()) {
              char[] chars = base.toCharArray();
              chars[pos] = value.getCode();
              codes.add(new String(chars));
            }
          }
        }
      }
      Collections.addAll(
          codes, "", "X", "XX", "XXX", "XXXX", "XXXXX", "XXXXXX", "OC", "OPASPS", "ESVUFR", "abc",
          "ABCDEFG", "A\u0000", "\ud800", "\uff21");
      for (String code : codes) {
        try {
          row(out, "C", hex(code), projection(CFI.valueOf(code)));
        } catch (IllegalArgumentException ex) {
          row(out, "C", hex(code), "!" + ex.getMessage());
        }
      }
      row(out, "C", "~", projection(CFI.valueOf((String) null)));
      int[] ints = {
        0,
        1,
        -1,
        Integer.MIN_VALUE,
        Integer.MAX_VALUE,
        1 << 30,
        831283992,
        CFI.valueOf("OP").getIntCode()
      };
      for (int code : ints) {
        try {
          row(out, "I", code, projection(CFI.valueOf(code)));
        } catch (IllegalArgumentException ex) {
          row(out, "I", code, "!" + ex.getMessage());
        }
      }
      String[] texts = {
        null,
        "",
        "A 1",
        "B 2; A 1",
        "A 1; A 2",
        "; A 1",
        "A 1;",
        "A 1;B 2",
        "A 1;  B 2",
        " A 1",
        "A 1 ",
        "A\tB 2",
        "A;B 1",
        "A 0",
        "A -0",
        "A +0",
        "A .5",
        "A 1.",
        "A .",
        "A +",
        "A NaN",
        "A Infinity",
        "A 1e309",
        "A -1e309",
        "A 1e-309",
        "A -1e-309",
        "A 0e309",
        "A 1e",
        "A 1e+",
        "A 1e-",
        "A 1e2147483647",
        "A 1e2147483648",
        "A 1e4294967296",
        "A 1e-2147483648",
        "A 1e+000",
        "A 1e--1",
        "A 1.2.3",
        "A 0x1p0",
        "A 1f",
        "A 1\n",
        "\ud800 2",
        "\udc00 -0",
        "\u0000 3",
        "\u00e9 1",
        "A 123456789012345678901234567890.012345678901234567890",
        "A 1; B nope; A 3",
        "bad A 4",
        "A 1; junk; A 3"
      };
      for (String text : texts) {
        try {
          AdditionalUnderlyings value = AdditionalUnderlyings.valueOf(text);
          StringBuilder map = new StringBuilder();
          for (Map.Entry<String, Double> entry : new TreeMap<>(value.getMap()).entrySet()) {
            if (map.length() != 0) map.append('|');
            map.append(hex(entry.getKey())).append(',').append(bits(entry.getValue()));
          }
          row(out, "A", hex(text), hex(value.getText()), map.length() == 0 ? "-" : map);
        } catch (IllegalArgumentException ex) {
          row(out, "A", hex(text), "!" + ex.getMessage(), "-");
        }
        for (String symbol : new String[] {null, "", "A", "B", "junk", "\ud800", "\u0000"})
          row(out, "Q", hex(text), hex(symbol), bits(AdditionalUnderlyings.getSPC(text, symbol)));
      }
      for (int subnormal = 1; subnormal <= 128; ++subnormal) {
        for (long raw : new long[] {subnormal, Long.MIN_VALUE | subnormal}) {
          double number = Double.longBitsToDouble(raw);
          AdditionalUnderlyings value =
              AdditionalUnderlyings.valueOf(Collections.singletonMap("A", number));
          row(out, "M", Long.toHexString(raw), hex(value.getText()), bits(value.getSPC("A")));
        }
      }
      for (int power = -323; power <= 308; ++power) {
        double base = Double.parseDouble("1e" + power);
        for (double number :
            new double[] {
              Math.nextDown(base),
              base,
              Math.nextUp(base),
              -Math.nextDown(base),
              -base,
              -Math.nextUp(base)
            }) {
          if (!Double.isFinite(number)) continue;
          AdditionalUnderlyings value =
              AdditionalUnderlyings.valueOf(Collections.singletonMap("A", number));
          row(out, "M", bits(number), hex(value.getText()), bits(value.getSPC("A")));
        }
      }
      Random random = new Random(219);
      long[] boundaries = {
        0,
        Long.MIN_VALUE,
        1,
        Long.MIN_VALUE | 1,
        0x000fffffffffffffL,
        0x0010000000000000L,
        0x0010000000000001L,
        0x800fffffffffffffL,
        0x8010000000000000L,
        0x8010000000000001L,
        0x7fefffffffffffffL,
        0x7ff0000000000000L,
        0xfff0000000000000L,
        0x7ff8000000000000L
      };
      for (int i = 0; i < 8206; ++i) {
        long raw = i < boundaries.length ? boundaries[i] : random.nextLong();
        double number = Double.longBitsToDouble(raw);
        try {
          AdditionalUnderlyings value =
              AdditionalUnderlyings.valueOf(Collections.singletonMap("A", number));
          row(out, "M", Long.toHexString(raw), hex(value.getText()), bits(value.getSPC("A")));
        } catch (IllegalArgumentException ex) {
          row(out, "M", Long.toHexString(raw), "!" + ex.getMessage(), "-");
        }
      }
      for (double number :
          new double[] {
            1e-9,
            Math.nextDown(1e-9),
            Math.nextUp(1e-9),
            1e10,
            Math.nextDown(1e10),
            Math.nextUp(1e10),
            1.234567890123445,
            1.234567890123455,
            9.999999999999995,
            1e23
          }) {
        AdditionalUnderlyings value =
            AdditionalUnderlyings.valueOf(Collections.singletonMap("A", number));
        row(out, "M", bits(number), hex(value.getText()), bits(value.getSPC("A")));
      }
      // Exercise open-interval shortest-digit selection before precision rounding.
      long[] precisionSeeds = {0x4351b77fdd2f78d4L, 0xc35ebe0663df0964L};
      for (long seed : precisionSeeds) {
        for (int adjacent = -8; adjacent <= 8; ++adjacent) {
          double number = Double.longBitsToDouble(seed + adjacent);
          AdditionalUnderlyings value =
              AdditionalUnderlyings.valueOf(Collections.singletonMap("A", number));
          row(out, "M", bits(number), hex(value.getText()), bits(value.getSPC("A")));
        }
      }
      for (int exponent = 15; exponent <= 18; ++exponent) {
        double step = Math.pow(10, exponent - 14);
        for (int sample = 0; sample < 64; ++sample) {
          double midpoint = (100000000000000L + sample * 13743769123456L + 0.5) * step;
          long center = Double.doubleToRawLongBits(midpoint);
          for (int adjacent = -2; adjacent <= 2; ++adjacent) {
            for (long sign : new long[] {0, Long.MIN_VALUE}) {
              double number = Double.longBitsToDouble((center + adjacent) | sign);
              AdditionalUnderlyings value =
                  AdditionalUnderlyings.valueOf(Collections.singletonMap("A", number));
              row(out, "M", bits(number), hex(value.getText()), bits(value.getSPC("A")));
            }
          }
        }
      }
    }
  }
}
