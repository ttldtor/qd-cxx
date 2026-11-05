// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.DXFeedSubscription;
import com.dxfeed.event.market.Quote;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayDeque;
import java.util.Collections;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Actual public production event dispatch; finite application-owned schedulers, no network. */
public final class CallbackExecutionOracle {
  static PrintStream output;
  static String current = "caller";

  static void row(String label, Object value) {
    output.println(label + "\t" + value);
  }

  static void require(boolean condition, String label) {
    if (!condition) throw new AssertionError(label);
  }

  static void await(CountDownLatch latch, String label) throws Exception {
    require(latch.await(10, TimeUnit.SECONDS), label);
  }

  static Quote quote(int value) {
    Quote event = new Quote("IBM");
    event.setBidPrice(value);
    return event;
  }

  static DXEndpoint endpoint() {
    return DXEndpoint.newBuilder().withRole(DXEndpoint.Role.LOCAL_HUB).build();
  }

  static final class Manual implements Executor {
    final ArrayDeque<Runnable> queue = new ArrayDeque<Runnable>();
    final String name;

    Manual(String name) {
      this.name = name;
    }

    public void execute(Runnable task) {
      require(queue.size() < 64, "finite queue");
      queue.addLast(task);
    }

    void drain() {
      String previous = current;
      current = name;
      try {
        int turns = 64;
        while (!queue.isEmpty()) {
          require(turns-- > 0, "finite drain");
          queue.removeFirst().run();
        }
      } finally {
        current = previous;
      }
    }
  }

  static void policySelection() throws Exception {
    DXEndpoint endpoint = endpoint();
    Manual a = new Manual("endpoint"), b = new Manual("override");
    try {
      endpoint.executor(a);
      DXFeedSubscription<Quote> sub = endpoint.getFeed().createSubscription(Quote.class);
      row("subscription.initial.inherits", sub.getExecutor() == null);
      AtomicInteger calls = new AtomicInteger();
      final String[] selected = {"none"};
      sub.addEventListener(
          events -> {
            calls.incrementAndGet();
            selected[0] = current;
          });
      sub.addSymbols("IBM");
      a.drain();
      endpoint.getPublisher().publishEvents(Collections.singletonList(quote(1)));
      row("accepted.before.callback", calls.get() == 0 && !a.queue.isEmpty());
      a.drain();
      row("endpoint.selected", selected[0]);
      sub.setExecutor(b);
      row("subscription.override.identity", sub.getExecutor() == b);
      endpoint.getPublisher().publishEvents(Collections.singletonList(quote(2)));
      a.drain();
      b.drain();
      row("override.selected", selected[0]);
      sub.setExecutor(null);
      row("subscription.reset.inherits", sub.getExecutor() == null);
      endpoint.getPublisher().publishEvents(Collections.singletonList(quote(3)));
      a.drain();
      b.drain();
      row("reset.selected", selected[0]);
      row("three.actual.notifications", calls.get() == 3);
      sub.close();
      sub.setExecutor(b);
      row("closed.subscription.setter", sub.getExecutor() == b);
      try {
        endpoint.executor(null);
        row("null.endpoint", "accepted");
      } catch (NullPointerException expected) {
        row("null.endpoint", "NullPointerException");
      }
      a.drain();
      b.drain();
    } finally {
      endpoint.close();
      a.drain();
      b.drain();
    }
  }

  static void inline() throws Exception {
    DXEndpoint endpoint = endpoint();
    try {
      endpoint.executor(Runnable::run);
      DXFeedSubscription<Quote> sub = endpoint.getFeed().createSubscription(Quote.class);
      AtomicInteger calls = new AtomicInteger();
      sub.addEventListener(events -> calls.incrementAndGet());
      sub.addSymbols("IBM");
      endpoint.getPublisher().publishEvents(Collections.singletonList(quote(4)));
      row("inline.callback.before.publish.return", calls.get() == 1);
      sub.close();
    } finally {
      endpoint.close();
    }
  }

  static void serial() throws Exception {
    ExecutorService workers = Executors.newFixedThreadPool(2);
    DXEndpoint endpoint = endpoint();
    CountDownLatch first = new CountDownLatch(1),
        release = new CountDownLatch(1),
        second = new CountDownLatch(1);
    AtomicInteger active = new AtomicInteger(),
        maximum = new AtomicInteger(),
        calls = new AtomicInteger();
    try {
      endpoint.executor(workers);
      DXFeedSubscription<Quote> sub = endpoint.getFeed().createSubscription(Quote.class);
      sub.addEventListener(
          events -> {
            int inside = active.incrementAndGet();
            maximum.accumulateAndGet(inside, Math::max);
            int ordinal = calls.incrementAndGet();
            try {
              if (ordinal == 1) {
                first.countDown();
                require(release.await(10, TimeUnit.SECONDS), "release");
              } else second.countDown();
            } catch (InterruptedException error) {
              Thread.currentThread().interrupt();
              throw new AssertionError(error);
            } finally {
              active.decrementAndGet();
            }
          });
      sub.addSymbols("IBM");
      endpoint.getPublisher().publishEvents(Collections.singletonList(quote(5)));
      await(first, "first callback");
      endpoint.getPublisher().publishEvents(Collections.singletonList(quote(6)));
      row("same.subscription.while.first.running", calls.get() == 1);
      release.countDown();
      await(second, "second callback");
      sub.close();
      row("same.subscription.maximum.active", maximum.get());
    } finally {
      release.countDown();
      endpoint.close();
      workers.shutdown();
      if (!workers.awaitTermination(10, TimeUnit.SECONDS)) {
        workers.shutdownNow();
        require(workers.awaitTermination(10, TimeUnit.SECONDS), "workers retired");
      }
    }
    row("owned.executor.terminated", workers.isTerminated());
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) throw new IllegalArgumentException("One output path required");
    PrintStream stream = new PrintStream(args[0], "UTF-8");
    output = stream;
    try {
      policySelection();
      inline();
      serial();
    } finally {
      stream.flush();
      stream.close();
      if (stream.checkError()) throw new IOException("Oracle output failed");
    }
  }
}
