// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.dxfeed.ipf.InstrumentProfile;
import com.dxfeed.ipf.live.InstrumentProfileCollector;
import com.dxfeed.ipf.live.InstrumentProfileConnection;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Finite loopback observations of actual locked QD connection classes. */
public final class ConnectionOracle {
  private static final String HEADER = "#STOCK::=TYPE,SYMBOL,DESCRIPTION,X\n";
  private static final String T1 = "Sun, 06 Nov 1994 08:49:37 GMT";
  private static final String T2 = "Sun, 06 Nov 1994 08:49:38 GMT";
  private static final String T3 = "Sun, 06 Nov 1994 08:49:39 GMT";
  private static final String T4 = "Sun, 06 Nov 1994 08:49:40 GMT";

  private static String record(String symbol) {
    return "STOCK," + symbol + "," + symbol.toLowerCase() + "," + symbol + "-custom\n";
  }

  private static void row(String name, Object value) {
    System.out.println(name + "\t" + value);
  }

  private static final class Reply {
    final int status;
    final String modified;
    final String live;
    final String body;

    Reply(int status, String modified, String live, String body) {
      this.status = status;
      this.modified = modified;
      this.live = live;
      this.body = body;
    }
  }

  /** One explicitly bounded server; owns and joins its only thread and every socket. */
  private static final class Server implements AutoCloseable {
    final ServerSocket listener;
    final Thread thread;
    final List<String> requests = Collections.synchronizedList(new ArrayList<String>());
    final AtomicReference<Throwable> failure = new AtomicReference<>();
    volatile Socket active;

    Server(Reply... replies) throws Exception {
      listener = new ServerSocket(0, 1, InetAddress.getByName("127.0.0.1"));
      listener.setSoTimeout(4000);
      thread =
          new Thread(
              () -> {
                try {
                  for (Reply reply : replies) {
                    try (Socket socket = listener.accept()) {
                      active = socket;
                      socket.setSoTimeout(3000);
                      ByteArrayOutputStream request = new ByteArrayOutputStream();
                      InputStream input = socket.getInputStream();
                      int matched = 0;
                      final byte[] end = new byte[] {13, 10, 13, 10};
                      while (matched < end.length) {
                        int next = input.read();
                        if (next < 0 || request.size() >= 32768) {
                          throw new IOException("bounded request header");
                        }
                        request.write(next);
                        matched = next == end[matched] ? matched + 1 : (next == 13 ? 1 : 0);
                      }
                      requests.add(new String(request.toByteArray(), StandardCharsets.US_ASCII));
                      byte[] body = reply.body.getBytes(StandardCharsets.UTF_8);
                      String header =
                          "HTTP/1.1 "
                              + reply.status
                              + (reply.status == 304 ? " Not Modified" : " OK")
                              + "\r\nConnection: close\r\nContent-Length: "
                              + body.length
                              + "\r\n"
                              + (reply.modified == null
                                  ? ""
                                  : "Last-Modified: " + reply.modified + "\r\n")
                              + (reply.live == null ? "" : "X-Live: " + reply.live + "\r\n")
                              + "\r\n";
                      socket.getOutputStream().write(header.getBytes(StandardCharsets.US_ASCII));
                      socket.getOutputStream().write(body);
                      socket.getOutputStream().flush();
                    } finally {
                      active = null;
                    }
                  }
                } catch (Throwable problem) {
                  if (!listener.isClosed()) failure.set(problem);
                }
              },
              "bounded-ipf-oracle-server");
      thread.start();
    }

    String address() {
      return "http://127.0.0.1:" + listener.getLocalPort() + "/profiles.ipf";
    }

    @Override
    public void close() throws Exception {
      listener.close();
      Socket socket = active;
      if (socket != null) socket.close();
      thread.join(5000);
      if (thread.isAlive()) throw new AssertionError("server did not retire");
      if (failure.get() != null) throw new AssertionError("server", failure.get());
    }
  }

  /**
   * Private Handler instrumentation, explicitly distinct from public asynchronous lifecycle proof.
   */
  private static final class Probe implements AutoCloseable {
    final InstrumentProfileCollector collector = new InstrumentProfileCollector();
    final InstrumentProfileConnection connection;
    final Object handler;
    final Method check;
    final Field pending;
    final Field old;
    final Field generation;
    final Field live;
    final List<Object> markerGenerations = new ArrayList<>();

    Probe(String address) throws Exception {
      collector.setExecutor(Runnable::run);
      connection = InstrumentProfileConnection.createConnection(address, collector);
      Class<?> type = Class.forName("com.dxfeed.ipf.live.InstrumentProfileConnection$Handler");
      Constructor<?> constructor = type.getDeclaredConstructor(InstrumentProfileConnection.class);
      constructor.setAccessible(true);
      handler = constructor.newInstance(connection);
      check = type.getDeclaredMethod("checkAndDownload");
      check.setAccessible(true);
      pending = field(type, "instrumentProfiles");
      old = field(type, "oldGenerations");
      generation = field(type, "newGeneration");
      live = field(type, "supportsLive");
      collector.addUpdateListener(
          items -> {
            while (items.hasNext()) {
              InstrumentProfile p = items.next();
              if (p.getType().equals("STOCK")
                  && (p.getSymbol().equals("G") || p.getSymbol().equals("H"))) {
                try {
                  markerGenerations.add(generation.get(handler));
                } catch (Exception e) {
                  throw new AssertionError(e);
                }
              }
            }
          });
    }

    String attempt() throws Exception {
      try {
        check.invoke(handler);
        return "OK";
      } catch (InvocationTargetException failure) {
        return failure.getCause().getClass().getSimpleName();
      }
    }

    int pendingCount() throws Exception {
      return ((List<?>) pending.get(handler)).size();
    }

    int oldCount() throws Exception {
      return ((Set<?>) old.get(handler)).size();
    }

    boolean supportsLive() throws Exception {
      return live.getBoolean(handler);
    }

    String snapshot() {
      List<String> values = new ArrayList<>();
      for (InstrumentProfile p : collector.view()) {
        values.add(
            p.getType() + ":" + p.getSymbol() + ":" + p.getDescription() + ":" + p.getField("X"));
      }
      Collections.sort(values);
      return String.join(",", values);
    }

    @Override
    public void close() {
      connection.close();
    }
  }

  private static Field field(Class<?> type, String name) throws Exception {
    Field field = type.getDeclaredField(name);
    field.setAccessible(true);
    return field;
  }

  private static String property(String request, String name) {
    for (String line : request.split("\r\n")) {
      int colon = line.indexOf(':');
      if (colon > 0 && line.substring(0, colon).equalsIgnoreCase(name))
        return line.substring(colon + 1).trim();
    }
    return "ABSENT";
  }

  private static void carryover() throws Exception {
    Reply broken = new Reply(200, T1, null, HEADER + record("A") + "STOCK,\"unterminated");
    Reply fresh = new Reply(200, T2, null, HEADER + record("C") + "##COMPLETE\n");
    try (Server server = new Server(broken, fresh);
        Probe probe = new Probe(server.address())) {
      row("before-flush-error", probe.attempt());
      row("before-flush-pending", probe.pendingCount());
      row("before-flush-model", probe.snapshot());
      row("before-flush-retry", probe.attempt());
      row("before-flush-carried-model", probe.snapshot());
    }
    Reply afterFlush =
        new Reply(
            200, T1, null, HEADER + record("A") + "##\n" + record("B") + "STOCK,\"unterminated");
    Reply unchanged = new Reply(304, null, null, "");
    Reply sameDate = new Reply(200, T2, "provided", "not-valid-ipf");
    Reply liveEof = new Reply(200, T3, "provided", HEADER + record("D"));
    Reply liveMarker =
        new Reply(200, T4, "provided", HEADER + record("G") + "##COMPLETE\n" + record("H"));
    try (Server server = new Server(afterFlush, fresh, unchanged, sameDate, liveEof, liveMarker);
        Probe probe = new Probe(server.address())) {
      row("after-flush-error", probe.attempt());
      row("after-flush-pending", probe.pendingCount());
      row("after-flush-model", probe.snapshot());
      row("after-flush-old-generations", probe.oldCount());
      row("retry-result", probe.attempt());
      row("retry-carried-model", probe.snapshot());
      row("retry-pending", probe.pendingCount());
      row("304-result", probe.attempt());
      row("304-model", probe.snapshot());
      row("request-live", property(server.requests.get(0), "X-Live"));
      row("first-conditional", property(server.requests.get(0), "If-Modified-Since"));
      row("304-conditional", property(server.requests.get(2), "If-Modified-Since"));
      row("same-date-result", probe.attempt());
      row("same-date-preserves-capability", !probe.supportsLive());
      row("live-eof-result", probe.attempt());
      row("live-eof-retains-old", probe.snapshot());
      row("live-marker-result", probe.attempt());
      row("live-marker-replaces-old", probe.snapshot());
      row(
          "one-generation-across-complete",
          probe.markerGenerations.size() == 2
              && probe.markerGenerations.get(0) == probe.markerGenerations.get(1));
      row("live-retry-no-conditional", property(server.requests.get(5), "If-Modified-Since"));
    }
    try (Server server = new Server(new Reply(304, null, null, ""));
        Probe probe = new Probe(server.address())) {
      row("unsolicited304", probe.attempt());
    }
    try (Server server = new Server(new Reply(200, null, "Provided", HEADER + record("A")));
        Probe probe = new Probe(server.address())) {
      row("mixed-case-result", probe.attempt());
      row("mixed-case-live", probe.supportsLive());
    }
  }

  private static void publicLifecycle() throws Exception {
    try (Server server =
        new Server(new Reply(200, T1, "provided", HEADER + record("A") + "##COMPLETE\n"))) {
      InstrumentProfileCollector collector = new InstrumentProfileCollector();
      collector.setExecutor(Runnable::run);
      InstrumentProfileConnection connection =
          InstrumentProfileConnection.createConnection(server.address(), collector);
      connection.setUpdatePeriod(5000);
      List<String> states = Collections.synchronizedList(new ArrayList<String>());
      AtomicReference<Thread> worker = new AtomicReference<>();
      CountDownLatch closed = new CountDownLatch(1);
      connection.addStateChangeListener(
          event -> {
            states.add(event.getOldValue() + ">" + event.getNewValue());
            if (connection.getState() == InstrumentProfileConnection.State.CONNECTED)
              worker.set(Thread.currentThread());
            if (connection.getState() == InstrumentProfileConnection.State.COMPLETED)
              connection.close();
            if (connection.getState() == InstrumentProfileConnection.State.CLOSED)
              closed.countDown();
          });
      connection.start();
      if (!closed.await(4, TimeUnit.SECONDS)) throw new AssertionError("lifecycle deadline");
      connection.close();
      if (worker.get() == null) throw new AssertionError("missing handler");
      worker.get().join(4000);
      if (worker.get().isAlive()) throw new AssertionError("handler did not retire");
      row("public-state-order", String.join(",", states));
      row("public-worker-retired", true);
      try {
        connection.start();
        throw new AssertionError("restart accepted");
      } catch (IllegalStateException expected) {
        row("public-restart", "IllegalStateException");
      }
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      System.out.println("ConnectionOracle --run | --help | --help-ru");
      System.out.println(
          "Finite JDK8/QD loopback connection observations; no external input/service.");
      System.out.println(
          "No args/help start no sockets/threads. --run owns/joins sockets and workers; exit0"
              + " success, nonzero failure.");
      return;
    }
    if (args.length == 1 && args[0].equals("--help-ru")) {
      System.out.println("ConnectionOracle --run | --help | --help-ru");
      System.out.println("Конечные JDK8/QD loopback наблюдения; без внешних входов/сервиса.");
      System.out.println(
          "Без аргументов/справка не запускают sockets/threads; --run владеет и завершает их. Код0:"
              + " успех, иной: ошибка.");
      return;
    }
    if (args.length != 1 || !args[0].equals("--run"))
      throw new IllegalArgumentException("use --help");
    String[] periods = {
      "0",
      "1",
      "1ms",
      "1s",
      "0.5s",
      "1m",
      "1h",
      "2d",
      "00:00:01",
      "1S",
      "1.5m",
      "-1s",
      "1m30s",
      "PT1S",
      "inf",
      "INF",
      "2147483647d",
      "9223372036854776s",
      "1d9223372036854776s",
      "\u0661d",
      "\u0661s",
      "1\u017f",
      "\u0131nf"
    };
    for (String period : periods) {
      try {
        InstrumentProfileConnection c =
            InstrumentProfileConnection.createConnection(
                "http://127.0.0.1:1/profiles[update=" + period + "]",
                new InstrumentProfileCollector());
        row("period-" + period, c.getUpdatePeriod());
        c.close();
      } catch (IllegalArgumentException expected) {
        row("period-" + period, "IllegalArgumentException");
      }
    }
    StringBuilder digitStarts = new StringBuilder();
    for (int ch = 0; ch <= 0xffff; ch++) {
      if (Character.isDigit((char) ch) && Character.digit((char) ch, 10) == 0) {
        if (digitStarts.length() != 0) digitStarts.append(',');
        digitStarts.append(Integer.toHexString(ch));
      }
    }
    row("period-bmp-digit-starts", digitStarts);
    carryover();
    publicLifecycle();
  }
}
