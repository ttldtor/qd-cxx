package qdcxx.oracle;

import java.lang.management.ManagementFactory;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Internal raw-stream worker launched only inside the native runner's owned process job. */
public final class RawChild {
  private static final String HELP =
      "Internal RawChild: TIMEOUT_MS MAX_BYTES STDOUT STDERR STATUS CWD EXE [ARG ...]\n"
          + "Requires native runner process-tree ownership before JVM startup.\n"
          + "Redirects child streams directly to raw files; writes bounded TSV status.\n"
          + "Exit: child status, 125 resource stop, 126 setup/status failure.\n"
          + "No arguments/--help prints help without file work.\n";

  private static void status(Path path, int exit, boolean timeout, boolean output)
      throws Exception {
    String pid = ManagementFactory.getRuntimeMXBean().getName().split("@", 2)[0];
    String text =
        "exit-code\t"
            + exit
            + "\ntimed-out\t"
            + timeout
            + "\toutput-exceeded\t"
            + output
            + "\nworker-pid\t"
            + pid
            + "\n";
    Files.write(path, text.getBytes(StandardCharsets.US_ASCII));
  }

  /** Internal caller: Acceptance.Common.ps1; all arguments are explicit and no shell is used. */
  public static void main(String[] args) {
    if (args.length == 0 || args.length == 1 && args[0].equals("--help")) {
      System.out.print(HELP);
      if (System.out.checkError()) {
        System.exit(126);
      }
      return;
    }
    Path status = null;
    try {
      if (args.length < 7) {
        throw new IllegalArgumentException();
      }
      long timeoutMs = Long.parseLong(args[0]);
      long maxBytes = Long.parseLong(args[1]);
      if (timeoutMs <= 0 || timeoutMs > 300000 || maxBytes <= 0 || maxBytes > 1073741824L) {
        throw new IllegalArgumentException();
      }
      Path stdout = Paths.get(args[2]);
      Path stderr = Paths.get(args[3]);
      status = Paths.get(args[4]);
      List<String> command = new ArrayList<>();
      for (int i = 6; i < args.length; i++) {
        command.add(args[i]);
      }
      long started = System.nanoTime();
      Process child =
          new ProcessBuilder(command)
              .directory(Paths.get(args[5]).toFile())
              .redirectOutput(stdout.toFile())
              .redirectError(stderr.toFile())
              .start();
      for (; ; ) {
        boolean exited = child.waitFor(20, TimeUnit.MILLISECONDS);
        boolean timeout = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - started) >= timeoutMs;
        boolean output = Files.size(stdout) > maxBytes || Files.size(stderr) > maxBytes;
        if (timeout || output) {
          status(status, 125, timeout, output);
          // Exiting the job root asks the native owner to terminate every remaining descendant.
          System.exit(125);
        }
        if (exited) {
          int exit = child.exitValue();
          status(status, exit, false, false);
          System.exit(exit);
        }
      }
    } catch (Throwable failure) {
      if (status != null) {
        try {
          status(status, 126, false, false);
        } catch (Throwable ignored) {
        }
      }
      System.err.println("raw-child-failure\t" + failure.getClass().getSimpleName());
      System.exit(126);
    }
  }
}
