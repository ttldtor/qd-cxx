// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.event.market.MarketEventSymbols;
import java.io.File;
import java.io.PrintWriter;

/** Observes exact UTF-16/null market-symbol operations without normalizing malformed inputs. */
public final class MarketSymbolOracle {
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
      PrintWriter out, String operation, String symbol, String argument, String result) {
    out.println(operation + "\t" + hex(symbol) + "\t" + hex(argument) + "\t" + result);
  }

  /**
   * Write one TSV file; caller supplies the sole output path, errors propagate with nonzero exit.
   */
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output TSV filename");
    }
    String[] symbols = {
      null,
      "",
      "SPY",
      "SPY&Q",
      "&Q",
      "&",
      "SPY&&",
      "SPY&QQ",
      "SPY&?",
      "SPY&\u0000",
      "SPY&\ud800",
      "SPY&\udc00",
      "SPY&\ud83d\ude00",
      "\ud83d\ude00&Q",
      "\u0000&Q",
      "SPY&Q{price=bid}",
      "SPY&Q{}",
      "SPY&Q{x}",
      "SPY&Q{=}",
      "SPY&Q{{}",
      "SPY&Q{a{b}",
      "SPY&Q{a}tail",
      "SPY&Q{a",
      "SPY&Q}",
      "{}",
      "{x}",
      "{a}{}",
      "{a{b}",
      "{a}}",
      "SPY&Q{a}{b}",
      "SPY&Q{z=2,a=1,a=0}",
      "SPY&Q{\ud800}",
      "SPY&Q{\u0000}",
      "SPY&Q{a\u0000b}",
      "SPY&\u00e9"
    };
    char[] codes = {
      0, 'Q', '0', 'z', '?', '&', '{', '}', '\u0001', '\u007f', '\u00e9', '\ud800', '\udc00'
    };
    String[] bases = {null, "", "IBM", "IBM&N", "IBM{a=1}", "\ud800", "\ud83d\ude00", "a\u0000b"};
    try (PrintWriter out = new PrintWriter(new File(args[0]), "UTF-8")) {
      row(out, "supported", null, null, hex(MarketEventSymbols.SUPPORTED_EXCHANGES));
      row(out, "default", null, null, hex(MarketEventSymbols.DEFAULT_EXCHANGES));
      for (String symbol : symbols) {
        row(out, "has", symbol, null, Boolean.toString(MarketEventSymbols.hasExchangeCode(symbol)));
        row(
            out,
            "code",
            symbol,
            null,
            hex(String.valueOf(MarketEventSymbols.getExchangeCode(symbol))));
        row(out, "base", symbol, null, hex(MarketEventSymbols.getBaseSymbol(symbol)));
        for (char code : codes) {
          row(
              out,
              "exchange",
              symbol,
              String.valueOf(code),
              hex(MarketEventSymbols.changeExchangeCode(symbol, code)));
        }
        for (String base : bases) {
          row(out, "replace", symbol, base, hex(MarketEventSymbols.changeBaseSymbol(symbol, base)));
        }
      }
      for (int code = 0; code <= Character.MAX_VALUE; code++) {
        String symbol = "A&" + (char) code;
        row(out, "unit", symbol, null, hex(MarketEventSymbols.getBaseSymbol(symbol)));
      }
      if (out.checkError()) {
        throw new IllegalStateException("Output failed");
      }
    }
  }
}
