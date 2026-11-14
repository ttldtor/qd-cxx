# Distribution qualification

[Русская версия](README.ru.md).

This local check runs a QTP source, native relay and two receiver processes. It verifies late Quote replay, duplicate TimeAndSale records, indexed Order snapshot and removal, retention of overlapping demand and withdrawal after both clients leave. The child processes bind loopback addresses and ephemeral ports. A successful run prints `PASS <mode>`. Each child has separate stdout and stderr logs. Use a new output directory for each run.

Use a [standalone sample built from the installed Feed SDK](../../../examples/feed_distribution/README.md), a JDK 21 or later and the production Java QD classes from the revision in [upstream.lock](../../../upstream.lock). The Java classpath must include the built QD modules and their required runtime dependencies. Native mode needs only the driver and supervisor classes.

```sh
javac --release 21 -cp "$QD_CLASSPATH" -d "$CLASSES" tests/conformance/distribution/*.java
java -cp "$CLASSES" MixedProcessHarness --help
java -cp "$CLASSES" MixedProcessHarness native unused unused "$SAMPLE" "$SAMPLE" "$SAMPLE" "$LOGS/native"
java -cp "$CLASSES" MixedProcessHarness java-source java "$CLASSES:$QD_CLASSPATH" "$SAMPLE" "$SAMPLE" "$SAMPLE" "$LOGS/java-source"
java -cp "$CLASSES" MixedProcessHarness java-feed java "$CLASSES:$QD_CLASSPATH" "$SAMPLE" "$SAMPLE" "$SAMPLE" "$LOGS/java-feed"
```

Use `;` as the classpath separator on Windows. Set the variables to absolute paths. The test accepts paths to separate sample executables or the same executable for all three roles. The `java-source` mode replaces the publisher with Java QD, and `java-feed` replaces both receivers with Java QD. No credentials or external network service are required.

The publisher conformance suite also checks reconnect, canonical Penta subscription bytes and isolation from a non-reading peer with the native listener configured for bounded writes.
