package qdcxx.oracle;

import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;

/**
 * Test-only caller: java -cp DRIVER_CLASSES:PROVIDERS qdcxx.oracle.HelpFailureProbe ORACLE_CLASS.
 * Invokes one allowlisted oracle's help with a failing stdout stream. Expected oracle exit is 1
 * (collector/session) or 2 (wire), with only PROBE_READY on stderr. Exit 98 is an unexpected
 * exception; exit 99 means the oracle incorrectly returned after output failed. No capture inputs
 * are opened.
 */
public final class HelpFailureProbe {
  public static void main(String[] args) throws Exception {
    if (args.length != 1 || !args[0].matches("qdcxx\\.oracle\\.(Wire|Collector|Session)Oracle")) {
      System.err.println("HelpFailureProbe requires an allowlisted oracle class");
      System.exit(3);
    }
    System.setOut(
        new PrintStream(
            new OutputStream() {
              @Override
              public void write(int value) throws IOException {
                throw new IOException("synthetic stdout failure");
              }
            },
            true,
            "UTF-8"));
    System.err.println("PROBE_READY");
    try {
      Class.forName(args[0])
          .getMethod("main", String[].class)
          .invoke(null, (Object) new String[] {"--help"});
    } catch (Exception error) {
      System.err.println("UNEXPECTED_EXCEPTION");
      System.exit(98);
    }
    System.err.println("MAIN_RETURNED");
    System.exit(99);
  }
}
