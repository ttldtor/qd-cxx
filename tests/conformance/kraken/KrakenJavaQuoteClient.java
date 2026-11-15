// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.DXFeedSubscription;
import com.dxfeed.event.market.Quote;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.math.BigDecimal;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicInteger;

/** Independent Java QD receiver for the offline Quote comparison. */
public final class KrakenJavaQuoteClient {
  private static String decimal(double value) {
    return BigDecimal.valueOf(value).stripTrailingZeros().toPlainString();
  }

  private static void emit(Quote quote) {
    System.out.printf(
        "QUOTE %s - - - - %s %s %s %s %d %d %d %d %d %d %d %d%n",
        quote.getEventSymbol(),
        decimal(quote.getBidPrice()),
        decimal(quote.getAskPrice()),
        decimal(quote.getBidSizeAsDouble()),
        decimal(quote.getAskSizeAsDouble()),
        quote.getBidTime(),
        quote.getAskTime(),
        quote.getTime(),
        quote.getEventTime(),
        quote.getSequence(),
        quote.getTimeNanoPart(),
        (int) quote.getBidExchangeCode(),
        (int) quote.getAskExchangeCode());
    System.out.flush();
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 0) {
      throw new IllegalArgumentException("Usage: KrakenJavaQuoteClient (supervisor stdin)");
    }

    AtomicInteger count = new AtomicInteger();
    try (DXEndpoint endpoint = DXEndpoint.create(DXEndpoint.Role.FEED);
        BufferedReader input =
            new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8))) {
      DXFeedSubscription<Quote> subscription = endpoint.getFeed().createSubscription(Quote.class);
      subscription.addEventListener(
          quotes -> {
            for (Quote quote : quotes) {
              emit(quote);
              count.incrementAndGet();
            }
          });
      subscription.addSymbols("BTC/USD");
      System.out.println("CLIENT_PREPARED");
      System.out.flush();
      String connect = input.readLine();
      if (connect == null || !connect.startsWith("CONNECT 127.0.0.1:")) {
        throw new IllegalArgumentException("expected CONNECT to a loopback address");
      }
      endpoint.connect(connect.substring("CONNECT ".length()));
      System.out.println("CLIENT_READY");
      System.out.flush();
      if (!"STOP".equals(input.readLine())) {
        throw new IllegalStateException("expected STOP before client exit");
      }
      endpoint.closeAndAwaitTermination();
    }

    if (count.get() == 0) {
      throw new IllegalStateException("Java client received no Quote");
    }
    System.out.println("CLIENT_STOP " + count.get());
  }
}
