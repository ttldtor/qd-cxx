package qdcxx.oracle;

import java.lang.management.ManagementFactory;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.concurrent.TimeUnit;

/** Test-Lifetime.ps1 compiles this child to reproduce inherited pipes and active-root timeout. */
public final class ProcessLifetimeProbe {
  private static final long CHILD_HOLD_MILLIS = 20000;
  private static final long READINESS_TIMEOUT_NANOS = TimeUnit.SECONDS.toNanos(2);
  private static final long READINESS_POLL_MILLIS = 10;

  private ProcessLifetimeProbe() {}

  public static void main(String[] args) throws Exception {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      System.out.println(
          "ProcessLifetimeProbe parent|child EXISTING_PRIVATE_DIRECTORY ROOT_SLEEP_MS");
      return;
    }
    if (args.length != 3 || !(args[0].equals("parent") || args[0].equals("child"))) {
      System.exit(2);
      return;
    }
    Path directory = Paths.get(args[1]);
    String pid = ManagementFactory.getRuntimeMXBean().getName().split("@")[0];
    Files.write(directory.resolve(args[0] + ".pid"), pid.getBytes(StandardCharsets.UTF_8));
    if (args[0].equals("child")) {
      Thread.sleep(CHILD_HOLD_MILLIS);
      return;
    }
    String java =
        Paths.get(
                System.getProperty("java.home"),
                "bin",
                System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java")
            .toString();
    new ProcessBuilder(
            java,
            "-Xmx32m",
            "-Dfile.encoding=UTF-8",
            "-cp",
            System.getProperty("java.class.path"),
            ProcessLifetimeProbe.class.getName(),
            "child",
            directory.toString(),
            "0")
        .inheritIO()
        .start();
    long deadline = System.nanoTime() + READINESS_TIMEOUT_NANOS;
    while (!Files.exists(directory.resolve("child.pid"))) {
      if (System.nanoTime() >= deadline) {
        throw new IllegalStateException("child readiness timeout");
      }
      Thread.sleep(READINESS_POLL_MILLIS);
    }
    System.out.println("parent/child ready");
    Thread.sleep(Long.parseLong(args[2]));
  }
}
