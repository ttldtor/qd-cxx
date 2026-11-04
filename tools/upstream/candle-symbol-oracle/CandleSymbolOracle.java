// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.event.candle.CandleAlignment;
import com.dxfeed.event.candle.CandleDataType;
import com.dxfeed.event.candle.CandleExchange;
import com.dxfeed.event.candle.CandlePeriod;
import com.dxfeed.event.candle.CandlePrice;
import com.dxfeed.event.candle.CandlePriceLevel;
import com.dxfeed.event.candle.CandleSession;
import com.dxfeed.event.candle.CandleSymbol;
import com.dxfeed.event.candle.CandleSymbolAttribute;
import com.dxfeed.event.candle.CandleType;
import java.io.BufferedWriter;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Random;

/** Finite offline observations of unmodified production CandleSymbol factories and identities. */
public final class CandleSymbolOracle {
  private CandleSymbolOracle() {}

  private static String hex(String text) {
    if (text == null) {
      return "NULL";
    }
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

  private static String text(String hex) {
    if (hex.equals("NULL")) {
      return null;
    }
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < hex.length(); i += 4) {
      result.append((char) Integer.parseInt(hex.substring(i, i + 4), 16));
    }
    return result.toString();
  }

  private static double number(String bits) {
    return Double.longBitsToDouble(Long.parseUnsignedLong(bits, 16));
  }

  private static CandleSymbol make(String input, String descriptors) {
    List<CandleSymbolAttribute<?>> attributes = new ArrayList<>();
    if (!descriptors.isEmpty()) {
      for (String entry : descriptors.split(";", -1)) {
        String value = entry.substring(1);
        switch (entry.charAt(0)) {
          case 'e':
            attributes.add(CandleExchange.valueOf((char) Integer.parseInt(value, 16)));
            break;
          case 'p':
            attributes.add(CandlePrice.values()[Integer.parseInt(value)]);
            break;
          case 's':
            attributes.add(CandleSession.values()[Integer.parseInt(value)]);
            break;
          case 'a':
            attributes.add(CandleAlignment.values()[Integer.parseInt(value)]);
            break;
          case 'l':
            attributes.add(CandlePriceLevel.valueOf(number(value)));
            break;
          case 'd':
            attributes.add(CandleDataType.valueOf(text(value)));
            break;
          case 't':
            int slash = value.indexOf('/');
            attributes.add(
                CandlePeriod.valueOf(
                    number(value.substring(0, slash)),
                    CandleType.values()[Integer.parseInt(value.substring(slash + 1))]));
            break;
          default:
            throw new IllegalArgumentException("Unknown controlled descriptor");
        }
      }
    }
    if (attributes.isEmpty()) {
      return CandleSymbol.valueOf(input);
    }
    return CandleSymbol.valueOf(
        input,
        attributes.get(0),
        attributes.subList(1, attributes.size()).toArray(new CandleSymbolAttribute<?>[0]));
  }

  private static void row(Writer out, String operation, String input, String operand)
      throws Exception {
    String[] result = new String[16];
    Arrays.fill(result, "-");
    result[0] = operation;
    result[1] = hex(input);
    result[2] = operand;
    try {
      CandleSymbol symbol = make(input, operation.equals("create") ? operand : "");
      result[3] = "OK";
      if (operation.equals("equals")) {
        CandleSymbol other = make(text(operand), "");
        try {
          result[4] = Boolean.toString(symbol.equals(other));
        } catch (RuntimeException error) {
          result[4] = "ERROR";
          result[15] = error.getClass().getSimpleName();
        }
      } else {
        result[4] = hex(symbol.toString());
        result[5] = hex(symbol.getBaseSymbol());
        result[6] = Integer.toHexString(symbol.getExchange().getExchangeCode());
        result[7] = Integer.toString(symbol.getPrice().ordinal());
        result[8] = Integer.toString(symbol.getSession().ordinal());
        result[9] = Long.toHexString(Double.doubleToRawLongBits(symbol.getPeriod().getValue()));
        result[10] = Integer.toString(symbol.getPeriod().getType().ordinal());
        result[11] = Integer.toString(symbol.getAlignment().ordinal());
        result[12] =
            Long.toHexString(Double.doubleToRawLongBits(symbol.getPriceLevel().getValue()));
        result[13] = hex(symbol.getDataType().toString());
        try {
          result[14] = Integer.toString(symbol.hashCode());
        } catch (NullPointerException error) {
          result[14] = "ERROR";
        }
      }
    } catch (RuntimeException error) {
      result[3] = "ERROR";
      result[15] = error.getClass().getSimpleName();
    }
    out.write(String.join("\t", result) + "\n");
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected one TSV output path");
    }
    try (Writer out =
        new BufferedWriter(
            new OutputStreamWriter(new FileOutputStream(args[0]), StandardCharsets.UTF_8))) {
      String[] symbols = {
        null,
        "",
        "AAPL",
        "AAPL&Q",
        "AAPL&Q{=15Minutes,price=BID,tho=TRUE,a=S,pl=0x1.0p-1,dt=source}",
        "A{z=1,a=m}",
        "A{price=last,price=bid}",
        "A{price=LAST,price=ask}",
        "A{=t}",
        "A{=1T}",
        "A{=NaNt}",
        "A{=1e309d}",
        "A{=}",
        "A{tho=t}",
        "A{tho=TRUE}",
        "A{dt=}",
        "A{dt=SOURCE}",
        "A{a=bad}",
        "A{price=b}",
        "A{price=unknown}",
        "A{pl=-0}",
        "A{pl=NaN}",
        "A{pl=+NaN}",
        "A{}",
        "A{price}",
        "A{,price=bid}",
        "A{price=bid,}",
        "A{price=bid}tail",
        "A{unknown=x,=d,unknown=y}",
        "A&\u0000{=m}",
        "A&\ud800{=m}",
        "\ud800{=m}",
        "\u0000{dt=x}",
        "A{PRICE=BID}"
      };
      String[] attributes = {
        "",
        "e51",
        "e0",
        "p1",
        "p0",
        "s1",
        "s0",
        "a1",
        "a0",
        "l3fe0000000000000",
        "l7ff8000000000001",
        "t3ff0000000000000/0",
        "t8000000000000000/0",
        "t7ff0000000000000/4",
        "t7ff8000000000001/4",
        "d",
        "d0073006f0075007200630065",
        "e51;t4004000000000000/2;p1;s1;a1;l3fe0000000000000;d007200740068",
        "p1;p2",
        "t3ff0000000000000/0;t3ff0000000000000/0",
        "d0078;d0079"
      };
      for (String symbol : symbols) {
        for (String attribute : attributes) {
          row(out, "create", symbol, attribute);
        }
      }
      for (String left : symbols) {
        for (String right : symbols) {
          row(out, "equals", left, hex(right));
        }
      }
      String[] keys = {"", "price", "tho", "a", "pl", "dt", "unknown", "PRICE"};
      String[] values = {
        "m", "1T", "bid", "BID", "true", "t", "m", "0.5", "NaN", "-0", "", "source", "SOURCE",
        "wrong"
      };
      Random random = new Random(205);
      for (int n = 0; n < 4096; ++n) {
        StringBuilder symbol = new StringBuilder("AAPL&Q{");
        int count = 1 + random.nextInt(8);
        for (int i = 0; i < count; ++i) {
          if (i != 0) {
            symbol.append(',');
          }
          symbol
              .append(keys[random.nextInt(keys.length)])
              .append('=')
              .append(values[random.nextInt(values.length)]);
        }
        symbol.append('}');
        row(out, "create", symbol.toString(), "");
      }
      for (int unit = 0; unit <= 0xffff; ++unit) {
        if (unit % 17 == 0) {
          row(out, "create", "A" + (char) unit + "&Q{unknown=x,=15Minutes,price=BID}", "");
        }
      }
    }
  }
}
