// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.util.TimeFormat;
import com.dxfeed.schedule.Day;
import com.dxfeed.schedule.DayFilter;
import com.dxfeed.schedule.Schedule;
import com.dxfeed.schedule.Session;
import com.dxfeed.schedule.SessionFilter;
import java.io.BufferedWriter;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.DayOfWeek;
import java.time.Month;
import java.time.zone.ZoneOffsetTransition;
import java.time.zone.ZoneOffsetTransitionRule;
import java.time.zone.ZoneRules;
import java.util.List;
import java.util.TimeZone;

/**
 * Observes actual production query methods; this tool contains no substitute Schedule algorithm.
 */
public final class ScheduleQueryOracle {
  private static final int MAX_CASES = 4096;
  private static final int MAX_BYTES = 16 * 1024 * 1024;
  private static final long[] ZONE_INSTANTS = {
    -2208988800001L, -2208988800000L, 0L, 1767225600000L, 1782864000000L, 1793793600000L
  };

  private ScheduleQueryOracle() {}

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

  private static int bit(boolean value) {
    return value ? 1 : 0;
  }

  private static String core(Day day) {
    return day.getDayId()
        + ","
        + day.getYearMonthDay()
        + ","
        + day.getYear()
        + ","
        + day.getMonthOfYear()
        + ","
        + day.getDayOfMonth()
        + ","
        + day.getDayOfWeek()
        + ","
        + bit(day.isHoliday())
        + ","
        + bit(day.isShortDay())
        + ","
        + bit(day.isTrading())
        + ","
        + day.getStartTime()
        + ","
        + day.getEndTime()
        + ","
        + day.getResetTime();
  }

  private static String core(Session session) {
    return session.getDay().getDayId()
        + ":"
        + session.getType().name()
        + ":"
        + session.getStartTime()
        + ":"
        + session.getEndTime()
        + ":"
        + bit(session.isTrading())
        + ":"
        + bit(session.isEmpty());
  }

  private interface Observation {
    String read() throws Exception;
  }

  private static String semantic(Observation operation) throws Exception {
    try {
      return operation.read();
    } catch (IllegalArgumentException | java.util.NoSuchElementException exception) {
      return "ERROR:" + exception.getClass().getSimpleName();
    }
  }

  private static String day(Day value) throws Exception {
    StringBuilder sessions = new StringBuilder();
    for (Session session : value.getSessions()) {
      sessions
          .append(core(session))
          .append(':')
          .append(bit(session.containsTime(session.getStartTime())))
          .append(':')
          .append(bit(session.containsTime(session.getEndTime())))
          .append(';');
    }
    Session first = value.getFirstSession(SessionFilter.ANY);
    Session last = value.getLastSession(SessionFilter.ANY);
    return core(value)
        + "\t"
        + sessions
        + "\t"
        + semantic(() -> core(value.getPrevDay(DayFilter.ANY)))
        + "\t"
        + semantic(() -> core(value.getNextDay(DayFilter.ANY)))
        + "\t"
        + core(first)
        + "\t"
        + core(last)
        + "\t"
        + semantic(() -> core(first.getPrevSession(SessionFilter.ANY)))
        + "\t"
        + semantic(() -> core(last.getNextSession(SessionFilter.ANY)))
        + "\t"
        + bit(value.containsTime(value.getStartTime()))
        + ","
        + bit(value.containsTime(value.getEndTime()));
  }

  private static String session(Session value) throws Exception {
    return core(value)
        + "\t"
        + core(value.getDay())
        + "\t"
        + semantic(() -> core(value.getPrevSession(SessionFilter.ANY)))
        + "\t"
        + semantic(() -> core(value.getNextSession(SessionFilter.ANY)));
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

  private static void checkTransition(TimeZone zone, ZoneOffsetTransition transition) {
    long time = transition.toEpochSecond() * 1000;
    if (zone.getOffset(time - 1) != transition.getOffsetBefore().getTotalSeconds() * 1000
        || zone.getOffset(time) != transition.getOffsetAfter().getTotalSeconds() * 1000) {
      throw new IllegalStateException(
          "Legacy ZoneRules/production TimeZone differs: " + zone.getID() + " " + transition);
    }
  }

  private static void identities(Path idsPath, Path metadataPath, Path legacyPath)
      throws Exception {
    if (Files.size(idsPath) > 65536 || Files.exists(metadataPath) || Files.exists(legacyPath)) {
      throw new IllegalArgumentException("Invalid identity observation paths/size");
    }
    List<String> ids = Files.readAllLines(idsPath, StandardCharsets.UTF_8);
    if (ids.size() != 631) {
      throw new IllegalArgumentException("Expected complete631 identity input");
    }
    try (BufferedWriter metadata = Files.newBufferedWriter(metadataPath, StandardCharsets.UTF_8);
        BufferedWriter legacy = Files.newBufferedWriter(legacyPath, StandardCharsets.UTF_8)) {
      for (String id : ids) {
        TimeZone zone = TimeZone.getTimeZone(id);
        if (!zone.getID().equals(id)) {
          throw new IllegalStateException("Identity normalization differs: " + id);
        }
        StringBuilder row =
            new StringBuilder(
                id
                    + "\t"
                    + zone.toZoneId().getId()
                    + "\t"
                    + zone.getRawOffset()
                    + "\t"
                    + zone.getDSTSavings()
                    + "\t"
                    + zone.useDaylightTime());
        for (long instant : ZONE_INSTANTS) {
          row.append('\t').append(zone.getOffset(instant));
        }
        metadata.write(row.toString());
        metadata.write('\n');
        if (id.startsWith("SystemV/")) {
          ZoneRules rules = zone.toZoneId().getRules();
          for (ZoneOffsetTransition transition : rules.getTransitions()) {
            checkTransition(zone, transition);
            legacy.write(
                id
                    + "\t"
                    + transition.toEpochSecond()
                    + "\t"
                    + transition.getOffsetBefore().getTotalSeconds()
                    + "\t"
                    + transition.getOffsetAfter().getTotalSeconds()
                    + "\n");
          }
          for (ZoneOffsetTransitionRule rule : rules.getTransitionRules()) {
            boolean spring = rule.getMonth() == Month.APRIL && rule.getDayOfMonthIndicator() == 24;
            boolean autumn =
                rule.getMonth() == Month.OCTOBER && rule.getDayOfMonthIndicator() == 25;
            if ((!spring && !autumn)
                || rule.getDayOfWeek() != DayOfWeek.SUNDAY
                || rule.getLocalTime().toSecondOfDay() != 7200
                || rule.getTimeDefinition() != ZoneOffsetTransitionRule.TimeDefinition.WALL
                || rule.getStandardOffset().getTotalSeconds() * 1000 != zone.getRawOffset()) {
              throw new IllegalStateException(
                  "Unexpected legacy recurring profile: " + id + " " + rule);
            }
            for (int year : new int[] {1978, 1987, 2007, 2024, 2038, 2100, 9999}) {
              checkTransition(zone, rule.createTransition(year));
            }
          }
        }
      }
    }
  }

  private static String observe(String[] cells) throws Exception {
    String mode = cells[1];
    long argument = Long.parseLong(cells[4]);
    String definition = text(cells[3]);
    if (mode.equals("R")) {
      TimeZone zone = TimeZone.getTimeZone(definition);
      if (!zone.getID().equals(definition)) {
        throw new IllegalArgumentException("Unknown exact identity");
      }
      int[] offsets = new int[2];
      Method method = zone.getClass().getMethod("getOffsets", long.class, int[].class);
      int total = (Integer) method.invoke(zone, argument, offsets);
      return "RAW\t"
          + cells[0]
          + "\t"
          + hex(zone.getID())
          + "\t"
          + offsets[0]
          + "\t"
          + offsets[1]
          + "\t"
          + total
          + "\t"
          + zone.getRawOffset();
    }
    if (mode.equals("H")) {
      String[] updates = cells[5].split("~", -1);
      update("1780000000000", updates[0]);
      Schedule schedule = Schedule.getInstance(definition);
      Day old = schedule.getDayById((int) argument);
      Session oldSession = old.getFirstSession(SessionFilter.ANY);
      String before = core(old);
      String beforeNext = semantic(() -> core(old.getNextDay(DayFilter.ANY)));
      update("1780000001000", updates[1]);
      Day current = schedule.getDayById((int) argument);
      return "HOT\t"
          + cells[0]
          + "\t"
          + before
          + "\t"
          + core(old)
          + "\t"
          + core(current)
          + "\t"
          + core(oldSession)
          + "\t"
          + bit(old.equals(current))
          + "\t"
          + bit(oldSession.equals(current.getFirstSession(SessionFilter.ANY)))
          + "\t"
          + hex(old.getSchedule().getName())
          + "\t"
          + beforeNext
          + "\t"
          + semantic(() -> core(old.getNextDay(DayFilter.ANY)));
    }
    Schedule schedule = Schedule.getInstance(definition);
    switch (mode) {
      case "D":
        return "DAY\t" + cells[0] + "\t" + day(schedule.getDayById((int) argument));
      case "Y":
        return "DAY\t" + cells[0] + "\t" + day(schedule.getDayByYearMonthDay((int) argument));
      case "T":
        return "DAY\t" + cells[0] + "\t" + day(schedule.getDayByTime(argument));
      case "S":
        return "SESSION\t" + cells[0] + "\t" + session(schedule.getSessionByTime(argument));
      default:
        throw new IllegalArgumentException("Unknown query mode");
    }
  }

  private static void help(boolean russian) {
    if (russian) {
      System.out.println(
          "ScheduleQueryOracle CASES OUTPUT IDS METADATA LEGACY: фактические Schedule/Day/Session и"
              + " timezone-конвенции Java8.\n"
              + "CASES: UTF8 TSV6полей, максимум4096строк/16MiB; IDS:631закреплённых ID/64KiB.\n"
              + "OUTPUT/METADATA/LEGACY должны быть новыми файлами; результат UTF8, максимум16MiB"
              + " основной таблицы.\n"
              + "Нужны готовые production classes/JDK8. Используйте Invoke-Oracle.ps1 для"
              + " heap128MiB/timeout120s.\n"
              + "Без аргументов/--help/--help-ru: справка без чтения inputs/инициализацииQD. Exit0"
              + " успех/справка; ошибки nonzero.\n"
              + "Меняются только собственные outputs и JVMdefaults; сети, загрузок и upstreamзаписи"
              + " нет.");
    } else {
      System.out.println(
          "ScheduleQueryOracle CASES OUTPUT IDS METADATA LEGACY: actual Java8 Schedule/Day/Session"
              + " and timezone conventions.\n"
              + "CASES: UTF8 six-field TSV, at most4096rows/16MiB; IDS:631pinned IDs/64KiB.\n"
              + "OUTPUT/METADATA/LEGACY must be new files; UTF8 results, at most16MiB main table.\n"
              + "Requires existing production classes/JDK8. Use Invoke-Oracle.ps1 for"
              + " heap128MiB/timeout120s.\n"
              + "Noargs/--help/--help-ru: input-free help without input reads/QD initialization."
              + " Exit0 success/help; failures nonzero.\n"
              + "Writes only owned outputs and own JVMdefaults; no network/download/upstream"
              + " writes.");
    }
  }

  /**
   * Reads actual production methods and immutable identity observations, without algorithm
   * substitution.
   */
  public static void main(String[] args) throws Exception {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      help(false);
      return;
    }
    if (args.length == 1 && args[0].equals("--help-ru")) {
      help(true);
      return;
    }
    if (args.length != 5) {
      throw new IllegalArgumentException("Use --help");
    }
    Path input = Paths.get(args[0]);
    Path output = Paths.get(args[1]);
    if (Files.size(input) > MAX_BYTES || Files.exists(output)) {
      throw new IllegalArgumentException("Invalid case/output paths or budget");
    }
    List<String> cases = Files.readAllLines(input, StandardCharsets.UTF_8);
    if (cases.size() > MAX_CASES) {
      throw new IllegalArgumentException("Case budget exceeded");
    }
    identities(Paths.get(args[2]), Paths.get(args[3]), Paths.get(args[4]));
    try (BufferedWriter writer = Files.newBufferedWriter(output, StandardCharsets.UTF_8)) {
      int written = 0;
      for (String line : cases) {
        String[] cells = line.split("\t", -1);
        if (cells.length != 6) {
          throw new IllegalArgumentException("Expected six cells");
        }
        String result;
        try {
          result = observe(cells);
        } catch (IllegalArgumentException | java.util.NoSuchElementException exception) {
          result = "ERROR\t" + cells[0] + "\t" + exception.getClass().getSimpleName();
        }
        if (result.length() + 1 > MAX_BYTES - written) {
          throw new IllegalArgumentException("Output budget exceeded");
        }
        written += result.length() + 1;
        writer.write(result);
        writer.write('\n');
      }
    }
  }
}
