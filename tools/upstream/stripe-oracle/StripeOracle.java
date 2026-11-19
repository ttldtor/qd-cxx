// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.QDFilter;
import com.devexperts.qd.SymbolStriper;
import com.devexperts.qd.kit.CompositeFilters;
import com.devexperts.qd.kit.HashStriper;
import com.devexperts.qd.kit.RangeStriper;
import com.dxfeed.api.impl.DXFeedScheme;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.BitSet;
import java.util.List;

/**
 * Invoked by Invoke-Oracle.ps1 with one output path. Observes pinned production stripe factories,
 * selectors and filter intersections; writes exact UTF-16 TSV and propagates unexpected failures.
 */
public final class StripeOracle {
  private static String units(String value) {
    StringBuilder result = new StringBuilder();
    for (int index = 0; index < value.length(); index++) {
      if (index != 0) {
        result.append(',');
      }
      result.append(String.format(java.util.Locale.ROOT, "%04X", (int) value.charAt(index)));
    }
    return result.toString();
  }

  private static void row(Object... values) {
    for (int index = 0; index < values.length; index++) {
      if (index != 0) {
        System.out.print('\t');
      }
      System.out.print(values[index]);
    }
    System.out.println();
  }

  private static void grammar(DataScheme scheme) {
    for (String spec :
        new String[] {"", "by1", "byhash2", "byhash0004", "byhash16", "byrange-0-A-M-Z-a-z-"}) {
      SymbolStriper striper = SymbolStriper.definedValueOf(scheme, spec);
      row("striper", units(spec), striper.getName(), striper.getStripeCount());
      for (int index = 0; index < striper.getStripeCount(); index++) {
        row("stripe", striper.getName(), index, striper.getStripeFilter(index));
      }
    }
    for (String spec :
        new String[] {
          "byhash", "byhash0", "byhash1", "byhash3", "byhash-2", "byhash+2", "byhash2147483648",
          "byhash2 ", "BYHASH2", "byrange", "byrange-", "byrange--", "byrange-A--", "byrange-A-A-",
          "byrange-Z-A-", "byrange-123456789-", "byrange-A_B-", "byrange-\u00C0-", "other"
        }) {
      try {
        SymbolStriper.definedValueOf(scheme, spec);
        throw new AssertionError("Unexpected valid specification: " + spec);
      } catch (com.devexperts.qd.kit.FilterSyntaxException expected) {
        row("invalid", units(spec));
      }
    }
    row("factory", "hash-count-1", HashStriper.valueOf(scheme, 1).getName());
    row(
        "factory",
        "empty-ranges",
        RangeStriper.valueOf(scheme, java.util.Collections.emptyList()).getName());
  }

  private static void symbols(DataScheme scheme) {
    List<String> symbols = new ArrayList<>();
    symbols.addAll(
        Arrays.asList(
            "",
            "A",
            "AAPL",
            "IBM",
            "MSFT",
            "SPY",
            "aapl",
            "123",
            "*",
            "A\u0000B",
            "\u0000",
            "\u007F",
            "\u0080",
            "\uFFFF",
            "\uD800",
            "\uDC00",
            "\uD83D\uDE80",
            "A\u0080",
            "=2*AAPL",
            "=-2.5*IBM",
            "=+3.*MSFT",
            "=0*Z",
            "=1..2*A",
            "=+A",
            "=--A",
            "/ES",
            "./ABC",
            "A&Q",
            "A{=d}",
            "ABCDEFGH",
            "ABCDEFGH0",
            "ABCDEFGG\uFFFF",
            "ABCDEFGI",
            "[A]",
            "\u00C0M"));
    for (char value = 1; value < 128; value++) {
      symbols.add(String.valueOf(value));
    }
    for (String boundary : new String[] {"0", "A", "M", "Z", "a", "z", "ABCDEFGH"}) {
      symbols.add(boundary);
      symbols.add(boundary + "0");
      symbols.add(boundary + "\u0000");
      symbols.add("=2*" + boundary);
    }
    for (String spec :
        new String[] {
          "by1", "byhash2", "byhash4", "byhash16", "byrange-0-A-M-Z-a-z-", "byrange-ABCDEFGH-"
        }) {
      SymbolStriper striper = SymbolStriper.definedValueOf(scheme, spec);
      for (String symbol : symbols) {
        int cipher = scheme.getCodec().encode(symbol);
        int index = striper.getStripeIndex(symbol);
        int pairIndex = striper.getStripeIndex(cipher, symbol);
        int accepted = 0;
        int acceptedIndex = -1;
        for (int stripe = 0; stripe < striper.getStripeCount(); stripe++) {
          if (striper.getStripeFilter(stripe).accept(QDContract.STREAM, null, cipher, symbol)) {
            accepted++;
            acceptedIndex = stripe;
          }
        }
        row("symbol", spec, units(symbol), cipher, index, pairIndex, accepted, acceptedIndex);
      }
    }
  }

  private static void intersections(DataScheme scheme) {
    for (String spec :
        new String[] {"by1", "byhash4", "byhash16", "byrange-D-M-", "byrange-A-AB-ABCDEFGH-Z-"}) {
      SymbolStriper striper = SymbolStriper.definedValueOf(scheme, spec);
      for (String expression :
          new String[] {
            "*",
            "hash0of2",
            "hash1of2",
            "hash1of4",
            "hash03of16",
            "range---",
            "range--D-",
            "range-D-M-",
            "range-M--",
            "range-E-L-",
            "range-AB-ABCDEFGH-",
            "A*",
            "A,B"
          }) {
        QDFilter filter = CompositeFilters.valueOf(expression, scheme);
        BitSet stripes = striper.getIntersectingStripes(filter);
        row(
            "intersection",
            spec,
            expression,
            filter.toString(),
            stripes == null ? "unknown" : stripes.toString());
      }
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Invoke through Invoke-Oracle.ps1 with one output path");
    }
    try (PrintStream output = new PrintStream(args[0], "UTF-8")) {
      System.setOut(output);
      DataScheme scheme = DXFeedScheme.getInstance();
      grammar(scheme);
      symbols(scheme);
      intersections(scheme);
    }
  }
}
