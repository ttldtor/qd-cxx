// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.devexperts.qd.QDDistributor;
import com.devexperts.qd.QDHistory;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordProvider;
import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.DXFeedTimeSeriesSubscription;
import com.dxfeed.api.impl.DXEndpointImpl;
import com.dxfeed.event.IndexedEvent;
import com.dxfeed.event.market.TimeAndSale;
import java.io.PrintStream;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.concurrent.Executor;

/** Real DXFeedTimeSeriesSubscription -> actual QDAgent HISTORY -> public callback trace. */
public final class LiveRangeOracle {
  private static String phase = "initial";
  private static int rows;
  private static int events;

  private static void row(Object... cells) {
    if (++rows > 4096) throw new IllegalStateException("row bound exceeded");
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) System.out.print('\t');
      System.out.print(cells[i]);
    }
    System.out.println();
  }

  private static final class BoundedExecutor implements Executor {
    final ArrayDeque<Runnable> jobs = new ArrayDeque<>();

    public void execute(Runnable job) {
      if (jobs.size() >= 128) throw new IllegalStateException("queued job bound exceeded");
      jobs.addLast(job);
    }

    void drain() {
      int count = 0;
      while (!jobs.isEmpty()) {
        if (++count > 256) throw new IllegalStateException("executor turn bound exceeded");
        jobs.removeFirst().run();
      }
    }
  }

  private static void demand(QDDistributor source, String label) {
    drain(source.getAddedRecordProvider(), label, "ADD", true);
    drain(source.getRemovedRecordProvider(), label, "REMOVE", false);
  }

  private static void drain(RecordProvider provider, String label, String kind, boolean added) {
    RecordBuffer output = new RecordBuffer(RecordMode.HISTORY_SUBSCRIPTION);
    output.setCapacityLimit(128);
    int count = 0;
    boolean more = true;
    for (int turn = 0; more && turn < 8; ++turn) {
      more = provider.retrieve(output);
      for (RecordCursor cursor; (cursor = output.next()) != null; ) {
        if (++count > 128) throw new IllegalStateException("demand bound exceeded");
        row(
            "wire-demand",
            label,
            kind,
            cursor.getRecord().getName(),
            cursor.getDecodedSymbol(),
            added ? cursor.getTime() : 0);
      }
      output.clear();
    }
    if (more) throw new IllegalStateException("demand turn bound exceeded");
    row("demand-count", label, kind, count);
  }

  private static void attach(DXFeedTimeSeriesSubscription<TimeAndSale> subscription, String name) {
    subscription.addEventListener(
        batch -> {
          for (TimeAndSale event : batch) {
            if (++events > 512) throw new IllegalStateException("event bound exceeded");
            row(
                "delivery",
                phase,
                name,
                subscription.getFromTime(),
                event.getEventSymbol(),
                event.getIndex(),
                event.getTime(),
                event.getEventFlags(),
                event.getPrice());
          }
        });
  }

  private static TimeAndSale event(long millis, double price, int flags) {
    TimeAndSale result = new TimeAndSale("IBM");
    result.setTime(millis);
    result.setSequence(0);
    result.setPrice(price);
    result.setSizeAsDouble(100.5);
    result.setEventFlags(flags);
    return result;
  }

  private static void property(
      DXFeedTimeSeriesSubscription<TimeAndSale> subscription, String name) {
    row("property", phase, name, subscription.getFromTime(), subscription.getSymbols().size());
  }

  private static void observe() {
    BoundedExecutor executor = new BoundedExecutor();
    DXEndpoint endpoint = DXEndpoint.create(DXEndpoint.Role.LOCAL_HUB).executor(executor);
    QDHistory history = ((DXEndpointImpl) endpoint).getQDEndpoint().getHistory();
    QDDistributor source = history.distributorBuilder().build();
    DXFeedTimeSeriesSubscription<TimeAndSale> moving =
        endpoint.getFeed().createTimeSeriesSubscription(TimeAndSale.class);
    DXFeedTimeSeriesSubscription<TimeAndSale> keeper =
        endpoint.getFeed().createTimeSeriesSubscription(TimeAndSale.class);
    try {
      attach(moving, "moving");
      attach(keeper, "keeper");
      property(moving, "moving");
      moving.addSymbols("IBM");
      executor.drain();
      demand(source, "MAX-with-symbol");
      phase = "first-range";
      moving.setFromTime(2000);
      executor.drain();
      property(moving, "moving");
      demand(source, phase);
      endpoint
          .getPublisher()
          .publishEvents(
              Arrays.asList(
                  event(3000, 30.25, IndexedEvent.SNAPSHOT_BEGIN),
                  event(2000, 20.25, IndexedEvent.SNAPSHOT_END)));
      executor.drain();
      phase = "queued-before-shrink";
      endpoint.getPublisher().publishEvents(Arrays.asList(event(2600, 26.25, 0)));
      row("queued", phase, executor.jobs.size());
      phase = "queued-after-shrink";
      moving.setFromTime(3000);
      property(moving, "moving");
      demand(source, phase);
      executor.drain();
      phase = "expand";
      moving.setFromTime(1000);
      executor.drain();
      property(moving, "moving");
      demand(source, phase);
      endpoint
          .getPublisher()
          .publishEvents(
              Arrays.asList(
                  event(4000, 40.25, IndexedEvent.SNAPSHOT_BEGIN),
                  event(2000, 20.25, 0),
                  event(1000, 0, IndexedEvent.SNAPSHOT_END | IndexedEvent.REMOVE_EVENT)));
      executor.drain();
      phase = "disable";
      moving.setFromTime(Long.MAX_VALUE);
      executor.drain();
      property(moving, "moving");
      demand(source, phase);
      phase = "disabled-data";
      int beforeDisabled = events;
      endpoint.getPublisher().publishEvents(Arrays.asList(event(5000, 50.25, 0)));
      executor.drain();
      row("disabled-events", events - beforeDisabled);
      phase = "rapid-latest";
      moving.setFromTime(-1);
      moving.setFromTime(3000);
      executor.drain();
      property(moving, "moving");
      demand(source, phase);
      phase = "empty-property";
      moving.removeSymbols("IBM");
      moving.setFromTime(-1001);
      executor.drain();
      property(moving, "moving");
      demand(source, phase);
      phase = "overlap";
      keeper.setFromTime(0);
      keeper.addSymbols("IBM");
      moving.setFromTime(2000);
      moving.addSymbols("IBM");
      executor.drain();
      demand(source, phase);
      endpoint
          .getPublisher()
          .publishEvents(
              Arrays.asList(
                  event(3000, 31.25, IndexedEvent.SNAPSHOT_BEGIN),
                  event(2000, 21.25, 0),
                  event(1000, 11.25, 0),
                  event(0, 0, IndexedEvent.SNAPSHOT_END | IndexedEvent.REMOVE_EVENT)));
      executor.drain();
      phase = "overlap-shrink";
      moving.setFromTime(3000);
      executor.drain();
      demand(source, phase);
      phase = "remove-moving";
      moving.removeSymbols("IBM");
      executor.drain();
      demand(source, phase);
      phase = "close-keeper";
      keeper.close();
      executor.drain();
      demand(source, phase);
      phase = "closed-property";
      moving.close();
      property(moving, "moving");
      row("total-events", events);
    } finally {
      moving.close();
      keeper.close();
      executor.drain();
      source.close();
      endpoint.close();
    }
  }

  public static void main(String[] arguments) throws Exception {
    if (arguments.length != 1)
      throw new IllegalArgumentException("one private output path required");
    try (PrintStream output = new PrintStream(arguments[0], "UTF-8")) {
      System.setOut(output);
      observe();
    }
  }
}
