// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.QDDistributor;
import com.devexperts.qd.QDFactory;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.qtp.AgentAdapter;
import com.devexperts.qd.qtp.MessageConnector;
import com.devexperts.qd.qtp.MessageConnectors;
import com.devexperts.qd.qtp.QDEndpoint;
import com.devexperts.qd.qtp.socket.ServerSocketConnector;
import com.devexperts.util.WideDecimal;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Publishes the synthetic Quote STREAM benchmark through the Java QD record API. */
public final class QuoteStreamPublisher {
  private static final String QD_VERSION = "QDS-3.357";
  private static final String SYMBOL = "IBM";
  private static final int MAX_BATCH = 8;
  private static final int MAX_COUNT = 10_000_000;
  private static final int MAX_RATE = 10_000_000;
  private static final long MICROS_PER_SECOND = 1_000_000;
  private static final long NANOS_PER_SECOND = 1_000_000_000;
  private static final long STARTUP_TIMEOUT = TimeUnit.SECONDS.toNanos(30);

  private QuoteStreamPublisher() {}

  public static void main(String[] args) throws IOException, InterruptedException {
    if (args.length == 0 || (args.length == 1 && "--help".equals(args[0]))) {
      System.out.println("Usage: QuoteStreamPublisher PORT COUNT RATE BATCH [--clients N]");
      System.out.println("Publishes physical IBM Quote records over a loopback QTP STREAM server.");
      System.out.println("PORT 1..65535, COUNT 2..10000000, RATE 0..10000000, BATCH 1..8.");
      System.out.println("--clients N sets the maximum connections (default 1, N >= 1).");
      System.out.println(
          "Rate 0 submits without pacing. Send PROBE until every receiver confirms,");
      System.out.println("then GO. Send CLOSE after all receivers report COUNT_REACHED.");
      System.out.println("Exit 0 on success, nonzero on invalid input or publication failure.");
      return;
    }
    if (args.length != 4 && (args.length != 6 || !"--clients".equals(args[4]))) {
      throw new IllegalArgumentException("Expected PORT COUNT RATE BATCH [--clients N]");
    }
    int port = number(args[0], 1, 65535);
    int count = number(args[1], 2, MAX_COUNT);
    int rate = number(args[2], 0, MAX_RATE);
    int batch = number(args[3], 1, MAX_BATCH);
    int clients = args.length == 6 ? number(args[5], 1, Integer.MAX_VALUE) : 1;
    String version = QDFactory.getVersion();
    if (!QD_VERSION.equals(version)) {
      throw new IllegalStateException("Expected Java QD " + QD_VERSION + ", got " + version);
    }

    BufferedReader control =
        new BufferedReader(new InputStreamReader(System.in, StandardCharsets.US_ASCII));
    try (QDEndpoint endpoint =
        QDEndpoint.newBuilder()
            .withName("quote-stream-publisher")
            .withCollectors(List.of(QDContract.STREAM))
            .build()) {
      DataRecord record = endpoint.getScheme().findRecordByName("Quote");
      int cipher = endpoint.getScheme().getCodec().encode(SYMBOL);
      QuoteFields fields = new QuoteFields(record, cipher);
      AgentAdapter.Factory factory = new AgentAdapter.Factory(endpoint.getStream());
      List<MessageConnector> connectors =
          MessageConnectors.createMessageConnectors(factory, ":" + port, endpoint.getRootStats());
      endpoint.addConnectors(connectors);
      if (connectors.size() != 1 || !(connectors.get(0) instanceof ServerSocketConnector server)) {
        throw new IllegalStateException("Expected one QD socket server");
      }
      server.setBindAddr("127.0.0.1");
      server.setMaxConnections(clients);
      QDDistributor distributor = endpoint.getStream().distributorBuilder().build();
      RecordBuffer records = RecordBuffer.getInstance(RecordMode.DATA);
      try {
        endpoint.startConnectors();
        System.out.println("PORT " + port);
        awaitDemand(distributor, record, cipher);
        fields.append(records, 0);
        distributor.process(records);
        records.clear();
        System.out.println("READY");
        String command;
        while ("PROBE".equals(command = control.readLine())) {
          fields.append(records, 0);
          distributor.process(records);
          records.clear();
        }
        if (!"GO".equals(command)) {
          throw new IOException("Expected PROBE or GO on stdin");
        }
        long started = System.nanoTime();
        for (int published = 0; published < count; ) {
          int size = Math.min(batch, count - published);
          if (rate != 0) {
            long due = (long) published * NANOS_PER_SECOND / rate;
            while (System.nanoTime() - started < due) {
              Thread.yield();
            }
          }
          for (int i = 0; i < size; i++) {
            fields.append(records, published + i + 1);
          }
          distributor.process(records);
          records.clear();
          published += size;
        }
        System.out.println(
            "RESULT published=" + count + " elapsedNs=" + (System.nanoTime() - started));
        requireCommand(control, "CLOSE");
      } finally {
        try {
          endpoint.stopConnectorsAndWait();
        } finally {
          records.release();
          distributor.close();
        }
      }
    }
  }

  private static int number(String value, int min, int max) {
    int parsed = Integer.parseInt(value);
    if (parsed < min || parsed > max) {
      throw new IllegalArgumentException("Expected integer in " + min + ".." + max);
    }
    return parsed;
  }

  private static void requireCommand(BufferedReader control, String command) throws IOException {
    if (!command.equals(control.readLine())) {
      throw new IOException("Expected " + command + " on stdin");
    }
  }

  private static void awaitDemand(QDDistributor distributor, DataRecord record, int cipher)
      throws InterruptedException {
    RecordBuffer added = RecordBuffer.getInstance(RecordMode.SUBSCRIPTION);
    try {
      long started = System.nanoTime();
      do {
        distributor.getAddedRecordProvider().retrieve(added);
        RecordCursor cursor;
        while ((cursor = added.next()) != null) {
          if (cursor.getRecord() == record
              && cursor.getCipher() == cipher
              && (cipher != 0 || SYMBOL.equals(cursor.getSymbol()))) {
            return;
          }
        }
        added.clear();
        Thread.sleep(1);
      } while (System.nanoTime() - started < STARTUP_TIMEOUT);
      throw new IllegalStateException("Timed out waiting for Quote/STREAM demand");
    } finally {
      added.release();
    }
  }

  private static int fieldIndex(DataRecord record, String name, boolean wide) {
    DataField field = record.findFieldByName(name);
    if (!(field instanceof DataIntField numeric) || numeric.getSerialType().isLong() != wide) {
      throw new IllegalStateException("Missing or incompatible Quote field " + name);
    }
    return numeric.getIndex();
  }

  private static final class QuoteFields {
    private final DataRecord record;
    private final int cipher;
    private final int bidTime;
    private final int askTime;
    private final int bidExchange;
    private final int askExchange;
    private final int bidPrice;
    private final int askPrice;
    private final int bidSize;
    private final int askSize;

    QuoteFields(DataRecord record, int cipher) {
      if (record == null) {
        throw new IllegalStateException("Missing Quote record");
      }
      this.record = record;
      this.cipher = cipher;
      bidTime = fieldIndex(record, "Bid.Time", false);
      askTime = fieldIndex(record, "Ask.Time", false);
      bidExchange = fieldIndex(record, "Bid.Exchange", false);
      askExchange = fieldIndex(record, "Ask.Exchange", false);
      bidPrice = fieldIndex(record, "Bid.Price", true);
      askPrice = fieldIndex(record, "Ask.Price", true);
      bidSize = fieldIndex(record, "Bid.Size", true);
      askSize = fieldIndex(record, "Ask.Size", true);
    }

    void append(RecordBuffer buffer, int sequence) {
      RecordCursor cursor = buffer.add(record, cipher, cipher == 0 ? SYMBOL : null);
      long micros = System.nanoTime() / 1000;
      int seconds = Math.toIntExact(micros / MICROS_PER_SECOND);
      cursor.setInt(bidTime, seconds);
      cursor.setInt(askTime, seconds);
      cursor.setInt(bidExchange, 'B');
      cursor.setInt(askExchange, 'A');
      cursor.setLong(bidPrice, WideDecimal.composeWide(sequence));
      cursor.setLong(askPrice, WideDecimal.composeWide(sequence + 1));
      cursor.setLong(bidSize, WideDecimal.composeWide(micros % MICROS_PER_SECOND + 1));
      cursor.setLong(askSize, WideDecimal.composeWide(2));
    }
  }
}
