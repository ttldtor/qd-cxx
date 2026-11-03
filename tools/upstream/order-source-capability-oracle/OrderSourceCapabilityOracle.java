// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

package qdcxx.oracle;

import com.dxfeed.api.osub.IndexedEventSubscriptionSymbol;
import com.dxfeed.event.IndexedEventSource;
import com.dxfeed.event.custom.NuamOrder;
import com.dxfeed.event.market.AnalyticOrder;
import com.dxfeed.event.market.MarketEvent;
import com.dxfeed.event.market.Order;
import com.dxfeed.event.market.OrderImbalance;
import com.dxfeed.event.market.OrderSource;
import com.dxfeed.event.market.OtcMarketsOrder;
import com.dxfeed.event.market.Quote;
import com.dxfeed.event.market.SpreadOrder;
import java.io.File;
import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;

/**
 * Production capabilities and specific-source symbol observations; launcher supplies one TSV path.
 */
public final class OrderSourceCapabilityOracle {
  private static final List<Class<? extends MarketEvent>> TYPES =
      Arrays.asList(
          Order.class,
          AnalyticOrder.class,
          OtcMarketsOrder.class,
          SpreadOrder.class,
          NuamOrder.class,
          OrderImbalance.class);

  private static void capabilities(PrintWriter out, OrderSource source) {
    for (int i = 0; i < TYPES.size(); i++) {
      out.println("CAP\t" + source.id() + "\t" + i + "\t" + source.isPublishable(TYPES.get(i)));
    }
    out.println("FULL\t" + source.id() + "\t" + source.isFullOrderBook());
  }

  private static void list(PrintWriter out, String type, List<OrderSource> sources) {
    out.print("LIST\t" + type);
    for (OrderSource source : sources) {
      out.print("\t" + source.id());
    }
    out.println();
  }

  private static void invalidType(PrintWriter out, Class<? extends MarketEvent> type) {
    String name = type == null ? "null" : type.getSimpleName();
    try {
      OrderSource.DEFAULT.isPublishable(type);
      throw new AssertionError("Invalid type accepted");
    } catch (IllegalArgumentException expected) {
      out.println("INVALID\tinstance\t" + name + "\t" + expected.getClass().getSimpleName());
    }
    try {
      OrderSource.publishable(type);
      throw new AssertionError("Invalid type accepted");
    } catch (IllegalArgumentException expected) {
      out.println("INVALID\tlist\t" + name + "\t" + expected.getClass().getSimpleName());
    }
  }

  /**
   * Writes observations; input/I/O/reflection failures exit nonzero without modifying production
   * classes.
   */
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output TSV filename");
    }
    try (PrintWriter out = new PrintWriter(new File(args[0]), "UTF-8")) {
      Field[] fields = OrderSource.class.getFields();
      Arrays.sort(fields, Comparator.comparing(Field::getName));
      for (Field field : fields) {
        if (field.getType() == OrderSource.class && Modifier.isStatic(field.getModifiers())) {
          capabilities(out, (OrderSource) field.get(null));
        }
      }
      for (String name : Arrays.asList("A", "0", "TEST", "Ab12", "zzzz")) {
        capabilities(out, OrderSource.valueOf(name));
      }
      for (int i = 0; i < TYPES.size(); i++) {
        list(out, Integer.toString(i), OrderSource.publishable(TYPES.get(i)));
      }
      list(out, "FULL", OrderSource.fullOrderBook());
      invalidType(out, Quote.class);
      invalidType(out, null);
      for (int id : new int[] {Integer.MIN_VALUE, -1, 0, 1, 9, 10, 0x54455354, Integer.MAX_VALUE}) {
        IndexedEventSource source = new IndexedEventSource(id, null);
        IndexedEventSubscriptionSymbol<String> specific =
            new IndexedEventSubscriptionSymbol<>("IBM", source);
        IndexedEventSubscriptionSymbol<String> same =
            new IndexedEventSubscriptionSymbol<>("IBM", new IndexedEventSource(id, "other"));
        out.println(
            "SPECIFIC\t"
                + id
                + "\t"
                + specific.getSource().id()
                + "\t"
                + specific.equals(same)
                + "\t"
                + specific.equals("IBM"));
      }
      try {
        new IndexedEventSubscriptionSymbol<>("IBM", null);
        throw new AssertionError("Null source accepted");
      } catch (NullPointerException expected) {
        out.println("NULL_SOURCE\t" + expected.getClass().getSimpleName());
      }
      if (out.checkError()) {
        throw new java.io.IOException("Observation output failed");
      }
    }
  }
}
