// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.QDAgent;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.QDFactory;
import com.devexperts.qd.ng.AbstractRecordSink;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.qtp.DistributorAdapter;
import com.devexperts.qd.qtp.MessageAdapter;
import com.devexperts.qd.qtp.MessageConnector;
import com.devexperts.qd.qtp.MessageConnectorState;
import com.devexperts.qd.qtp.MessageConnectors;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.QDEndpoint;
import com.devexperts.qd.stats.QDStats;
import java.io.IOException;
import java.time.Instant;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;

/** Verifies one native NetTest TimeAndSale STREAM peer through Java QD's record API. */
public final class StreamReceiver {
  private static final String QD_VERSION = "QDS-3.357";
  private static final long IDLE_NANOS = TimeUnit.SECONDS.toNanos(15);
  private static final long PROBE_STARTUP_NANOS = TimeUnit.SECONDS.toNanos(75);
  private static final long FINAL_DRAIN_NANOS = TimeUnit.SECONDS.toNanos(5);
  private static final long POLL_NANOS = TimeUnit.MILLISECONDS.toNanos(20);
  private static final int MAX_CHUNK = 4096;
  private static final int SEQUENCE_BITS = 22;
  private static final int SEQUENCE_MASK = (1 << SEQUENCE_BITS) - 1;
  private static final int FIELD_SPREAD = 120;
  private static final int WIDE_DECIMAL_INTEGER_RANK = 128;
  private static final long NANOS_PER_MILLI = 1_000_000L;
  private static final long NANOS_PER_SECOND = 1_000_000_000L;
  private static final long FINE_MIN_NANOS = -10_000_000L;
  private static final long FINE_MAX_NANOS = 100_000_000L;
  private static final long FINE_STEP_NANOS = 1_000L;
  private static final long MIN_NANOS = -NANOS_PER_SECOND;
  private static final long MAX_NANOS = 30 * NANOS_PER_SECOND;
  private static final int LOWER_BUCKETS = (int) ((FINE_MIN_NANOS - MIN_NANOS) / NANOS_PER_MILLI);
  private static final int FINE_BUCKETS =
      (int) ((FINE_MAX_NANOS - FINE_MIN_NANOS) / FINE_STEP_NANOS);
  private static final int UPPER_BUCKETS =
      (int) ((MAX_NANOS - FINE_MAX_NANOS) / NANOS_PER_MILLI) + 1;

  private StreamReceiver() {}

  /**
   * Accepts a loopback address and one subscribed symbol. The parent sends the final producer count
   * on stdin after the producer exits.
   */
  public static void main(String[] args) throws IOException, InterruptedException {
    if ((args.length != 2 && args.length != 4 && args.length != 5)
        || !args[0].matches("[0-9]{1,3}(\\.[0-9]{1,3}){3}:[0-9]{1,5}")
        || args[1].isEmpty()
        || (args.length >= 4 && !args[2].equals("stream") && !args[2].equals("ticker"))
        || (args.length >= 4 && !args[3].equals("ns") && !args[3].equals("ms"))
        || (args.length == 5 && !args[4].equals("probe"))) {
      throw new IllegalArgumentException(
          "Usage: StreamReceiver HOST:PORT SYMBOL [stream|ticker ns|ms [probe]]");
    }
    boolean ticker = args.length >= 4 && args[2].equals("ticker");
    boolean nanos = args.length >= 4 && args[3].equals("ns");
    boolean probe = args.length == 5;
    QDContract contract = ticker ? QDContract.TICKER : QDContract.STREAM;
    try (QDEndpoint endpoint =
        QDEndpoint.newBuilder()
            .withName("stream-benchmark")
            .withCollectors(Collections.singletonList(contract))
            .build()) {
      String version = QDFactory.getVersion();
      if (!QD_VERSION.equals(version)) {
        throw new IllegalStateException("Expected Java QD " + QD_VERSION + ", got " + version);
      }
      DataRecord record = endpoint.getScheme().findRecordByName(ticker ? "Quote" : "TimeAndSale");
      int cipher = endpoint.getScheme().getCodec().encode(args[1]);
      Object tickerLock = new Object();
      Observation observation =
          new Observation(record, cipher, args[1], ticker, nanos, probe, tickerLock);
      QDAgent agent = (ticker ? endpoint.getTicker() : endpoint.getStream()).agentBuilder().build();
      try {
        subscribe(agent, record, cipher, args[1]);
        Thread receiver = Thread.currentThread();
        agent.setRecordListener(provider -> LockSupport.unpark(receiver));
        DistributorAdapter.Factory factory =
            ticker
                ? new LockedTickerFactory(endpoint, tickerLock)
                : new DistributorAdapter.Factory(endpoint, null);
        List<MessageConnector> connectors =
            MessageConnectors.createMessageConnectors(factory, args[0], endpoint.getRootStats());
        if (connectors.size() != 1) {
          throw new IllegalStateException("Expected one loopback connector");
        }
        endpoint.addConnectors(connectors);
        CompletableFuture<Long> expectedInput = readExpected(receiver);
        endpoint.startConnectors();
        long expected = receive(agent, observation, connectors.get(0), expectedInput);
        endpoint.stopConnectorsAndWait();
        while (observation.retrieve(agent)) {
          // Drain any records queued before connector shutdown.
        }
        observation.writeResult(version, expected);
        if ((!ticker && observation.count != expected)
            || (ticker && observation.lastSequence != expected)
            || observation.sequenceErrors != 0
            || observation.fieldErrors != 0
            || observation.latency.samples != observation.count) {
          throw new IllegalStateException("Record count, sequence, fields or timestamps differ");
        }
      } finally {
        agent.close();
      }
    }
  }

  private static void subscribe(QDAgent agent, DataRecord record, int cipher, String symbol) {
    RecordBuffer subscription = RecordBuffer.getInstance(RecordMode.SUBSCRIPTION);
    try {
      subscription.add(record, cipher, cipher == 0 ? symbol : null);
      agent.setSubscription(subscription);
    } finally {
      subscription.release();
    }
  }

  private static CompletableFuture<Long> readExpected(Thread receiver) {
    CompletableFuture<Long> result = new CompletableFuture<>();
    Thread reader =
        new Thread(
            () -> {
              try {
                long count = 0;
                int digits = 0;
                int ch = System.in.read();
                while (ch >= '0' && ch <= '9' && digits < 19) {
                  count = Math.addExact(Math.multiplyExact(count, 10), ch - '0');
                  digits++;
                  ch = System.in.read();
                }
                if (ch == '\r') {
                  ch = System.in.read();
                }
                if (ch != '\n' || count <= 0) {
                  throw new IOException("Expected a positive decimal record count on stdin");
                }
                result.complete(count);
              } catch (IOException | ArithmeticException e) {
                result.completeExceptionally(e);
              } finally {
                LockSupport.unpark(receiver);
              }
            },
            "expected-record-count");
    reader.setDaemon(true);
    reader.start();
    return result;
  }

  private static long receive(
      QDAgent agent,
      Observation observation,
      MessageConnector connector,
      CompletableFuture<Long> expectedInput) {
    long expected = 0;
    long controlStart = 0;
    long start = System.nanoTime();
    long startupLimit = observation.probeEnabled ? PROBE_STARTUP_NANOS : IDLE_NANOS;
    boolean connected = false;
    while (true) {
      boolean pending = observation.retrieve(agent);
      long now = System.nanoTime();
      if (!connected && connector.getState() == MessageConnectorState.CONNECTED) {
        connected = true;
        System.out.println("CONNECTED");
      }
      if (expected == 0 && expectedInput.isDone()) {
        expected = expectedInput.join();
        controlStart = now;
      }
      if (expected > 0) {
        if (!pending && connector.getState() != MessageConnectorState.CONNECTED) {
          return expected;
        }
        if (now - controlStart >= FINAL_DRAIN_NANOS) {
          throw new IllegalStateException("Source connection or pending records did not drain");
        }
      } else if (observation.count == 0
          ? now - start >= startupLimit
          : now - observation.lastArrival >= IDLE_NANOS) {
        throw new IllegalStateException("Timed out waiting for data or the producer's final count");
      }
      if (!pending) {
        LockSupport.parkNanos(POLL_NANOS);
      }
    }
  }

  private static int findInt(DataRecord record, String name, boolean wide) {
    DataField candidate = record.findFieldByName(name);
    if (!(candidate instanceof DataIntField)) {
      throw new IllegalStateException("Missing TimeAndSale field " + name);
    }
    DataIntField field = (DataIntField) candidate;
    if (field.getSerialType().isLong() != wide) {
      throw new IllegalStateException("Unexpected TimeAndSale field width " + name);
    }
    return field.getIndex();
  }

  /** Serialize ticker updates with reads of QD's shared matrix-backed cursors. */
  private static final class LockedTickerFactory extends DistributorAdapter.Factory {
    private final Object lock;

    LockedTickerFactory(QDEndpoint endpoint, Object lock) {
      super(endpoint, null);
      this.lock = lock;
    }

    @Override
    public MessageAdapter createAdapter(QDStats stats) {
      return new DistributorAdapter(
          endpoint, ticker, stream, history, getFilter(), getStripe(), stats, null) {
        @Override
        protected void processData(DataIterator iterator, MessageType message) {
          synchronized (lock) {
            super.processData(iterator, message);
          }
        }
      };
    }
  }

  private static final class Observation extends AbstractRecordSink {
    private final DataRecord record;
    private final int cipher;
    private final String symbol;
    private final boolean ticker;
    private final boolean nanos;
    private final boolean probeEnabled;
    private final Object tickerLock;
    private boolean expectingProbe;
    private final int time,
        sequence,
        nano,
        exchange,
        tradeId,
        price,
        size,
        bid,
        ask,
        conditions,
        flags;
    private final int askTime, askExchange, bidSize, askSize;
    private final Latency latency = new Latency();
    private long count;
    private long lastSequence;
    private long sequenceErrors;
    private long fieldErrors;
    private long nonzeroNanoParts;
    private long firstArrival;
    private long lastArrival;
    private long chunkArrival;
    private int chunkRecords;

    Observation(
        DataRecord record,
        int cipher,
        String symbol,
        boolean ticker,
        boolean nanos,
        boolean probe,
        Object tickerLock) {
      if (record == null) {
        throw new IllegalStateException("QD scheme is missing TimeAndSale");
      }
      this.record = record;
      this.cipher = cipher;
      this.symbol = symbol;
      this.ticker = ticker;
      this.nanos = nanos;
      this.probeEnabled = probe;
      this.tickerLock = tickerLock;
      this.expectingProbe = probe;
      time = findInt(record, ticker ? "Bid.Time" : "Time", false);
      sequence = findInt(record, "Sequence", false);
      nano = nanos ? findInt(record, "TimeNanoPart", false) : -1;
      exchange = findInt(record, ticker ? "Bid.Exchange" : "Exchange", false);
      tradeId = ticker ? -1 : findInt(record, "TradeId", true);
      price = ticker ? -1 : findInt(record, "Price", true);
      size = ticker ? -1 : findInt(record, "Size", true);
      bid = findInt(record, "Bid.Price", true);
      ask = findInt(record, "Ask.Price", true);
      conditions = ticker ? -1 : findInt(record, "ExchangeSaleConditions", false);
      flags = ticker ? -1 : findInt(record, "Flags", false);
      askTime = ticker ? findInt(record, "Ask.Time", false) : -1;
      askExchange = ticker ? findInt(record, "Ask.Exchange", false) : -1;
      bidSize = ticker ? findInt(record, "Bid.Size", true) : -1;
      askSize = ticker ? findInt(record, "Ask.Size", true) : -1;
    }

    boolean retrieve(QDAgent agent) {
      chunkRecords = 0;
      if (ticker) {
        synchronized (tickerLock) {
          return agent.retrieve(this);
        }
      }
      return agent.retrieve(this);
    }

    @Override
    public boolean hasCapacity() {
      return chunkRecords < MAX_CHUNK;
    }

    @Override
    public void append(RecordCursor cursor) {
      if (chunkRecords++ == 0) {
        chunkArrival = System.nanoTime();
      }
      if (cursor.getRecord() != record
          || cursor.getCipher() != cipher
          || (cipher == 0 && !symbol.equals(cursor.getSymbol()))) {
        fieldErrors++;
        return;
      }
      int wireSequence = cursor.getInt(sequence);
      long generation = wireSequence & SEQUENCE_MASK;
      if (probeEnabled && generation == 0 && count == 0) {
        if (expectingProbe) {
          expectingProbe = false;
          System.out.println("PROBE_RECEIVED");
        }
        return;
      }
      if (expectingProbe) {
        throw new IllegalStateException("Received workload before probe");
      }
      if (++count == 1) {
        firstArrival = chunkArrival;
        System.out.println("ACTIVE");
      }
      lastArrival = chunkArrival;
      if ((ticker && generation <= lastSequence) || (!ticker && generation != count)) {
        sequenceErrors++;
      }
      lastSequence = generation;
      int millis = wireSequence >>> SEQUENCE_BITS;
      int subMilli = nanos ? cursor.getInt(nano) : 0;
      if (nanos && subMilli != 0) {
        nonzeroNanoParts++;
      }
      if (millis < 1000 && subMilli >= 0 && subMilli < NANOS_PER_MILLI) {
        Instant now = Instant.now();
        long arrivalNanos = now.getEpochSecond() * NANOS_PER_SECOND + now.getNano();
        long sourceNanos =
            cursor.getInt(time) * NANOS_PER_SECOND + millis * NANOS_PER_MILLI + subMilli;
        latency.add(arrivalNanos - sourceNanos);
      }
      boolean badFields =
          ticker
              ? cursor.getInt(askTime) != cursor.getInt(time)
                  || cursor.getInt(exchange) != 'A' + generated(generation, 1) % 26
                  || cursor.getInt(askExchange) != 'A' + generated(generation, 5) % 26
                  || cursor.getLong(bid) != wideDecimal(generation, 2)
                  || cursor.getLong(bidSize) != wideDecimal(generation, 3)
                  || cursor.getLong(ask) != wideDecimal(generation, 6)
                  || cursor.getLong(askSize) != wideDecimal(generation, 7)
              : cursor.getInt(exchange) != 'A' + generated(generation, 2) % 26
                  || cursor.getLong(tradeId) != generated(generation, 3)
                  || cursor.getLong(price) != wideDecimal(generation, 4)
                  || cursor.getLong(size) != wideDecimal(generation, 5)
                  || cursor.getLong(bid) != wideDecimal(generation, 6)
                  || cursor.getLong(ask) != wideDecimal(generation, 7)
                  || cursor.getInt(conditions) != generated(generation, 8)
                  || cursor.getInt(flags) != generated(generation, 9);
      if (millis >= 1000 || subMilli < 0 || subMilli >= NANOS_PER_MILLI || badFields) {
        fieldErrors++;
      }
    }

    private int generated(long generation, int index) {
      // The producer uses physical wire-field order, which excludes QD's long-field placeholders.
      return (int) ((generation + index) % FIELD_SPREAD) + 1;
    }

    private int wideDecimal(long generation, int index) {
      return (generated(generation, index) << 8) | WIDE_DECIMAL_INTEGER_RANK;
    }

    void writeResult(String version, long expected) {
      double elapsed = (lastArrival - firstArrival) / (double) TimeUnit.SECONDS.toNanos(1);
      System.out.printf(
          Locale.ROOT,
          "RESULT {\"qdVersion\":\"%s\",\"expected\":%d,\"delivered\":%d,\"lastSequence\":%d,"
              + "\"precision\":\"%s\",\"latencyFineResolutionNs\":1000,\"nonzeroNanoParts\":%d,"
              + "\"sequenceErrors\":%d,\"fieldErrors\":%d,\"elapsedSeconds\":%.9f,"
              + "\"latencySamples\":%d,\"latencyMinMs\":%s,\"latencyP50Ms\":%s,"
              + "\"latencyP95Ms\":%s,\"latencyP99Ms\":%s,\"latencyMaxMs\":%s,"
              + "\"negativeLatencySamples\":%d,\"latencyBelowRange\":%d,"
              + "\"latencyAboveRange\":%d,\"latencyMinNs\":%s,\"latencyP50Ns\":%s,"
              + "\"latencyP95Ns\":%s,\"latencyP99Ns\":%s,\"latencyMaxNs\":%s}%n",
          version,
          expected,
          count,
          lastSequence,
          nanos ? "ns" : "ms",
          nonzeroNanoParts,
          sequenceErrors,
          fieldErrors,
          elapsed,
          latency.samples,
          latency.samples == 0
              ? "null"
              : Long.toString(Math.floorDiv(latency.min, NANOS_PER_MILLI)),
          latency.percentileMillis(50),
          latency.percentileMillis(95),
          latency.percentileMillis(99),
          latency.samples == 0
              ? "null"
              : Long.toString(Math.floorDiv(latency.max, NANOS_PER_MILLI)),
          latency.negative,
          latency.below,
          latency.above,
          latency.samples == 0 ? "null" : Long.toString(latency.min),
          latency.percentile(50),
          latency.percentile(95),
          latency.percentile(99),
          latency.samples == 0 ? "null" : Long.toString(latency.max));
    }
  }

  private static final class Latency {
    private final long[] histogram = new long[LOWER_BUCKETS + FINE_BUCKETS + UPPER_BUCKETS];
    private long samples;
    private long min = Long.MAX_VALUE;
    private long max = Long.MIN_VALUE;
    private long negative;
    private long below;
    private long above;

    void add(long value) {
      samples++;
      min = Math.min(min, value);
      max = Math.max(max, value);
      if (value < 0) {
        negative++;
      }
      if (value < MIN_NANOS) {
        below++;
      } else if (value > MAX_NANOS) {
        above++;
      } else {
        int index;
        if (value < FINE_MIN_NANOS) {
          index = (int) ((value - MIN_NANOS) / NANOS_PER_MILLI);
        } else if (value < FINE_MAX_NANOS) {
          index = LOWER_BUCKETS + (int) ((value - FINE_MIN_NANOS) / FINE_STEP_NANOS);
        } else {
          index = LOWER_BUCKETS + FINE_BUCKETS + (int) ((value - FINE_MAX_NANOS) / NANOS_PER_MILLI);
        }
        histogram[index]++;
      }
    }

    String percentileMillis(int percent) {
      String value = percentile(percent);
      return value.equals("null")
          ? value
          : Long.toString(Math.floorDiv(Long.parseLong(value), NANOS_PER_MILLI));
    }

    String percentile(int percent) {
      long rank = (samples * percent + 99) / 100;
      if (samples == 0 || rank <= below || rank > samples - above) {
        return "null";
      }
      long cumulative = below;
      for (int index = 0; index < histogram.length; index++) {
        cumulative += histogram[index];
        if (cumulative >= rank) {
          long value;
          if (index < LOWER_BUCKETS) {
            value = MIN_NANOS + index * NANOS_PER_MILLI;
          } else if (index < LOWER_BUCKETS + FINE_BUCKETS) {
            value = FINE_MIN_NANOS + (index - LOWER_BUCKETS) * FINE_STEP_NANOS;
          } else {
            value = FINE_MAX_NANOS + (index - LOWER_BUCKETS - FINE_BUCKETS) * NANOS_PER_MILLI;
          }
          return Long.toString(value);
        }
      }
      throw new IllegalStateException("Latency histogram did not reconcile");
    }
  }
}
