// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.api.impl.EventDelegateSet;
import com.dxfeed.api.osub.IndexedEventSubscriptionSymbol;
import com.dxfeed.api.osub.TimeSeriesSubscriptionSymbol;
import com.dxfeed.api.osub.WildcardSymbol;
import com.dxfeed.event.IndexedEventSource;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.OrderSource;
import java.io.PrintStream;
import java.util.HashSet;
import java.util.Set;

/**
 * Observes actual production selector identities, delegate routing and raw demand-time conversion
 * offline.
 */
public final class SelectorOracle {
  private static void pair(String name, Object left, Object right) {
    System.out.println(
        "pair\t"
            + name
            + "\t"
            + left.getClass().getName()
            + "\t"
            + right.getClass().getName()
            + "\t"
            + left.equals(right)
            + "\t"
            + right.equals(left)
            + "\t"
            + left.hashCode()
            + "\t"
            + right.hashCode());
  }

  private static void rejectNull(String name, Runnable operation) {
    try {
      operation.run();
      System.out.println("construction\t" + name + "\tACCEPTED");
    } catch (RuntimeException failure) {
      System.out.println("construction\t" + name + "\t" + failure.getClass().getName());
    }
  }

  @SuppressWarnings({"unchecked", "rawtypes"})
  private static void demand() {
    DataScheme scheme = DXFeedScheme.getInstance();
    MarketFactoryImpl factory = new MarketFactoryImpl();
    EventDelegate first =
        factory.createStreamOnlyDelegates(scheme.findRecordByName("Quote")).iterator().next();
    EventDelegateSet quotes = first.createDelegateSet();
    for (String name : new String[] {"Quote", "Quote&Q"}) {
      for (EventDelegate value : factory.createStreamOnlyDelegates(scheme.findRecordByName(name))) {
        if (value.getEventType() == first.getEventType()) {
          quotes.add(value);
        }
      }
    }
    quotes.completeConstruction();
    for (String symbol :
        new String[] {"AAPL", "AAPL&Q", "AAPL&N", "AAPL&Q{x=y}", "AB*", "A?", "*", "*A"}) {
      for (int source : new int[] {-1, 0, 1, OrderSource.NTV.id()}) {
        java.util.List<EventDelegate> selected =
            quotes.getSubDelegatesBySubscriptionSymbol(symbol, source);
        String route = "-";
        String physical = "-";
        if (!selected.isEmpty()) {
          route = selected.get(0).getRecord().getName();
          physical = selected.get(0).getQDSymbolByEventSymbol(symbol);
        }
        System.out.println(
            "route\tQuote\t"
                + symbol
                + "\t"
                + source
                + "\t"
                + selected.size()
                + "\t"
                + route
                + "\t"
                + physical);
      }
      System.out.println(
          "time-series-delegates\tQuote\t"
              + symbol
              + "\t"
              + quotes.getTimeSeriesDelegatesByEventSymbol(symbol).size());
    }
    System.out.println(
        "wildcard-delegates\tQuote\t"
            + quotes.getSubDelegatesBySubscriptionSymbol(WildcardSymbol.ALL, -1).size());
    for (long millis :
        new long[] {
          Long.MIN_VALUE,
          Long.MIN_VALUE + 1,
          -2147483648001L,
          -1001,
          -1,
          0,
          1,
          999,
          1000,
          2147483647999L,
          2147483648000L,
          Long.MAX_VALUE - 1,
          Long.MAX_VALUE
        }) {
      System.out.println("time\t" + millis + "\t" + first.getQDTimeByEventTime(millis));
    }
    DataRecord ordinary = scheme.findRecordByName("Order");
    EventDelegate order = factory.createDelegates(ordinary).iterator().next();
    EventDelegateSet orders = order.createDelegateSet();
    for (String name : new String[] {"Order", "Order#NTV", "Order#BYX"}) {
      DataRecord record = scheme.findRecordByName(name);
      if (record == null) {
        throw new IllegalStateException("Missing configured production source record: " + name);
      }
      for (EventDelegate value : factory.createDelegates(record)) {
        if (value.getEventType() == order.getEventType()) {
          orders.add(value);
        }
      }
    }
    orders.completeConstruction();
    for (int source :
        new int[] {
          -1, -2, Integer.MIN_VALUE, 0, OrderSource.NTV.id(), OrderSource.BYX.id(), 123456789
        }) {
      java.util.List<EventDelegate> selected =
          orders.getSubDelegatesBySubscriptionSymbol("AAPL", source);
      StringBuilder names = new StringBuilder();
      for (EventDelegate value : selected) {
        if (names.length() != 0) {
          names.append('|');
        }
        names.append(value.getRecord().getName());
      }
      System.out.println(
          "source-route\tOrder\t"
              + source
              + "\t"
              + selected.size()
              + "\t"
              + (names.length() == 0 ? "-" : names));
    }
  }

  /** Takes one output TSV path; starts no SDK/network owner and propagates every failure. */
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output TSV path");
    }
    PrintStream previous = System.out;
    try (PrintStream output = new PrintStream(args[0], "UTF-8")) {
      System.setOut(output);
      try {
        IndexedEventSubscriptionSymbol<String> ies =
            new IndexedEventSubscriptionSymbol<>("AAPL", IndexedEventSource.DEFAULT);
        IndexedEventSubscriptionSymbol<String> equalId =
            new IndexedEventSubscriptionSymbol<>("AAPL", new IndexedEventSource(0, "ALIAS"));
        IndexedEventSubscriptionSymbol<String> nullName =
            new IndexedEventSubscriptionSymbol<>("AAPL", new IndexedEventSource(0, null));
        IndexedEventSubscriptionSymbol<String> otherSource =
            new IndexedEventSubscriptionSymbol<>("AAPL", new IndexedEventSource(1, "ONE"));
        TimeSeriesSubscriptionSymbol<String> zero = new TimeSeriesSubscriptionSymbol<>("AAPL", 0);
        TimeSeriesSubscriptionSymbol<String> minus = new TimeSeriesSubscriptionSymbol<>("AAPL", -1);
        TimeSeriesSubscriptionSymbol<String> plus = new TimeSeriesSubscriptionSymbol<>("AAPL", 1);
        TimeSeriesSubscriptionSymbol<String> max =
            new TimeSeriesSubscriptionSymbol<>("AAPL", Long.MAX_VALUE);
        TimeSeriesSubscriptionSymbol<String> min =
            new TimeSeriesSubscriptionSymbol<>("AAPL", Long.MIN_VALUE);
        pair("ies-self", ies, ies);
        pair("ies-same-id-different-name", ies, equalId);
        pair("ies-same-id-null-name", ies, nullName);
        pair("ies-different-source", ies, otherSource);
        pair("ies-tss-zero", ies, zero);
        pair("ies-tss-minus", ies, minus);
        pair("ies-tss-plus", ies, plus);
        pair("ies-tss-max", ies, max);
        pair("ies-tss-min", ies, min);
        pair("other-source-tss", otherSource, zero);
        pair("tss-zero-minus", zero, minus);
        pair("tss-zero-plus", zero, plus);
        pair("tss-zero-max", zero, max);
        pair("tss-zero-min", zero, min);
        pair("tss-other-symbol", zero, new TimeSeriesSubscriptionSymbol<>("MSFT", 0));
        pair("ies-plain-text", ies, "AAPL");
        pair("tss-plain-text", zero, "AAPL");
        pair("wildcard-self", WildcardSymbol.ALL, WildcardSymbol.ALL);
        pair("wildcard-literal-star", WildcardSymbol.ALL, new String("*"));
        pair("wildcard-leading-star", WildcardSymbol.ALL, "*AAPL");
        System.out.println("value\ttss-zero\t" + zero.getFromTime() + "\t" + zero.getSource().id());
        System.out.println("value\ttss-max\t" + max.getFromTime() + "\t" + max.getSource().id());
        System.out.println("value\ttss-min\t" + min.getFromTime() + "\t" + min.getSource().id());
        System.out.println(
            "value\twildcard-text\t"
                + WildcardSymbol.ALL.toString()
                + "\t"
                + WildcardSymbol.RESERVED_PREFIX);
        Set<Object> forward = new HashSet<>();
        forward.add(ies);
        forward.add(zero);
        Set<Object> reverse = new HashSet<>();
        reverse.add(zero);
        reverse.add(ies);
        System.out.println("collection\tHashSet-ies-then-tss\t" + forward.size());
        System.out.println("collection\tHashSet-tss-then-ies\t" + reverse.size());
        rejectNull(
            "ies-null-symbol",
            () -> new IndexedEventSubscriptionSymbol<String>(null, IndexedEventSource.DEFAULT));
        rejectNull(
            "ies-null-source", () -> new IndexedEventSubscriptionSymbol<String>("AAPL", null));
        rejectNull(
            "tss-null-symbol",
            () -> new TimeSeriesSubscriptionSymbol<String>(null, Long.MAX_VALUE));

        demand();
        if (output.checkError()) {
          throw new IllegalStateException("Observation write failed");
        }
      } finally {
        System.setOut(previous);
      }
    }
  }
}
