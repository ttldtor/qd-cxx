// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
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
import com.dxfeed.event.market.TimeAndSale;
import java.io.PrintStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

/** Actual standard delegate and HISTORY collector, without a native/public business DTO claim. */
public final class HistoryReceiverOracle {
  private static Path outputDirectory;
  private static BinaryQTPComposer composer;
  private static ByteArrayOutput wireOutput;
  private static int wireStep;

  private static void row(Object... cells) {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) {
        System.out.print('\t');
      }
      System.out.print(cells[i]);
    }
    System.out.println();
  }

  private static void drain(
      String phase, RecordProvider provider, boolean data, EventDelegate<TimeAndSale> delegate) {
    RecordBuffer output =
        new RecordBuffer(data ? RecordMode.FLAGGED_DATA : RecordMode.HISTORY_SUBSCRIPTION);
    output.setCapacityLimit(128);
    int count = 0;
    boolean more = true;
    for (int turn = 0; more && turn < 8; ++turn) {
      more = provider.retrieve(output);
      for (RecordCursor cursor; (cursor = output.next()) != null; ) {
        if (++count > 256) {
          throw new IllegalStateException("output record bound exceeded");
        }
        if (data) {
          TimeAndSale event = delegate.createEvent(cursor);
          row(
              "output",
              phase,
              count,
              cursor.getRecord().getName(),
              cursor.getDecodedSymbol(),
              cursor.getTime(),
              cursor.getEventFlags(),
              event.getIndex(),
              event.getTime(),
              event.getPrice(),
              event.getSizeAsDouble());
          for (int field = 0; field < cursor.getRecord().getIntFieldCount(); ++field) {
            int type = cursor.getRecord().getIntField(field).getSerialType().getId();
            if (type != 0) {
              long value =
                  cursor.getRecord().getIntField(field).getSerialType().isLong()
                      ? cursor.getLong(field)
                      : cursor.getInt(field);
              row("raw", phase, count, field, type, value);
            }
          }
        } else {
          row(
              "demand",
              phase,
              count,
              cursor.getRecord().getName(),
              cursor.getDecodedSymbol(),
              cursor.getTime());
        }
      }
      output.clear();
    }
    if (more) {
      throw new IllegalStateException("retrieval turn bound exceeded");
    }
    row("count", phase, count);
  }

  private static void subscribe(QDAgent agent, DataRecord record, DataScheme scheme, long bound) {
    RecordBuffer input = new RecordBuffer(RecordMode.HISTORY_SUBSCRIPTION);
    input.add(record, scheme.getCodec().encode("IBM"), "IBM").setTime(bound);
    agent.addSubscription(input);
  }

  private static void send(
      QDDistributor distributor,
      EventDelegate<TimeAndSale> delegate,
      long millis,
      int sequence,
      double price,
      int flags)
      throws Exception {
    TimeAndSale event = new TimeAndSale("IBM");
    event.setTime(millis);
    event.setSequence(sequence);
    event.setPrice(price);
    event.setSizeAsDouble(100.5);
    event.setEventFlags(flags);
    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA);
    RecordCursor cursor = delegate.putEvent(event, input);
    row("input", millis, sequence, cursor.getTime(), cursor.getEventFlags(), event.getIndex());
    int firstByte = wireOutput.toByteArray().length;
    composer.visitData(input, MessageType.HISTORY_DATA);
    byte[] bytes = wireOutput.toByteArray();
    if (bytes.length > 65536) {
      throw new IllegalStateException("wire byte bound exceeded");
    }
    Files.write(
        outputDirectory.resolve("wire-step-" + wireStep++ + ".qtp"),
        Arrays.copyOfRange(bytes, firstByte, bytes.length));
    input.rewind();
    distributor.process(input);
  }

  @SuppressWarnings("unchecked")
  private static void observe() throws Exception {
    DataScheme scheme = DXFeedScheme.getInstance();
    DataRecord record = scheme.findRecordByName("TimeAndSale");
    if (record == null || !record.hasTime()) {
      throw new IllegalStateException("actual standard timed record missing");
    }
    EventDelegate<TimeAndSale> delegate = null;
    for (EventDelegate<?> candidate : new MarketFactoryImpl().createDelegates(record)) {
      if (candidate.getEventType() == TimeAndSale.class
          && candidate.getContract() == QDContract.HISTORY) {
        delegate = (EventDelegate<TimeAndSale>) candidate;
        break;
      }
    }
    if (delegate == null) {
      throw new IllegalStateException("actual HISTORY TimeAndSale delegate missing");
    }
    wireOutput = new ByteArrayOutput();
    composer = new BinaryQTPComposer(scheme, true);
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    composer.setOutput(wireOutput);
    row("wire-options", "hs");
    row(
        "schema",
        record.getName(),
        record.hasTime(),
        record.getIntFieldCount(),
        record.getObjFieldCount());
    row("delegate", delegate.getClass().getName(), delegate.getContract(), delegate.isSub());
    for (int i = 0; i < record.getIntFieldCount(); ++i) {
      row(
          "int-field",
          i,
          record.getIntField(i).getName(),
          record.getIntField(i).getPropertyName(),
          record.getIntField(i).getSerialType().getId());
    }
    for (int i = 0; i < record.getObjFieldCount(); ++i) {
      row(
          "obj-field",
          i,
          record.getObjField(i).getName(),
          record.getObjField(i).getSerialType().getId());
    }
    for (long millis : new long[] {Long.MIN_VALUE, -1001, -1, 0, 1, 999, 1000, Long.MAX_VALUE}) {
      row("bound", millis, delegate.getQDTimeByEventTime(millis));
    }

    QDCollector collector =
        QDFactory.getDefaultFactory()
            .collectorBuilder(QDContract.HISTORY)
            .withScheme(scheme)
            .withStats(QDStats.VOID)
            .withStoreEverything(false)
            .withEventTimeSequence(false)
            .build();
    collector.setErrorHandler(QDErrorHandler.THROW);
    QDDistributor distributor = null;
    QDAgent low = null;
    QDAgent high = null;
    try {
      distributor = collector.distributorBuilder().build();
      low = collector.agentBuilder().withHistorySnapshot(true).withEventTimeSequence(false).build();
      high =
          collector.agentBuilder().withHistorySnapshot(true).withEventTimeSequence(false).build();
      subscribe(low, record, scheme, delegate.getQDTimeByEventTime(0));
      subscribe(high, record, scheme, delegate.getQDTimeByEventTime(2000));
      drain("two-ranges", distributor.getAddedRecordProvider(), false, delegate);
      send(distributor, delegate, 3000, 0, 30.25, IndexedEvent.SNAPSHOT_BEGIN);
      send(distributor, delegate, 2000, 0, 20.25, 0);
      drain("low-partial", low, true, delegate);
      drain("high-covered", high, true, delegate);
      send(distributor, delegate, 0, 0, 0, IndexedEvent.SNAPSHOT_END | IndexedEvent.REMOVE_EVENT);
      drain("low-completed", low, true, delegate);
      drain("high-after-source-end", high, true, delegate);
      send(distributor, delegate, 2000, 1, 21.25, IndexedEvent.TX_PENDING);
      drain("low-pending", low, true, delegate);
      send(distributor, delegate, 2000, 1, 22.25, 0);
      drain("low-commit", low, true, delegate);
      send(distributor, delegate, 4000, 0, 40.25, IndexedEvent.SNAPSHOT_BEGIN);
      send(distributor, delegate, 2000, 0, 23.25, IndexedEvent.SNAPSHOT_SNIP);
      drain("low-snipped", low, true, delegate);
      drain("high-snipped", high, true, delegate);
      low.close();
      low = collector.agentBuilder().withHistorySnapshot(true).withEventTimeSequence(false).build();
      subscribe(low, record, scheme, delegate.getQDTimeByEventTime(0));
      drain("reattached-agent", low, true, delegate);
    } finally {
      if (high != null) {
        high.close();
      }
      if (low != null) {
        low.close();
      }
      if (distributor != null) {
        distributor.close();
      }
      collector.close();
    }
  }

  public static void main(String[] arguments) throws Exception {
    if (arguments.length != 1) {
      throw new IllegalArgumentException("one private output path required");
    }
    try (PrintStream output = new PrintStream(arguments[0], "UTF-8")) {
      outputDirectory = Paths.get(arguments[0] + ".wire");
      Files.createDirectories(outputDirectory);
      System.setOut(output);
      observe();
    }
  }
}
