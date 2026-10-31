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
import com.devexperts.qd.QDFactory;
import com.devexperts.qd.QDStream;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.EventFlag;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordProvider;
import com.devexperts.qd.stats.QDStats;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Run: java -cp CLASSES;QD_CLASSPATH qdcxx.oracle.CollectorOracle --output NEW_DIRECTORY. Uses
 * bounded synthetic records with production collectors; writes UTF-8 LF TSV observations. No
 * arguments or --help prints help without constructing QD objects or touching files. Invalid
 * arguments exit 3; capture failures exit 1 and preserve the fresh directory for diagnosis.
 * Observed agent flags are raw QD output, not a dxFeed IndexedEvent model completion claim.
 */
public final class CollectorOracle {
  private static final String HELP =
      "Usage: CollectorOracle --output NEW_DIRECTORY\n"
          + "Captures bounded synthetic QD collector observations offline.\n"
          + "NEW_DIRECTORY must not exist; parent must exist. No defaults or network input.\n"
          + "Writes observations.tsv and cases.tsv in UTF-8 with LF line endings.\n"
          + "Exit codes: 0 success/help, 3 invalid arguments/output path, 1 capture failure.\n";
  private final StringBuilder observations = new StringBuilder("case\tphase\tindex\tkey\tvalue\n");
  private final StringBuilder cases =
      new StringBuilder("case\tevidence_kind\tproduction_path\toutcome\n");
  private final Set<String> coordinates = new HashSet<>();
  private String currentCase;
  private static final String A = "ALPHA";
  private static final String B = "BETA";
  private static final int TX = EventFlag.TX_PENDING.flag();
  private static final int REMOVE = EventFlag.REMOVE_EVENT.flag();
  private static final int BEGIN = EventFlag.SNAPSHOT_BEGIN.flag();
  private static final int END = EventFlag.SNAPSHOT_END.flag();
  private static final int SNIP = EventFlag.SNAPSHOT_SNIP.flag();

  public static void main(String[] args) {
    if (args.length == 0 || (args.length == 1 && "--help".equals(args[0]))) {
      System.out.print(HELP);
      if (System.out.checkError()) {
        System.exit(1);
      }
      return;
    }
    if (args.length != 2 || !"--output".equals(args[0]) || args[1].isEmpty()) {
      System.err.print(HELP);
      System.exit(3);
    }
    Path output;
    try {
      output = Paths.get(args[1]).toAbsolutePath();
      if (Files.exists(output) || !Files.isDirectory(output.getParent())) {
        throw new IllegalArgumentException("output must be fresh and its parent must exist");
      }
      Files.createDirectory(output);
    } catch (Exception e) {
      System.err.println("Invalid output path: " + e.getClass().getSimpleName());
      System.exit(3);
      return;
    }
    CollectorOracle oracle = new CollectorOracle();
    try {
      oracle.run();
      oracle.write(output);
    } catch (Exception e) {
      try {
        oracle.write(output);
      } catch (IOException writeFailure) {
        e.addSuppressed(writeFailure);
      }
      e.printStackTrace(System.err);
      System.exit(1);
    }
  }

  private void write(Path output) throws IOException {
    Files.write(
        output.resolve("observations.tsv"),
        observations.toString().getBytes(StandardCharsets.UTF_8));
    Files.write(output.resolve("cases.tsv"), cases.toString().getBytes(StandardCharsets.UTF_8));
  }

  private void run() {
    for (QDContract contract :
        new QDContract[] {QDContract.STREAM, QDContract.TICKER, QDContract.HISTORY}) {
      duplicateMembership(contract);
      wildcard(contract, false);
    }
    streamOrder();
    streamIdenticalFilter();
    wildcard(QDContract.STREAM, true);
    ticker(false);
    ticker(true);
    historyBounds();
    historySnapshot("empty", true);
    historySnapshot("complete", true);
    historySnapshot("snip", true);
    historySnapshot("restart-overlap", true);
    historySnapshot("complete", false);
    historyTransactions(false);
    historyTransactions(true);
    historyIsolation();
    historyInterrupted();
    historyUntimedRecord();
  }

  private void start(String name) {
    currentCase = name;
  }

  private void finish(Hub hub) {
    cases
        .append(currentCase)
        .append("\tproduction-observation\t")
        .append("QDFactory.getDefaultFactory/collectorBuilder/")
        .append(hub.collector.getContract().name())
        .append("/QDAgent/QDDistributor/RecordBuffer\tobserved\n");
  }

  private void row(String phase, int index, String key, String value) {
    String coordinate = currentCase + "\t" + phase + "\t" + index + "\t" + key;
    if (!coordinates.add(coordinate)) {
      throw new IllegalStateException("duplicate observation coordinate: " + coordinate);
    }
    observations.append(coordinate).append('\t').append(value).append('\n');
  }

  private void integer(String phase, int index, String key, long value) {
    row(phase, index, key, "i:" + value);
  }

  private void bool(String phase, String key, boolean value) {
    row(phase, 0, key, value ? "b:1" : "b:0");
  }

  private static String utf16(String value) {
    if (value == null) {
      return "n:null";
    }
    StringBuilder result = new StringBuilder("u:");
    for (int i = 0; i < value.length(); i++) {
      String hex = Integer.toHexString(value.charAt(i));
      for (int pad = hex.length(); pad < 4; pad++) {
        result.append('0');
      }
      result.append(hex);
    }
    return result.toString();
  }

  private void records(String phase, RecordProvider provider, RecordMode mode, boolean data) {
    RecordBuffer buffer = new RecordBuffer(mode);
    int ordinal = 0;
    boolean more = true;
    for (int batch = 0; more && batch < 16; batch++) {
      more = provider.retrieve(buffer);
      for (RecordCursor cursor; (cursor = buffer.next()) != null; ordinal++) {
        if (ordinal >= 256) {
          throw new IllegalStateException("record bound exceeded");
        }
        row(phase, ordinal, "record", utf16(cursor.getRecord().getName()));
        row(phase, ordinal, "symbol", utf16(cursor.getDecodedSymbol()));
        integer(phase, ordinal, "cipher", cursor.getCipher());
        row(phase, ordinal, "raw-symbol", utf16(cursor.getSymbol()));
        if (cursor.hasTime()) {
          integer(phase, ordinal, "raw-time", cursor.getTime());
        }
        if (data) {
          integer(phase, ordinal, "value", cursor.getInt(2));
          integer(phase, ordinal, "flags", cursor.getEventFlags());
        }
      }
      buffer.clear();
    }
    if (more) {
      throw new IllegalStateException("retrieve batch bound exceeded");
    }
    integer(phase, -1, "record-count", ordinal);
    bool(phase, "more", more);
  }

  private void data(String phase, QDAgent agent) {
    records(phase, agent, RecordMode.FLAGGED_DATA, true);
  }

  private void aggregate(String phase, Hub hub) {
    records(
        phase + "-add",
        hub.distributor.getAddedRecordProvider(),
        RecordMode.addedSubscriptionFor(hub.collector.getContract()),
        false);
    records(
        phase + "-remove",
        hub.distributor.getRemovedRecordProvider(),
        RecordMode.SUBSCRIPTION,
        false);
  }

  private void duplicateMembership(QDContract contract) {
    start(contract.name().toLowerCase(java.util.Locale.ROOT) + "-duplicate-two-clients");
    try (Hub hub = new Hub(contract, false, false, true)) {
      QDAgent one = hub.agent(true);
      QDAgent two = hub.agent(true);
      hub.sub(one, false, hub.first, A, 2);
      aggregate("first-add", hub);
      hub.sub(one, false, hub.first, A, 2);
      aggregate("duplicate-add", hub);
      hub.sub(two, false, hub.first, A, 2);
      aggregate("second-client-add", hub);
      hub.send(hub.first, A, 3, 11, 0);
      data("first-client-data", one);
      data("second-client-data", two);
      hub.sub(one, true, hub.first, A, 0);
      aggregate("first-client-remove", hub);
      hub.sub(one, true, hub.first, A, 0);
      aggregate("duplicate-remove", hub);
      hub.send(hub.first, A, 4, 12, 0);
      data("removed-client-data", one);
      data("remaining-client-data", two);
      hub.sub(two, true, hub.first, A, 0);
      aggregate("last-client-remove", hub);
      finish(hub);
    }
  }

  private void streamOrder() {
    start("stream-every-event-order");
    try (Hub hub = new Hub(QDContract.STREAM, false, false, true)) {
      QDAgent agent = hub.agent(false);
      hub.sub(agent, false, hub.first, A, 0);
      hub.sub(agent, false, hub.first, B, 0);
      hub.sendBatch(
          new Item(hub.first, A, 1, 10, 0),
          new Item(hub.first, B, 1, 20, 0),
          new Item(hub.first, A, 1, 11, 0),
          new Item(hub.first, A, 1, 12, 0));
      data("single-batch", agent);
      hub.send(hub.first, B, 2, 21, 0);
      hub.send(hub.first, A, 2, 13, 0);
      data("separate-batches", agent);
      finish(hub);
    }
  }

  private void streamIdenticalFilter() {
    start("stream-identical-filter");
    try (Hub hub = new Hub(QDContract.STREAM, false, false, true)) {
      QDAgent agent = hub.agent(false);
      hub.send(hub.first, A, 1, 9, 0);
      hub.sub(agent, false, hub.first, A, 0);
      data("after-add", agent);
      hub.sendBatch(
          new Item(hub.first, A, 1, 10, 0),
          new Item(hub.first, A, 1, 10, 0),
          new Item(hub.first, B, 1, 20, 0),
          new Item(hub.second, A, 1, 30, 0),
          new Item(hub.first, A, 1, 10, 0));
      data("filtered-batch", agent);
      hub.send(hub.first, A, 2, 11, 0);
      data("next-batch", agent);
      finish(hub);
    }
  }

  private void wildcard(QDContract contract, boolean enabled) {
    start(contract.name().toLowerCase(java.util.Locale.ROOT) + "-wildcard-" + enabled);
    try (Hub hub = new Hub(contract, false, enabled, true)) {
      QDAgent agent = hub.agent(true);
      hub.sub(agent, false, hub.first, "*", 0);
      aggregate("wildcard-only", hub);
      hub.send(hub.first, A, 2, 10, 0);
      hub.send(hub.first, B, 2, 20, 0);
      data("wildcard-only-data", agent);
      hub.sub(agent, false, hub.first, A, 0);
      aggregate("exact-overlap", hub);
      hub.send(hub.first, A, 3, 11, 0);
      hub.send(hub.first, B, 3, 21, 0);
      data("overlap-data", agent);
      hub.sub(agent, true, hub.first, "*", 0);
      aggregate("wildcard-removed", hub);
      hub.send(hub.first, A, 4, 12, 0);
      hub.send(hub.first, B, 4, 22, 0);
      data("exact-remains", agent);
      finish(hub);
    }
  }

  private void ticker(boolean storeEverything) {
    start("ticker-latest-retention-" + storeEverything);
    try (Hub hub = new Hub(QDContract.TICKER, storeEverything, false, true)) {
      QDAgent agent = hub.agent(false);
      hub.send(hub.first, A, 1, 10, 0);
      hub.sub(agent, false, hub.first, A, 0);
      data("initial-after-unsubscribed-data", agent);
      hub.send(hub.first, A, 2, 20, 0);
      hub.send(hub.first, A, 3, 30, 0);
      data("latest-before-retrieve", agent);
      QDAgent late = hub.agent(false);
      hub.sub(late, false, hub.first, A, 0);
      data("late-agent-initial", late);
      hub.send(hub.first, A, 4, 40, 0);
      data("live-first", agent);
      data("live-late", late);
      hub.sub(agent, true, hub.first, A, 0);
      hub.sub(late, true, hub.first, A, 0);
      hub.sub(agent, false, hub.first, A, 0);
      data("last-unsubscribe-readd", agent);
      hub.sub(agent, true, hub.first, A, 0);
      hub.send(hub.first, A, 5, 50, 0);
      hub.sub(agent, false, hub.first, A, 0);
      data("unsubscribed-update-readd", agent);
      finish(hub);
    }
  }

  private void historyBounds() {
    start("history-raw-bound-change-aggregate-inclusive");
    try (Hub hub = new Hub(QDContract.HISTORY, true, false, true)) {
      long lower = (1L << 32) + 2;
      QDAgent one = hub.agent(true);
      QDAgent two = hub.agent(true);
      hub.sub(one, false, hub.first, A, lower);
      aggregate("first-bound", hub);
      hub.sub(two, false, hub.first, A, lower - 1);
      aggregate("earlier-client", hub);
      hub.sendBatch(
          new Item(hub.first, A, lower + 1, 31, BEGIN),
          new Item(hub.first, A, lower, 30, 0),
          new Item(hub.first, A, lower - 1, 29, END));
      data("inclusive-first", one);
      data("inclusive-second", two);
      hub.sub(two, false, hub.first, A, lower + 1);
      aggregate("second-bound-later", hub);
      data("changed-bound-data", two);
      hub.sub(one, true, hub.first, A, 0);
      aggregate("remove-earliest", hub);
      hub.sub(two, false, hub.first, A, -1);
      aggregate("negative-bound", hub);
      data("negative-bound-data", two);
      finish(hub);
    }
  }

  private void historySnapshot(String kind, boolean snapshotAgent) {
    start("history-snapshot-" + kind + "-agent-" + snapshotAgent);
    try (Hub hub = new Hub(QDContract.HISTORY, false, false, snapshotAgent)) {
      QDAgent agent = hub.agent(snapshotAgent);
      hub.sub(agent, false, hub.first, A, 0);
      if ("empty".equals(kind)) {
        hub.send(hub.first, A, 0, 0, BEGIN | END | REMOVE);
        data("empty-snapshot", agent);
      } else {
        hub.sendBatch(
            new Item(hub.first, A, 3, 30, BEGIN),
            new Item(hub.first, A, 2, 20, 0),
            new Item(hub.first, A, 0, 0, END | REMOVE));
        data("baseline-complete", agent);
        if ("snip".equals(kind)) {
          hub.sendBatch(new Item(hub.first, A, 5, 50, BEGIN), new Item(hub.first, A, 2, 22, SNIP));
          data("snipped", agent);
        } else if ("restart-overlap".equals(kind)) {
          hub.send(hub.first, A, 4, 40, BEGIN);
          data("incomplete-first-begin", agent);
          hub.send(hub.first, A, 3, 33, BEGIN);
          data("restarted-begin", agent);
          hub.sendBatch(
              new Item(hub.first, A, 1, 11, 0), new Item(hub.first, A, 0, 0, END | REMOVE));
          data("restart-completed", agent);
          QDAgent late = hub.agent(true);
          hub.sub(late, false, hub.first, A, 0);
          data("late-current-snapshot", late);
        } else {
          hub.send(hub.first, A, 2, 21, 0);
          data("live-update", agent);
          hub.send(hub.first, A, 2, 0, REMOVE);
          data("remove-existing", agent);
          hub.send(hub.first, A, 2, 0, REMOVE);
          data("remove-absent", agent);
        }
      }
      finish(hub);
    }
  }

  private void historyTransactions(boolean batch) {
    start("history-transaction-" + (batch ? "single-batch" : "separate-batches"));
    try (Hub hub = new Hub(QDContract.HISTORY, false, false, true)) {
      QDAgent agent = hub.agent(true);
      hub.sub(agent, false, hub.first, A, 0);
      hub.sendBatch(
          new Item(hub.first, A, 3, 30, BEGIN),
          new Item(hub.first, A, 2, 20, 0),
          new Item(hub.first, A, 0, 0, END | REMOVE));
      data("baseline", agent);
      Item first = new Item(hub.first, A, 3, 31, TX);
      Item second = new Item(hub.first, A, 2, 21, TX);
      Item last = new Item(hub.first, A, 1, 11, 0);
      if (batch) {
        hub.sendBatch(first, second, last);
        data("transaction-batch", agent);
      } else {
        hub.sendBatch(first);
        data("pending-first", agent);
        hub.sendBatch(second);
        data("pending-second", agent);
        hub.sendBatch(last);
        data("transaction-end", agent);
      }
      finish(hub);
    }
  }

  private void historyIsolation() {
    start("history-transaction-record-key-isolation");
    try (Hub hub = new Hub(QDContract.HISTORY, false, false, true)) {
      QDAgent agent = hub.agent(true);
      hub.sub(agent, false, hub.first, A, 0);
      hub.sub(agent, false, hub.first, B, 0);
      hub.sub(agent, false, hub.second, A, 0);
      hub.send(hub.first, A, 3, 30, BEGIN | TX);
      data("record-A-pending", agent);
      hub.send(hub.first, B, 0, 0, BEGIN | END | REMOVE);
      data("other-key-complete", agent);
      hub.send(hub.second, A, 0, 0, BEGIN | END | REMOVE);
      data("other-record-complete", agent);
      hub.send(hub.first, A, 0, 0, END | REMOVE);
      data("record-A-complete", agent);
      finish(hub);
    }
  }

  private void historyInterrupted() {
    start("history-interrupted-transaction-unsubscribe");
    try (Hub hub = new Hub(QDContract.HISTORY, false, false, true)) {
      QDAgent one = hub.agent(true);
      QDAgent keeper = hub.agent(true);
      hub.sub(one, false, hub.first, A, 0);
      hub.sub(keeper, false, hub.first, A, 0);
      hub.send(hub.first, A, 3, 30, BEGIN | TX);
      data("pending-before-withdraw", one);
      hub.sub(one, true, hub.first, A, 0);
      data("withdrawn-agent", one);
      hub.sub(one, false, hub.first, A, 0);
      data("readded-pending-view", one);
      hub.send(hub.first, A, 0, 0, END | REMOVE);
      data("readded-after-end", one);
      data("keeper-after-end", keeper);
      finish(hub);
    }
  }

  private void historyUntimedRecord() {
    start("history-rejects-untimed-record");
    try (Hub hub = new Hub(QDContract.HISTORY, false, false, true)) {
      QDAgent agent = hub.agent(true);
      try {
        hub.sub(agent, false, hub.untimed, A, 0);
        row("untimed-add", 0, "exception", "n:null");
      } catch (IllegalArgumentException e) {
        row("untimed-add", 0, "exception", utf16(e.getClass().getName()));
        row("untimed-add", 0, "message", utf16(e.getMessage()));
      }
      aggregate("after-untimed-add", hub);
      finish(hub);
    }
  }

  private static final class Item {
    final DataRecord record;
    final String symbol;
    final long time;
    final int value;
    final int flags;

    Item(DataRecord record, String symbol, long time, int value, int flags) {
      this.record = record;
      this.symbol = symbol;
      this.time = time;
      this.value = value;
      this.flags = flags;
    }
  }

  private final class Hub implements AutoCloseable {
    final DataRecord first = record(0, "Oracle#ONE");
    final DataRecord second = record(1, "Oracle#TWO");
    final DataRecord untimed =
        new DefaultRecord(
            2,
            "Untimed",
            false,
            new DataIntField[] {new CompactIntField(0, "Untimed.Value")},
            new DataObjField[0]);
    final DataScheme scheme = new DefaultScheme(PentaCodec.INSTANCE, first, second, untimed);
    final QDCollector collector;
    final QDDistributor distributor;
    final List<QDAgent> agents = new ArrayList<>();
    int subscriptionOrdinal;
    int dataBatchOrdinal;

    Hub(QDContract contract, boolean storeEverything, boolean wildcards, boolean snapshot) {
      collector =
          QDFactory.getDefaultFactory()
              .collectorBuilder(contract)
              .withScheme(scheme)
              .withStats(QDStats.VOID)
              .withStoreEverything(storeEverything)
              .withEventTimeSequence(false)
              .build();
      collector.setErrorHandler(QDErrorHandler.THROW);
      if (collector instanceof QDStream) {
        ((QDStream) collector).setEnableWildcards(wildcards);
      }
      distributor = collector.distributorBuilder().build();
      row("options", 0, "contract", "t:" + contract.name());
      bool("options", "store-everything", storeEverything);
      bool("options", "stream-wildcards", wildcards);
      bool("options", "history-snapshot-agent", snapshot);
      bool("options", "event-time-sequence", false);
      row("options", 0, "factory-class", utf16(QDFactory.getDefaultFactory().getClass().getName()));
      row("options", 0, "collector-class", utf16(collector.getClass().getName()));
      integer("options", 0, "wildcard-cipher", scheme.getCodec().getWildcardCipher());
      for (int id = 0; id < scheme.getRecordCount(); id++) {
        DataRecord record = scheme.getRecord(id);
        row("schema", id, "record", utf16(record.getName()));
        row("schema", id, "has-time", record.hasTime() ? "b:1" : "b:0");
        integer("schema", id, "int-field-count", record.getIntFieldCount());
        for (int field = 0; field < record.getIntFieldCount(); field++) {
          row("schema", id, "int-field-" + field, utf16(record.getIntField(field).getName()));
          integer(
              "schema",
              id,
              "int-field-type-" + field,
              record.getIntField(field).getSerialType().getId());
        }
      }
    }

    QDAgent agent(boolean snapshot) {
      QDAgent result =
          collector
              .agentBuilder()
              .withHistorySnapshot(snapshot)
              .withEventTimeSequence(false)
              .build();
      agents.add(result);
      row("agents", agents.size() - 1, "history-snapshot", snapshot ? "b:1" : "b:0");
      row("agents", agents.size() - 1, "event-time-sequence", "b:0");
      return result;
    }

    void sub(QDAgent agent, boolean remove, DataRecord record, String symbol, long bound) {
      int ordinal = subscriptionOrdinal++;
      integer("subscription-input", ordinal, "agent", agents.indexOf(agent));
      row("subscription-input", ordinal, "operation", remove ? "t:remove" : "t:add");
      row("subscription-input", ordinal, "record", utf16(record.getName()));
      row("subscription-input", ordinal, "symbol", utf16(symbol));
      if (!remove && collector.getContract() == QDContract.HISTORY) {
        integer("subscription-input", ordinal, "raw-bound", bound);
      }
      RecordBuffer buffer =
          new RecordBuffer(
              remove
                  ? RecordMode.SUBSCRIPTION
                  : RecordMode.addedSubscriptionFor(collector.getContract()));
      RecordCursor cursor = buffer.add(record, scheme.getCodec().encode(symbol), null);
      if (!remove && collector.getContract() == QDContract.HISTORY && record.hasTime()) {
        cursor.setTime(bound);
      }
      if (remove) {
        agent.removeSubscription(buffer);
      } else {
        agent.addSubscription(buffer);
      }
    }

    void send(DataRecord record, String symbol, long time, int value, int flags) {
      sendBatch(new Item(record, symbol, time, value, flags));
    }

    void sendBatch(Item... items) {
      String phase = "data-input-batch-" + dataBatchOrdinal++;
      RecordBuffer buffer = new RecordBuffer(RecordMode.FLAGGED_DATA);
      for (int ordinal = 0; ordinal < items.length; ordinal++) {
        Item item = items[ordinal];
        row(phase, ordinal, "record", utf16(item.record.getName()));
        row(phase, ordinal, "symbol", utf16(item.symbol));
        integer(phase, ordinal, "raw-time", item.time);
        integer(phase, ordinal, "value", item.value);
        integer(phase, ordinal, "flags", item.flags);
        RecordCursor cursor = buffer.add(item.record, scheme.getCodec().encode(item.symbol), null);
        cursor.setTime(item.time);
        cursor.setInt(2, item.value);
        cursor.setEventFlags(item.flags);
      }
      distributor.process(buffer);
    }

    @Override
    public void close() {
      Throwable failure = null;
      for (QDAgent agent : agents) {
        try {
          agent.close();
        } catch (RuntimeException | Error e) {
          failure = remember(failure, e);
        }
      }
      try {
        distributor.close();
      } catch (RuntimeException | Error e) {
        failure = remember(failure, e);
      }
      try {
        collector.close();
      } catch (RuntimeException | Error e) {
        failure = remember(failure, e);
      }
      if (failure instanceof Error) {
        throw (Error) failure;
      }
      if (failure != null) {
        throw (RuntimeException) failure;
      }
    }
  }

  private static Throwable remember(Throwable previous, Throwable next) {
    if (previous == null) {
      return next;
    }
    previous.addSuppressed(next);
    return previous;
  }

  private static DataRecord record(int id, String name) {
    return new DefaultRecord(
        id,
        name,
        true,
        new DataIntField[] {
          new CompactIntField(0, name + ".TimeHigh"),
          new CompactIntField(1, name + ".TimeLow"),
          new CompactIntField(2, name + ".Value")
        },
        new DataObjField[0]);
  }
}
