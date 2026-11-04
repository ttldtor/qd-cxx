// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.dxfeed.ipf.InstrumentProfile;
import com.dxfeed.ipf.live.InstrumentProfileCollector;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

/** Bounded production-QD collector observations. No input or network is used. */
public final class CollectorOracle {
  private static InstrumentProfile profile(String symbol, String type) {
    InstrumentProfile value = new InstrumentProfile();
    value.setSymbol(symbol);
    value.setType(type);
    value.setDescription("standard");
    value.setField("Custom", "owned\u03a9");
    return value;
  }

  private static void row(String name, Object value) {
    System.out.println(name + "\t" + value);
  }

  private static String consume(Iterator<InstrumentProfile> iterator) {
    List<String> values = new ArrayList<>();
    int count = 0;
    while (iterator.hasNext()) {
      if (++count > 32) {
        throw new AssertionError("bounded iterator exceeded");
      }
      InstrumentProfile p = iterator.next();
      values.add(
          p.getType()
              + ":"
              + p.getSymbol()
              + ":"
              + p.getDescription()
              + ":"
              + p.getField("Custom"));
    }
    return String.join(",", values);
  }

  public static void main(String[] args) {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      System.out.println("CollectorOracle --run | --help | --help-ru");
      System.out.println(
          "Print finite TSV observations from pinned QD classes; no files/network/workers.");
      System.out.println(
          "No args/help do no oracle work. Exit 0 success; invalid args/failure nonzero.");
      return;
    }
    if (args.length == 1 && args[0].equals("--help-ru")) {
      System.out.println("CollectorOracle --run | --help | --help-ru");
      System.out.println("Конечные TSV наблюдения закреплённого QD; без файлов/сети/потоков.");
      System.out.println("Без аргументов/справка не запускают oracle. Код 0: успех; иной: ошибка.");
      return;
    }
    if (args.length != 1 || !args[0].equals("--run")) {
      throw new IllegalArgumentException("use --help");
    }
    InstrumentProfileCollector c = new InstrumentProfileCollector();
    c.setExecutor(Runnable::run);
    List<String> notifications = new ArrayList<>();
    c.addUpdateListener(it -> notifications.add(consume(it)));
    row("empty-listener-calls", notifications.size());
    InstrumentProfile a = profile("A", "STOCK");
    Object g1 = new Object();
    Object g2 = new Object();
    c.updateInstrumentProfiles(Collections.singletonList(a), g1);
    row("insert", notifications.get(0));
    long before = c.getLastUpdateTime();
    c.updateInstrumentProfiles(Collections.singletonList(new InstrumentProfile(a)), g2);
    row("equal-calls", notifications.size());
    row("equal-time-unchanged", c.getLastUpdateTime() == before);
    c.removeGenerations(Collections.singleton(g1));
    row("old-generation-calls", notifications.size());
    c.removeGenerations(Collections.singleton(g2));
    row("new-generation-removal", notifications.get(notifications.size() - 1));
    row("changed-time-floor", c.getLastUpdateTime() >= before + 1000);
    c.updateInstrumentProfile(profile("absent", "REMOVED"));
    row("absent-removed-calls", notifications.size());
    row("empty-view", !c.view().iterator().hasNext());
    c.updateInstrumentProfile(a);
    List<String> initial = new ArrayList<>();
    c.addUpdateListener(it -> initial.add(consume(it)));
    row("initial", initial.get(0));
    InstrumentProfile removed = profile("A", "REMOVED");
    c.updateInstrumentProfile(removed);
    row("supplied-removal", notifications.get(notifications.size() - 1));

    InstrumentProfileCollector queued = new InstrumentProfileCollector();
    ArrayDeque<Runnable> queue = new ArrayDeque<>();
    queued.setExecutor(queue::add);
    List<String> output = new ArrayList<>();
    queued.addUpdateListener(it -> output.add(consume(it)));
    InstrumentProfile old = profile("A", "STOCK");
    InstrumentProfile fresh = new InstrumentProfile(old);
    fresh.setDescription("latest");
    queued.updateInstrumentProfiles(
        Arrays.asList(old, fresh, profile("B", "STOCK"), profile("B", "REMOVED")), null);
    int turns = 0;
    while (!queue.isEmpty()) {
      if (++turns > 32) {
        throw new AssertionError("bounded executor exceeded");
      }
      queue.remove().run();
    }
    row("queued-latest", output.get(0));
    row("queued-calls", output.size());
    InstrumentProfileCollector referenceBase = new InstrumentProfileCollector();
    referenceBase.setExecutor(Runnable::run);
    List<String> baseNotifications = new ArrayList<>();
    referenceBase.addUpdateListener(it -> baseNotifications.add(consume(it)));
    referenceBase.updateInstrumentProfile(a);
    referenceBase.updateInstrumentProfile(a);
    row("base-same-instance-calls", baseNotifications.size());
    InstrumentProfileCollector defensive =
        new InstrumentProfileCollector() {
          @Override
          protected InstrumentProfile copyInstrumentProfile(InstrumentProfile input) {
            return new InstrumentProfile(input);
          }
        };
    defensive.setExecutor(Runnable::run);
    List<String> defensiveNotifications = new ArrayList<>();
    defensive.addUpdateListener(it -> defensiveNotifications.add(consume(it)));
    defensive.updateInstrumentProfile(a);
    defensive.updateInstrumentProfile(a);
    row("defensive-equal-resubmit-calls", defensiveNotifications.size());
    row("collector-class", InstrumentProfileCollector.class.getName());
  }
}
