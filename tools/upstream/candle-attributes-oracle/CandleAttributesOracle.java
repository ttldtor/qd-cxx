// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.event.candle.CandleAlignment;
import com.dxfeed.event.candle.CandleDataType;
import com.dxfeed.event.candle.CandleExchange;
import com.dxfeed.event.candle.CandlePrice;
import com.dxfeed.event.candle.CandlePriceLevel;
import com.dxfeed.event.candle.CandleSession;
import java.io.BufferedWriter;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.util.Random;

/** Finite offline observations of unmodified production Candle attribute classes. */
public final class CandleAttributesOracle {
  private CandleAttributesOracle() {}

  private static String hex(String text) {
    if (text == null) {
      return "NULL";
    }
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < text.length(); ++i) {
      String unit = Integer.toHexString(text.charAt(i));
      for (int j = unit.length(); j < 4; ++j) {
        result.append('0');
      }
      result.append(unit);
    }
    return result.toString();
  }

  private static String level(CandlePriceLevel value) {
    return Long.toHexString(Double.doubleToLongBits(value.getValue()))
        + ":"
        + hex(value.toString());
  }

  private static String observe(String kind, String operation, String input, String operand) {
    if (kind.equals("price")) {
      if (operation.equals("parse")) {
        return hex(CandlePrice.parse(input).toString());
      }
      if (operation.equals("get")) {
        return hex(CandlePrice.getAttributeForSymbol(input).toString());
      }
      if (operation.equals("normalize")) {
        return hex(CandlePrice.normalizeAttributeForSymbol(input));
      }
      return hex(CandlePrice.parse(operand).changeAttributeForSymbol(input));
    }
    if (kind.equals("session")) {
      if (operation.equals("parse")) {
        return hex(CandleSession.parse(input).toString());
      }
      if (operation.equals("get")) {
        return hex(CandleSession.getAttributeForSymbol(input).toString());
      }
      if (operation.equals("normalize")) {
        return hex(CandleSession.normalizeAttributeForSymbol(input));
      }
      return hex(CandleSession.parse(operand).changeAttributeForSymbol(input));
    }
    if (kind.equals("alignment")) {
      if (operation.equals("parse")) {
        return hex(CandleAlignment.parse(input).toString());
      }
      if (operation.equals("get")) {
        return hex(CandleAlignment.getAttributeForSymbol(input).toString());
      }
      if (operation.equals("normalize")) {
        return hex(CandleAlignment.normalizeAttributeForSymbol(input));
      }
      return hex(CandleAlignment.parse(operand).changeAttributeForSymbol(input));
    }
    if (kind.equals("level")) {
      if (operation.equals("parse")) {
        return level(CandlePriceLevel.parse(input));
      }
      if (operation.equals("value")) {
        return level(
            CandlePriceLevel.valueOf(Double.longBitsToDouble(Long.parseUnsignedLong(input, 16))));
      }
      if (operation.equals("get")) {
        return level(CandlePriceLevel.getAttributeForSymbol(input));
      }
      if (operation.equals("normalize")) {
        return hex(CandlePriceLevel.normalizeAttributeForSymbol(input));
      }
      return hex(CandlePriceLevel.parse(operand).changeAttributeForSymbol(input));
    }
    if (kind.equals("data")) {
      if (operation.equals("parse")) {
        return hex(CandleDataType.parse(input).toString());
      }
      if (operation.equals("get")) {
        return hex(CandleDataType.getAttributeForSymbol(input).toString());
      }
      if (operation.equals("normalize")) {
        return hex(CandleDataType.normalizeAttributeForSymbol(input));
      }
      return hex(CandleDataType.parse(operand).changeAttributeForSymbol(input));
    }
    if (operation.equals("get")) {
      CandleExchange value = CandleExchange.getAttributeForSymbol(input);
      return Integer.toHexString(value.getExchangeCode()) + ":" + hex(value.toString());
    }
    char code = (char) Integer.parseInt(operand, 16);
    CandleExchange value = CandleExchange.valueOf(code);
    if (operation.equals("value")) {
      return Integer.toHexString(code) + ":" + hex(value.toString());
    }
    return hex(value.changeAttributeForSymbol(input));
  }

  private static void row(Writer out, String kind, String operation, String input, String operand)
      throws Exception {
    String result;
    try {
      result = observe(kind, operation, input, operand);
    } catch (IllegalArgumentException | NullPointerException | IndexOutOfBoundsException error) {
      result = "ERROR";
    }
    out.write(
        kind + "\t" + operation + "\t" + hex(input) + "\t" + hex(operand) + "\t" + result + "\n");
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected one output TSV path");
    }
    try (Writer out =
        new BufferedWriter(
            new OutputStreamWriter(new FileOutputStream(args[0]), StandardCharsets.UTF_8))) {
      String[] names = {"price", "session", "alignment", "level", "data"};
      String[][] values = {
        {
          null,
          "",
          "last",
          "LAST",
          "l",
          "b",
          "a",
          "m",
          "s",
          "settlement",
          "laſt",
          "bıd",
          "BİD",
          " last",
          "last ",
          "unknown",
          "\ud800"
        },
        {
          null, "", "false", "true", "f", "t", "False", "TRUE", "any", "regular", "true ", "falſe",
          "unknown"
        },
        {null, "", "m", "M", "s", "S", "ſ", "midnight", "session", " m", "unknown"},
        {
          null,
          "",
          "NaN",
          "+NaN",
          "-NaN",
          "nan",
          "Infinity",
          "-Infinity",
          "-0",
          "-0.0",
          "-1",
          "0",
          "+0",
          "1",
          " 1.5 ",
          "\u00001\u0020",
          "0x1.8p1",
          "1f",
          "1D",
          "1e309",
          "1e-999",
          "-1e-999",
          "4.9e-324",
          "1e23",
          "9223372036854775808",
          "0.0009999999999999998",
          "1,2"
        },
        {
          null, "", "source", "tho", "all", "rth", "eth", "SOURCE", "unknown", " ", "x,y=z",
          "\ud800", "\u0000"
        }
      };
      String[] symbols = {
        null,
        "",
        "A",
        "A&Q",
        "A{}",
        "A{x=1}",
        "A{z=3,a=2}",
        "A{price=bid,price=ask}",
        "A{price}",
        "A{x=1,}",
        "A{,x=1}",
        "A{=v}",
        "A{price=bid}tail"
      };
      for (int k = 0; k < names.length; ++k) {
        for (String value : values[k]) {
          row(out, names[k], "parse", value, "");
          String key = new String[] {"price", "tho", "a", "pl", "dt"}[k];
          String symbol = "A&Q{" + key + "=" + value + ",x=1}";
          row(out, names[k], "get", symbol, "");
          row(out, names[k], "normalize", symbol, "");
          for (String original : symbols) {
            row(out, names[k], "change", original, value);
          }
        }
        for (String symbol : symbols) {
          row(out, names[k], "get", symbol, "");
          row(out, names[k], "normalize", symbol, "");
        }
      }
      for (int unit = 0; unit <= 0xffff; ++unit) {
        String text = String.valueOf((char) unit);
        row(out, "price", "parse", text, "");
        row(out, "session", "parse", text, "");
        row(out, "alignment", "parse", text, "");
        row(out, "exchange", "value", "", Integer.toHexString(unit));
      }
      for (String symbol : symbols) {
        row(out, "exchange", "get", symbol, "");
        for (String code : new String[] {"0", "26", "41", "7b", "d800", "ffff"}) {
          row(out, "exchange", "change", symbol, code);
        }
      }
      long[] edges = {
        0,
        Long.MIN_VALUE,
        1,
        Long.MIN_VALUE | 1,
        0x0010000000000000L,
        0x7fefffffffffffffL,
        0x7ff0000000000000L,
        0xfff0000000000000L,
        0x7ff8000000000001L,
        0xfff0000000000001L,
        0x43e0000000000000L,
        0x43dfffffffffffffL,
        0x43e0000000000001L
      };
      for (long bits : edges) {
        row(out, "level", "value", Long.toHexString(bits), "");
      }
      Random random = new Random(204);
      for (int i = 0; i < 2048; ++i) {
        row(out, "level", "value", Long.toHexString(random.nextLong()), "");
      }
    }
  }
}
