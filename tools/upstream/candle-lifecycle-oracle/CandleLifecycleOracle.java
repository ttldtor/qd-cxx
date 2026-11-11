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
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolOption;
import com.devexperts.qd.stats.QDStats;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.IndexedEvent;
import com.dxfeed.event.candle.Candle;
import com.dxfeed.event.candle.CandleDelegate;
import com.dxfeed.event.candle.CandleFactoryImpl;
import com.dxfeed.event.candle.CandleSymbol;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

/**
 * Captures the real Candle delegate and HISTORY collector through a completed snapshot and live
 * update.
 */
public final class CandleLifecycleOracle {
  private static final String SYMBOL = "AAPL{=m}";
  private static final long BOUND_MS = 1000;
  private static final int MAX_TURNS = 8;
  private static final int MAX_WIRE_BYTES = 65536;

  private final DataScheme scheme;
  private final DataRecord record;
  private final CandleDelegate delegate;
  private final Path directory;
  private final PrintWriter out;

  private CandleLifecycleOracle(
      DataScheme scheme,
      DataRecord record,
      CandleDelegate delegate,
      Path directory,
      PrintWriter out) {
    this.scheme = scheme;
    this.record = record;
    this.delegate = delegate;
    this.directory = directory;
    this.out = out;
  }

  private void row(Object... cells) {
    for (int index = 0; index < cells.length; ++index) {
      if (index != 0) out.print('\t');
      out.print(cells[index]);
    }
    out.print('\n');
  }

  private final class Wire {
    private final BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    private final ByteArrayOutput sink = new ByteArrayOutput();

    private Wire() {
      composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
      composer.setOutput(sink);
    }

    private int position() {
      return sink.toByteArray().length;
    }

    private void send(RecordBuffer buffer) {
      buffer.rewind();
      composer.visitData(buffer, MessageType.HISTORY_DATA);
    }

    private void save(String phase, int start) throws Exception {
      byte[] bytes = sink.toByteArray();
      if (bytes.length > MAX_WIRE_BYTES) throw new IllegalStateException("QTP byte bound exceeded");
      Files.write(
          directory.resolve(phase + ".qtp"), Arrays.copyOfRange(bytes, start, bytes.length));
    }
  }

  private QDAgent attach(QDCollector collector, long bound) {
    QDAgent agent =
        collector.agentBuilder().withHistorySnapshot(true).withEventTimeSequence(false).build();
    RecordBuffer demand = new RecordBuffer(RecordMode.HISTORY_SUBSCRIPTION);
    demand.add(record, scheme.getCodec().encode(SYMBOL), SYMBOL).setTime(bound);
    agent.addSubscription(demand);
    return agent;
  }

  private void drainDemand(String phase, QDDistributor distributor) {
    RecordBuffer demand = new RecordBuffer(RecordMode.HISTORY_SUBSCRIPTION);
    boolean more = distributor.getAddedRecordProvider().retrieve(demand);
    int count = 0;
    for (RecordCursor cursor; (cursor = demand.next()) != null; ) {
      row(
          "demand",
          phase,
          ++count,
          cursor.getRecord().getName(),
          cursor.getDecodedSymbol(),
          cursor.getTime());
    }
    if (more) throw new IllegalStateException("Unexpected additional demand");
    row("count", phase, count);
  }

  private void record(String kind, String phase, int position, RecordCursor cursor) {
    Candle event = delegate.createEvent(cursor);
    row(
        kind,
        phase,
        position,
        cursor.getRecord().getName(),
        cursor.getDecodedSymbol(),
        cursor.getTime(),
        cursor.getEventFlags(),
        event.getEventSymbol(),
        event.getIndex(),
        event.getTime(),
        event.getSequence(),
        event.getCount(),
        Double.doubleToRawLongBits(event.getClose()));
  }

  private void drain(String phase, QDAgent agent, Wire wire) throws Exception {
    RecordBuffer output = new RecordBuffer(RecordMode.FLAGGED_DATA);
    output.setCapacityLimit(32);
    int count = 0;
    int start = wire.position();
    boolean more = true;
    for (int turn = 0; more && turn < MAX_TURNS; ++turn) {
      more = agent.retrieve(output);
      int previous = count;
      for (RecordCursor cursor; (cursor = output.next()) != null; )
        record("output", phase, ++count, cursor);
      if (count != previous) wire.send(output);
      output.clear();
    }
    if (more) throw new IllegalStateException("Additional output after retrieval bound");
    wire.save(phase, start);
    row("count", phase, count);
  }

  private void send(
      String phase,
      QDDistributor distributor,
      Wire wire,
      long time,
      int sequence,
      long count,
      double close,
      int flags)
      throws Exception {
    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA);
    Candle event = new Candle(CandleSymbol.valueOf(SYMBOL));
    event.setTime(time);
    event.setSequence(sequence);
    event.setCount(count);
    event.setClose(close);
    event.setEventFlags(flags);
    RecordCursor cursor = delegate.putEvent(event, input);
    record("input", phase, 1, cursor);
    int start = wire.position();
    wire.send(input);
    wire.save(phase, start);
    input.rewind();
    distributor.process(input);
  }

  private void capture() throws Exception {
    long bound = delegate.getQDTimeByEventTime(BOUND_MS);
    row(
        "schema",
        record.getName(),
        record.hasTime(),
        record.getIntFieldCount(),
        record.getObjFieldCount());
    row("bound", BOUND_MS, bound);
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
    QDAgent first = null;
    QDAgent second = null;
    QDAgent third = null;
    try {
      distributor = collector.distributorBuilder().build();
      first = attach(collector, bound);
      drainDemand("initial-add", distributor);
      Wire source = new Wire();
      Wire firstOutput = new Wire();
      Wire secondOutput = new Wire();
      Wire thirdOutput = new Wire();
      send("source-begin", distributor, source, 2000, 7, 11, 101, IndexedEvent.SNAPSHOT_BEGIN);
      drain("first-partial", first, firstOutput);
      send(
          "source-end",
          distributor,
          source,
          BOUND_MS,
          0,
          0,
          Double.NaN,
          IndexedEvent.REMOVE_EVENT | IndexedEvent.SNAPSHOT_END);
      drain("first-completed", first, firstOutput);
      second = attach(collector, bound);
      drainDemand("second-add", distributor);
      drain("second-replay", second, secondOutput);
      send("source-correction", distributor, source, 2000, 7, 5, 102, 0);
      drain("first-correction", first, firstOutput);
      drain("second-correction", second, secondOutput);
      third = attach(collector, bound);
      drainDemand("third-add", distributor);
      drain("third-replay", third, thirdOutput);
    } finally {
      if (third != null) third.close();
      if (second != null) second.close();
      if (first != null) first.close();
      if (distributor != null) distributor.close();
      collector.close();
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "CandleLifecycleOracle <новый-каталог>\n"
                  + "Офлайн-захват HISTORY для Candle в стандартной схеме QD.\n"
                  + "Создаёт новый каталог с таблицей TSV и бинарными фрагментами QTP.\n"
                  + "Без аргументов, --help и --help=ru печатают справку без захвата.\n"
                  + "Код 0 означает успех. Ошибка даёт ненулевой код и может оставить часть файлов."
              : "CandleLifecycleOracle <new-directory>\n"
                  + "Capture Candle HISTORY offline with the standard QD schema.\n"
                  + "Creates a new directory containing one TSV table and binary QTP chunks.\n"
                  + "No arguments, --help and --help=ru display help without a capture.\n"
                  + "Exit 0 means success. Failure exits nonzero and can leave partial output.");
      return;
    }
    if (args.length != 1) throw new IllegalArgumentException("Expected one new output directory");
    Path root = Files.createDirectory(Paths.get(args[0]));
    DataScheme scheme = DXFeedScheme.getInstance();
    DataRecord record = scheme.findRecordByName("Candle");
    if (record == null || !record.hasTime())
      throw new IllegalStateException("Timed Candle record missing");
    CandleDelegate delegate = null;
    for (EventDelegate<?> candidate : new CandleFactoryImpl().createDelegates(record)) {
      if (candidate instanceof CandleDelegate && candidate.getContract() == QDContract.HISTORY) {
        delegate = (CandleDelegate) candidate;
        break;
      }
    }
    if (delegate == null) throw new IllegalStateException("Candle HISTORY delegate missing");
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(root.resolve("oracle.tsv"), StandardCharsets.UTF_8))) {
      new CandleLifecycleOracle(scheme, record, delegate, root, out).capture();
      if (out.checkError()) throw new IllegalStateException("Output write failed");
    }
  }
}
