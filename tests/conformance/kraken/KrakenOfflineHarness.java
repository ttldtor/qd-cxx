// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Arrays;
import java.util.List;

/** Bounded, locally supervised Kraken WSS to QTP Quote qualification. */
public final class KrakenOfflineHarness {
  private static final String SYMBOL = "BTC/USD";
  private static final String DURATION_MS = "12000";
  private static final Duration QUIET_WINDOW = Duration.ofMillis(300);

  private record Level(String bid, String ask, String bidSize, String askSize, int atSecond) {
    String timestamp() {
      return "2026-01-01T00:00:0" + atSecond + ".000000Z";
    }

    List<String> fields() {
      return List.of(bid, ask, bidSize, askSize);
    }

    String message(String kind) {
      return """
      {"channel":"ticker","type":"%s","data":[{"symbol":"%s","timestamp":"%s","bid":%s,"ask":%s,"bid_qty":%s,"ask_qty":%s}]}
      """
          .formatted(kind, SYMBOL, timestamp(), bid, ask, bidSize, askSize)
          .strip();
    }
  }

  private static final Level FIRST = new Level("101.25", "101.5", "0.125", "0.5", 1);
  private static final Level UPDATE = new Level("102.25", "102.5", "0.25", "0.75", 2);
  private static final Level PRE_SNAPSHOT = new Level("900", "901", "1", "2", 3);
  private static final Level REOPEN = new Level("103.25", "103.5", "0.375", "0.875", 4);
  private static final Level REMAINING = new Level("104.25", "104.5", "0.5", "1.125", 5);

  private static ProcessSupervisor.Child spawn(List<String> command, Path out, String name)
      throws IOException {
    return new ProcessSupervisor.Child(
        command, out.resolve(name + ".out"), out.resolve(name + ".err"));
  }

  private static void check(boolean condition, String message) throws IOException {
    if (!condition) {
      throw new IOException(message);
    }
  }

  private static int port(String text) throws IOException {
    int result = Integer.parseInt(text);
    check(result > 0 && result <= 65535, "invalid fixture port " + result);
    return result;
  }

  private static void send(ProcessSupervisor.Child fixture, String command) throws Exception {
    fixture.send(command);
    fixture.awaitExact("SENT");
  }

  private static List<Long> quote(
      ProcessSupervisor.Child client, Level expected, boolean nativeConsumer) throws Exception {
    String line = client.await("QUOTE ");
    String[] fields = line.split(" ");
    check(fields.length == 18 && fields[1].equals(SYMBOL), "wrong Quote envelope " + line);
    check(
        Arrays.asList(fields).subList(6, 10).equals(expected.fields()),
        "wrong Quote level " + line + ", expected " + expected.fields());
    check(
        Arrays.stream(fields, 10, fields.length).allMatch("0"::equals),
        "expected zero projected times, sequence, and exchange codes: " + line);
    if (!nativeConsumer) {
      check(
          Arrays.stream(fields, 2, 6).allMatch("-"::equals), "unexpected Java raw fields " + line);
      return List.of();
    }
    return Arrays.stream(fields, 2, 6).map(Long::parseLong).toList();
  }

  private static void matchPublish(
      ProcessSupervisor.Child gateway, long generation, Level expected, List<Long> nativeRaw)
      throws Exception {
    String line = gateway.await("PUBLISH ");
    String[] fields = line.split(" ");
    check(fields.length == 7, "invalid gateway trace " + line);
    check(
        Long.parseLong(fields[1]) == generation && fields[2].equals(expected.timestamp()),
        "wrong generation or provider timestamp " + line);
    List<Long> published = Arrays.stream(fields, 3, 7).map(Long::parseLong).toList();
    check(published.equals(nativeRaw), "gateway and native Quote raw values differ: " + line);
  }

  private static void stopClient(ProcessSupervisor.Child client, int expectedQuotes)
      throws Exception {
    client.send("STOP");
    client.requireCleanExit();
    client.awaitExact("CLIENT_STOP " + expectedQuotes);
  }

  private static void run(String[] args, Path output) throws Exception {
    String gatewayPath = args[0];
    String fixturePath = args[1];
    String nativeClient = args[2];
    String java = args[3];
    String classpath = args[4];
    Path tls = Path.of(args[5]).toAbsolutePath();

    try (var peer = spawn(List.of(fixturePath, tls.toString()), output, "peer");
        var jvm =
            spawn(
                List.of(java, "-Duser.timezone=UTC", "-cp", classpath, "KrakenJavaQuoteClient"),
                output,
                "java")) {
      // JVM warm-up must finish before the gateway's finite observation interval begins.
      jvm.awaitExact("CLIENT_PREPARED");
      int sourcePort = port(peer.await("PORT ").substring("PORT ".length()));
      try (var gateway =
          spawn(
              List.of(
                  gatewayPath,
                  "--trust-pem",
                  tls.resolve("ca.pem").toString(),
                  "--source-host",
                  "127.0.0.1",
                  "--source-identity",
                  "localhost",
                  "--source-port",
                  Integer.toString(sourcePort),
                  "--listen-port",
                  "0",
                  "--duration-ms",
                  DURATION_MS,
                  "--trace-quotes"),
              output,
              "gateway")) {
        String listen = gateway.await("LISTEN 127.0.0.1 ");
        String address = "127.0.0.1:" + port(listen.substring("LISTEN 127.0.0.1 ".length()));
        try (var cpp = spawn(List.of(nativeClient, address), output, "native")) {
          cpp.awaitExact("CLIENT_READY");
          String subscription = peer.await("SUBSCRIBE ");
          long firstRequest = Long.parseLong(subscription.substring("SUBSCRIBE ".length()));
          send(peer, "ACK");
          send(peer, "SEND " + FIRST.message("snapshot"));
          matchPublish(gateway, 1, FIRST, quote(cpp, FIRST, true));
          jvm.send("CONNECT " + address);
          jvm.awaitExact("CLIENT_READY");
          quote(jvm, FIRST, false);

          send(peer, "SEND " + UPDATE.message("update"));
          matchPublish(gateway, 1, UPDATE, quote(cpp, UPDATE, true));
          quote(jvm, UPDATE, false);

          peer.send("DROP");
          peer.awaitExact("DROPPED");
          long nextRequest =
              Long.parseLong(peer.await("SUBSCRIBE ").substring("SUBSCRIBE ".length()));
          check(nextRequest > firstRequest, "gateway did not assign a new request id");
          send(peer, "ACK");
          send(peer, "SEND " + PRE_SNAPSHOT.message("update"));
          gateway.requireNoLine("PUBLISH ", QUIET_WINDOW);
          cpp.requireNoLine("QUOTE ", QUIET_WINDOW);
          jvm.requireNoLine("QUOTE ", QUIET_WINDOW);
          send(peer, "SEND " + REOPEN.message("snapshot"));
          matchPublish(gateway, 2, REOPEN, quote(cpp, REOPEN, true));
          quote(jvm, REOPEN, false);
          stopClient(jvm, 3);
          send(peer, "SEND " + REMAINING.message("update"));
          matchPublish(gateway, 2, REMAINING, quote(cpp, REMAINING, true));
          stopClient(cpp, 4);
        }
        gateway.requireCleanExit();
        String counters = gateway.await("STATS ");
        check(
            counters.contains("observed=4 published=4 ")
                && counters.matches(
                    ".* sent_frames=[1-9][0-9]* retired_peers=2 pending_publications=0"),
            "wrong gateway publication or shutdown counters: " + counters);
      }
      peer.send("STOP");
      peer.requireCleanExit();
      peer.awaitExact("PEER_STOP");
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 7) {
      throw new IllegalArgumentException(
          "Usage: KrakenOfflineHarness GATEWAY PEER NATIVE_CLIENT JAVA CLASSPATH TLS_FIXTURES"
              + " OUTPUT");
    }
    Path output = Path.of(args[6]);
    if (output.getParent() != null) {
      Files.createDirectories(output.getParent());
    }
    Files.createDirectory(output);
    run(args, output);
    System.out.println("PASS Kraken WSS, native/Java Quote, reconnect, demand isolation, shutdown");
  }
}
