// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.dxfeed.ipf.live.InstrumentProfileCollector;
import com.dxfeed.ipf.live.InstrumentProfileConnection;
import java.beans.PropertyChangeListener;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Bounded observations of the pinned production first-snapshot wait method. */
public final class CompletionWaitOracle {
  private static final String BODY = "#STOCK::=TYPE,SYMBOL\nSTOCK,A\n##COMPLETE\n";
  private static final List<InstrumentProfileConnection> CONNECTIONS = new ArrayList<>();
  private static final List<Thread> THREADS = new ArrayList<>();
  private static final Field WORKER;
  private static final Field LISTENERS;

  static {
    try {
      WORKER = InstrumentProfileConnection.class.getDeclaredField("handlerThread");
      WORKER.setAccessible(true);
      LISTENERS = InstrumentProfileConnection.class.getDeclaredField("stateChangeListeners");
      LISTENERS.setAccessible(true);
    } catch (ReflectiveOperationException failure) {
      throw new ExceptionInInitializerError(failure);
    }
  }

  private static void row(String key, Object value) {
    System.out.println(key + "\t" + value);
  }

  private static InstrumentProfileConnection connection(Path file) {
    InstrumentProfileConnection value =
        InstrumentProfileConnection.createConnection(
            file.toUri().toString() + "[update=inf]", new InstrumentProfileCollector());
    CONNECTIONS.add(value);
    return value;
  }

  /** Retain every test waiter for bounded exceptional cleanup. */
  private static Thread ownedThread(Runnable action, String name) {
    Thread value = new Thread(action, name);
    THREADS.add(value);
    return value;
  }

  /** Capture the actual handler during the synchronous CONNECTING event, before callback close. */
  private static Thread start(InstrumentProfileConnection connection) throws Exception {
    AtomicReference<Thread> worker = new AtomicReference<>();
    PropertyChangeListener capture =
        event -> {
          if (event.getNewValue() == InstrumentProfileConnection.State.CONNECTING) {
            try {
              worker.set((Thread) WORKER.get(connection));
            } catch (IllegalAccessException failure) {
              throw new AssertionError(failure);
            }
          }
        };
    connection.addStateChangeListener(capture);
    connection.start();
    connection.removeStateChangeListener(capture);
    if (worker.get() == null) throw new AssertionError("missing owned handler");
    THREADS.add(worker.get());
    return worker.get();
  }

  private static void retire(InstrumentProfileConnection connection, Thread worker)
      throws Exception {
    connection.close();
    worker.join(4000);
    if (worker.isAlive()) throw new AssertionError("handler did not retire");
  }

  /**
   * Internal registration barrier only; the actual public wait algorithm is never reimplemented.
   */
  private static void registered(InstrumentProfileConnection connection) throws Exception {
    long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(2);
    while (((List<?>) LISTENERS.get(connection)).isEmpty()) {
      if (System.nanoTime() >= deadline) throw new AssertionError("wait did not register");
      Thread.yield();
    }
  }

  private static void joined(Thread worker) throws Exception {
    worker.join(4000);
    if (worker.isAlive()) {
      worker.interrupt();
      worker.join(1000);
      throw new AssertionError("waiter did not retire");
    }
  }

  private static void run() throws Exception {
    Path file = Files.createTempFile("qd-ipf-wait-", ".ipf");
    Files.write(file, BODY.getBytes(StandardCharsets.UTF_8));
    try {
      InstrumentProfileConnection idle = connection(file);
      row("idle-zero", idle.waitUntilCompleted(0, TimeUnit.MILLISECONDS));
      row("idle-negative", idle.waitUntilCompleted(-1, TimeUnit.MILLISECONDS));
      row("idle-timeout", idle.waitUntilCompleted(1, TimeUnit.MILLISECONDS));
      row("idle-state", idle.getState());
      row("idle-listeners-after-timeout", ((List<?>) LISTENERS.get(idle)).size());
      idle.close();
      row("closed-before-wait", idle.waitUntilCompleted(Long.MAX_VALUE, TimeUnit.DAYS));

      InstrumentProfileConnection interrupted = connection(file);
      AtomicReference<Boolean> cancelled = new AtomicReference<>();
      AtomicReference<Boolean> interruptFlag = new AtomicReference<>();
      Thread waiter =
          ownedThread(
              () -> {
                cancelled.set(interrupted.waitUntilCompleted(Long.MAX_VALUE, TimeUnit.DAYS));
                interruptFlag.set(Thread.currentThread().isInterrupted());
              },
              "bounded-ipf-interrupted-wait");
      waiter.start();
      registered(interrupted);
      waiter.interrupt();
      joined(waiter);
      row("interrupt-pending", cancelled.get());
      row("interrupt-restored", interruptFlag.get());
      row("interrupt-does-not-close", interrupted.getState());
      row("interrupt-listeners-after", ((List<?>) LISTENERS.get(interrupted)).size());
      interrupted.close();

      InstrumentProfileConnection closing = connection(file);
      AtomicReference<Boolean> closed = new AtomicReference<>();
      Thread closedWaiter =
          ownedThread(
              () -> closed.set(closing.waitUntilCompleted(Long.MAX_VALUE, TimeUnit.DAYS)),
              "bounded-ipf-close-wait");
      closedWaiter.start();
      registered(closing);
      closing.close();
      joined(closedWaiter);
      row("close-pending", closed.get());

      InstrumentProfileConnection beforeStart = connection(file);
      AtomicReference<Boolean> result = new AtomicReference<>();
      Thread beforeStartWaiter =
          ownedThread(
              () -> result.set(beforeStart.waitUntilCompleted(3, TimeUnit.SECONDS)),
              "bounded-ipf-before-start-wait");
      beforeStartWaiter.start();
      registered(beforeStart);
      Thread handler = start(beforeStart);
      joined(beforeStartWaiter);
      row("wait-before-start", result.get());
      row("completed-zero", beforeStart.waitUntilCompleted(0, TimeUnit.MILLISECONDS));
      row("completed-negative", beforeStart.waitUntilCompleted(-1, TimeUnit.MILLISECONDS));
      Thread.currentThread().interrupt();
      row("completed-before-interrupt", beforeStart.waitUntilCompleted(1, TimeUnit.SECONDS));
      row("completed-preserves-interrupt", Thread.interrupted());
      retire(beforeStart, handler);
      row("completed-before-close", beforeStart.waitUntilCompleted(0, TimeUnit.MILLISECONDS));

      InstrumentProfileConnection callback = connection(file);
      AtomicReference<Boolean> selfWait = new AtomicReference<>();
      AtomicReference<Boolean> callbackCompleted = new AtomicReference<>();
      AtomicReference<Boolean> callbackClosed = new AtomicReference<>();
      callback.addStateChangeListener(
          event -> {
            if (event.getNewValue() == InstrumentProfileConnection.State.CONNECTED) {
              selfWait.set(callback.waitUntilCompleted(5, TimeUnit.MILLISECONDS));
            } else if (event.getNewValue() == InstrumentProfileConnection.State.COMPLETED) {
              callbackCompleted.set(callback.waitUntilCompleted(0, TimeUnit.MILLISECONDS));
              callback.close();
              callbackClosed.set(callback.waitUntilCompleted(0, TimeUnit.MILLISECONDS));
            }
          });
      Thread callbackHandler = start(callback);
      row("outer-wait-callback-close", callback.waitUntilCompleted(3, TimeUnit.SECONDS));
      retire(callback, callbackHandler);
      row("worker-pending-finite-self-wait", selfWait.get());
      row("worker-completed-immediate", callbackCompleted.get());
      row("worker-completed-then-close", callbackClosed.get());
      row("callback-final-state", callback.getState());
      Files.write(file, new byte[0]);
      InstrumentProfileConnection empty = connection(file);
      Thread emptyHandler = start(empty);
      row("empty-first-snapshot", empty.waitUntilCompleted(3, TimeUnit.SECONDS));
      retire(empty, emptyHandler);
    } finally {
      for (InstrumentProfileConnection value : CONNECTIONS) {
        Thread active = (Thread) WORKER.get(value);
        if (active != null && !THREADS.contains(active)) THREADS.add(active);
        value.close();
      }
      for (Thread value : THREADS) value.interrupt();
      for (Thread value : THREADS) value.join(4000);
      Files.delete(file);
      for (Thread value : THREADS) {
        if (value.isAlive()) throw new AssertionError("owned thread did not retire");
      }
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0 || (args.length == 1 && "--help".equals(args[0]))) {
      System.out.println(
          "CompletionWaitOracle --run | --help | --help-ru\n"
              + "Observe pinned QD first-snapshot waits using one owned regular temporary IPF"
              + " file.\n"
              + "No args/help starts no work. --run: finite waits, owned/joined threads, file"
              + " deleted;\n"
              + "stdout TSV; exit0 success, exit1 exception, exit2 invalid arguments. No network.");
      return;
    }
    if (args.length == 1 && "--help-ru".equals(args[0])) {
      System.out.println(
          "CompletionWaitOracle --run | --help | --help-ru\n"
              + "Наблюдает first-snapshot waits закреплённого QD на собственном временном regular"
              + " IPF.\n"
              + "Без аргументов/help работа не начинается. --run: конечные waits, свои joined"
              + " threads,\n"
              + "файл удаляется; stdout TSV; exit0 успех, exit1 exception, exit2 неверные"
              + " аргументы. Без сети.");
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
