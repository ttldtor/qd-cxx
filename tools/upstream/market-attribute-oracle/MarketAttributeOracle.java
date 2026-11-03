// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.event.market.MarketEventSymbols;
import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/** Calls production attribute helpers; inputs and results preserve individual UTF-16 units. */
public final class MarketAttributeOracle {
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

  private static void row(PrintWriter out, String op, String symbol, String key, String value) {
    String result;
    try {
      if (op.equals("has")) {
        result = Boolean.toString(MarketEventSymbols.hasAttributes(symbol));
      } else if (op.equals("get")) {
        result = hex(MarketEventSymbols.getAttributeStringByKey(symbol, key));
      } else if (op.equals("remove")) {
        result = hex(MarketEventSymbols.removeAttributeStringByKey(symbol, key));
      } else {
        result = hex(MarketEventSymbols.changeAttributeStringByKey(symbol, key, value));
      }
    } catch (RuntimeException exception) {
      result = "THROW:" + exception.getClass().getSimpleName();
    }
    out.println(op + "\t" + hex(symbol) + "\t" + hex(key) + "\t" + hex(value) + "\t" + result);
  }

  /** Caller supplies a sole TSV output filename; process errors propagate as a nonzero exit. */
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output TSV filename");
    }
    String[] keys = {
      null, "", "a", "b", "z", "=", ",", "}", "{", "a=b", "a,b", "\u0000", "\ud800", "\udc00",
      "\uffff"
    };
    String[] values = {null, "", "x", "a=b", "a,b", "{", "}", "\u0000", "\ud800", "\udc00"};
    List<String> symbols = new ArrayList<>();
    String[] explicit = {
      null,
      "",
      "SPY",
      "SPY&Q",
      "{}",
      "S&Q{}",
      "S&Q{a}",
      "S&Q{=}",
      "S&Q{a=}",
      "S&Q{a=1,a=2,a=3}",
      "S&Q{z=2,a=1,b=3}",
      "S&Q{a=1,z=2,a=3}",
      "S&Q{a=1,}",
      "S&Q{,a=1}",
      "S&Q{a=1,,b=2}",
      "S&Q{x,a=1}",
      "S&Q{a=1,x}",
      "S&Q{a==1}",
      "S&Q{a=1}{b=2}",
      "S&Q{{a=1}",
      "S&Q{a={b=2}",
      "S&Q{a=1}}",
      "S&Q{a=1}tail",
      "S&Q{a=1",
      "S&Q{\udc00=low,\ud800=high,\uffff=end}",
      "S&Q{\u0000=nul}"
    };
    for (String symbol : explicit) {
      symbols.add(symbol);
    }
    // Exhaustive short malformed blocks exercise separators, missing values and braces.
    char[] alphabet = {'a', '=', ',', '{', '}'};
    for (int length = 1; length <= 4; length++) {
      int count = (int) Math.pow(alphabet.length, length);
      for (int index = 0; index < count; index++) {
        int digits = index;
        StringBuilder block = new StringBuilder("S&Q{");
        for (int unit = 0; unit < length; unit++) {
          block.append(alphabet[digits % alphabet.length]);
          digits /= alphabet.length;
        }
        symbols.add(block.append('}').toString());
      }
    }
    try (PrintWriter out = new PrintWriter(new File(args[0]), "UTF-8")) {
      for (String symbol : symbols) {
        row(out, "has", symbol, null, null);
        for (String key : keys) {
          row(out, "get", symbol, key, null);
          row(out, "remove", symbol, key, null);
          for (String value : values) {
            row(out, "change", symbol, key, value);
          }
        }
      }
      // Cover unsigned lexical comparison for every code unit against ASCII keys.
      for (int unit = 0; unit <= Character.MAX_VALUE; unit++) {
        String key = String.valueOf((char) unit);
        row(out, "change", "S&Q{a=1,z=2}", key, "v");
        row(out, "get", "S&Q{" + key + "=v}", key, null);
      }
      if (out.checkError()) {
        throw new IllegalStateException("Output failed");
      }
    }
  }
}
