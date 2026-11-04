// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.schedule.Schedule;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.List;

/** Captures bounded observations from actual production schedule filters. */
public final class ScheduleFilterOracle {
  private static final int MAX_CASES = 4096;
  private static final long MAX_BYTES = 16 * 1024 * 1024;

  private ScheduleFilterOracle() {}

  private static void help(boolean russian) {
    String text =
        russian
            ? "ScheduleFilterOracle CASES OUTPUT: наблюдения фильтров Java QD.\n"
                + "CASES: UTF-8 TSV, не более 4096 строк и 16 MiB. OUTPUT: новый файл UTF-8 LF.\n"
                + "Требуются готовые production classes и JDK8; без сети и изменения входов.\n"
                + "Без аргументов, --help, --help-ru: справка без входов и вычисления"
                + " расписания.\n"
                + "Код 0: успех; ненулевой: ошибка входа, записи или Java QD; возможен неполный"
                + " OUTPUT."
            : "ScheduleFilterOracle CASES OUTPUT: capture actual Java QD filter observations.\n"
                + "CASES: UTF-8 TSV, at most 4096 rows and 16 MiB. OUTPUT: new UTF-8 LF file.\n"
                + "Requires built production classes and JDK8; no network or input"
                + " modification.\n"
                + "No arguments, --help, --help-ru: help without inputs or schedule"
                + " computation.\n"
                + "Exit 0: success; nonzero: input, output or Java QD failure; OUTPUT can be"
                + " partial.";
    byte[] utf8 = (text + "\n").getBytes(StandardCharsets.UTF_8);
    System.out.write(utf8, 0, utf8.length);
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || (args.length == 1 && (args[0].equals("--help") || args[0].equals("--help-ru")))) {
      help(args.length == 1 && args[0].equals("--help-ru"));
      return;
    }
    if (args.length != 2) {
      throw new IllegalArgumentException("Expected CASES OUTPUT; use --help");
    }
    Path input = Paths.get(args[0]);
    Path output = Paths.get(args[1]);
    if (Files.size(input) > MAX_BYTES || Files.exists(output)) {
      throw new IllegalArgumentException("Input exceeds limit or output exists");
    }
    List<String> rows = Files.readAllLines(input, StandardCharsets.UTF_8);
    if (rows.size() > MAX_CASES) {
      throw new IllegalArgumentException("Too many cases");
    }
    try (BufferedWriter writer =
        Files.newBufferedWriter(
            output,
            StandardCharsets.UTF_8,
            StandardOpenOption.CREATE_NEW,
            StandardOpenOption.WRITE)) {
      for (String line : rows) {
        String[] cells = line.split("\t", -1);
        if (cells.length != 6 || !cells[1].equals("F") || cells[3].length() % 4 != 0) {
          throw new IllegalArgumentException("Invalid fixture row");
        }
        StringBuilder definition = new StringBuilder();
        for (int i = 0; i < cells[3].length(); i += 4) {
          definition.append((char) Integer.parseInt(cells[3].substring(i, i + 4), 16));
        }
        com.dxfeed.schedule.Day day =
            Schedule.getInstance(definition.toString())
                .getDayByYearMonthDay(Integer.parseInt(cells[4]));
        if (cells[5].equals("B")) {
          com.dxfeed.schedule.Day match = day.findNextDay(com.dxfeed.schedule.DayFilter.HOLIDAY);
          writer.write(
              "BOUNDARY\t" + cells[0] + "\t" + (match == null ? "NONE" : match.getYearMonthDay()));
        } else {
          writer.write("FILTER\t" + cells[0] + "\t" + FilterObservation.observe(day));
        }
        writer.write('\n');
      }
    }
  }
}
