// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

package qdcxx.oracle;

import com.dxfeed.event.IndexedEventSource;
import com.dxfeed.event.market.OrderSource;
import java.io.File;
import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.Comparator;

/** Captures production source identities; invoked by Invoke-Oracle.ps1 with one output path. */
public final class EventSourceOracle {
  private static String hex(String value) {
    if (value == null) {
      return "NULL";
    }
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < value.length(); i++) {
      result.append(String.format("%04x", (int) value.charAt(i)));
    }
    return result.toString();
  }

  private static void row(
      PrintWriter out, String operation, String input, IndexedEventSource value) {
    out.println(
        operation
            + "\t"
            + input
            + "\t"
            + value.id()
            + "\t"
            + hex(value.name())
            + "\t"
            + value.hashCode());
  }

  private static void name(PrintWriter out, String input) {
    try {
      row(out, "NAME", hex(input), OrderSource.valueOf(input));
    } catch (RuntimeException error) {
      out.println("NAME\t" + hex(input) + "\t" + error.getClass().getSimpleName());
    }
  }

  private static void id(PrintWriter out, int input) {
    try {
      row(out, "ID", Integer.toString(input), OrderSource.valueOf(input));
    } catch (RuntimeException error) {
      out.println("ID\t" + input + "\t" + error.getClass().getSimpleName());
    }
  }

  /** Writes observations to the sole requested file; input/I/O/reflection failures exit nonzero. */
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output TSV filename");
    }
    try (PrintWriter out = new PrintWriter(new File(args[0]), "UTF-8")) {
      Field[] fields = OrderSource.class.getFields();
      Arrays.sort(fields, Comparator.comparing(Field::getName));
      for (Field field : fields) {
        if (field.getType() == OrderSource.class && Modifier.isStatic(field.getModifiers())) {
          OrderSource source = (OrderSource) field.get(null);
          row(out, "BUILTIN", field.getName(), source);
          name(out, source.name());
          id(out, source.id());
        }
      }
      String[] names = {
        null,
        "",
        "A",
        "0",
        "a",
        "aZ09",
        "ZZZZ",
        "1234",
        "zzzz",
        "NTV",
        "ntv",
        "TEST",
        "Ab12",
        "ABCDE",
        "default",
        "COMPOSITE_BID",
        "COMPOSITE_bid",
        "A_B",
        "A B",
        "\u0000",
        "A\u0000",
        "\u00e9",
        "\ud800",
        "\udc00",
        "\ud83d\ude00"
      };
      for (String input : names) {
        name(out, input);
      }
      int[] ids = {
        Integer.MIN_VALUE,
        -1,
        -256,
        0,
        1,
        9,
        10,
        31,
        32,
        47,
        48,
        57,
        58,
        64,
        65,
        90,
        91,
        96,
        97,
        122,
        123,
        127,
        128,
        255,
        256,
        0x4130,
        0x410041,
        0x304100,
        0x41414141,
        0x7a7a7a7a,
        0x7fffffff,
        0x80414141
      };
      for (int input : ids) {
        id(out, input);
      }
      String[] baseNames = {null, "", "DEFAULT", "different", "\u0000\ud800\udc00"};
      int[] baseIds = {Integer.MIN_VALUE, -1, 0, 1, 0x4e5456, Integer.MAX_VALUE};
      for (int input : baseIds) {
        for (String baseName : baseNames) {
          IndexedEventSource base = new IndexedEventSource(input, baseName);
          row(out, "BASE", hex(baseName), base);
          IndexedEventSource duplicate = new IndexedEventSource(input, "another");
          out.println(
              "EQUAL\t"
                  + input
                  + "\t"
                  + base.equals(duplicate)
                  + "\t"
                  + duplicate.equals(base)
                  + "\t"
                  + (base.hashCode() == duplicate.hashCode()));
        }
      }
      IndexedEventSource ntv = new IndexedEventSource(OrderSource.NTV.id(), null);
      out.println(
          "CROSS\t"
              + ntv.equals(OrderSource.NTV)
              + "\t"
              + OrderSource.NTV.equals(ntv)
              + "\t"
              + (ntv.hashCode() == OrderSource.NTV.hashCode()));
      out.println(
          "UNEQUAL\t"
              + new IndexedEventSource(-1, "same").equals(new IndexedEventSource(1, "same")));
      if (out.checkError()) {
        throw new java.io.IOException("Observation output failed");
      }
    }
  }
}
