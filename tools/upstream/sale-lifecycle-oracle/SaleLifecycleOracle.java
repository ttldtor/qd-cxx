// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.QDAgent;
import com.devexperts.qd.QDCollector;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.QDDistributor;
import com.devexperts.qd.QDErrorHandler;
import com.devexperts.qd.QDFactory;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordProvider;
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolOption;
import com.devexperts.qd.stats.QDStats;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.IndexedEvent;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.OptionSale;
import com.dxfeed.event.market.TimeAndSale;
import com.dxfeed.event.market.TimeAndSaleType;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

/** Observes typed sale updates through the actual QD HISTORY collector and QTP composer. */
public final class SaleLifecycleOracle {
  private static final String SYMBOL = "IBM";
  private static final int MAX_RECORDS = 256;
  private static final int MAX_TURNS = 8;

  private final DataScheme scheme;
  private final DataRecord record;
  private final EventDelegate<?> delegate;
  private final boolean option;
  private final Path directory;
  private final PrintWriter out;
  private final BinaryQTPComposer composer;
  private final ByteArrayOutput wire = new ByteArrayOutput();
  private int step;

  private SaleLifecycleOracle(
      DataScheme scheme,
      DataRecord record,
      EventDelegate<?> delegate,
      Path directory,
      PrintWriter out) {
    this.scheme = scheme;
    this.record = record;
    this.delegate = delegate;
    this.option = record.getName().equals("OptionSale");
    this.directory = directory;
    this.out = out;
    this.composer = new BinaryQTPComposer(scheme, true);
    this.composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    this.composer.setOutput(wire);
  }

  private void row(Object... cells) {
    for (int index = 0; index < cells.length; ++index) {
      if (index != 0) {
        out.print('\t');
      }
      out.print(cells[index]);
    }
    out.print('\n');
  }

  private void subscribe(QDAgent agent, long bound) {
    RecordBuffer demand = new RecordBuffer(RecordMode.HISTORY_SUBSCRIPTION);
    demand.add(record, scheme.getCodec().encode(SYMBOL), SYMBOL).setTime(bound);
    agent.addSubscription(demand);
  }

  private void drain(String phase, RecordProvider provider, boolean data) {
    RecordBuffer output =
        new RecordBuffer(data ? RecordMode.FLAGGED_DATA : RecordMode.HISTORY_SUBSCRIPTION);
    output.setCapacityLimit(128);
    int count = 0;
    boolean more = true;
    for (int turn = 0; more && turn < MAX_TURNS; ++turn) {
      more = provider.retrieve(output);
      for (RecordCursor cursor; (cursor = output.next()) != null; ) {
        if (++count > MAX_RECORDS) {
          throw new IllegalStateException("Output record bound exceeded");
        }
        if (!data) {
          row(
              "demand",
              phase,
              count,
              cursor.getRecord().getName(),
              cursor.getDecodedSymbol(),
              cursor.getTime());
        } else if (option) {
          OptionSale event = (OptionSale) delegate.createEvent(cursor);
          row(
              "output",
              phase,
              count,
              cursor.getTime(),
              cursor.getEventFlags(),
              event.getIndex(),
              event.getTimeSequence(),
              event.getTime(),
              event.getSequence(),
              event.getType().getCode(),
              Double.doubleToLongBits(event.getPrice()),
              event.getSource().id());
        } else {
          TimeAndSale event = (TimeAndSale) delegate.createEvent(cursor);
          row(
              "output",
              phase,
              count,
              cursor.getTime(),
              cursor.getEventFlags(),
              event.getIndex(),
              event.getIndex(),
              event.getTime(),
              event.getSequence(),
              event.getType().getCode(),
              Double.doubleToLongBits(event.getPrice()),
              event.getSource().id());
        }
      }
      output.clear();
    }
    if (more) {
      throw new IllegalStateException("Retrieval turn bound exceeded");
    }
    row("count", phase, count);
  }

  private void send(
      QDDistributor distributor,
      String phase,
      int position,
      int sequence,
      double price,
      TimeAndSaleType type,
      int eventFlags)
      throws Exception {
    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA);
    RecordCursor cursor;
    long eventIndex;
    long eventTime;
    if (option) {
      OptionSale event = new OptionSale(SYMBOL);
      event.setIndex(position);
      event.setTime(1000L * (position == -1 ? 3 : position));
      event.setSequence(sequence);
      event.setType(type);
      event.setPrice(price);
      event.setSize(100.5);
      event.setEventFlags(eventFlags);
      cursor = ((com.dxfeed.event.market.OptionSaleDelegate) delegate).putEvent(event, input);
      eventIndex = event.getIndex();
      eventTime = event.getTime();
    } else {
      TimeAndSale event = new TimeAndSale(SYMBOL);
      event.setTime(1000L * position);
      event.setSequence(sequence);
      event.setType(type);
      event.setPrice(price);
      event.setSizeAsDouble(100.5);
      event.setEventFlags(eventFlags);
      cursor = ((com.dxfeed.event.market.TimeAndSaleDelegate) delegate).putEvent(event, input);
      eventIndex = event.getIndex();
      eventTime = event.getTime();
    }
    row(
        "input",
        phase,
        position,
        eventIndex,
        eventTime,
        sequence,
        type.getCode(),
        eventFlags,
        cursor.getTime());
    int start = wire.toByteArray().length;
    composer.visitData(input, MessageType.HISTORY_DATA);
    byte[] bytes = wire.toByteArray();
    if (bytes.length > 65536) {
      throw new IllegalStateException("QTP byte bound exceeded");
    }
    Files.write(
        directory.resolve(String.format("wire-%02d.qtp", step++)),
        Arrays.copyOfRange(bytes, start, bytes.length));
    input.rewind();
    distributor.process(input);
  }

  private QDCollector collector() {
    QDCollector result =
        QDFactory.getDefaultFactory()
            .collectorBuilder(QDContract.HISTORY)
            .withScheme(scheme)
            .withStats(QDStats.VOID)
            .withStoreEverything(false)
            .withEventTimeSequence(false)
            .build();
    result.setErrorHandler(QDErrorHandler.THROW);
    return result;
  }

  private void capture() throws Exception {
    long lowBound = 0;
    long highBound = option ? 2 : delegate.getQDTimeByEventTime(2000);
    row(
        "schema",
        record.getName(),
        record.hasTime(),
        record.getIntFieldCount(),
        record.getObjFieldCount());
    row("bounds", lowBound, highBound);
    QDCollector collector = collector();
    QDDistributor distributor = null;
    QDAgent low = null;
    QDAgent high = null;
    try {
      distributor = collector.distributorBuilder().build();
      low = collector.agentBuilder().withHistorySnapshot(true).withEventTimeSequence(false).build();
      high =
          collector.agentBuilder().withHistorySnapshot(true).withEventTimeSequence(false).build();
      subscribe(low, lowBound);
      subscribe(high, highBound);
      drain("demand-initial", distributor.getAddedRecordProvider(), false);

      send(
          distributor,
          "begin",
          option ? -1 : 3,
          0,
          30.25,
          TimeAndSaleType.NEW,
          IndexedEvent.SNAPSHOT_BEGIN);
      send(distributor, "partial", 2, 0, 20.25, TimeAndSaleType.NEW, 0);
      drain("low-partial", low, true);
      drain("high-covered", high, true);
      send(
          distributor,
          "end",
          0,
          0,
          0,
          TimeAndSaleType.NEW,
          IndexedEvent.SNAPSHOT_END | IndexedEvent.REMOVE_EVENT);
      drain("low-completed", low, true);
      drain("high-after-end", high, true);

      send(
          distributor,
          "correction-pending",
          2,
          1,
          21.25,
          TimeAndSaleType.CORRECTION,
          IndexedEvent.TX_PENDING);
      drain("low-correction-pending", low, true);
      send(distributor, "cancel-committed", 2, 1, 22.25, TimeAndSaleType.CANCEL, 0);
      drain("low-cancel-committed", low, true);
      send(distributor, "remove", 2, 1, 0, TimeAndSaleType.NEW, IndexedEvent.REMOVE_EVENT);
      drain("low-indexed-removal", low, true);

      send(
          distributor,
          "restart",
          option ? -1 : 3,
          2,
          31.25,
          TimeAndSaleType.NEW,
          IndexedEvent.SNAPSHOT_BEGIN);
      drain("low-restarted", low, true);
      drain("high-restarted", high, true);
      send(distributor, "snip", 2, 0, 23.25, TimeAndSaleType.NEW, IndexedEvent.SNAPSHOT_SNIP);
      drain("low-truncated", low, true);
      drain("high-truncated", high, true);
      low.close();
      low = collector.agentBuilder().withHistorySnapshot(true).withEventTimeSequence(false).build();
      subscribe(low, lowBound);
      drain("reattached-agent", low, true);
    } finally {
      if (high != null) high.close();
      if (low != null) low.close();
      if (distributor != null) distributor.close();
      collector.close();
    }

    QDCollector fresh = collector();
    QDAgent next = null;
    QDDistributor nextDistributor = null;
    try {
      Path freshDirectory = Files.createDirectory(directory.resolve("fresh"));
      SaleLifecycleOracle freshSource =
          new SaleLifecycleOracle(scheme, record, delegate, freshDirectory, out);
      nextDistributor = fresh.distributorBuilder().build();
      next = fresh.agentBuilder().withHistorySnapshot(true).withEventTimeSequence(false).build();
      subscribe(next, lowBound);
      drain("fresh-demand", nextDistributor.getAddedRecordProvider(), false);
      drain("fresh-collector-before-data", next, true);
      freshSource.send(
          nextDistributor,
          "fresh-begin",
          option ? -1 : 5,
          4,
          51.25,
          TimeAndSaleType.NEW,
          IndexedEvent.SNAPSHOT_BEGIN);
      drain("fresh-partial", next, true);
      freshSource.send(
          nextDistributor,
          "fresh-end",
          0,
          0,
          0,
          TimeAndSaleType.NEW,
          IndexedEvent.SNAPSHOT_END | IndexedEvent.REMOVE_EVENT);
      drain("fresh-completed", next, true);
    } finally {
      if (next != null) next.close();
      if (nextDistributor != null) nextDistributor.close();
      fresh.close();
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "SaleLifecycleOracle <новый-каталог>: офлайн HISTORY QD. Успех: 0; ошибка:"
                  + " ненулевой код."
              : "SaleLifecycleOracle <new-directory>: offline QD HISTORY capture. Exit 0 on"
                  + " success, nonzero on failure.");
      return;
    }
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected one new output directory");
    }
    Path root = Files.createDirectory(Paths.get(args[0]));
    DataScheme scheme = DXFeedScheme.getInstance();
    for (String name : Arrays.asList("TimeAndSale", "OptionSale")) {
      DataRecord record = scheme.findRecordByName(name);
      if (record == null || !record.hasTime()) {
        throw new IllegalStateException("Timed record missing: " + name);
      }
      EventDelegate<?> delegate = null;
      for (EventDelegate<?> candidate : new MarketFactoryImpl().createDelegates(record)) {
        if (candidate.getContract() == QDContract.HISTORY) {
          delegate = candidate;
          break;
        }
      }
      if (delegate == null) {
        throw new IllegalStateException("HISTORY delegate missing: " + name);
      }
      Path directory = Files.createDirectory(root.resolve(name));
      try (PrintWriter out =
          new PrintWriter(
              Files.newBufferedWriter(directory.resolve("oracle.tsv"), StandardCharsets.UTF_8))) {
        new SaleLifecycleOracle(scheme, record, delegate, directory, out).capture();
        if (out.checkError()) throw new IllegalStateException("Output write failed");
      }
    }
  }
}
