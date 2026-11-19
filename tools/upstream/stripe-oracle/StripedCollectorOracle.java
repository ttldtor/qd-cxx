// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.QDAgent;
import com.devexperts.qd.QDCollector;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.QDDistributor;
import com.devexperts.qd.QDErrorHandler;
import com.devexperts.qd.QDStream;
import com.devexperts.qd.SymbolStriper;
import com.devexperts.qd.impl.stripe.StripedFactory;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.CompositeFilters;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordProvider;
import com.devexperts.qd.stats.QDStats;
import java.io.PrintStream;

/**
 * Internal harness: Invoke-Oracle.ps1 compiles pinned qd-stripe sources into the private run
 * classes, then invokes this class with one output path. Writes actual production observations; a
 * launcher deadline/heap bounds failures without an arbitrary output-row ceiling. No upstream tree
 * is rebuilt.
 */
public final class StripedCollectorOracle {
  private static PrintStream output;
  private static final int SINK_CAPACITY = 2;
  private static final DataRecord RECORD =
      new DefaultRecord(
          0,
          "Oracle#ONE",
          true,
          new DataIntField[] {
            new CompactIntField(0, "Oracle#ONE.TimeHigh"),
            new CompactIntField(1, "Oracle#ONE.TimeLow"),
            new CompactIntField(2, "Oracle#ONE.Value")
          },
          new DataObjField[0]);
  private static final DataScheme SCHEME = new DefaultScheme(PentaCodec.INSTANCE, RECORD);

  private static String units(String text) {
    StringBuilder result = new StringBuilder();
    for (int index = 0; index < text.length(); index++) {
      if (index != 0) {
        result.append(',');
      }
      result.append(String.format(java.util.Locale.ROOT, "%04X", (int) text.charAt(index)));
    }
    return result.toString();
  }

  private static void row(Object... cells) {
    for (int index = 0; index < cells.length; index++) {
      if (index != 0) {
        output.print('\t');
      }
      output.print(cells[index]);
    }
    output.println();
  }

  private static QDCollector collector(QDContract contract, String spec) {
    QDCollector collector =
        new StripedFactory(0)
            .collectorBuilder(contract)
            .withScheme(SCHEME)
            .withStriper(SymbolStriper.definedValueOf(SCHEME, spec))
            .withStats(QDStats.VOID)
            .withStoreEverything(false)
            .withEventTimeSequence(false)
            .build();
    collector.setErrorHandler(QDErrorHandler.THROW);
    if (collector instanceof QDStream) {
      ((QDStream) collector).setEnableWildcards(true);
    }
    return collector;
  }

  private static void subscription(QDContract contract, QDAgent agent, String... symbols) {
    RecordBuffer source = new RecordBuffer(RecordMode.addedSubscriptionFor(contract));
    for (String symbol : symbols) {
      int cipher = SCHEME.getCodec().encode(symbol);
      RecordCursor cursor = source.add(RECORD, cipher, cipher == 0 ? symbol : null);
      if (contract == QDContract.HISTORY) {
        cursor.setTime(0);
      }
    }
    agent.setSubscription(source);
  }

  private static void data(QDDistributor distributor, int firstValue, String... symbols) {
    RecordBuffer source = new RecordBuffer(RecordMode.FLAGGED_DATA);
    for (int index = 0; index < symbols.length; index++) {
      int cipher = SCHEME.getCodec().encode(symbols[index]);
      RecordCursor cursor = source.add(RECORD, cipher, cipher == 0 ? symbols[index] : null);
      cursor.setTime(index + 10);
      cursor.setInt(2, firstValue + index);
      cursor.setEventFlags(0);
    }
    distributor.process(source);
  }

  private static void drain(String name, String phase, RecordProvider provider) {
    RecordBuffer sink = new RecordBuffer(RecordMode.FLAGGED_DATA);
    sink.setCapacityLimit(SINK_CAPACITY);
    int ordinal = 0;
    int turns = 0;
    boolean more;
    do {
      sink.clear();
      more = provider.retrieve(sink);
      turns++;
      for (RecordCursor cursor; (cursor = sink.next()) != null; ordinal++) {
        row(
            "record",
            name,
            phase,
            ordinal,
            units(cursor.getDecodedSymbol()),
            cursor.getTime(),
            cursor.getInt(2),
            cursor.getEventFlags());
      }
    } while (more);
    row("drain", name, phase, ordinal, turns, more);
  }

  private static void ordinary(QDContract contract, String spec) {
    String name = contract + ":" + spec;
    QDCollector collector = collector(contract, spec);
    QDDistributor distributor = collector.distributorBuilder().build();
    QDAgent first = collector.agentBuilder().withHistorySnapshot(true).build();
    QDAgent second = collector.agentBuilder().withHistorySnapshot(true).build();
    try {
      subscription(contract, first, "AAPL", "IBM", "MSFT");
      subscription(contract, second, "IBM", "SPY");
      row(
          "size",
          name,
          "initial",
          collector.getSubscriptionSize(),
          first.getSubscriptionSize(),
          second.getSubscriptionSize());
      data(distributor, 101, "AAPL", "IBM", "MSFT", "SPY", "Z");
      drain(name, "first", first);
      drain(name, "second", second);
      drain(name, "snapshot-first", first.getSnapshotProvider());
      row("drops", name, "initial", first.getDroppedRecords(), second.getDroppedRecords());
      subscription(contract, first, "SPY");
      row(
          "size",
          name,
          "replacement",
          collector.getSubscriptionSize(),
          first.getSubscriptionSize(),
          second.getSubscriptionSize());
      data(distributor, 201, "AAPL", "IBM", "MSFT", "SPY", "Z");
      drain(name, "replacement-first", first);
      drain(name, "replacement-second", second);
      first.close();
      row(
          "size",
          name,
          "first-closed",
          collector.getSubscriptionSize(),
          first.getSubscriptionSize(),
          second.getSubscriptionSize());
    } finally {
      first.close();
      second.close();
      distributor.close();
      collector.close();
    }
    QDCollector[] children = StripedFactory.getStripes(collector);
    for (int stripe = 0; stripe < children.length; stripe++) {
      row("closed-child", name, stripe, children[stripe].getSubscriptionSize());
    }
  }

  private static void hintAndFilter() {
    String spec = "byhash4";
    SymbolStriper fine = SymbolStriper.definedValueOf(SCHEME, "byhash8");
    String[] selected = new String[2];
    for (int index = 0; selected[0] == null || selected[1] == null; index++) {
      if (index == Integer.MAX_VALUE) {
        throw new IllegalStateException("No finite representative found");
      }
      String symbol = "SYM" + index;
      int stripe = fine.getStripeIndex(symbol);
      if (stripe < 2 && selected[stripe] == null) {
        selected[stripe] = symbol;
      }
    }
    QDCollector collector = collector(QDContract.STREAM, spec);
    QDDistributor distributor = collector.distributorBuilder().build();
    QDAgent hint =
        collector.agentBuilder().withStripe(CompositeFilters.valueOf("hash0of8", SCHEME)).build();
    QDAgent filtered =
        collector
            .agentBuilder()
            .withStripe(CompositeFilters.valueOf("hash0of8", SCHEME))
            .withFilter(CompositeFilters.valueOf("hash0of8", SCHEME))
            .build();
    try {
      subscription(QDContract.STREAM, hint, "*");
      subscription(QDContract.STREAM, filtered, "*");
      row("hint-input", spec, units(selected[0]), units(selected[1]));
      data(distributor, 301, selected);
      drain(spec, "hint-only", hint);
      drain(spec, "hint-and-filter", filtered);
      row(
          "size",
          spec,
          "wildcard-hint",
          collector.getSubscriptionSize(),
          hint.getSubscriptionSize(),
          filtered.getSubscriptionSize());
    } finally {
      hint.close();
      filtered.close();
      distributor.close();
      collector.close();
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Invoke through Invoke-Oracle.ps1 with one output path");
    }
    try (PrintStream sink = new PrintStream(args[0], "UTF-8")) {
      output = sink;
      for (QDContract contract :
          new QDContract[] {QDContract.STREAM, QDContract.TICKER, QDContract.HISTORY}) {
        for (String spec : new String[] {"by1", "byhash4", "byrange-D-M-"}) {
          ordinary(contract, spec);
        }
      }
      hintAndFilter();
      if (output.checkError()) {
        throw new java.io.IOException("Cannot write stripe observations");
      }
    }
  }
}
