// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamInput;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.qtp.AgentAdapter;
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.DistributorAdapter;
import com.devexperts.qd.qtp.MessageAdapter;
import com.devexperts.qd.qtp.MessageConnectors;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageDescriptor;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.QDEndpoint;
import com.devexperts.qd.stats.QDStats;
import com.devexperts.util.TimePeriod;
import com.devexperts.util.TimePeriodInfo;
import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.impl.DXFeedScheme;
import java.io.IOException;
import java.io.PrintStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Collections;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Actual production adapters and binary composer/parser; no fabricated aggregation resolver or ACK.
 */
public final class ServerAggregationOracle {
  static void row(String kind, String label, Object value) {
    System.out.println(
        kind
            + "\t"
            + label
            + "\t"
            + String.valueOf(value).replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n"));
  }

  static String hex(byte[] bytes) {
    StringBuilder out = new StringBuilder(bytes.length * 2);
    for (byte b : bytes) out.append(String.format("%02x", b & 255));
    return out.toString();
  }

  static QDEndpoint endpoint() {
    return endpoint(QDContract.TICKER);
  }

  static QDEndpoint endpoint(QDContract contract) {
    return QDEndpoint.newBuilder()
        .withScheme(DXFeedScheme.getInstance())
        .withCollectors(Collections.singletonList(contract))
        .build();
  }

  static final class Pair implements AutoCloseable {
    final QDEndpoint server, client;
    final AgentAdapter agent;
    final DistributorAdapter distributor;
    final BinaryQTPParser toServer = new BinaryQTPParser(DXFeedScheme.getInstance());
    final BinaryQTPParser toClient = new BinaryQTPParser(DXFeedScheme.getInstance());

    Pair() {
      this(QDContract.TICKER);
    }

    Pair(QDContract contract) {
      server = endpoint(contract);
      client = endpoint(contract);
      AgentAdapter.Factory sf =
          new AgentAdapter.Factory(
              contract == QDContract.TICKER ? server.getTicker() : null,
              contract == QDContract.STREAM ? server.getStream() : null,
              null,
              null);
      sf.setMinAggregationPeriod(TimePeriod.valueOf("1s"));
      sf.setDefaultAggregationPeriod(TimePeriod.valueOf("3s"));
      sf.setMaxAggregationPeriod(TimePeriod.valueOf("10s"));
      agent = (AgentAdapter) sf.createAdapter(QDStats.VOID);
      distributor =
          (DistributorAdapter)
              new DistributorAdapter.Factory(
                      contract == QDContract.TICKER ? client.getTicker() : null,
                      contract == QDContract.STREAM ? client.getStream() : null,
                      null,
                      null)
                  .createAdapter(QDStats.VOID);
      agent.useDescribeProtocol();
      distributor.useDescribeProtocol();
      agent.start();
      distributor.start();
    }

    void transfer(
        String label, MessageAdapter source, BinaryQTPParser parser, MessageAdapter destination)
        throws Exception {
      ByteArrayOutput output = new ByteArrayOutput();
      BinaryQTPComposer composer = new BinaryQTPComposer(DXFeedScheme.getInstance(), true);
      composer.setOutput(output);
      source.retrieveMessages(composer);
      byte[] bytes = output.toByteArray();
      row("wire", label, hex(bytes));
      parser.setInput(new ByteArrayInput(bytes));
      parser.parse(destination);
      row("info", label, distributor.getAggregationPeriodInfo());
    }

    void handshake(String label) throws Exception {
      transfer(label + "-request", distributor, toServer, agent);
      transfer(label + "-response", agent, toClient, distributor);
    }

    void peerDescriptor(String label, ProtocolDescriptor descriptor) throws Exception {
      ByteArrayOutput output = new ByteArrayOutput();
      BinaryQTPComposer composer = new BinaryQTPComposer(DXFeedScheme.getInstance(), true);
      composer.setOutput(output);
      composer.visitDescribeProtocol(descriptor);
      byte[] bytes = output.toByteArray();
      row("wire", label, hex(bytes));
      toClient.setInput(new ByteArrayInput(bytes));
      toClient.parse(distributor);
      row("info", label, distributor.getAggregationPeriodInfo());
    }

    public void close() {
      distributor.close();
      agent.close();
      client.close();
      server.close();
    }
  }

  static void requestBounds() throws Exception {
    DXEndpoint endpoint = DXEndpoint.newBuilder().withRole(DXEndpoint.Role.STREAM_FEED).build();
    try {
      for (long value : new long[] {-1, 0, 1, 86400000, 86400001, Long.MAX_VALUE}) {
        try {
          endpoint.setRequestedAggregationPeriod(TimePeriod.valueOf(value));
          row("bound", Long.toString(value), endpoint.getRequestedAggregationPeriod().getTime());
        } catch (IllegalArgumentException failure) {
          row("bound", Long.toString(value), failure.getClass().getSimpleName());
        }
      }
      endpoint.setRequestedAggregationPeriod(null);
      row("bound", "null", endpoint.getRequestedAggregationPeriod());
      endpoint.close();
      endpoint.setRequestedAggregationPeriod(TimePeriod.valueOf(5));
      row("lifecycle", "set-after-close", endpoint.getRequestedAggregationPeriod().getTime());
    } finally {
      endpoint.closeAndAwaitTermination();
    }
  }

  static void peerInfos() {
    String[] texts = {
      null,
      "",
      "{\"min\":-1,\"max\":-1}",
      "{\"min\":0,\"max\":0}",
      "{\"min\":1.5,\"max\":2}",
      "{\"min\":2,\"max\":1}",
      "{\"max\":2,\"min\":1}",
      "{\"min\": 1,\"max\":2}",
      "{\"min\":1e-3,\"max\":2E-3}",
      "{\"min\":-0.0005,\"max\":0}",
      "{\"min\":-0.0006,\"max\":0}",
      "{\"min\":0.0005,\"max\":0.0015}",
      "{\"min\":1e1000,\"max\":1e1000}",
      "{\"min\":1,\"max\":2,\"future\":true}",
      "{\"min\":1,\"max\":2,\"future\":{\"x\":1}}",
      "{\"min\":1,\"max\":2,\"future\":[[1]]}",
      "{\"min\":1,\"max\":2,\"future\":\"a,b\"}"
    };
    for (int i = 0; i < texts.length; i++) {
      TimePeriodInfo info = TimePeriodInfo.valueOf(texts[i]);
      row(
          "parse",
          Integer.toString(i),
          info.getMin() + "," + info.getMax() + "," + info.isUnknown());
    }
  }

  static void socketReconnect() throws Exception {
    try (ServerSocket listener = new ServerSocket(0, 2, InetAddress.getByName("127.0.0.1"))) {
      listener.setSoTimeout(5000);
      QDEndpoint client = endpoint();
      CountDownLatch[] observed = {new CountDownLatch(1), new CountDownLatch(1)};
      AtomicReference<Throwable> failure = new AtomicReference<>();
      String[] requests = new String[2];
      Thread peer =
          new Thread(
              () -> {
                try {
                  for (int index = 0; index < 2; index++) {
                    final int ordinal = index;
                    try (Socket socket = listener.accept();
                        Pair server = new Pair()) {
                      socket.setSoTimeout(5000);
                      BinaryQTPParser parser = new BinaryQTPParser(DXFeedScheme.getInstance());
                      long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(5);
                      StreamInput input =
                          new StreamInput(
                              new java.io.FilterInputStream(socket.getInputStream()) {
                                private void beforeRead() throws IOException {
                                  long remaining = deadline - System.nanoTime();
                                  if (remaining <= 0)
                                    throw new IOException("Socket cumulative deadline");
                                  socket.setSoTimeout(
                                      (int) Math.max(1, TimeUnit.NANOSECONDS.toMillis(remaining)));
                                }

                                @Override
                                public int read() throws IOException {
                                  beforeRead();
                                  return in.read();
                                }

                                @Override
                                public int read(byte[] target, int offset, int length)
                                    throws IOException {
                                  beforeRead();
                                  return in.read(target, offset, length);
                                }
                              });
                      for (int frames = 0; requests[ordinal] == null && frames < 16; frames++) {
                        long remaining = deadline - System.nanoTime();
                        if (remaining <= 0) throw new IOException("Socket descriptor deadline");
                        socket.setSoTimeout(
                            (int) Math.max(1, TimeUnit.NANOSECONDS.toMillis(remaining)));
                        long length = input.readCompactLong();
                        if (length < 0 || length > 65536)
                          throw new IOException("Socket descriptor frame bound");
                        remaining = deadline - System.nanoTime();
                        if (remaining <= 0)
                          throw new IOException("Socket descriptor body deadline");
                        socket.setSoTimeout(
                            (int) Math.max(1, TimeUnit.NANOSECONDS.toMillis(remaining)));
                        byte[] body = new byte[(int) length];
                        input.readFully(body);
                        ByteArrayOutput frame = new ByteArrayOutput();
                        frame.writeCompactLong(length);
                        frame.write(body);
                        parser.setInput(new ByteArrayInput(frame.toByteArray()));
                        parser.parse(
                            new MessageConsumerAdapter() {
                              @Override
                              public void processDescribeProtocol(
                                  ProtocolDescriptor descriptor, boolean log) {
                                MessageDescriptor data =
                                    descriptor.getReceive(MessageType.TICKER_DATA);
                                if (data == null)
                                  throw new IllegalStateException(
                                      "Missing receive-data descriptor");
                                requests[ordinal] =
                                    data.getProperty(
                                        ProtocolDescriptor.REQUESTED_AGGREGATION_PERIOD_PROPERTY);
                                server.agent.processDescribeProtocol(descriptor, false);
                                ByteArrayOutput output = new ByteArrayOutput();
                                BinaryQTPComposer composer =
                                    new BinaryQTPComposer(DXFeedScheme.getInstance(), true);
                                composer.setOutput(output);
                                server.agent.retrieveMessages(composer);
                                try {
                                  socket.getOutputStream().write(output.toByteArray());
                                  socket.getOutputStream().flush();
                                } catch (IOException error) {
                                  throw new IllegalStateException(error);
                                }
                                observed[ordinal].countDown();
                              }
                            });
                      }
                      if (requests[ordinal] == null)
                        throw new IOException("Socket descriptor frame count");
                    }
                  }
                } catch (Throwable error) {
                  failure.set(error);
                  for (CountDownLatch latch : observed) latch.countDown();
                }
              },
              "aggregation-oracle-owned-peer");
      try {
        client.setRequestedAggregationPeriod(TimePeriod.valueOf("5s"));
        client.addConnectors(
            MessageConnectors.createMessageConnectors(
                new DistributorAdapter.Factory(client.getTicker()),
                "127.0.0.1:" + listener.getLocalPort(),
                QDStats.VOID));
        peer.start();
        client.startConnectors();
        if (!observed[0].await(5, TimeUnit.SECONDS) || failure.get() != null)
          throw new IllegalStateException("First socket handshake", failure.get());
        client.stopConnectorsAndWait();
        client.startConnectors();
        if (!observed[1].await(5, TimeUnit.SECONDS) || failure.get() != null)
          throw new IllegalStateException("Reconnect socket handshake", failure.get());
        row("socket", "first-request", requests[0]);
        row("socket", "reconnect-request", requests[1]);
      } finally {
        client.stopConnectorsAndWait();
        client.close();
        listener.close();
        peer.join(6000);
        if (peer.isAlive()) throw new IllegalStateException("Owned peer did not terminate");
      }
      if (failure.get() != null)
        throw new IllegalStateException("Socket peer failed", failure.get());
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) throw new IllegalArgumentException("Expected output path");
    try (PrintStream output = new PrintStream(args[0], "UTF-8")) {
      System.setOut(output);
      requestBounds();
      peerInfos();
      try (Pair pair = new Pair()) {
        pair.handshake("default");
        for (String value : new String[] {"5s", "0s", "20s"}) {
          pair.distributor.setRequestedAggregationPeriod(TimePeriod.valueOf(value));
          pair.handshake(value);
        }
        pair.distributor.setRequestedAggregationPeriod((TimePeriod) null);
        pair.handshake("reset");
        ProtocolDescriptor omission = ProtocolDescriptor.newEmptyProtocolDescriptor();
        omission.setProperty("marker", "delta");
        pair.peerDescriptor("delta-omits-info", omission);
        ProtocolDescriptor invalid = ProtocolDescriptor.newEmptyProtocolDescriptor();
        MessageDescriptor invalidData = invalid.newMessageDescriptor(MessageType.TICKER_DATA);
        invalidData.setProperty(ProtocolDescriptor.AGGREGATION_PERIOD_INFO_PROPERTY, "invalid");
        invalid.addSend(invalidData);
        pair.peerDescriptor("malformed-info", invalid);
      }
      try (Pair fresh = new Pair()) {
        fresh.distributor.setRequestedAggregationPeriod(TimePeriod.valueOf("5s"));
        fresh.handshake("fresh-adapter-reconnect");
      }
      try (Pair legacy = new Pair()) {
        ProtocolDescriptor descriptor = ProtocolDescriptor.newEmptyProtocolDescriptor();
        descriptor.addSend(descriptor.newMessageDescriptor(MessageType.TICKER_DATA));
        legacy.peerDescriptor("legacy-fresh-no-info", descriptor);
      }
      try (Pair inherited = new Pair()) {
        ProtocolDescriptor descriptor = ProtocolDescriptor.newEmptyProtocolDescriptor();
        descriptor.setProperty(
            ProtocolDescriptor.AGGREGATION_PERIOD_INFO_PROPERTY, "{\"min\":1.5,\"max\":2}");
        descriptor.addSend(descriptor.newMessageDescriptor(MessageType.TICKER_DATA));
        inherited.peerDescriptor("inherited-info", descriptor);
      }
      try (Pair stream = new Pair(QDContract.STREAM)) {
        stream.handshake("stream-default");
        stream.distributor.setRequestedAggregationPeriod(TimePeriod.valueOf("5s"));
        stream.handshake("stream-5s");
      }
      socketReconnect();
      TimePeriodInfo overflow =
          TimePeriodInfo.valueOf(
              "{\"min\":10000000000000000000e2147483647,\"max\":10000000000000000000e2147483647}");
      row(
          "compatibility",
          "java-int-exponent-overflow",
          overflow.getMin() + "," + overflow.getMax() + "," + overflow.isUnknown());
      output.flush();
      output.close();
      if (output.checkError()) throw new IOException("Failed writing oracle output");
    }
  }
}
