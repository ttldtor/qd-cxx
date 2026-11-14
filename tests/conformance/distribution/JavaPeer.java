// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.devexperts.qd.qtp.socket.ServerSocketTestHelper;
import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.DXFeedSubscription;
import com.dxfeed.api.osub.IndexedEventSubscriptionSymbol;
import com.dxfeed.api.osub.ObservableSubscriptionChangeListener;
import com.dxfeed.event.IndexedEvent;
import com.dxfeed.event.market.Order;
import com.dxfeed.event.market.Quote;
import com.dxfeed.event.market.Side;
import com.dxfeed.event.market.TimeAndSale;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.Arrays;
import java.util.Collections;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Test-owned Java QD peer. One line commands arrive on stdin; observations go to stdout. */
public final class JavaPeer {
  private static final String SYMBOL = "IBM";

  private static synchronized void emit(String text) {
    System.out.println(text);
    System.out.flush();
  }

  private static boolean containsSymbol(Set<?> symbols, String expected) {
    return symbols.stream()
        .anyMatch(
            symbol ->
                expected.equals(symbol)
                    || (symbol instanceof IndexedEventSubscriptionSymbol
                        && expected.equals(
                            ((IndexedEventSubscriptionSymbol<?>) symbol).getEventSymbol())));
  }

  private static void source() throws Exception {
    try (DXEndpoint endpoint = DXEndpoint.create(DXEndpoint.Role.PUBLISHER)) {
      for (Class<?> type : Arrays.asList(Quote.class, TimeAndSale.class, Order.class)) {
        final String name = type.getSimpleName();
        endpoint
            .getPublisher()
            .getSubscription(type)
            .addChangeListener(
                new ObservableSubscriptionChangeListener() {
                  public void symbolsAdded(Set<?> symbols) {
                    if (containsSymbol(symbols, SYMBOL)) emit("ADD " + name);
                  }

                  public void symbolsRemoved(Set<?> symbols) {
                    if (containsSymbol(symbols, SYMBOL)) emit("REMOVE " + name);
                  }
                });
      }
      var port = ServerSocketTestHelper.createPortPromise("qualification-java-source");
      endpoint.connect(
          ":0[name=qualification-java-source,bindAddr=127.0.0.1,subscriptionKeepAlive=0]");
      emit("PORT " + port.await(15, TimeUnit.SECONDS));
      try (BufferedReader input = new BufferedReader(new InputStreamReader(System.in))) {
        for (String command; (command = input.readLine()) != null; ) {
          if (command.equals("STOP")) break;
          if (command.equals("QUOTE")) {
            Quote quote = new Quote(SYMBOL);
            quote.setBidPrice(101);
            quote.setBidSize(3);
            quote.setAskPrice(102);
            quote.setAskSize(4);
            quote.setBidTime(1_700_000_000_000L);
            quote.setAskTime(1_700_000_000_000L);
            endpoint.getPublisher().publishEvents(Collections.singleton(quote));
          } else if (command.equals("EVENTS")) {
            TimeAndSale sale = new TimeAndSale(SYMBOL);
            sale.setTime(1_700_000_000_000L);
            sale.setPrice(102);
            sale.setSize(4);
            Order first = new Order(SYMBOL);
            first.setIndex(7);
            first.setPrice(103);
            first.setSize(5);
            first.setOrderSide(Side.BUY);
            first.setEventFlags(IndexedEvent.SNAPSHOT_BEGIN);
            Order end = new Order(SYMBOL);
            end.setIndex(0);
            end.setPrice(103);
            end.setSize(2);
            end.setOrderSide(Side.BUY);
            end.setEventFlags(IndexedEvent.SNAPSHOT_END);
            endpoint.getPublisher().publishEvents(Arrays.asList(sale, sale, first, end));
          } else if (command.equals("REMOVE")) {
            Order removed = new Order(SYMBOL);
            removed.setIndex(7);
            removed.setEventFlags(IndexedEvent.REMOVE_EVENT);
            endpoint.getPublisher().publishEvents(Collections.singleton(removed));
          } else {
            throw new IllegalArgumentException("Unknown source command");
          }
          endpoint.awaitProcessed();
          emit("SENT " + command);
        }
      }
    }
  }

  private static void feed(int port, boolean second) throws Exception {
    try (DXEndpoint endpoint = DXEndpoint.create(DXEndpoint.Role.FEED)) {
      DXFeedSubscription<?> subscription =
          endpoint.getFeed().createSubscription(Quote.class, TimeAndSale.class, Order.class);
      subscription.addEventListener(
          events -> {
            for (Object event : events) {
              if (event instanceof Quote) {
                Quote quote = (Quote) event;
                emit("QUOTE " + quote.getBidPrice());
              } else if (event instanceof TimeAndSale) {
                TimeAndSale sale = (TimeAndSale) event;
                emit("SALE " + sale.getPrice());
              } else if (event instanceof Order) {
                Order order = (Order) event;
                emit("ORDER " + order.getIndex() + " " + order.getEventFlags());
              }
            }
          });
      subscription.addSymbols(SYMBOL);
      if (second) subscription.addSymbols("MSFT");
      endpoint.connect("127.0.0.1:" + port);
      emit("STARTED");
      try (BufferedReader input = new BufferedReader(new InputStreamReader(System.in))) {
        for (String command; (command = input.readLine()) != null; ) {
          if (command.equals("STOP")) break;
          if (command.equals("CLEAR")) {
            subscription.clear();
            emit("CLEARED");
          } else {
            throw new IllegalArgumentException("Unknown feed command");
          }
        }
      }
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 1 && args[0].equals("source")) source();
    else if ((args.length == 2 || args.length == 3) && args[0].equals("feed"))
      feed(Integer.parseInt(args[1]), args.length == 3);
    else throw new IllegalArgumentException("source | feed PORT");
  }
}
