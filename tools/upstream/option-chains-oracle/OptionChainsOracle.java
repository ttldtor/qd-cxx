// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.ipf.InstrumentProfile;
import com.dxfeed.ipf.option.OptionChain;
import com.dxfeed.ipf.option.OptionChainsBuilder;
import com.dxfeed.ipf.option.OptionSeries;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;

/**
 * Invoked by Invoke-Oracle.ps1 with input TSV and output TSV; delegates grouping/selection to QD.
 */
public final class OptionChainsOracle {
  private OptionChainsOracle() {}

  private static String hex(String text) {
    if (text.isEmpty()) {
      return "-";
    }
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < text.length(); i++) {
      result.append(String.format(Locale.ROOT, "%04x", (int) text.charAt(i)));
    }
    return result.toString();
  }

  private static String unhex(String text) {
    if (text.equals("-")) {
      return "";
    }
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < text.length(); i += 4) {
      result.append((char) Integer.parseInt(text.substring(i, i + 4), 16));
    }
    return result.toString();
  }

  private static String bits(double value) {
    return String.format(Locale.ROOT, "%016x", Double.doubleToLongBits(value));
  }

  private static String strikes(List<Double> values) {
    List<String> result = new ArrayList<>();
    for (double value : values) {
      result.add(bits(value));
    }
    return result.isEmpty() ? "-" : String.join(",", result);
  }

  private static void row(BufferedWriter out, Object... values) throws Exception {
    for (int i = 0; i < values.length; i++) {
      if (i != 0) {
        out.write('\t');
      }
      out.write(values[i].toString());
    }
    out.write('\n');
  }

  private static void emit(BufferedWriter out, String name, List<InstrumentProfile> profiles)
      throws Exception {
    Map<String, OptionChain<InstrumentProfile>> chains =
        new TreeMap<>(OptionChainsBuilder.build(profiles).getChains());
    row(out, "CASE", name, chains.size());
    for (OptionChain<InstrumentProfile> chain : chains.values()) {
      String symbol = hex(chain.getSymbol());
      row(out, "CHAIN", name, symbol, chain.getSeries().size());
      int index = 0;
      for (OptionSeries<InstrumentProfile> series : chain.getSeries()) {
        row(
            out,
            "SERIES",
            name,
            symbol,
            index,
            series.getExpiration(),
            series.getLastTrade(),
            bits(series.getMultiplier()),
            bits(series.getSPC()),
            hex(series.getAdditionalUnderlyings()),
            hex(series.getMMY()),
            hex(series.getOptionType()),
            hex(series.getExpirationStyle()),
            hex(series.getSettlementStyle()),
            hex(series.getCFI()));
        for (Map.Entry<Double, InstrumentProfile> call : series.getCalls().entrySet()) {
          row(
              out,
              "CALL",
              name,
              symbol,
              index,
              bits(call.getKey()),
              hex(call.getValue().getSymbol()));
        }
        for (Map.Entry<Double, InstrumentProfile> put : series.getPuts().entrySet()) {
          row(out, "PUT", name, symbol, index, bits(put.getKey()), hex(put.getValue().getSymbol()));
        }
        row(out, "STRIKES", name, symbol, index, strikes(series.getStrikes()));
        for (int count : new int[] {0, 1, 2, 3, 20}) {
          for (double center :
              new double[] {
                Double.NEGATIVE_INFINITY, -0.0, 0.0, 12, 100, Double.POSITIVE_INFINITY, Double.NaN
              }) {
            row(
                out,
                "AROUND",
                name,
                symbol,
                index,
                count,
                bits(center),
                strikes(series.getNStrikesAround(count, center)));
          }
        }
        try {
          series.getNStrikesAround(-1, 0);
          throw new AssertionError("Negative count unexpectedly accepted");
        } catch (IllegalArgumentException expected) {
          row(out, "NEGATIVE", name, symbol, index, "IllegalArgumentException");
        }
        index++;
      }
    }
  }

  /**
   * Read hex UTF-16 input fields, invoke production grouping and write deterministic observations.
   */
  public static void main(String[] args) throws Exception {
    if (args.length != 2) {
      throw new IllegalArgumentException("Expected input TSV and output TSV paths");
    }
    List<String> lines = Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8);
    String[] fields = lines.get(0).split("\t", -1);
    Map<String, List<InstrumentProfile>> cases = new LinkedHashMap<>();
    cases.put("empty", new ArrayList<InstrumentProfile>());
    for (int i = 1; i < lines.size(); i++) {
      String[] cells = lines.get(i).split("\t", -1);
      if (cells.length != fields.length) {
        throw new IllegalArgumentException("Wrong field count on line " + (i + 1));
      }
      InstrumentProfile profile = new InstrumentProfile();
      for (int j = 1; j < fields.length; j++) {
        profile.setField(fields[j], unhex(cells[j]));
      }
      cases.computeIfAbsent(cells[0], unused -> new ArrayList<>()).add(profile);
    }
    Path output = Paths.get(args[1]);
    try (BufferedWriter out = Files.newBufferedWriter(output, StandardCharsets.UTF_8)) {
      for (Map.Entry<String, List<InstrumentProfile>> entry : cases.entrySet()) {
        emit(out, entry.getKey(), entry.getValue());
      }
    }
  }
}
