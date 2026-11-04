// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.dxfeed.ipf.InstrumentProfile;
import com.dxfeed.ipf.live.InstrumentProfileCollector;
import com.dxfeed.ipf.live.InstrumentProfileConnection;
import com.dxfeed.ipf.live.InstrumentProfileUpdateListener;
import java.beans.PropertyChangeListener;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Bounded actual-QD observations; no endpoint, SDK or inferred C++ expectations. */
public final class ShutdownOracle {
  private static final List<InstrumentProfileConnection> CONNECTIONS = new ArrayList<>();
  private static final List<Thread> THREADS = new ArrayList<>();
  private static final List<CountDownLatch> RELEASES = new ArrayList<>();

  private static void row(String name, Object value) {
    System.out.println(name + "\t" + value);
  }

  private static void require(boolean value, String message) {
    if (!value) throw new AssertionError(message);
  }

  private static void await(CountDownLatch latch) {
    try {
      require(latch.await(3, TimeUnit.SECONDS), "owned callback deadline");
    } catch (InterruptedException failure) {
      Thread.currentThread().interrupt();
      throw new AssertionError(failure);
    }
  }

  /** Reflection only retains the actual public-start handler for bounded cleanup. */
  private static Thread start(InstrumentProfileConnection connection) throws Exception {
    Field field = InstrumentProfileConnection.class.getDeclaredField("handlerThread");
    field.setAccessible(true);
    AtomicReference<Thread> worker = new AtomicReference<>();
    PropertyChangeListener capture =
        event -> {
          if (event.getNewValue() == InstrumentProfileConnection.State.CONNECTING) {
            try {
              worker.set((Thread) field.get(connection));
            } catch (IllegalAccessException failure) {
              throw new AssertionError(failure);
            }
          }
        };
    connection.addStateChangeListener(capture);
    connection.start();
    connection.removeStateChangeListener(capture);
    require(worker.get() != null, "actual handler identity");
    THREADS.add(worker.get());
    return worker.get();
  }

  private static InstrumentProfileConnection connection(Path file) {
    InstrumentProfileCollector model = new InstrumentProfileCollector();
    model.setExecutor(Runnable::run);
    InstrumentProfileConnection value =
        InstrumentProfileConnection.createConnection(
            file.toUri().toString() + "[update=inf]", model);
    CONNECTIONS.add(value);
    return value;
  }

  private static void retire(InstrumentProfileConnection connection, Thread worker)
      throws Exception {
    connection.close();
    worker.join(4000);
    require(!worker.isAlive(), "owned handler retirement");
  }

  private static void run() throws Exception {
    Path file = Files.createTempFile("qd-ipf-shutdown-", ".ipf");
    Files.write(
        file, "#STOCK::=TYPE,SYMBOL\nSTOCK,A\n##COMPLETE\n".getBytes(StandardCharsets.UTF_8));
    try {
      InstrumentProfileConnection removed = connection(file);
      CountDownLatch entered = new CountDownLatch(1), release = new CountDownLatch(1);
      RELEASES.add(release);
      AtomicInteger selectedCalls = new AtomicInteger(), completed = new AtomicInteger();
      PropertyChangeListener selected =
          event -> {
            selectedCalls.incrementAndGet();
            if (event.getNewValue() == InstrumentProfileConnection.State.CONNECTED) {
              entered.countDown();
              await(release);
            }
          };
      removed.addStateChangeListener(selected);
      removed.addStateChangeListener(
          event -> {
            if (event.getNewValue() == InstrumentProfileConnection.State.COMPLETED)
              completed.incrementAndGet();
          });
      Thread handler = start(removed);
      await(entered);
      removed.removeStateChangeListener(selected);
      row("remove-returned-while-active", handler.isAlive() && release.getCount() == 1);
      row("selected-calls-before-release", selectedCalls.get());
      release.countDown();
      row("removed-survivor-completed", removed.waitUntilCompleted(3, TimeUnit.SECONDS));
      retire(removed, handler);
      row("removed-selected-final-calls", selectedCalls.get());
      row("removed-survivor-completion-count", completed.get());
      row("removed-handler-retired", !handler.isAlive());

      InstrumentProfileConnection throwing = connection(file);
      AtomicInteger failures = new AtomicInteger(), survived = new AtomicInteger();
      throwing.addStateChangeListener(
          event -> {
            if (event.getNewValue() == InstrumentProfileConnection.State.CONNECTED) {
              failures.incrementAndGet();
              throw new IllegalStateException("bounded listener witness");
            }
          });
      throwing.addStateChangeListener(
          event -> {
            if (event.getNewValue() == InstrumentProfileConnection.State.COMPLETED)
              survived.incrementAndGet();
          });
      Thread throwingHandler = start(throwing);
      row("throwing-first-snapshot", throwing.waitUntilCompleted(3, TimeUnit.SECONDS));
      retire(throwing, throwingHandler);
      row("throwing-callback-count", failures.get());
      row("throwing-survivor-completion-count", survived.get());
      row("throwing-handler-retired", !throwingHandler.isAlive());

      InstrumentProfileCollector model = new InstrumentProfileCollector();
      model.setExecutor(Runnable::run);
      AtomicInteger failed = new AtomicInteger(), delivered = new AtomicInteger();
      InstrumentProfileUpdateListener failure =
          iterator -> {
            if (iterator.hasNext()) {
              failed.incrementAndGet();
              throw new IllegalStateException("bounded collector witness");
            }
          };
      InstrumentProfileUpdateListener survivor =
          iterator -> {
            while (iterator.hasNext()) {
              iterator.next();
              delivered.incrementAndGet();
              require(delivered.get() <= 2, "bounded collector rows");
            }
          };
      model.addUpdateListener(failure);
      model.addUpdateListener(survivor);
      InstrumentProfile first = new InstrumentProfile();
      first.setType("STOCK");
      first.setSymbol("A");
      model.updateInstrumentProfile(first);
      model.removeUpdateListener(failure);
      InstrumentProfile second = new InstrumentProfile();
      second.setType("STOCK");
      second.setSymbol("B");
      model.updateInstrumentProfile(second);
      model.removeUpdateListener(survivor);
      row("collector-contained-failure-count", failed.get());
      row("collector-survivor-profile-count", delivered.get());
    } finally {
      for (CountDownLatch release : RELEASES) release.countDown();
      for (InstrumentProfileConnection value : CONNECTIONS) value.close();
      for (Thread value : THREADS) value.interrupt();
      for (Thread value : THREADS) value.join(4000);
      Files.delete(file);
      for (Thread value : THREADS) require(!value.isAlive(), "owned thread still active");
    }
  }

  public static void main(String[] args) {
    if (args.length == 0 || (args.length == 1 && "--help".equals(args[0]))) {
      System.out.println(
          "ShutdownOracle --run | --help | --help-ru\n"
              + "Observe pinned QD in-flight removal, contained callback failure and handler"
              + " retirement.\n"
              + "No args/help: no source/threads. --run: one owned regular temporary IPF,"
              + " deleted;\n"
              + "finite waits and joined threads, direct collector executor, no network; stdout"
              + " TSV/QD logs.\n"
              + "Exit0 observations complete; exit1 exception; exit2 invalid arguments. No other"
              + " parameters.");
      return;
    }
    if (args.length == 1 && "--help-ru".equals(args[0])) {
      System.out.println(
          "ShutdownOracle --run | --help | --help-ru\n"
              + "Наблюдает QD in-flight removal, contained callback failure и handler retirement.\n"
              + "Без аргументов/help source/threads не создаются. --run: собственный временный"
              + " regular IPF,\n"
              + "файл удаляется, waits конечны, threads joined, collector executor direct, без"
              + " сети;\n"
              + "stdout TSV/QD logs. Exit0 наблюдения завершены; exit1 exception; exit2 неверные"
              + " аргументы.\n"
              + "Других параметров нет.");
      return;
    }
    if (args.length == 1 && "--run".equals(args[0])) {
      try {
        run();
      } catch (Throwable failure) {
        failure.printStackTrace();
        System.exit(1);
      }
      return;
    }
    System.err.println("invalid arguments; use --help");
    System.exit(2);
  }
}
