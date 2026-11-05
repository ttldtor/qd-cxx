// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.dxfeed.api.DXFeedTimeSeriesSubscription;
import com.dxfeed.api.osub.IndexedEventSubscriptionSymbol;
import com.dxfeed.api.osub.ObservableSubscriptionChangeListener;
import com.dxfeed.api.osub.TimeSeriesSubscriptionSymbol;
import com.dxfeed.api.osub.WildcardSymbol;
import com.dxfeed.event.IndexedEvent;
import com.dxfeed.event.IndexedEventSource;
import com.dxfeed.event.TimeSeriesEvent;
import com.dxfeed.event.market.TimeAndSale;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Observes pinned production time-series identity and detached range changes, without a feed. */
public final class TimeSeriesOracle {
  private static final class CustomTimeSeries extends TimeAndSale {}

  @SuppressWarnings({"rawtypes", "unchecked"})
  private static final class Probe extends DXFeedTimeSeriesSubscription {
    Probe(Class<?>... types) {
      super((Class[]) types);
    }

    Object decorate(Object symbol) {
      return decorateSymbol(symbol);
    }

    Object undecorate(Object symbol) {
      return undecorateSymbol(symbol);
    }
  }

  private static void row(Object... values) {
    for (int index = 0; index < values.length; ++index) {
      if (index != 0) {
        System.out.print('\t');
      }
      System.out.print(values[index]);
    }
    System.out.println();
  }

  private static String identity(Object symbol) {
    if (symbol == null) {
      return "null";
    }
    if (symbol instanceof TimeSeriesSubscriptionSymbol) {
      TimeSeriesSubscriptionSymbol<?> value = (TimeSeriesSubscriptionSymbol<?>) symbol;
      return "T(" + identity(value.getEventSymbol()) + "," + value.getFromTime() + ")";
    }
    if (symbol instanceof IndexedEventSubscriptionSymbol) {
      IndexedEventSubscriptionSymbol<?> value = (IndexedEventSubscriptionSymbol<?>) symbol;
      return "I(" + identity(value.getEventSymbol()) + "," + value.getSource().id() + ")";
    }
    if (symbol == WildcardSymbol.ALL) {
      return "ALL";
    }
    if (symbol instanceof String) {
      StringBuilder result = new StringBuilder("S:");
      for (char unit : ((String) symbol).toCharArray()) {
        result.append(String.format(java.util.Locale.ROOT, "%04x", (int) unit));
      }
      return result.toString();
    }
    return symbol.getClass().getName() + ":" + symbol;
  }

  private static String symbols(Set<?> symbols) {
    List<String> result = new ArrayList<>();
    for (Object symbol : symbols) {
      result.add(identity(symbol));
    }
    Collections.sort(result);
    return String.join(";", result);
  }

  private static void pair(String label, Object first, Object second) {
    Set<Object> forward = new HashSet<>();
    forward.add(first);
    forward.add(second);
    Set<Object> reverse = new HashSet<>();
    reverse.add(second);
    reverse.add(first);
    row(
        "pair",
        label,
        identity(first),
        identity(second),
        first.equals(second),
        second.equals(first),
        first.hashCode(),
        second.hashCode(),
        forward.size(),
        reverse.size());
  }

  private static void attempt(String label, Runnable action) {
    try {
      action.run();
      row("attempt", label, "ACCEPTED");
    } catch (RuntimeException failure) {
      row("attempt", label, failure.getClass().getSimpleName());
    }
  }

  private static void snapshot(String label, Probe subscription) {
    row(
        "state",
        label,
        subscription.getFromTime(),
        symbols(subscription.getSymbols()),
        symbols(subscription.getDecoratedSymbols()),
        subscription.isClosed());
  }

  private static void observe() throws Exception {
    long[] times = {Long.MIN_VALUE, -1, 0, 1, 1000, Long.MAX_VALUE};
    for (String text : Arrays.asList("", "IBM", "AAPL{=m}", "*", "\u03a9\ud800\u0000")) {
      for (long time : times) {
        TimeSeriesSubscriptionSymbol<String> symbol =
            new TimeSeriesSubscriptionSymbol<>(text, time);
        row("symbol", identity(symbol), symbol.getSource().id(), symbol.hashCode());
        pair("time-ignored", symbol, new TimeSeriesSubscriptionSymbol<>(text, 0));
      }
    }
    Object indexed = new IndexedEventSubscriptionSymbol<>("IBM", IndexedEventSource.DEFAULT);
    Object timed = new TimeSeriesSubscriptionSymbol<>("IBM", 7);
    pair("approved-ADR12-cross-kind", indexed, timed);
    pair(
        "nested-ADR12-cross-kind",
        new TimeSeriesSubscriptionSymbol<>(indexed, 0),
        new TimeSeriesSubscriptionSymbol<>(timed, 0));
    pair(
        "nested-time-ignored",
        new TimeSeriesSubscriptionSymbol<>(timed, -1),
        new TimeSeriesSubscriptionSymbol<>(
            new TimeSeriesSubscriptionSymbol<>("IBM", 8), Long.MAX_VALUE));
    pair(
        "wildcard-not-text",
        new TimeSeriesSubscriptionSymbol<>(WildcardSymbol.ALL, 0),
        new TimeSeriesSubscriptionSymbol<>("*", 0));
    attempt("null-symbol", () -> new TimeSeriesSubscriptionSymbol<>(null, 0));
    attempt("empty-types", () -> new Probe());
    attempt("null-type", () -> new Probe((Class<?>) null));
    String[] names = {
      "market.Quote",
      "market.Profile",
      "market.Trade",
      "market.TradeETH",
      "market.Summary",
      "misc.TextMessage",
      "misc.TextConfiguration",
      "misc.Message",
      "misc.Configuration",
      "market.Order",
      "market.TimeAndSale",
      "candle.Candle",
      "option.Greeks",
      "option.TheoPrice",
      "option.Underlying",
      "option.Series",
      "custom.NuamTimeAndSale"
    };
    for (String name : names) {
      Class<?> type = Class.forName("com.dxfeed.event." + name);
      row(
          "capability",
          name,
          IndexedEvent.class.isAssignableFrom(type),
          TimeSeriesEvent.class.isAssignableFrom(type));
      attempt("raw-constructor-" + name, () -> new Probe(type).close());
    }
    row(
        "capability",
        "custom-inherited",
        IndexedEvent.class.isAssignableFrom(CustomTimeSeries.class),
        TimeSeriesEvent.class.isAssignableFrom(CustomTimeSeries.class));
    attempt("raw-constructor-custom-inherited", () -> new Probe(CustomTimeSeries.class).close());
    Probe subscription = new Probe(Class.forName("com.dxfeed.event.market.TimeAndSale"));
    subscription.addChangeListener(
        new ObservableSubscriptionChangeListener() {
          @Override
          public void symbolsAdded(Set<?> added) {
            row("notice", "added", symbols(added));
          }

          @Override
          public void symbolsRemoved(Set<?> removed) {
            row("notice", "removed", symbols(removed));
          }

          @Override
          public void subscriptionClosed() {
            row("notice", "closed");
          }
        });
    snapshot("initial", subscription);
    subscription.addSymbols("IBM", "AAPL");
    snapshot("symbols-before-range", subscription);
    subscription.setFromTime(0);
    snapshot("range-zero", subscription);
    subscription.setFromTime(0);
    snapshot("range-zero-noop", subscription);
    subscription.setFromTime(Long.MIN_VALUE);
    snapshot("range-min", subscription);
    subscription.setFromTime(Long.MAX_VALUE);
    snapshot("range-max", subscription);
    subscription.setSymbols(indexed, timed, WildcardSymbol.ALL, "*");
    snapshot("nested-inputs", subscription);
    row("protected", "decorate-nested", identity(subscription.decorate(timed)));
    row("protected", "undecorate-plain", identity(subscription.undecorate("IBM")));
    attempt("undecorate-null", () -> subscription.undecorate(null));
    subscription.clear();
    subscription.setFromTime(-1);
    snapshot("empty-range-change", subscription);
    subscription.close();
    subscription.setFromTime(42);
    snapshot("closed-range-change", subscription);
  }

  public static void main(String[] arguments) throws Exception {
    if (arguments.length == 0
        || arguments[0].equals("--help")
        || arguments[0].equals("--help-ru")) {
      System.out.println(
          arguments.length > 0 && arguments[0].equals("--help-ru")
              ? "TimeSeriesOracle <output.tsv>: офлайн-наблюдения identity/range/capability pinned"
                  + " QD; файл UTF-8 заменяется. Коды: 0 успех/справка, 1 ошибка; сеть и SDK не"
                  + " запускаются."
              : "TimeSeriesOracle <output.tsv>: offline pinned QD identity/range/capability"
                  + " observations; replaces UTF-8 output. Exit0 success/help; exit1 error. No"
                  + " network or SDK work.");
      return;
    }
    if (arguments.length != 1) {
      throw new IllegalArgumentException("Expected one output path; use --help");
    }
    try (PrintStream output = new PrintStream(arguments[0], "UTF-8")) {
      System.setOut(output);
      observe();
    }
  }
}
