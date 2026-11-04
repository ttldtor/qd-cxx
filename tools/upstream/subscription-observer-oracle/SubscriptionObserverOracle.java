// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.dxfeed.api.DXFeedSubscription;
import com.dxfeed.api.osub.IndexedEventSubscriptionSymbol;
import com.dxfeed.api.osub.ObservableSubscriptionChangeListener;
import com.dxfeed.api.osub.TimeSeriesSubscriptionSymbol;
import com.dxfeed.api.osub.WildcardSymbol;
import com.dxfeed.event.IndexedEventSource;
import com.dxfeed.event.market.Profile;
import com.dxfeed.event.market.Quote;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Offline production subscription observations; set contents are sorted, callback order is
 * preserved.
 */
public final class SubscriptionObserverOracle {
  private static String scenario;
  private static int depth;

  private static String values(Set<?> values) {
    List<String> result = new ArrayList<>();
    for (Object value : values) {
      if (value instanceof TimeSeriesSubscriptionSymbol) {
        TimeSeriesSubscriptionSymbol<?> time = (TimeSeriesSubscriptionSymbol<?>) value;
        result.add("TSS:" + time.getEventSymbol() + ":" + time.getFromTime());
      } else if (value instanceof IndexedEventSubscriptionSymbol) {
        IndexedEventSubscriptionSymbol<?> indexed = (IndexedEventSubscriptionSymbol<?>) value;
        result.add("IES:" + indexed.getEventSymbol() + ":" + indexed.getSource().id());
      } else if (value == WildcardSymbol.ALL) {
        result.add("ALL");
      } else if (value instanceof Class) {
        result.add(((Class<?>) value).getSimpleName());
      } else {
        result.add(value.getClass().getSimpleName() + ":" + value);
      }
    }
    java.util.Collections.sort(result);
    return result.toString();
  }

  private static void row(String action, String value) {
    System.out.println(scenario + "\t" + action + "\t" + value);
  }

  private static ObservableSubscriptionChangeListener listener(
      String name, DXFeedSubscription<?> subscription) {
    return new ObservableSubscriptionChangeListener() {
      @Override
      public void symbolsAdded(Set<?> symbols) {
        row(
            name + ".added",
            values(symbols)
                + ";current="
                + values(subscription.getDecoratedSymbols())
                + ";monitor="
                + Thread.holdsLock(subscription));
      }

      @Override
      public void symbolsRemoved(Set<?> symbols) {
        row(
            name + ".removed",
            values(symbols)
                + ";current="
                + values(subscription.getDecoratedSymbols())
                + ";monitor="
                + Thread.holdsLock(subscription));
      }

      @Override
      public void subscriptionClosed() {
        row(
            name + ".closed",
            "closed=" + subscription.isClosed() + ";monitor=" + Thread.holdsLock(subscription));
      }
    };
  }

  private static void operation(String name, Runnable operation) {
    try {
      operation.run();
      row(name + ".return", "success");
    } catch (RuntimeException error) {
      row(name + ".return", error.getClass().getSimpleName());
    }
  }

  private static void ordinary() {
    scenario = "ordinary";
    DXFeedSubscription<Quote> subscription = new DXFeedSubscription<>(Quote.class);
    ObservableSubscriptionChangeListener listener = listener("L", subscription);
    subscription.addChangeListener(listener);
    row("empty-registration", "returned");
    subscription.addChangeListener(listener);
    operation("add-A-B", () -> subscription.addSymbols("A", "B"));
    operation("add-A-again", () -> subscription.addSymbols("A"));
    operation("replace-B-C", () -> subscription.setSymbols("B", "C"));
    operation("remove-absent", () -> subscription.removeSymbols("ABSENT"));
    operation("remove-B", () -> subscription.removeSymbols("B"));
    operation("clear", subscription::clear);
    operation("clear-again", subscription::clear);
    operation("remove-listener", () -> subscription.removeChangeListener(listener));
    operation("remove-listener-again", () -> subscription.removeChangeListener(listener));
    operation("after-removal", () -> subscription.addSymbols("D"));
    operation("close", subscription::close);
    row("closed-symbols", values(subscription.getSymbols()));
    operation("add-listener-after-close", () -> subscription.addChangeListener(listener));
  }

  private static void introspection() {
    scenario = "introspection";
    DXFeedSubscription<Object> subscription =
        new DXFeedSubscription<>(Quote.class, Profile.class, Quote.class);
    row("types", values(subscription.getEventTypes()));
    row("contains-Quote", Boolean.toString(subscription.containsEventType(Quote.class)));
    row("contains-String", Boolean.toString(subscription.containsEventType(String.class)));
    operation("mutate-types", () -> subscription.getEventTypes().clear());
    subscription.setSymbols("A");
    operation("mutate-symbols", () -> subscription.getSymbols().clear());
    operation("mutate-decorated", () -> subscription.getDecoratedSymbols().clear());
    operation(
        "initial-registration", () -> subscription.addChangeListener(listener("L", subscription)));
    subscription.close();
  }

  private static void decorated() {
    scenario = "decorated";
    DXFeedSubscription<Quote> subscription = new DXFeedSubscription<>(Quote.class);
    subscription.addChangeListener(listener("L", subscription));
    IndexedEventSubscriptionSymbol<String> indexed =
        new IndexedEventSubscriptionSymbol<>("A", new IndexedEventSource(7, "first"));
    IndexedEventSubscriptionSymbol<String> same =
        new IndexedEventSubscriptionSymbol<>("A", new IndexedEventSource(7, "second"));
    operation("add-indexed", () -> subscription.addSymbols(indexed));
    operation("same-indexed", () -> subscription.addSymbols(same));
    operation("remove-equal-indexed", () -> subscription.removeSymbols(indexed));
    operation("add-plain-all", () -> subscription.addSymbols("A", WildcardSymbol.ALL));
    operation("clear", subscription::clear);
    subscription.close();
  }

  private static void reentry() {
    scenario = "reentry";
    DXFeedSubscription<Quote> subscription = new DXFeedSubscription<>(Quote.class);
    subscription.setSymbols("A");
    ObservableSubscriptionChangeListener listener =
        new ObservableSubscriptionChangeListener() {
          @Override
          public void symbolsAdded(Set<?> symbols) {
            row("added", values(symbols));
          }

          @Override
          public void symbolsRemoved(Set<?> symbols) {
            ++depth;
            row("removed", "depth=" + depth + ";current=" + values(subscription.getSymbols()));
            subscription.removeChangeListener(this);
            row("remove-self-return", "depth=" + depth);
            --depth;
          }

          @Override
          public void subscriptionClosed() {
            ++depth;
            row("closed", "depth=" + depth);
            --depth;
          }
        };
    subscription.addChangeListener(listener);
    operation("replace", () -> subscription.setSymbols("B"));
    subscription.close();
  }

  private static void failure() {
    scenario = "failure";
    DXFeedSubscription<Quote> subscription = new DXFeedSubscription<>(Quote.class);
    ObservableSubscriptionChangeListener bad =
        symbols -> {
          row("first.added", values(symbols));
          throw new IllegalStateException("fixed oracle fault");
        };
    subscription.addChangeListener(bad);
    subscription.addChangeListener(listener("second", subscription));
    operation("add", () -> subscription.addSymbols("A"));
    row("after-failure", values(subscription.getSymbols()) + ";closed=" + subscription.isClosed());
    subscription.close();
  }

  /**
   * @param arguments Exactly one destination; production classes are loaded from the pinned runner
   *     classpath.
   */
  public static void main(String[] arguments) throws Exception {
    if (arguments.length != 1) {
      throw new IllegalArgumentException("Expected one output path");
    }
    try (PrintStream output = new PrintStream(arguments[0], "UTF-8")) {
      System.setOut(output);
      ordinary();
      introspection();
      decorated();
      reentry();
      failure();
      output.flush();
      output.close();
      if (output.checkError()) {
        throw new IOException("Failed writing oracle output");
      }
    }
  }
}
