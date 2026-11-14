// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;

/** Bounded child owner; command arguments are never interpreted by a shell. */
public final class ProcessSupervisor {
  private static final Duration TIMEOUT = Duration.ofSeconds(15);
  private static final int MAX_OBSERVATIONS = 4096;

  public static final class Child implements AutoCloseable {
    private final Process process;
    private final BufferedWriter input;
    private final BlockingQueue<String> lines = new ArrayBlockingQueue<>(MAX_OBSERVATIONS);
    private final List<String> deferred = new ArrayList<>();
    private final Thread stdoutReader;

    public Child(List<String> command, Path stdout, Path stderr) throws IOException {
      process = new ProcessBuilder(command).redirectError(stderr.toFile()).start();
      input =
          new BufferedWriter(
              new OutputStreamWriter(process.getOutputStream(), StandardCharsets.UTF_8));
      stdoutReader =
          new Thread(
              () -> {
                try (BufferedReader reader =
                        new BufferedReader(
                            new InputStreamReader(
                                process.getInputStream(), StandardCharsets.UTF_8));
                    BufferedWriter log = Files.newBufferedWriter(stdout, StandardCharsets.UTF_8)) {
                  String line;
                  while ((line = reader.readLine()) != null) {
                    log.write(line);
                    log.newLine();
                    log.flush();
                    if (!lines.offer(line))
                      throw new IOException("Child produced too many unconsumed lines");
                  }
                } catch (IOException failure) {
                  lines.clear();
                  lines.offer("SUPERVISOR_IO_ERROR " + failure);
                }
              },
              "child-stdout");
      stdoutReader.start();
    }

    public void send(String command) throws IOException {
      input.write(command);
      input.newLine();
      input.flush();
    }

    public String await(String prefix) throws Exception {
      long deadline = System.nanoTime() + TIMEOUT.toNanos();
      for (int i = 0; i < deferred.size(); i++) {
        if (deferred.get(i).startsWith(prefix)) return deferred.remove(i);
      }
      while (System.nanoTime() < deadline) {
        String line = lines.poll(100, TimeUnit.MILLISECONDS);
        if (line != null) {
          if (line.startsWith("SUPERVISOR_IO_ERROR")) throw new IOException(line);
          if (line.startsWith(prefix)) return line;
          defer(line);
        }
        if (!process.isAlive() && lines.isEmpty())
          throw new IOException(
              "Child exited " + process.exitValue() + " while waiting for " + prefix);
      }
      throw new IOException("Timed out waiting for " + prefix + ", observed " + deferred);
    }

    public void awaitNumber(String label, double expected) throws Exception {
      String line = await(label + " ");
      double actual = Double.parseDouble(line.substring(label.length() + 1));
      if (Double.compare(actual, expected) != 0)
        throw new IOException("Expected " + label + " " + expected + ", observed " + line);
    }

    public void awaitExact(String expected) throws Exception {
      String line = await(expected);
      if (!line.equals(expected))
        throw new IOException("Expected " + expected + ", observed " + line);
    }

    public void awaitDesiredKeys(int count) throws Exception {
      long deadline = System.nanoTime() + TIMEOUT.toNanos();
      while (System.nanoTime() < deadline) {
        send("STATS");
        boolean ready = true;
        for (String family : List.of("TICKER", "STREAM", "HISTORY")) {
          ready &= await("STATS " + family + " ").contains(" desiredKeys=" + count + " ");
        }
        if (ready) return;
        Thread.sleep(25);
      }
      throw new IOException("Relay did not install second client demand");
    }

    public void awaitOrder(long index, int requiredFlags) throws Exception {
      String line = await("ORDER " + index + " ");
      int flags = Integer.parseInt(line.substring(line.lastIndexOf(' ') + 1));
      if ((flags & requiredFlags) != requiredFlags) throw new IOException("Unexpected " + line);
    }

    public void requireNoLine(String prefix, Duration duration) throws Exception {
      for (String line : deferred) {
        if (line.startsWith(prefix)) throw new IOException("Unexpected " + line);
      }
      long deadline = System.nanoTime() + duration.toNanos();
      while (System.nanoTime() < deadline) {
        String line = lines.poll(100, TimeUnit.MILLISECONDS);
        if (line == null) continue;
        if (line.startsWith(prefix)) throw new IOException("Unexpected " + line);
        if (line.startsWith("SUPERVISOR_IO_ERROR")) throw new IOException(line);
        defer(line);
      }
    }

    private void defer(String line) throws IOException {
      if (deferred.size() == MAX_OBSERVATIONS)
        throw new IOException("Too many unmatched child observations");
      deferred.add(line);
    }

    public void requireCleanExit() throws Exception {
      if (!process.waitFor(TIMEOUT.toMillis(), TimeUnit.MILLISECONDS))
        throw new IOException("Child did not stop within deadline");
      stdoutReader.join(TIMEOUT.toMillis());
      if (stdoutReader.isAlive()) throw new IOException("Child stdout did not close");
      if (process.exitValue() != 0) throw new IOException("Child exited " + process.exitValue());
    }

    @Override
    public void close() throws Exception {
      if (process.isAlive()) {
        try {
          send("STOP");
        } catch (IOException ignored) {
        }
        if (!process.waitFor(3, TimeUnit.SECONDS)) {
          process.destroy();
          if (!process.waitFor(2, TimeUnit.SECONDS)) {
            process.destroyForcibly();
            if (!process.waitFor(5, TimeUnit.SECONDS))
              throw new IOException("Child cleanup timed out");
          }
        }
      }
      input.close();
      stdoutReader.join(TIMEOUT.toMillis());
      if (stdoutReader.isAlive()) throw new IOException("Child stdout cleanup timed out");
    }
  }
}
