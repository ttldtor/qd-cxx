// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.List;

/** Local integration driver for the sample process line protocol. */
public final class MixedProcessHarness {
  private static ProcessSupervisor.Child spawn(List<String> command, Path out, String name)
      throws Exception {
    return new ProcessSupervisor.Child(
        command, out.resolve(name + ".out"), out.resolve(name + ".err"));
  }

  private static List<String> javaPeer(String java, String cp, String role, String port) {
    return port == null
        ? List.of(java, "-cp", cp, "JavaPeer", role)
        : List.of(java, "-cp", cp, "JavaPeer", role, port);
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      System.out.println(
          "Usage: MixedProcessHarness MODE JAVA CLASSPATH SOURCE SERVER CLIENT OUTPUT_DIR");
      System.out.println(
          "MODE: native, java-source, java-feed. Use unused for JAVA and CLASSPATH in native"
              + " mode.");
      System.out.println(
          "SOURCE, SERVER and CLIENT are sample executables. OUTPUT_DIR receives stdout/stderr"
              + " logs.");
      System.out.println(
          "Runs four local loopback processes with 15-second observation deadlines. Exit 0 on pass"
              + " or help; nonzero on error.");
      return;
    }
    if (args.length == 1 && args[0].equals("--help-ru")) {
      System.out.println(
          "Запуск: MixedProcessHarness MODE JAVA CLASSPATH SOURCE SERVER CLIENT OUTPUT_DIR");
      System.out.println(
          "MODE: native, java-source, java-feed. В режиме native используйте unused вместо JAVA и"
              + " CLASSPATH.");
      System.out.println(
          "SOURCE, SERVER и CLIENT — исполняемые файлы примера. OUTPUT_DIR получает журналы"
              + " stdout/stderr.");
      System.out.println(
          "Четыре локальных процесса, ожидание наблюдения до 15 секунд. Код 0 при успехе или"
              + " справке, иначе ненулевой.");
      return;
    }
    if (args.length != 7
        || !(args[0].equals("java-source")
            || args[0].equals("java-feed")
            || args[0].equals("native"))) {
      throw new IllegalArgumentException(
          "java-source|java-feed|native JAVA CLASSPATH SOURCE SERVER CLIENT OUTPUT_DIR");
    }
    String mode = args[0];
    Path output = Path.of(args[6]);
    if (output.getParent() != null) Files.createDirectories(output.getParent());
    Files.createDirectory(output);
    List<String> sourceCommand =
        mode.equals("java-source")
            ? javaPeer(args[1], args[2], "source", null)
            : List.of(args[3], "source", "0");
    try (ProcessSupervisor.Child source = spawn(sourceCommand, output, "source")) {
      String sourcePort = source.await("PORT ").substring(5);
      try (ProcessSupervisor.Child relay =
          spawn(List.of(args[4], "server", "0", sourcePort), output, "relay")) {
        String relayPort = relay.await("PORT ").substring(5);
        relay.await("SOURCE ");
        List<String> clientCommand =
            mode.equals("java-feed")
                ? javaPeer(args[1], args[2], "feed", relayPort)
                : List.of(args[5], "client", relayPort, "IBM");
        try (ProcessSupervisor.Child first = spawn(clientCommand, output, "first")) {
          first.await("STARTED");
          for (String type : List.of("Quote", "TimeAndSale", "Order")) source.await("ADD " + type);
          source.send("QUOTE");
          source.await("SENT QUOTE");
          first.awaitNumber("QUOTE", 101);
          try (ProcessSupervisor.Child second =
              spawn(
                  mode.equals("java-feed")
                      ? List.of(args[1], "-cp", args[2], "JavaPeer", "feed", relayPort, "second")
                      : List.of(args[5], "client", relayPort, "IBM", "MSFT"),
                  output,
                  "second")) {
            second.await("STARTED");
            second.awaitNumber("QUOTE", 101);
            relay.awaitDesiredKeys(2);
            source.send("EVENTS");
            source.await("SENT EVENTS");
            for (ProcessSupervisor.Child client : List.of(first, second)) {
              client.awaitNumber("SALE", 102);
              client.awaitNumber("SALE", 102);
              client.awaitOrder(7, 4);
              client.awaitExact("ORDER 0 8");
            }
            source.send("REMOVE");
            source.await("SENT REMOVE");
            first.awaitExact("ORDER 7 2");
            second.awaitExact("ORDER 7 2");
            first.send("STOP");
            first.requireCleanExit();
            source.requireNoLine("REMOVE ", Duration.ofSeconds(2));
            second.send("CLEAR");
            second.await("CLEARED");
            for (String type : List.of("Quote", "TimeAndSale", "Order"))
              source.await("REMOVE " + type);
            second.send("STOP");
            second.requireCleanExit();
          }
        }
        relay.send("STOP");
        relay.requireCleanExit();
      }
      source.send("STOP");
      source.requireCleanExit();
    }
    System.out.println("PASS " + mode);
  }
}
