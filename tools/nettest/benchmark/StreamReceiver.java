// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.QDAgent;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.QDFactory;
import com.devexperts.qd.ng.AbstractRecordSink;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.qtp.DistributorAdapter;
import com.devexperts.qd.qtp.MessageConnector;
import com.devexperts.qd.qtp.MessageConnectorState;
import com.devexperts.qd.qtp.MessageConnectors;
import com.devexperts.qd.qtp.QDEndpoint;
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
  private static final long FINAL_DRAIN_NANOS = TimeUnit.SECONDS.toNanos(5);
  private static final long POLL_NANOS = TimeUnit.MILLISECONDS.toNanos(20);
  private static final int MAX_CHUNK = 4096;
  private static final int SEQUENCE_BITS = 22;
  private static final int SEQUENCE_MASK = (1 << SEQUENCE_BITS) - 1;
  private static final int FIELD_SPREAD = 120;
  private static final int WIDE_DECIMAL_INTEGER_RANK = 128;
  private static final int MIN_BUCKET_MS = -1000;
  private static final int MAX_BUCKET_MS = 30_000;

  private StreamReceiver() {}

  /**
   * Accepts a loopback address and one subscribed symbol. The parent sends the final producer count
   * on stdin after the producer exits.
   */
  public static void main(String[] args) throws IOException, InterruptedException {
    if (args.length != 2 || !args[0].matches("127\\.0\\.0\\.1:[0-9]{1,5}") || args[1].isEmpty()) {
      throw new IllegalArgumentException("Usage: StreamReceiver 127.0.0.1:PORT SYMBOL");
    }
    try (QDEndpoint endpoint =
        QDEndpoint.newBuilder()
            .withName("stream-benchmark")
            .withCollectors(Collections.singletonList(QDContract.STREAM))
            .build()) {
      String version = QDFactory.getVersion();
      if (!QD_VERSION.equals(version)) {
        throw new IllegalStateException("Expected Java QD " + QD_VERSION + ", got " + version);
      }
      DataRecord record = endpoint.getScheme().findRecordByName("TimeAndSale");
      int cipher = endpoint.getScheme().getCodec().encode(args[1]);
      Observation observation = new Observation(record, cipher, args[1]);
      QDAgent agent = endpoint.getStream().agentBuilder().build();
      try {
        subscribe(agent, record, cipher, args[1]);
        Thread receiver = Thread.currentThread();
        agent.setRecordListener(provider -> LockSupport.unpark(receiver));
        List<MessageConnector> connectors =
            MessageConnectors.createMessageConnectors(
                new DistributorAdapter.Factory(endpoint, null), args[0], endpoint.getRootStats());
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
        if (observation.count != expected
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
    while (true) {
      boolean pending = observation.retrieve(agent);
      long now = System.nanoTime();
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
      } else if (now - (observation.count == 0 ? start : observation.lastArrival) >= IDLE_NANOS) {
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

  private static final class Observation extends AbstractRecordSink {
    private final DataRecord record;
    private final int cipher;
    private final String symbol;
    private final int time, sequence, exchange, tradeId, price, size, bid, ask, conditions, flags;
    private final Latency latency = new Latency();
    private long count;
    private long sequenceErrors;
    private long fieldErrors;
    private long firstArrival;
    private long lastArrival;
    private long chunkArrival;
    private int chunkRecords;

    Observation(DataRecord record, int cipher, String symbol) {
      if (record == null) {
        throw new IllegalStateException("QD scheme is missing TimeAndSale");
      }
      this.record = record;
      this.cipher = cipher;
      this.symbol = symbol;
      time = findInt(record, "Time", false);
      sequence = findInt(record, "Sequence", false);
      exchange = findInt(record, "Exchange", false);
      tradeId = findInt(record, "TradeId", true);
      price = findInt(record, "Price", true);
      size = findInt(record, "Size", true);
      bid = findInt(record, "Bid.Price", true);
      ask = findInt(record, "Ask.Price", true);
      conditions = findInt(record, "ExchangeSaleConditions", false);
      flags = findInt(record, "Flags", false);
    }

    boolean retrieve(QDAgent agent) {
      chunkRecords = 0;
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
      if (++count == 1) {
        firstArrival = chunkArrival;
        System.out.println("ACTIVE");
      }
      lastArrival = chunkArrival;
      if (cursor.getRecord() != record
          || cursor.getCipher() != cipher
          || (cipher == 0 && !symbol.equals(cursor.getSymbol()))) {
        fieldErrors++;
        return;
      }
      int wireSequence = cursor.getInt(sequence);
      if ((wireSequence & SEQUENCE_MASK) != (count & SEQUENCE_MASK)) {
        sequenceErrors++;
      }
      int millis = wireSequence >>> SEQUENCE_BITS;
      if (millis < 1000) {
        latency.add(Instant.now().toEpochMilli() - (cursor.getInt(time) * 1000L + millis));
      }
      if (millis >= 1000
          || cursor.getInt(exchange) != 'A' + generated(2) % 26
          || cursor.getLong(tradeId) != generated(3)
          || cursor.getLong(price) != wideDecimal(4)
          || cursor.getLong(size) != wideDecimal(5)
          || cursor.getLong(bid) != wideDecimal(6)
          || cursor.getLong(ask) != wideDecimal(7)
          || cursor.getInt(conditions) != generated(8)
          || cursor.getInt(flags) != generated(9)) {
        fieldErrors++;
      }
    }

    private int generated(int index) {
      // The producer uses physical wire-field order, which excludes QD's long-field placeholders.
      return (int) ((count + index) % FIELD_SPREAD) + 1;
    }

    private int wideDecimal(int index) {
      return (generated(index) << 8) | WIDE_DECIMAL_INTEGER_RANK;
    }

    void writeResult(String version, long expected) {
      double elapsed = (lastArrival - firstArrival) / (double) TimeUnit.SECONDS.toNanos(1);
      System.out.printf(
          Locale.ROOT,
          "RESULT {\"qdVersion\":\"%s\",\"expected\":%d,\"delivered\":%d,"
              + "\"sequenceErrors\":%d,\"fieldErrors\":%d,\"elapsedSeconds\":%.9f,"
              + "\"latencySamples\":%d,\"latencyMinMs\":%s,\"latencyP50Ms\":%s,"
              + "\"latencyP95Ms\":%s,\"latencyP99Ms\":%s,\"latencyMaxMs\":%s,"
              + "\"negativeLatencySamples\":%d,\"latencyBelowRange\":%d,"
              + "\"latencyAboveRange\":%d}%n",
          version,
          expected,
          count,
          sequenceErrors,
          fieldErrors,
          elapsed,
          latency.samples,
          latency.samples == 0 ? "null" : Long.toString(latency.min),
          latency.percentile(50),
          latency.percentile(95),
          latency.percentile(99),
          latency.samples == 0 ? "null" : Long.toString(latency.max),
          latency.negative,
          latency.below,
          latency.above);
    }
  }

  private static final class Latency {
    private final long[] histogram = new long[MAX_BUCKET_MS - MIN_BUCKET_MS + 1];
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
      if (value < MIN_BUCKET_MS) {
        below++;
      } else if (value > MAX_BUCKET_MS) {
        above++;
      } else {
        histogram[(int) value - MIN_BUCKET_MS]++;
      }
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
          return Integer.toString(index + MIN_BUCKET_MS);
        }
      }
      throw new IllegalStateException("Latency histogram did not reconcile");
    }
  }
}
