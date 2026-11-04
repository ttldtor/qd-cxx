// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.QDFilter;
import com.devexperts.qd.kit.CompositeFilters;
import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.DXFeedFilter;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.osub.IndexedEventSubscriptionSymbol;
import com.dxfeed.api.osub.TimeSeriesSubscriptionSymbol;
import com.dxfeed.api.osub.WildcardSymbol;
import com.dxfeed.event.EventType;
import com.dxfeed.event.IndexedEventSource;
import com.dxfeed.event.market.Quote;
import com.dxfeed.event.misc.TextConfiguration;
import com.dxfeed.event.misc.TextMessage;
import java.io.IOException;
import java.io.PrintStream;

/** Observes actual production expression and category paths without network connections. */
public final class FeedFilterOracle {
  private static final String[] EXPRESSIONS = {
    "A&B",
    "A&!A",
    "A*&!A*",
    "(A,B)&!A&!B",
    "[*]&A",
    "[*]&![*]",
    "([*],A)&([*],B)",
    "A[]&A*",
    "[][^]&A*",
    "[A-J][A-J][A-J][A-J]&![A-J][A-J][A-J][A-J]",
    "[A-K][A-K][A-K][A-K]&![A-K][A-K][A-K][A-K]",
    "*:Quote",
    "!:Quote*",
    "!A*",
    "!*",
    "!(:Quote*&A*)",
    "!:Quote*,!A*",
    "!(:Quote*,A*)",
    ":Quote*&!A*",
    "!nwc",
    "[A-Z]*",
    "A*B*C",
    "A**B",
    "A*B*C*D",
    "[a]bc",
    "abc",
    "A?",
    "A[]",
    "A[^]",
    "A[Z-A]",
    "A\\ ",
    "\\QABC\\E",
    "record Quote",
    "any",
    "feed",
    "compfeed",
    "regfeed",
    "chartdata",
    "convrates",
    "nwc",
    "bs",
    "bssymbol",
    "fut",
    "futsymbol",
    "opt",
    "optsymbol",
    "spread",
    "spreadsymbol",
    "cssymbol",
    "csoptsymbol",
    "bsoptsymbol",
    "futoptsymbol",
    "indsymbol",
    "prodsymbol",
    "fxsymbol",
    "hash0of2",
    "hash1of2",
    "hash0of1",
    "range-A-M-",
    "range---",
    "range-Z-A-"
  };

  private static String safe(String value) {
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < value.length(); i++) {
      char c = value.charAt(i);
      if (c < 32 || c >= 127 || c == '\\') {
        result.append(String.format("\\u%04x", (int) c));
      } else {
        result.append(c);
      }
    }
    return result.toString();
  }

  private static void expressions() {
    DataScheme scheme = DXFeedScheme.getInstance();
    for (String expression : EXPRESSIONS) {
      final QDFilter filter;
      try {
        filter = CompositeFilters.valueOf(expression, scheme);
      } catch (RuntimeException failure) {
        System.out.println("invalid\t" + safe(expression) + "\t" + failure.getClass().getName());
        continue;
      }
      for (String recordName : new String[] {"Quote", "Quote&Q", "Profile"}) {
        DataRecord record = scheme.findRecordByName(recordName);
        for (String symbol :
            new String[] {"AAPL", "MSFT", "A\u00e9", "A\ud83d\ude00", "ABC", "ABBC", "*"}) {
          System.out.println(
              "match\t"
                  + safe(expression)
                  + "\t"
                  + recordName
                  + "\t"
                  + safe(symbol)
                  + "\t"
                  + filter.accept(
                      QDContract.STREAM, record, scheme.getCodec().encode(symbol), symbol));
        }
        System.out.println(
            "wildcard\t"
                + safe(expression)
                + "\t"
                + recordName
                + "\t"
                + filter.accept(
                    QDContract.STREAM, record, scheme.getCodec().getWildcardCipher(), null));
      }
    }
  }

  private static void correlations() {
    DataScheme scheme = DXFeedScheme.getInstance();
    DataRecord record = scheme.findRecordByName("Quote");
    for (String expression : new String[] {"A&B", "A&!A", "A*&!A*", "(A,B)&!A&!B"}) {
      QDFilter filter = CompositeFilters.valueOf(expression, scheme);
      for (String symbol : new String[] {"A", "B", "C", "ALL"}) {
        boolean all = symbol.equals("ALL");
        int cipher = all ? scheme.getCodec().getWildcardCipher() : scheme.getCodec().encode(symbol);
        System.out.println(
            "correlation\t"
                + expression
                + "\t"
                + symbol
                + "\t"
                + filter.accept(QDContract.STREAM, record, cipher, all ? null : symbol));
      }
    }
  }

  private static void categories() {
    DXEndpoint endpoint = DXEndpoint.create();
    try {
      DXFeedFilter filter =
          DXFeedFilter.newBuilder(endpoint)
              .withCategory("regional", ":Quote[&]Q")
              .withCategory("composite", ":Quote")
              .withCategory("all", "*")
              .build();
      for (Object symbol :
          new Object[] {
            "AAPL",
            "AAPL&Q",
            "AAPL&N",
            "*",
            "*A",
            WildcardSymbol.ALL,
            new IndexedEventSubscriptionSymbol<String>("AAPL&Q", IndexedEventSource.DEFAULT),
            new TimeSeriesSubscriptionSymbol<String>("AAPL&Q", 123)
          }) {
        System.out.println(
            "category\t"
                + symbol.getClass().getSimpleName()
                + "\t"
                + safe(
                    symbol instanceof TimeSeriesSubscriptionSymbol
                        ? "AAPL&Q{fromTimeMillis=123}"
                        : symbol.toString())
                + "\t"
                + filter.accept(Quote.class, symbol)
                + "\t"
                + filter.getCategory(Quote.class, symbol));
      }
      for (String symbol : new String[] {"AAPL", "AAPL&Q", "AAPL&N", "*", "*A"}) {
        Quote event = new Quote(symbol);
        System.out.println(
            "event\t"
                + symbol
                + "\t"
                + filter.acceptEvent(event)
                + "\t"
                + filter.getEventCategory(event));
      }
      System.out.println(
          "static-tracker\t"
              + filter.isDynamic()
              + "\t"
              + (filter.getTracker().getCurrentFilter() == filter));
    } finally {
      endpoint.close();
    }
  }

  private static void textEventRow(
      DXFeedFilter filter, EventType<?> event, String kind, String symbol, String payload) {
    String label = symbol == null ? "<null>" : safe(symbol);
    try {
      System.out.println(
          "text-event\t"
              + kind
              + "\t"
              + label
              + "\t"
              + payload
              + "\t"
              + filter.acceptEvent(event)
              + "\t"
              + filter.getEventCategory(event));
    } catch (RuntimeException failure) {
      System.out.println(
          "text-event-error\t"
              + kind
              + "\t"
              + label
              + "\t"
              + payload
              + "\t"
              + failure.getClass().getName());
    }
  }

  private static void textEvents() {
    DXEndpoint endpoint = DXEndpoint.create();
    try {
      DXFeedFilter filter =
          DXFeedFilter.newBuilder(endpoint)
              .withCategory("decorated", "TOPIC[&]Q")
              .withCategory("message", ":TextMessage*")
              .withCategory("configuration", ":TextConfiguration*")
              .build();
      String[] payloads = {null, "", "body"};
      for (String symbol : new String[] {null, "", "TOPIC", "TOPIC&Q", "*", "*A"}) {
        for (int i = 0; i < payloads.length; i++) {
          String payload = i == 0 ? "null" : i == 1 ? "empty" : "body";
          textEventRow(
              filter, new TextMessage(symbol, payloads[i]), "TextMessage", symbol, payload);
          textEventRow(
              filter,
              new TextConfiguration(symbol, payloads[i]),
              "TextConfiguration",
              symbol,
              payload);
        }
      }
      textEventRow(filter, new TextMessage(), "TextMessage", null, "default");
      textEventRow(filter, new TextConfiguration(), "TextConfiguration", null, "default");
      try {
        filter.acceptEvent(null);
        System.out.println("text-null-model\tACCEPTED");
      } catch (RuntimeException failure) {
        System.out.println("text-null-model\t" + failure.getClass().getName());
      }
      Object[] symbols = {
        "TOPIC&Q",
        new IndexedEventSubscriptionSymbol<String>("TOPIC&Q", IndexedEventSource.DEFAULT),
        new IndexedEventSubscriptionSymbol<String>("TOPIC&Q", new IndexedEventSource(1, "ONE")),
        new IndexedEventSubscriptionSymbol<String>(
            "TOPIC&Q", new IndexedEventSource(-1, "NEGATIVE")),
        new TimeSeriesSubscriptionSymbol<String>("TOPIC&Q", 123),
        WildcardSymbol.ALL,
        "*",
        "*A"
      };
      String[] labels = {
        "plain",
        "indexed0",
        "indexed1",
        "indexed-negative",
        "time-series",
        "all",
        "star",
        "star-prefix"
      };
      for (int i = 0; i < symbols.length; i++) {
        for (boolean configuration : new boolean[] {false, true}) {
          Class<? extends EventType<?>> type =
              configuration ? TextConfiguration.class : TextMessage.class;
          System.out.println(
              "text-subscription\t"
                  + type.getSimpleName()
                  + "\t"
                  + labels[i]
                  + "\t"
                  + filter.accept(type, symbols[i])
                  + "\t"
                  + filter.getCategory(type, symbols[i]));
        }
      }
    } finally {
      endpoint.close();
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output TSV path");
    }
    try (PrintStream output = new PrintStream(args[0], "UTF-8")) {
      System.setOut(output);
      expressions();
      correlations();
      categories();
      textEvents();
      output.flush();
      output.close();
      if (output.checkError()) {
        throw new IOException("Failed writing oracle output");
      }
    }
  }
}
