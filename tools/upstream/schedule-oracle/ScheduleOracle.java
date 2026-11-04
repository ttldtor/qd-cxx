// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.util.TimeFormat;
import com.dxfeed.ipf.InstrumentProfile;
import com.dxfeed.schedule.Schedule;
import java.io.BufferedWriter;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.PrimitiveIterator;

/**
 * Reads the actual production Schedule factories and their constructed internal rule definitions.
 */
public final class ScheduleOracle {
  private static final int MAX_CASES = 4096;

  private ScheduleOracle() {}

  private static String hex(String text) {
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < text.length(); ++i) {
      String cell = Integer.toHexString(text.charAt(i));
      for (int n = cell.length(); n < 4; ++n) {
        result.append('0');
      }
      result.append(cell);
    }
    return result.toString();
  }

  private static String text(String value) {
    if (value.equals("-")) {
      return "";
    }
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < value.length(); i += 4) {
      result.append((char) Integer.parseInt(value.substring(i, i + 4), 16));
    }
    return result.toString();
  }

  private static Object field(Object object, String name) throws ReflectiveOperationException {
    Field value = object.getClass().getDeclaredField(name);
    value.setAccessible(true);
    return value.get(object);
  }

  private static String time(Object value) throws ReflectiveOperationException {
    return field(value, "day")
        + ":"
        + field(value, "hour")
        + ":"
        + field(value, "minute")
        + ":"
        + field(value, "second");
  }

  private static String day(Object value) throws ReflectiveOperationException {
    if (value == null) {
      return "-";
    }
    StringBuilder result = new StringBuilder();
    result
        .append('[')
        .append(time(field(value, "dayStart")))
        .append('>')
        .append(time(field(value, "dayEnd")))
        .append('/')
        .append(time(field(value, "resetTime")))
        .append('{');
    for (Object session : (Object[]) field(value, "sessions")) {
      result
          .append(field(session, "type"))
          .append(':')
          .append(time(field(session, "start")))
          .append('>')
          .append(time(field(session, "end")))
          .append(',');
    }
    return result.append("}]").toString();
  }

  private static long[] setValues(Object set) throws ReflectiveOperationException {
    Method method = set.getClass().getMethod("longIterator");
    method.setAccessible(true);
    PrimitiveIterator.OfLong iterator = (PrimitiveIterator.OfLong) method.invoke(set);
    List<Long> values = new ArrayList<>();
    while (iterator.hasNext()) {
      values.add(iterator.nextLong());
    }
    long[] result = new long[values.size()];
    for (int i = 0; i < result.length; ++i) {
      result[i] = values.get(i);
    }
    Arrays.sort(result);
    return result;
  }

  private static String days(Object set) throws ReflectiveOperationException {
    StringBuilder result = new StringBuilder();
    for (long value : setValues(set)) {
      result.append(value).append(',');
    }
    return result.toString();
  }

  private static String strategies(Object map) throws ReflectiveOperationException {
    Object keys = map.getClass().getMethod("longKeySet").invoke(map);
    StringBuilder result = new StringBuilder();
    for (long key : setValues(keys)) {
      Object strategy = map.getClass().getMethod("get", long.class).invoke(map, key);
      result
          .append(key)
          .append('=')
          .append(field(strategy, "isForcedTradingDay"))
          .append(',')
          .append(field(strategy, "isClosed"))
          .append(',')
          .append(field(strategy, "isEarlyClose"))
          .append(',')
          .append(field(strategy, "earlyCloseOffset"))
          .append(',')
          .append(day(field(strategy, "dayDef")))
          .append(';');
    }
    return result.toString();
  }

  private static String rules(Schedule schedule) throws ReflectiveOperationException {
    StringBuilder week = new StringBuilder();
    for (Object value : (Object[]) field(schedule, "weekDays")) {
      week.append(day(value)).append(';');
    }
    return hex(schedule.getDefinition())
        + "\t"
        + hex(schedule.getName())
        + "\t"
        + hex(schedule.getTimeZone().getID())
        + "\t"
        + field(schedule, "dayOffset")
        + "\t"
        + field(schedule, "earlyClose")
        + "\t"
        + field(schedule, "joinNextTradingDay")
        + "\t"
        + days(field(schedule, "holidays"))
        + "\t"
        + days(field(schedule, "shortdays"))
        + "\t"
        + week
        + "\t"
        + strategies(field(schedule, "dayOverrides"));
  }

  private static void update(String version, String entries) throws Exception {
    StringBuilder data = new StringBuilder("date=");
    data.append(TimeFormat.GMT.format(Long.parseLong(version))).append('\n');
    if (!entries.equals("-")) {
      for (String entry : entries.split("\\|", -1)) {
        String[] cells = entry.split("/", -1);
        String prefix;
        switch (cells[0]) {
          case "H":
            prefix = "hd.";
            break;
          case "S":
            prefix = "sd.";
            break;
          case "X":
            prefix = "xd.";
            break;
          case "T":
            prefix = "tv.";
            break;
          default:
            throw new IllegalArgumentException("Unknown typed entry kind");
        }
        data.append(prefix).append(text(cells[1])).append('=').append(text(cells[2])).append('\n');
      }
    }
    Schedule.setDefaults(data.toString().getBytes(StandardCharsets.UTF_8));
  }

  private static void help(boolean russian) {
    if (russian) {
      System.out.println(
          "ScheduleOracle CASES OUTPUT: фактические фабрики/внутренние правила закреплённого QD.\n"
              + "CASES: UTF-8 TSV, максимум 4096 строк; OUTPUT: новый UTF-8 TSV наблюдений.\n"
              + "Нужны production classes/JDK8; запускать через Invoke-Oracle.ps1 с heap/timeout.\n"
              + "Без args/--help/--help-ru: справка без чтения inputs/работы QD. Exit0 успех;"
              + " прочие ошибки nonzero.\n"
              + "Изменяется только OUTPUT и глобальные defaults собственного JVM; сети/записи"
              + " reference нет.");
    } else {
      System.out.println(
          "ScheduleOracle CASES OUTPUT: actual pinned QD factories and constructed rule models.\n"
              + "CASES: UTF-8 TSV, at most 4096 rows; OUTPUT: new UTF-8 observation TSV.\n"
              + "Requires existing production classes/JDK8; use Invoke-Oracle.ps1 for heap/timeout"
              + " bounds.\n"
              + "No args/--help/--help-ru: input-free help, no input reads or QD work. Exit0"
              + " success; other failures nonzero.\n"
              + "Writes OUTPUT and changes only its own JVM defaults; no network/reference"
              + " writes.");
    }
  }

  /** Runs a finite observation batch; this tool does not reimplement the schedule parser. */
  public static void main(String[] args) throws Exception {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      help(false);
      return;
    }
    if (args.length == 1 && args[0].equals("--help-ru")) {
      help(true);
      return;
    }
    if (args.length != 2) {
      throw new IllegalArgumentException("Use --help");
    }
    Path input = Paths.get(args[0]);
    if (Files.size(input) > 16 * 1024 * 1024) {
      throw new IllegalArgumentException("Case budget exceeded");
    }
    List<String> cases = Files.readAllLines(input, StandardCharsets.UTF_8);
    if (cases.size() > MAX_CASES) {
      throw new IllegalArgumentException("Case budget exceeded");
    }
    try (BufferedWriter output =
        Files.newBufferedWriter(Paths.get(args[1]), StandardCharsets.UTF_8)) {
      int written = 0;
      for (String line : cases) {
        String[] cells = line.split("\t", -1);
        String observation;
        try {
          if (cells.length != 6) {
            throw new IllegalArgumentException("Expected six case cells");
          }
          String definition = text(cells[3]);
          InstrumentProfile profile = new InstrumentProfile();
          profile.setTradingHours(definition);
          if (cells[1].equals("V")) {
            StringBuilder venues = new StringBuilder();
            for (String venue : Schedule.getTradingVenues(profile)) {
              venues.append(hex(venue)).append(',');
            }
            observation = "VENUES\t" + cells[0] + "\t" + venues;
          } else {
            if (cells[1].equals("U")) {
              update(cells[4], cells[5]);
            }
            Schedule schedule =
                cells[1].equals("N")
                    ? Schedule.getInstance(profile, text(cells[2]))
                    : Schedule.getInstance(definition);
            observation = "OK\t" + cells[0] + "\t" + rules(schedule);
          }
        } catch (Exception exception) {
          if (!(exception instanceof IllegalArgumentException)
              && !(exception instanceof java.util.NoSuchElementException)) {
            throw exception;
          }
          observation = "ERROR\t" + cells[0] + "\t" + exception.getClass().getSimpleName();
        }
        if (observation.length() + 1 > 16 * 1024 * 1024 - written) {
          throw new IllegalArgumentException("Output budget exceeded");
        }
        written += observation.length() + 1;
        output.write(observation);
        output.write('\n');
      }
    }
  }
}
